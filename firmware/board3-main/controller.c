#include "controller.h"

#include "button_fx.h"
#include "config.h"
#include "config_mode.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "nav_lights.h"
#include "rtc.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

/* ============================================================================
 * Relay assignment (main-board convention; the switching board driver simply
 * maps bit N to its Nth coil). Callers should address these via the macros so
 * the layout can be tweaked without touching action logic.
 * ============================================================================
 */

#define RELAY_NAV_ANCHORING 0
#define RELAY_NAV_TRICOLOR 1
#define RELAY_NAV_STEAMING 2
#define RELAY_NAV_BOW 3
#define RELAY_NAV_STERN 4
#define RELAY_INSTRUMENTS 5
#define RELAY_AUTOPILOT 6
#define RELAY_INVERTER 7
#define RELAY_WATER_PUMP 8
#define RELAY_FRIDGE 9
#define RELAY_DECK_LIGHTS 10
#define RELAY_CABIN_LIGHTS 11
#define RELAY_USB 12
#define RELAY_AUX_1 13
#define RELAY_AUX_2 14

#define NAV_MASK_BITS 0x001Fu /* relays 0..4 */

/* ============================================================================
 * Button → action mapping
 * ============================================================================
 */

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
    {ACTION_TOGGLE_POWER, 0},
    {ACTION_TOGGLE_RELAY, RELAY_INSTRUMENTS},
    {ACTION_TOGGLE_RELAY, RELAY_AUTOPILOT},
    {ACTION_TOGGLE_NAV_MODE, NAV_MODE_STEAMING},
    {ACTION_TOGGLE_NAV_MODE, NAV_MODE_RUNNING},
    {ACTION_TOGGLE_NAV_MODE, NAV_MODE_ANCHORING},
    {ACTION_TOGGLE_RELAY, RELAY_INVERTER},
};

/* Right panel (COMM_ADDRESS_BUTTON_BOARD_R = 0x46), buttons 0..6. */
static const ButtonAction right_actions[7] = {
    {ACTION_TOGGLE_RELAY, RELAY_WATER_PUMP},  {ACTION_TOGGLE_RELAY, RELAY_FRIDGE},
    {ACTION_TOGGLE_RELAY, RELAY_DECK_LIGHTS}, {ACTION_TOGGLE_RELAY, RELAY_CABIN_LIGHTS},
    {ACTION_TOGGLE_RELAY, RELAY_USB},         {ACTION_TOGGLE_RELAY, RELAY_AUX_1},
    {ACTION_TOGGLE_RELAY, RELAY_AUX_2},
};

/* ============================================================================
 * State shadow
 * ============================================================================
 */

static volatile uint8_t power_on;
static volatile uint8_t nav_mode; /* NavMode                     */
static volatile uint8_t nav_error;
static volatile uint16_t relay_intent;   /* user-facing: relays 5..15   */
static volatile uint16_t relay_target;   /* materialised after policy   */
static volatile uint16_t relay_physical; /* from relay_changed pushes   */
static volatile uint8_t sensor_state;
static volatile uint8_t relay_dirty; /* target diverged from bus    */
static volatile uint16_t battery_mv;
static volatile uint8_t levels[2];

/* Power-off snapshot kept in RAM only (per todo: no EEPROM persistence).
 * Captured on the active -> inactive transition and reapplied when power
 * returns so the user's prior selection of relays and nav mode survives
 * the power toggle without relying on flash wear or boot-time defaults. */
static uint16_t saved_relay_intent;
static uint8_t saved_nav_mode;

/* ============================================================================
 * Outbound retry queue
 *
 * Single-slot latest-wins for relay_state: if the target changes again before
 * the previous write landed, we just want to send the latest value. Collapse
 * reduces I2C traffic when a burst of button presses lands in one tick.
 * ============================================================================
 */

#define RETRY_TICK_MS 200u
#define POLL_TICK_MS 200u
/* RTC ticks once per second; polling at 250 ms keeps the displayed clock
 * within a quarter second of the chip without being wasteful. */
#define RTC_TICK_MS 250u

/* Mask + expected value of the relay bits an action touched. button_fx uses
 * this to decide what to wait for before the press completes. */
typedef struct {
    uint16_t mask;
    uint16_t value;
} ActionEffect;

static void retry_task(TaskId id, void* ctx);
static void poll_battery_task(TaskId id, void* ctx);
static void poll_levels_task(TaskId id, void* ctx);
static void poll_sensors_task(TaskId id, void* ctx);
static void poll_rtc_task(TaskId id, void* ctx);
static void on_relay_state_done(I2cResult result, uint8_t* rx, uint8_t rx_len, void* ctx);
static void on_rtc_read_done(uint8_t ok, const RtcTime* t, void* ctx);
static ActionEffect apply_action(const ButtonAction* a);
static void recompute_target(void);

static volatile RtcTime rtc_shadow;
static volatile uint8_t rtc_valid;

static volatile uint8_t relay_inflight;
static volatile uint8_t rtc_inflight;
static volatile uint16_t relay_inflight_value;

/* Staleness counters: incremented each poll tick (POLL_TICK_MS), reset
 * to 0 on successful response.  When the counter exceeds the threshold
 * the UI should show "error" instead of the stale value. */
#define STALE_THRESHOLD (10000u / POLL_TICK_MS) /* 10 s */

static volatile uint16_t batt_age;
static volatile uint16_t levels_age;
static volatile uint16_t sensors_age;

void controller_init(TaskController* ctrl) {
    power_on = 0;
    nav_mode = NAV_MODE_OFF;
    nav_error = 0;
    relay_intent = 0;
    relay_target = 0;
    relay_physical = 0;
    sensor_state = 0;
    relay_dirty = 0;
    battery_mv = 0;
    levels[0] = 0;
    levels[1] = 0;
    saved_relay_intent = 0;
    saved_nav_mode = NAV_MODE_OFF;
    rtc_valid = 0;
    relay_inflight = 0;
    rtc_inflight = 0;
    batt_age = STALE_THRESHOLD;
    levels_age = STALE_THRESHOLD;
    sensors_age = STALE_THRESHOLD;
    task_controller_add(ctrl, TASK_COMM_RETRY, RETRY_TICK_MS, retry_task, 0);
    task_controller_add(ctrl, TASK_POLL_BATTERY, POLL_TICK_MS, poll_battery_task, 0);
    task_controller_add(ctrl, TASK_POLL_LEVELS, POLL_TICK_MS, poll_levels_task, 0);
    task_controller_add(ctrl, TASK_POLL_SENSORS, POLL_TICK_MS, poll_sensors_task, 0);
    // task_controller_add(ctrl, TASK_POLL_RTC, RTC_TICK_MS, poll_rtc_task, 0);
}

/* ============================================================================
 * Inbound
 * ============================================================================
 */

void controller_on_button_changed(uint8_t sender, uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
    const uint8_t should_trigger = (mode == COMM_BUTTON_MODE_RELEASE) || (mode == COMM_BUTTON_MODE_HOLD) ||
                                   (mode == COMM_BUTTON_MODE_CHANGE && pressed);
    if (button_id >= 7 || !should_trigger) {
        return;
    }
    if (config_mode_active()) {
        /* Normal actions are suppressed while configuring; button presses
         * edit the nav-enabled mask instead. */
        config_mode_on_button_pressed(sender, button_id);
        return;
    }

    const ButtonAction* table;
    uint8_t side;
    switch (sender) {
        case COMM_ADDRESS_BUTTON_BOARD_L:
            table = left_actions;
            side = 0;
            break;
        case COMM_ADDRESS_BUTTON_BOARD_R:
            table = right_actions;
            side = 1;
            break;
        /* L2 / R2 not yet mapped. */
        default:
            return;
    }
    /* Per readme: a press on a button currently flagged as ERROR clears
     * the error and turns the channel off. The follow-up apply_action
     * toggles the intent which — since the prior attempt never landed
     * physically — converges to OFF. */
    if (button_fx_is_error(side, button_id)) {
        button_fx_clear(side, button_id);
    }
    ActionEffect eff = apply_action(&table[button_id]);
    if (eff.mask != 0) {
        button_fx_notify_press(side, button_id, eff.mask, eff.value);
    }
}

void controller_on_relay_changed(uint8_t sender, uint16_t prev_r, uint16_t curr_r, uint8_t prev_s, uint8_t curr_s) {
    (void)sender;
    (void)prev_r;
    (void)prev_s;
    relay_physical = curr_r;
    sensor_state = curr_s;
    button_fx_on_relay_physical(curr_r);
}

/* ============================================================================
 * Queries
 * ============================================================================
 */

uint8_t controller_power_on(void) {
    return power_on;
}
NavMode controller_nav_mode(void) {
    return (NavMode)nav_mode;
}
uint8_t controller_nav_error(void) {
    return nav_error;
}
uint16_t controller_relay_target(void) {
    return relay_target;
}
uint16_t controller_relay_physical(void) {
    return relay_physical;
}
uint16_t controller_battery_mv(void) {
    return battery_mv;
}
uint8_t controller_level(uint8_t i) {
    return (i < 2) ? levels[i] : 0;
}
uint8_t controller_sensors(void) {
    return sensor_state;
}
uint8_t controller_battery_stale(void) {
    return batt_age >= STALE_THRESHOLD;
}
uint8_t controller_levels_stale(void) {
    return levels_age >= STALE_THRESHOLD;
}
uint8_t controller_sensors_stale(void) {
    return sensors_age >= STALE_THRESHOLD;
}

uint8_t controller_button_base_on(uint8_t side, uint8_t button_idx) {
    /* Buttons per-side are 0..6. */
    if (button_idx >= 7) {
        return 0;
    }

    const ButtonAction* table = 0;
    if (side == 0) {
        table = left_actions;
    } else if (side == 1) {
        table = right_actions;
    } else {
        return 0;
    }

    const ButtonAction* action = &table[button_idx];
    switch (action->kind) {
        case ACTION_TOGGLE_POWER:
            return (uint8_t)power_on;
        case ACTION_TOGGLE_RELAY: {
            uint16_t bit = (uint16_t)(1u << action->param);
            return (uint8_t)((relay_target & bit) != 0);
        }
        case ACTION_TOGGLE_NAV_MODE:
            return (uint8_t)(nav_mode == action->param);
        default:
            return 0;
    }
}

/* ============================================================================
 * Action logic
 * ============================================================================
 */

static ActionEffect apply_action(const ButtonAction* a) {
    ActionEffect eff = {0, 0};
    switch (a->kind) {
        case ACTION_TOGGLE_POWER:
            if (power_on) {
                saved_relay_intent = relay_intent;
                saved_nav_mode = nav_mode;
                power_on = 0;
                /* Pull everything down: recompute_target() below will zero
                 * relay_target and mark it dirty so the retry task ships a
                 * relay_state write clearing every coil. */
            } else {
                relay_intent = saved_relay_intent;
                nav_mode = saved_nav_mode;
                power_on = 1;
            }
            eff.mask = 0xFFFFu;
            break;

        case ACTION_TOGGLE_RELAY: {
            uint16_t bit = (uint16_t)(1u << a->param);
            relay_intent ^= bit;
            eff.mask = bit;
            break;
        }

        case ACTION_TOGGLE_NAV_MODE: {
            NavMode requested = (NavMode)a->param;
            nav_mode = (uint8_t)((nav_mode == requested) ? NAV_MODE_OFF : requested);
            eff.mask = NAV_MASK_BITS;
            break;
        }

        default:
            return eff;
    }
    recompute_target();
    eff.value = (uint16_t)(relay_target & eff.mask);
    return eff;
}

/* Projects (power, nav_mode, relay_intent) onto the 16-bit relay target.
 * Also latches nav_error for UI. */
static void recompute_target(void) {
    uint16_t t = 0;
    uint8_t err = 0;

    if (power_on) {
        uint8_t enabled = config_get_nav_enabled_mask();
        NavResolution r = nav_lights_resolve((NavMode)nav_mode, enabled);
        err = r.error;

        /* Map 5-bit nav_lights_mask (NAV_LIGHT_* bit positions) onto the
         * relay indices RELAY_NAV_*. The two encodings are aligned (bit 0 =
         * anchoring, ..., bit 4 = stern) so the mask copies directly. */
        t |= (uint16_t)(r.lights_mask & NAV_MASK_BITS);
        t |= (uint16_t)(relay_intent & ~NAV_MASK_BITS);
    }

    INTERRUPT_PUSH;
    if (t != relay_target) {
        relay_target = t;
        relay_dirty = 1;
    }
    nav_error = err;
    INTERRUPT_POP;
}

/* ============================================================================
 * Outbound sync
 * ============================================================================
 */

static void retry_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    if (!relay_dirty || relay_inflight) {
        return;
    }

    uint16_t snapshot;
    INTERRUPT_PUSH;
    snapshot = relay_target;
    INTERRUPT_POP;

    relay_inflight_value = snapshot;
    relay_inflight = 1;
    if (comm_send_relay_state(snapshot, on_relay_state_done, 0) != I2C_RESULT_OK) {
        relay_inflight = 0;
    }
}

static void on_relay_state_done(I2cResult result, uint8_t* rx, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)rx;
    (void)rx_len;
    (void)ctx;
    /* Clear the dirty flag if the value we actually sent still matches
     * the current target — a producer could have bumped it while the
     * transaction was in flight. */
    INTERRUPT_PUSH;
    if (relay_target == relay_inflight_value) {
        relay_dirty = 0;
    }
    INTERRUPT_POP;
    relay_inflight = 0;
}

/* ============================================================================
 * Polling tasks — fire-and-forget reads to the switching board.
 * The response arrives via the adopter callback → controller_on_*_response.
 * A staleness counter tracks time since the last successful response;
 * the UI checks controller_*_stale() and shows "error" when it exceeds 10 s.
 * ============================================================================
 */

static void poll_battery_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    if (batt_age < STALE_THRESHOLD) {
        batt_age++;
    }
    comm_send_battery_read();
}

static void poll_levels_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    if (levels_age < STALE_THRESHOLD) {
        levels_age++;
    }
    comm_send_levels_read();
}

static void poll_sensors_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    if (sensors_age < STALE_THRESHOLD) {
        sensors_age++;
    }
    comm_send_sensors_read();
}

/* Response handlers — called from comm.c adopter callbacks. */

void controller_on_battery_response(const CommBattery* battery) {
    if (battery) {
        battery_mv = battery->voltage;
        batt_age = 0;
    }
}

void controller_on_levels_response(const CommLevels* lvl) {
    if (lvl) {
        levels[0] = lvl->level_0;
        levels[1] = lvl->level_1;
        levels_age = 0;
    }
}

void controller_on_sensors_response(const CommSensors* sns) {
    if (sns) {
        sensor_state = sns->sensors;
        sensors_age = 0;
    }
}

static void poll_rtc_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    if (rtc_inflight) {
        return;
    }
    rtc_inflight = 1;
    rtc_read(on_rtc_read_done, 0);
}

static void on_rtc_read_done(uint8_t ok, const RtcTime* t, void* ctx) {
    (void)ctx;
    if (ok) {
        INTERRUPT_PUSH;
        rtc_shadow = *t;
        rtc_valid = 1;
        INTERRUPT_POP;
    }
    rtc_inflight = 0;
}

uint8_t controller_time(RtcTime* out) {
    if (!rtc_valid) {
        return 0;
    }
    INTERRUPT_PUSH;
    *out = rtc_shadow;
    INTERRUPT_POP;
    return 1;
}

/* Single in-flight UI operation. The menu can only have one menu action
 * pending at a time, so a single slot suffices. */
static struct {
    ControllerOpCompletion op_cb;
    ControllerReadCompletion read_cb;
    void* ctx;
} ui_op;

static void on_set_time_write_done(uint8_t ok, void* ctx);
static void on_set_time_refresh_done(uint8_t ok, const RtcTime* t, void* ctx);
static void on_ui_config_write_done(I2cResult result, uint8_t* rx, uint8_t rx_len, void* ctx);

void controller_set_time(uint8_t hour, uint8_t minute, ControllerOpCompletion cb, void* ctx) {
    ui_op.op_cb = cb;
    ui_op.ctx = ctx;
    rtc_write_time(hour, minute, on_set_time_write_done, 0);
}

static void on_set_time_write_done(uint8_t ok, void* ctx) {
    (void)ctx;
    if (!ok) {
        ControllerOpCompletion cb = ui_op.op_cb;
        void* user_ctx = ui_op.ctx;
        ui_op.op_cb = 0;
        if (cb) {
            cb(0, user_ctx);
        }
        return;
    }
    /* Refresh the shadow immediately so the UI reflects the new time
     * without waiting for the next poll tick. */
    rtc_read(on_set_time_refresh_done, 0);
}

static void on_set_time_refresh_done(uint8_t ok, const RtcTime* t, void* ctx) {
    (void)ctx;
    if (ok) {
        INTERRUPT_PUSH;
        rtc_shadow = *t;
        rtc_valid = 1;
        INTERRUPT_POP;
    }
    ControllerOpCompletion cb = ui_op.op_cb;
    void* user_ctx = ui_op.ctx;
    ui_op.op_cb = 0;
    if (cb) {
        cb(1, user_ctx); /* write succeeded; refresh failure is non-fatal */
    }
}

void controller_read_switching_config(uint8_t address, ControllerReadCompletion cb, void* ctx) {
    ui_op.read_cb = cb;
    ui_op.ctx = ctx;
    if (comm_send_config_read(COMM_ADDRESS_SWITCHING, address) != I2C_RESULT_OK) {
        ui_op.read_cb = 0;
        if (cb) {
            cb(0, 0, ctx);
        }
    }
}

void controller_on_config_read_response(const uint8_t* value) {
    ControllerReadCompletion cb = ui_op.read_cb;
    void* user_ctx = ui_op.ctx;
    ui_op.read_cb = 0;
    if (cb) {
        cb(value != 0, value ? *value : 0, user_ctx);
    }
}

void controller_write_switching_config(uint8_t address, uint8_t value, ControllerOpCompletion cb, void* ctx) {
    ui_op.op_cb = cb;
    ui_op.ctx = ctx;
    if (comm_send_config(COMM_ADDRESS_SWITCHING, address, value, on_ui_config_write_done, 0) != I2C_RESULT_OK) {
        ui_op.op_cb = 0;
        if (cb) {
            cb(0, ctx);
        }
    }
}

static void on_ui_config_write_done(I2cResult result, uint8_t* rx, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)rx;
    (void)rx_len;
    (void)ctx;
    /* Completion fires only on success; final-retry failure is silent. */
    ControllerOpCompletion cb = ui_op.op_cb;
    void* user_ctx = ui_op.ctx;
    ui_op.op_cb = 0;
    if (cb) {
        cb(1, user_ctx);
    }
}
