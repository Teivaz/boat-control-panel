#include "config_mode.h"

#include "config.h"
#include "controller.h"
#include "libcomm.h"
#include "rtc.h"
#include "task_ids.h"

#include <xc.h>

/* Switching-board config-protocol addresses for the level-meter wire
 * offsets. Mirrors board1-switching/config.h — kept local because main
 * does not link against the switching board's sources. */
#define SWITCHING_OFFSET_WATER 0x10
#define SWITCHING_OFFSET_FUEL 0x11

#define SAMPLE_MS 20u
#define STABLE_TICKS 3u /* ~60 ms debounce on the RA7 switch */

/* RA7 is pulled up via WPUA7; the switch closes to ground when active. */
#define PIN_ACTIVE() (PORTAbits.RA7 == 0)

/* Button indices on the left / right boards used by the menu. */
#define BTN_UP 4u     /* left  */
#define BTN_DOWN 3u   /* left  */
#define BTN_BACK 2u   /* right */
#define BTN_SELECT 1u /* right */

#define SIDE_LEFT COMM_ADDRESS_BUTTON_BOARD_L
#define SIDE_RIGHT COMM_ADDRESS_BUTTON_BOARD_R

#define TIME_FIELD_HOUR 0u
#define TIME_FIELD_MINUTE 1u

static volatile uint8_t active;
static volatile uint8_t raw_last;
static volatile uint8_t raw_stable;
static volatile uint8_t stable_ticks;

static volatile uint8_t screen;       /* ConfigScreen */
static volatile uint8_t menu_cursor;  /* 0..CONFIG_MENU_COUNT-1 */
static volatile uint8_t nav_cursor;   /* 0..4 */
static volatile uint8_t working_mask; /* live nav-enabled mask */
static volatile uint8_t time_field;   /* 0=hour, 1=minute */
static volatile uint8_t working_hour;
static volatile uint8_t working_minute;
static volatile uint8_t offset_target; /* 0=water, 1=fuel */
static volatile uint8_t offset_value;

static void enter(void);
static void exit_config(void);
static void load_time(void);
static void enter_offset(uint8_t target);
static uint8_t offset_address(uint8_t target);
static void handle_menu(uint8_t btn);
static void handle_nav(uint8_t btn);
static void handle_time(uint8_t btn);
static void handle_offset(uint8_t btn);
static void sample_task(TaskId id, void* ctx);

void config_mode_init(TaskController* ctrl) {
    active = 0;
    screen = CONFIG_SCREEN_MENU;
    menu_cursor = 0;
    nav_cursor = 0;
    time_field = TIME_FIELD_HOUR;
    working_hour = 0;
    working_minute = 0;
    offset_target = 0;
    offset_value = 0;
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
    return nav_cursor;
}

uint8_t config_mode_nav_working_mask(void) {
    return working_mask;
}

uint8_t config_mode_time_field(void) {
    return time_field;
}

uint8_t config_mode_time_hour(void) {
    return working_hour;
}

uint8_t config_mode_time_minute(void) {
    return working_minute;
}

uint8_t config_mode_offset_target(void) {
    return offset_target;
}

uint8_t config_mode_offset_value(void) {
    return offset_value;
}

/* Runs from the I2C rx ISR (via controller dispatch). Keep it brief. */
void config_mode_on_button_pressed(uint8_t side, uint8_t button_idx) {
    if (!active) {
        return;
    }
    /* Translate (side, button) into the four logical actions. Anything else
     * is ignored — the menu only listens to the four mapped buttons. */
    uint8_t btn;
    if (side == SIDE_LEFT && (button_idx == BTN_UP || button_idx == BTN_DOWN)) {
        /* Encode as a logical event id reusing the original button index plus
         * a side bit so the per-screen handlers can distinguish all four. */
        btn = (uint8_t)(button_idx | 0x10u); /* left-side flag */
    } else if (side == SIDE_LEFT && (button_idx == BTN_BACK || button_idx == BTN_SELECT)) {
        btn = (uint8_t)(button_idx | 0x20u); /* right-side flag */
    } else {
        return;
    }

    switch (screen) {
        case CONFIG_SCREEN_MENU:   handle_menu(btn); break;
        case CONFIG_SCREEN_NAV:    handle_nav(btn); break;
        case CONFIG_SCREEN_TIME:   handle_time(btn); break;
        case CONFIG_SCREEN_OFFSET: handle_offset(btn); break;
        default: break;
    }
}

static void handle_menu(uint8_t btn) {
    switch (btn) {
        case 0x10u | BTN_UP:
            menu_cursor = (uint8_t)((menu_cursor + CONFIG_MENU_COUNT - 1u) % CONFIG_MENU_COUNT);
            break;
        case 0x10u | BTN_DOWN:
            menu_cursor = (uint8_t)((menu_cursor + 1u) % CONFIG_MENU_COUNT);
            break;
        case 0x20u | BTN_BACK:
            /* Back at the top-level menu exits config; commit happens on
             * RA7 release, but we mirror it here so a software exit also
             * persists the working mask. */
            exit_config();
            break;
        case 0x20u | BTN_SELECT:
            if (menu_cursor == CONFIG_MENU_NAV) {
                screen = CONFIG_SCREEN_NAV;
                nav_cursor = 0;
            } else if (menu_cursor == CONFIG_MENU_TIME) {
                load_time();
                time_field = TIME_FIELD_HOUR;
                screen = CONFIG_SCREEN_TIME;
            } else if (menu_cursor == CONFIG_MENU_OFFSET_WATER) {
                enter_offset(0);
            } else if (menu_cursor == CONFIG_MENU_OFFSET_FUEL) {
                enter_offset(1);
            }
            break;
        default: break;
    }
}

static void handle_nav(uint8_t btn) {
    switch (btn) {
        case 0x10u | BTN_UP:
            nav_cursor = (uint8_t)((nav_cursor + 4u) % 5u);
            break;
        case 0x10u | BTN_DOWN:
            nav_cursor = (uint8_t)((nav_cursor + 1u) % 5u);
            break;
        case 0x20u | BTN_BACK:
            screen = CONFIG_SCREEN_MENU;
            break;
        case 0x20u | BTN_SELECT:
            working_mask ^= (uint8_t)(1u << nav_cursor);
            working_mask &= NAV_LIGHT_ALL;
            break;
        default: break;
    }
}

static void handle_time(uint8_t btn) {
    switch (btn) {
        case 0x10u | BTN_UP:
            if (time_field == TIME_FIELD_HOUR) {
                working_hour = (uint8_t)((working_hour + 1u) % 24u);
            } else {
                working_minute = (uint8_t)((working_minute + 1u) % 60u);
            }
            break;
        case 0x10u | BTN_DOWN:
            if (time_field == TIME_FIELD_HOUR) {
                working_hour = (uint8_t)((working_hour + 23u) % 24u);
            } else {
                working_minute = (uint8_t)((working_minute + 59u) % 60u);
            }
            break;
        case 0x20u | BTN_BACK:
            if (time_field == TIME_FIELD_MINUTE) {
                time_field = TIME_FIELD_HOUR;
            } else {
                screen = CONFIG_SCREEN_MENU;
            }
            break;
        case 0x20u | BTN_SELECT:
            if (time_field == TIME_FIELD_HOUR) {
                time_field = TIME_FIELD_MINUTE;
            } else {
                /* Commit; on I2C failure, stay on the field so the user can
                 * retry. Returns to the menu on success. */
                if (controller_set_time(working_hour, working_minute)) {
                    screen = CONFIG_SCREEN_MENU;
                }
            }
            break;
        default: break;
    }
}

static void handle_offset(uint8_t btn) {
    switch (btn) {
        case 0x10u | BTN_UP:
            offset_value = (uint8_t)(offset_value + 1u);
            break;
        case 0x10u | BTN_DOWN:
            offset_value = (uint8_t)(offset_value - 1u);
            break;
        case 0x20u | BTN_BACK:
            screen = CONFIG_SCREEN_MENU;
            break;
        case 0x20u | BTN_SELECT:
            /* Commit; on I2C failure stay on the screen so the user can
             * retry. Returns to the menu on success. */
            if (controller_write_switching_config(offset_address(offset_target), offset_value)) {
                screen = CONFIG_SCREEN_MENU;
            }
            break;
        default: break;
    }
}

static uint8_t offset_address(uint8_t target) {
    return (target == 0) ? SWITCHING_OFFSET_WATER : SWITCHING_OFFSET_FUEL;
}

static void enter_offset(uint8_t target) {
    offset_target = target;
    /* Read the current calibration so the editor starts on the live value.
     * On I2C failure default to 0 — the user can still set a new value. */
    uint8_t v;
    if (!controller_read_switching_config(offset_address(target), &v)) {
        v = 0;
    }
    offset_value = v;
    screen = CONFIG_SCREEN_OFFSET;
}

static void load_time(void) {
    RtcTime t;
    if (controller_time(&t)) {
        working_hour = t.hour;
        working_minute = t.minute;
    } else {
        working_hour = 0;
        working_minute = 0;
    }
}

static void enter(void) {
    working_mask = config_get_nav_enabled_mask();
    screen = CONFIG_SCREEN_MENU;
    menu_cursor = 0;
    nav_cursor = 0;
    time_field = TIME_FIELD_HOUR;
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
