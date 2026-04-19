#include "config_mode.h"

#include "config.h"
#include "libcomm.h"
#include "task_ids.h"

#include <xc.h>

#define SAMPLE_MS 20u
#define STABLE_TICKS 3u /* ~60 ms debounce on the RA7 switch */

/* RA7 is pulled up via WPUA7; the switch closes to ground when active. */
#define PIN_ACTIVE() (PORTAbits.RA7 == 0)

static volatile uint8_t active;
static volatile uint8_t raw_last;
static volatile uint8_t raw_stable;
static volatile uint8_t stable_ticks;
static volatile uint8_t cursor;
static volatile uint8_t working_mask; /* live copy of the nav enabled mask */

static void enter(void);
static void exit_config(void);
static void sample_task(TaskId id, void* ctx);

void config_mode_init(TaskController* ctrl) {
    active = 0;
    cursor = 0;
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

uint8_t config_mode_cursor(void) {
    return cursor;
}

/* Runs from the I2C rx ISR (via controller dispatch). Keep it brief. */
void config_mode_on_buttons(uint8_t side, uint8_t rising) {
    if (!active || rising == 0) {
        return;
    }
    (void)side;
    /* Any new press on either panel advances the cursor and toggles the
     * focused bit — deliberately simple, since the config flow is rare and
     * visual feedback comes from the 5 RGB indicators + display. */
    for (uint8_t b = 0; b < 7; b++) {
        if ((rising & (uint8_t)(1u << b)) == 0) {
            continue;
        }
        if (b < 5) {
            /* Direct bit select: left-panel buttons 0..4 map to slots. */
            cursor = b;
            working_mask ^= (uint8_t)(1u << b);
        } else if (b == 5) {
            cursor = (uint8_t)((cursor + 1u) % 5u);
        } else {
            cursor = (uint8_t)((cursor + 4u) % 5u);
        }
    }
}

static void enter(void) {
    working_mask = config_get_nav_enabled_mask();
    cursor = 0;
    active = 1;
}

static void exit_config(void) {
    if (active) {
        config_write_byte(CONFIG_ADDR_NAV_ENABLED_MASK, (uint8_t)(working_mask & NAV_LIGHT_ALL));
    }
    active = 0;
    cursor = 0;
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
