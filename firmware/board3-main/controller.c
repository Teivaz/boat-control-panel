#include "controller.h"
#include "nav_lights.h"
#include "config.h"
#include "i2c.h"
#include "libcomm.h"
#include "task.h"
#include "task_ids.h"

/* ============================================================================
 * Relay assignment (main-board convention; the switching board driver simply
 * maps bit N to its Nth coil). Callers should address these via the macros so
 * the layout can be tweaked without touching action logic.
 * ============================================================================ */

#define RELAY_NAV_ANCHORING   0
#define RELAY_NAV_TRICOLOR    1
#define RELAY_NAV_STEAMING    2
#define RELAY_NAV_BOW         3
#define RELAY_NAV_STERN       4
#define RELAY_INSTRUMENTS     5
#define RELAY_AUTOPILOT       6
#define RELAY_INVERTER        7
#define RELAY_WATER_PUMP      8
#define RELAY_FRIDGE          9
#define RELAY_DECK_LIGHTS    10
#define RELAY_CABIN_LIGHTS   11
#define RELAY_USB            12
#define RELAY_AUX_1          13
#define RELAY_AUX_2          14

#define NAV_MASK_BITS  0x001Fu   /* relays 0..4 */

/* ============================================================================
 * Button → action mapping
 * ============================================================================ */

typedef enum {
    ACTION_NONE = 0,
    ACTION_TOGGLE_POWER,
    ACTION_TOGGLE_RELAY,
    ACTION_TOGGLE_NAV_MODE,
} ActionKind;

typedef struct {
    uint8_t kind;
    uint8_t param;
} ButtonAction;

/* Left panel (COMM_ADDRESS_BUTTON_BOARD_L = 0x44), buttons 0..6. */
static const ButtonAction left_actions[7] = {
    { ACTION_TOGGLE_POWER,    0                  },
    { ACTION_TOGGLE_RELAY,    RELAY_INSTRUMENTS  },
    { ACTION_TOGGLE_RELAY,    RELAY_AUTOPILOT    },
    { ACTION_TOGGLE_NAV_MODE, NAV_MODE_STEAMING  },
    { ACTION_TOGGLE_NAV_MODE, NAV_MODE_RUNNING   },
    { ACTION_TOGGLE_NAV_MODE, NAV_MODE_ANCHORING },
    { ACTION_TOGGLE_RELAY,    RELAY_INVERTER     },
};

/* Right panel (COMM_ADDRESS_BUTTON_BOARD_R = 0x46), buttons 0..6. */
static const ButtonAction right_actions[7] = {
    { ACTION_TOGGLE_RELAY, RELAY_WATER_PUMP   },
    { ACTION_TOGGLE_RELAY, RELAY_FRIDGE       },
    { ACTION_TOGGLE_RELAY, RELAY_DECK_LIGHTS  },
    { ACTION_TOGGLE_RELAY, RELAY_CABIN_LIGHTS },
    { ACTION_TOGGLE_RELAY, RELAY_USB          },
    { ACTION_TOGGLE_RELAY, RELAY_AUX_1        },
    { ACTION_TOGGLE_RELAY, RELAY_AUX_2        },
};

/* ============================================================================
 * State shadow
 * ============================================================================ */

static volatile uint8_t  power_on;
static volatile uint8_t  nav_mode;          /* NavMode                     */
static volatile uint8_t  nav_error;
static volatile uint16_t relay_intent;      /* user-facing: relays 5..15   */
static volatile uint16_t relay_target;      /* materialised after policy   */
static volatile uint16_t relay_physical;    /* from relay_changed pushes   */
static volatile uint8_t  sensor_state;
static volatile uint8_t  relay_dirty;       /* target diverged from bus    */
static volatile uint16_t battery_mv;
static volatile uint8_t  levels[2];

/* ============================================================================
 * Outbound retry queue
 *
 * Single-slot latest-wins for relay_state: if the target changes again before
 * the previous write landed, we just want to send the latest value. Collapse
 * reduces I2C traffic when a burst of button presses lands in one tick.
 * ============================================================================ */

#define RETRY_TICK_MS   TASK_MIN_MS
#define POLL_TICK_MS    200u   /* protocol minimum is 100 ms; 200 ms is ample */

static void retry_task        (TaskId id, void *ctx);
static void poll_battery_task (TaskId id, void *ctx);
static void poll_levels_task  (TaskId id, void *ctx);
static void poll_sensors_task (TaskId id, void *ctx);
static void apply_action      (const ButtonAction *a);
static void recompute_target  (void);

void controller_init(TaskController *ctrl) {
    power_on       = 0;
    nav_mode       = NAV_MODE_OFF;
    nav_error      = 0;
    relay_intent   = 0;
    relay_target   = 0;
    relay_physical = 0;
    sensor_state   = 0;
    relay_dirty    = 0;
    battery_mv     = 0;
    levels[0]      = 0;
    levels[1]      = 0;
    task_controller_add(ctrl, TASK_COMM_RETRY,    RETRY_TICK_MS, retry_task,        0);
    task_controller_add(ctrl, TASK_POLL_BATTERY,  POLL_TICK_MS,  poll_battery_task, 0);
    task_controller_add(ctrl, TASK_POLL_LEVELS,   POLL_TICK_MS,  poll_levels_task,  0);
    task_controller_add(ctrl, TASK_POLL_SENSORS,  POLL_TICK_MS,  poll_sensors_task, 0);
}

/* ============================================================================
 * Inbound
 * ============================================================================ */

void controller_on_button_changed(uint8_t sender, uint8_t prev, uint8_t curr) {
    const ButtonAction *table;
    switch (sender) {
        case COMM_ADDRESS_BUTTON_BOARD_L:  table = left_actions;  break;
        case COMM_ADDRESS_BUTTON_BOARD_R:  table = right_actions; break;
        /* L2 / R2 not yet mapped. */
        default: return;
    }
    /* Newly-pressed bits: 0 -> 1 transitions. */
    uint8_t rising = (uint8_t)(curr & ~prev);
    for (uint8_t i = 0; i < 7; i++) {
        if (rising & (uint8_t)(1u << i)) apply_action(&table[i]);
    }
}

void controller_on_relay_changed(uint8_t sender,
                                 uint16_t prev_r, uint16_t curr_r,
                                 uint8_t prev_s,  uint8_t curr_s) {
    (void)sender; (void)prev_r; (void)prev_s;
    relay_physical = curr_r;
    sensor_state   = curr_s;
}

/* ============================================================================
 * Queries
 * ============================================================================ */

uint8_t  controller_power_on(void)        { return power_on; }
NavMode  controller_nav_mode(void)        { return (NavMode)nav_mode; }
uint8_t  controller_nav_error(void)       { return nav_error; }
uint16_t controller_relay_target(void)    { return relay_target; }
uint16_t controller_relay_physical(void)  { return relay_physical; }
uint16_t controller_battery_mv(void)      { return battery_mv; }
uint8_t  controller_level(uint8_t i)      { return (i < 2) ? levels[i] : 0; }
uint8_t  controller_sensors(void)         { return sensor_state; }

/* ============================================================================
 * Action logic
 * ============================================================================ */

static void apply_action(const ButtonAction *a) {
    switch (a->kind) {
        case ACTION_TOGGLE_POWER:
            power_on = !power_on;
            break;

        case ACTION_TOGGLE_RELAY: {
            uint16_t bit = (uint16_t)(1u << a->param);
            relay_intent ^= bit;
            break;
        }

        case ACTION_TOGGLE_NAV_MODE: {
            NavMode requested = (NavMode)a->param;
            nav_mode = (uint8_t)((nav_mode == requested) ? NAV_MODE_OFF : requested);
            break;
        }

        default:
            return;
    }
    recompute_target();
}

/* Projects (power, nav_mode, relay_intent) onto the 16-bit relay target.
 * Also latches nav_error for UI. */
static void recompute_target(void) {
    uint16_t t = 0;
    uint8_t  err = 0;

    if (power_on) {
        uint8_t enabled = config_get_nav_enabled_mask();
        NavResolution r = nav_lights_resolve((NavMode)nav_mode, enabled);
        err = r.error;

        /* Map 5-bit nav_lights_mask (NAV_LIGHT_* bit positions) onto the
         * relay indices RELAY_NAV_*. The two encodings are aligned (bit 0 =
         * anchoring, ..., bit 4 = stern) so the mask copies directly. */
        t |= (uint16_t)(r.lights_mask & NAV_MASK_BITS);
        t |= (uint16_t)(relay_intent  & ~NAV_MASK_BITS);
    }

    INTERRUPT_PUSH;
    if (t != relay_target) {
        relay_target = t;
        relay_dirty  = 1;
    }
    nav_error = err;
    INTERRUPT_POP;
}

/* ============================================================================
 * Outbound sync
 * ============================================================================ */

static void retry_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    if (!relay_dirty) return;

    uint16_t snapshot;
    INTERRUPT_PUSH;
    snapshot = relay_target;
    INTERRUPT_POP;

    CommMessage msg;
    uint8_t len = comm_build_relay_state(&msg, snapshot);
    if (i2c_transmit(COMM_ADDRESS_SWITCHING,
                     (const uint8_t *)&msg, len) == I2C_RESULT_OK) {
        /* Only clear the flag if the value we actually sent still matches
         * the current target — a producer could have bumped it while we
         * were transmitting. */
        INTERRUPT_PUSH;
        if (relay_target == snapshot) relay_dirty = 0;
        INTERRUPT_POP;
    }
}

/* ============================================================================
 * Polling tasks — issue write-then-read transactions to the switching board
 * and latch the result into the shadow. On failure we simply skip this tick;
 * the task fires again on the next POLL_TICK_MS interval.
 * ============================================================================ */

static void poll_battery_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    uint8_t req = COMM_BATTERY_READ;
    uint8_t rx[2];
    if (i2c_receive(COMM_ADDRESS_SWITCHING, &req, 1, rx, sizeof(rx)) == I2C_RESULT_OK) {
        battery_mv = (uint16_t)(rx[0] | ((uint16_t)rx[1] << 8));
    }
}

static void poll_levels_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    uint8_t req = COMM_LEVELS_READ;
    uint8_t rx[2];
    if (i2c_receive(COMM_ADDRESS_SWITCHING, &req, 1, rx, sizeof(rx)) == I2C_RESULT_OK) {
        levels[0] = rx[0];
        levels[1] = rx[1];
    }
}

static void poll_sensors_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    uint8_t req = COMM_SENSORS_READ;
    uint8_t rx;
    if (i2c_receive(COMM_ADDRESS_SWITCHING, &req, 1, &rx, 1) == I2C_RESULT_OK) {
        sensor_state = rx;
    }
}
