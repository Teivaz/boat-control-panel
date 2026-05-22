#include "config_mode.h"

#include "config.h"
#include "controller.h"
#include "libcomm.h"
#include "task_ids.h"

#include <xc.h>

/* Switching-board config-protocol addresses. Mirrors board1-switching/config.h;
 * kept local because main does not link against the switching board's
 * sources. Values match `CONFIG_ADDR_*` over there. */
#define SWITCHING_WATER_CAL 0x10
#define SWITCHING_FUEL_CAL 0x11
#define SWITCHING_BATTERY_CAL 0x12
#define SWITCHING_LEVEL_MODE 0x13

/* Button-board LED brightness address. Mirrors board2-buttons/config.h:
 *   CONFIG_ADDR_LED_BRIGHTNESS = 0x10 + BUTTON_COUNT(7)*sizeof(CommTriggerConfig)(1)
 *                              + sizeof(CommButtonEffect)(4) = 0x1B */
#define BUTTON_LED_BRIGHTNESS 0x1B

/* Main-board indicator brightness — local config addr.  Routed through
 * controller_read/write_config like remote items; the controller
 * short-circuits to in-RAM config when the board address is our own. */
#define MAIN_INDICATOR_BRIGHTNESS CONFIG_ADDR_INDICATOR_BRIGHTNESS

/* Number of distinct level-meter modes (CommMeterMode enum range). */
#define LEVEL_MODE_COUNT 3u

#define SAMPLE_MS 20u
#define STABLE_TICKS 3u /* ~60 ms debounce on the RA7 switch */

/* RA7 is pulled up via WPUA7; the switch closes to ground when active. */
#define PIN_ACTIVE() (PORTAbits.RA7 == 0)

typedef enum {
    EDIT_KIND_BYTE,       /* full 8-bit value, wraps 0..255 */
    EDIT_KIND_LEVEL_MODE, /* 2-bit field inside a packed byte, cycles 0..2 */
} EditKind;

typedef struct {
    const char* label;
    uint8_t board_addr;
    uint8_t reg;
    EditKind kind;
    uint8_t bit_offset; /* for EDIT_KIND_LEVEL_MODE: 0 for meter 0, 2 for meter 1 */
} EditSpec;

/* One entry per editable ConfigMenuItem.  CONFIG_MENU_NAV has no entry
 * because it uses its own dedicated screen.  Order matches the
 * ConfigMenuItem enum so we can index directly with (menu_item - 1). */
static const EditSpec edit_specs[CONFIG_MENU_COUNT - 1u] = {
    {"WATER CAL",      COMM_ADDRESS_SWITCHING,      SWITCHING_WATER_CAL,       EDIT_KIND_BYTE,       0},
    {"FUEL CAL",       COMM_ADDRESS_SWITCHING,      SWITCHING_FUEL_CAL,        EDIT_KIND_BYTE,       0},
    {"BATTERY CAL",    COMM_ADDRESS_SWITCHING,      SWITCHING_BATTERY_CAL,     EDIT_KIND_BYTE,       0},
    {"WATER MODE",     COMM_ADDRESS_SWITCHING,      SWITCHING_LEVEL_MODE,      EDIT_KIND_LEVEL_MODE, 0},
    {"FUEL MODE",      COMM_ADDRESS_SWITCHING,      SWITCHING_LEVEL_MODE,      EDIT_KIND_LEVEL_MODE, 2},
    {"BRIGHTNESS L",   COMM_ADDRESS_BUTTON_BOARD_L, BUTTON_LED_BRIGHTNESS,     EDIT_KIND_BYTE,       0},
    {"BRIGHTNESS R",   COMM_ADDRESS_BUTTON_BOARD_R, BUTTON_LED_BRIGHTNESS,     EDIT_KIND_BYTE,       0},
    {"BRIGHTNESS IND", COMM_ADDRESS_MAIN,           MAIN_INDICATOR_BRIGHTNESS, EDIT_KIND_BYTE,       0},
};

static volatile uint8_t active;
static volatile uint8_t raw_last;
static volatile uint8_t raw_stable;
static volatile uint8_t stable_ticks;

static volatile uint8_t screen;       /* ConfigScreen */
static volatile uint8_t menu_cursor;  /* 0..CONFIG_MENU_COUNT-1 */
static volatile uint8_t nav_cursor;   /* 0..4 */
static volatile uint8_t working_mask; /* live nav-enabled mask */

/* Edit-screen state.  edit_menu_item identifies the item being edited
 * (>= CONFIG_MENU_WATER_CAL).  edit_value is the user-visible working
 * value; edit_loaded gates rendering until the initial read response
 * lands.  edit_stash holds the unmodified bits we need to preserve when
 * writing back a packed byte (currently just level-mode's other 2 bits). */
static volatile uint8_t edit_menu_item;
static volatile uint8_t edit_value;
static volatile uint8_t edit_loaded;
static volatile uint8_t edit_stash;
/* Set while a menu commit (read or write) is in flight. Suppresses
 * re-submission until the completion fires. */
static volatile uint8_t op_busy;

static void enter(void);
static void exit_config(void);
static void enter_edit(uint8_t menu_item);
static const EditSpec* spec_for(uint8_t menu_item);
static uint8_t extract_value(const EditSpec* s, uint8_t raw_byte);
static uint8_t merge_value(const EditSpec* s, uint8_t raw_byte, uint8_t value);
static void handle_menu(MenuControl c);
static void handle_nav(MenuControl c);
static void handle_edit(MenuControl c);
static void sample_task(TaskId id, void* ctx);
static void on_edit_read_done(uint8_t ok, uint8_t value, void* ctx);
static void on_edit_write_done(uint8_t ok, void* ctx);

void config_mode_init(TaskController* ctrl) {
    active = 0;
    screen = CONFIG_SCREEN_MENU;
    menu_cursor = 0;
    nav_cursor = 0;
    edit_menu_item = CONFIG_MENU_WATER_CAL;
    edit_value = 0;
    edit_loaded = 0;
    edit_stash = 0;
    op_busy = 0;
    working_mask = config_get_nav_enabled_mask();
    raw_last = PIN_ACTIVE() ? 1 : 0;
    raw_stable = raw_last;
    stable_ticks = STABLE_TICKS;
    if (raw_stable) {
        enter();
    }
    task_controller_add(ctrl, TASK_CONFIG_MODE, SAMPLE_MS, sample_task, 0);
}

uint8_t config_mode_active(void) {
    return active;
}

uint8_t config_mode_screen(void) {
    return screen;
}

uint8_t config_mode_menu_cursor(void) {
    return menu_cursor;
}

uint8_t config_mode_nav_cursor(void) {
    if (screen == CONFIG_SCREEN_NAV) {
        return nav_cursor;
    }
    return 0xFF;
}

uint8_t config_mode_nav_working_mask(void) {
    return working_mask;
}

uint8_t config_mode_edit_menu_item(void) {
    return edit_menu_item;
}

uint8_t config_mode_edit_loaded(void) {
    return edit_loaded;
}

uint8_t config_mode_edit_value(void) {
    return edit_value;
}

void config_mode_on_action(MenuControl control) {
    if (!active) {
        return;
    }
    switch (screen) {
        case CONFIG_SCREEN_MENU:
            handle_menu(control);
            break;
        case CONFIG_SCREEN_NAV:
            handle_nav(control);
            break;
        case CONFIG_SCREEN_EDIT:
            handle_edit(control);
            break;
        default:
            break;
    }
}

static void handle_menu(MenuControl c) {
    switch (c) {
        case MENU_CONTROL_NEXT:
            /* Cursor moves up the list (decrement), wrapping at the top. */
            menu_cursor = (uint8_t)((menu_cursor + CONFIG_MENU_COUNT - 1u) % CONFIG_MENU_COUNT);
            break;
        case MENU_CONTROL_PREV:
            /* Cursor moves down the list, wrapping at the bottom. */
            menu_cursor = (uint8_t)((menu_cursor + 1u) % CONFIG_MENU_COUNT);
            break;
        case MENU_CONTROL_EXIT:
            /* Back at the top-level menu exits config; commit happens on
             * RA7 release, but we mirror it here so a software exit also
             * persists the working mask. */
            exit_config();
            break;
        case MENU_CONTROL_ENTER:
            if (menu_cursor == CONFIG_MENU_NAV) {
                screen = CONFIG_SCREEN_NAV;
                nav_cursor = 0;
            } else {
                enter_edit(menu_cursor);
            }
            break;
        default:
            break;
    }
}

static void handle_nav(MenuControl c) {
    switch (c) {
        case MENU_CONTROL_NEXT:
            nav_cursor = (uint8_t)((nav_cursor + 4u) % 5u);
            break;
        case MENU_CONTROL_PREV:
            nav_cursor = (uint8_t)((nav_cursor + 1u) % 5u);
            break;
        case MENU_CONTROL_EXIT:
            screen = CONFIG_SCREEN_MENU;
            break;
        case MENU_CONTROL_ENTER:
            working_mask ^= (uint8_t)(1u << nav_cursor);
            working_mask &= NAV_LIGHT_ALL;
            break;
        default:
            break;
    }
}

static void handle_edit(MenuControl c) {
    const EditSpec* s = spec_for(edit_menu_item);
    if (!s) {
        screen = CONFIG_SCREEN_MENU;
        return;
    }
    switch (c) {
        case MENU_CONTROL_NEXT:
            if (!edit_loaded) {
                return;
            }
            if (s->kind == EDIT_KIND_LEVEL_MODE) {
                edit_value = (uint8_t)((edit_value + 1u) % LEVEL_MODE_COUNT);
            } else {
                edit_value = (uint8_t)(edit_value + 1u);
            }
            break;
        case MENU_CONTROL_PREV:
            if (!edit_loaded) {
                return;
            }
            if (s->kind == EDIT_KIND_LEVEL_MODE) {
                edit_value = (uint8_t)((edit_value + LEVEL_MODE_COUNT - 1u) % LEVEL_MODE_COUNT);
            } else {
                edit_value = (uint8_t)(edit_value - 1u);
            }
            break;
        case MENU_CONTROL_EXIT:
            /* Cancel: discard working value and return to menu. */
            screen = CONFIG_SCREEN_MENU;
            break;
        case MENU_CONTROL_ENTER:
            /* Commit: on I2C failure stay on the screen so the user can
             * retry. Returns to the menu on success (in completion). */
            if (!edit_loaded || op_busy) {
                return;
            }
            op_busy = 1;
            uint8_t to_write = merge_value(s, edit_stash, edit_value);
            controller_write_config(s->board_addr, s->reg, to_write, on_edit_write_done, 0);
            break;
        default:
            break;
    }
}

static void enter_edit(uint8_t menu_item) {
    const EditSpec* s = spec_for(menu_item);
    if (!s) {
        return;
    }
    edit_menu_item = menu_item;
    edit_value = 0;
    edit_loaded = 0;
    edit_stash = 0;
    screen = CONFIG_SCREEN_EDIT;
    if (!op_busy) {
        op_busy = 1;
        controller_read_config(s->board_addr, s->reg, on_edit_read_done, 0);
    }
}

static void on_edit_read_done(uint8_t ok, uint8_t value, void* ctx) {
    (void)ctx;
    if (ok) {
        const EditSpec* s = spec_for(edit_menu_item);
        if (s) {
            edit_stash = value;
            edit_value = extract_value(s, value);
            edit_loaded = 1;
        }
    }
    op_busy = 0;
}

static void on_edit_write_done(uint8_t ok, void* ctx) {
    (void)ctx;
    if (ok) {
        screen = CONFIG_SCREEN_MENU;
    }
    op_busy = 0;
}

static const EditSpec* spec_for(uint8_t menu_item) {
    if (menu_item == CONFIG_MENU_NAV || menu_item >= CONFIG_MENU_COUNT) {
        return 0;
    }
    return &edit_specs[menu_item - 1u];
}

static uint8_t extract_value(const EditSpec* s, uint8_t raw_byte) {
    if (s->kind == EDIT_KIND_LEVEL_MODE) {
        return (uint8_t)((raw_byte >> s->bit_offset) & 0x03u);
    }
    return raw_byte;
}

static uint8_t merge_value(const EditSpec* s, uint8_t raw_byte, uint8_t value) {
    if (s->kind == EDIT_KIND_LEVEL_MODE) {
        uint8_t mask = (uint8_t)(0x03u << s->bit_offset);
        return (uint8_t)((raw_byte & ~mask) | ((value & 0x03u) << s->bit_offset));
    }
    return value;
}

static void enter(void) {
    working_mask = config_get_nav_enabled_mask();
    screen = CONFIG_SCREEN_MENU;
    menu_cursor = 0;
    nav_cursor = 0;
    active = 1;
}

static void exit_config(void) {
    if (active) {
        config_write_byte(CONFIG_ADDR_NAV_ENABLED_MASK, (uint8_t)(working_mask & NAV_LIGHT_ALL));
    }
    active = 0;
    screen = CONFIG_SCREEN_MENU;
}

static void sample_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    uint8_t raw = PIN_ACTIVE() ? 1 : 0;
    if (raw != raw_last) {
        raw_last = raw;
        stable_ticks = 0;
        return;
    }
    if (stable_ticks < STABLE_TICKS) {
        stable_ticks++;
        if (stable_ticks == STABLE_TICKS && raw != raw_stable) {
            raw_stable = raw;
            if (raw_stable) {
                enter();
            } else {
                exit_config();
            }
        }
    }
}
