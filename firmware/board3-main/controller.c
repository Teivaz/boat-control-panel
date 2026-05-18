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
 * Relay assignment — bit position in the 16-bit relay word matches the
 * switching board's wiring (see board1-switching/readme.md "Protocol Bit"
 * column). Callers address via these macros so the layout can be tweaked
 * without touching action logic.
 * ============================================================================
 */

#define RELAY_MAIN 0 /* main contactor — engaged whenever power_on */
#define RELAY_INSTRUMENTS 1
#define RELAY_AUTOPILOT 2
#define RELAY_NAV_BOW 3
#define RELAY_NAV_STERN 4
#define RELAY_NAV_STEAMING 5
#define RELAY_NAV_ANCHORING 6
#define RELAY_NAV_TRICOLOR 7
#define RELAY_INVERTER 8
#define RELAY_WATER_PUMP 9
#define RELAY_FRIDGE 10
#define RELAY_DECK_LIGHTS 11
#define RELAY_CABIN_LIGHTS 12
#define RELAY_USB 13
#define RELAY_AUX_1 14
#define RELAY_AUX_2 15

/* Nav-light bits in the relay word — contiguous at 3..7. */
#define NAV_MASK_BITS                                                                                                  \
    ((uint16_t)((1u << RELAY_NAV_BOW) | (1u << RELAY_NAV_STERN) | (1u << RELAY_NAV_STEAMING) |                         \
                (1u << RELAY_NAV_ANCHORING) | (1u << RELAY_NAV_TRICOLOR)))

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
 * Power state machine
 *
 * OFF      ─L0──→ PENDING(target=ON)  ─channel_changed confirms main on──→ ON
 *                                     ─timeout (~2 s)────────────────→ ERROR
 * ON       ─L0──→ PENDING(target=OFF) ─channel_changed confirms main off─→ OFF
 *                                     ─timeout──────────────────────→ ERROR
 * ERROR    ─L0────────────────────────→ OFF
 *          ─late channel_changed (target match)──→ ON / OFF
 *
 * In OFF / PENDING / ERROR the display is blank, the nav-light RGB ring is
 * dark, only L0 is acted on (other button presses ignored), and the
 * battery / level / sensor polls run at a much slower cadence so the
 * inspector still sees freshness on the bus.
 * ============================================================================
 */

typedef enum {
    PWR_OFF = 0,
    PWR_PENDING = 1,
    PWR_ON = 2,
    PWR_ERROR = 3,
} PowerState;

static volatile uint8_t power_state;
static volatile uint8_t power_target;        /* PWR_OFF / PWR_ON — meaningful in PENDING / ERROR */
static volatile uint16_t power_pending_ticks; /* counted in retry_task at RETRY_TICK_MS */

#define POWER_PENDING_TIMEOUT_TICKS (2000u / RETRY_TICK_MS) /* ~2 s */

static volatile uint8_t nav_mode; /* NavMode                     */
static volatile uint8_t nav_error;
static volatile uint16_t relay_intent;  /* user-facing: relays 5..15   */
static volatile uint16_t relay_target;  /* materialised after policy   */
static volatile uint16_t channel_state; /* from channel_changed pushes */
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
#define POLL_TICK_SLOW_MS 5000u
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
static void poll_channels_task(TaskId id, void* ctx);
static void poll_rtc_task(TaskId id, void* ctx);
static void on_relay_state_done(I2cResult result, uint8_t* rx, uint8_t rx_len, void* ctx);
static void on_rtc_read_done(uint8_t ok, const RtcTime* t, void* ctx);
static ActionEffect apply_action(const ButtonAction* a);
static void recompute_target(void);
static void apply_channel_observation(uint16_t curr_c);

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
static volatile uint16_t channels_age;

/* Cached for the poll-task callbacks so they can adjust their own interval
 * when the power state changes. */
static TaskController* g_ctrl;

void controller_init(TaskController* ctrl) {
    g_ctrl = ctrl;
    power_state = PWR_OFF;
    power_target = PWR_OFF;
    power_pending_ticks = 0;
    nav_mode = NAV_MODE_OFF;
    nav_error = 0;
    relay_intent = 0;
    relay_target = 0;
    channel_state = 0;
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
    channels_age = STALE_THRESHOLD;
    task_controller_add(ctrl, TASK_COMM_RETRY, RETRY_TICK_MS, retry_task, 0);
    task_controller_add(ctrl, TASK_POLL_BATTERY, POLL_TICK_MS, poll_battery_task, 0);
    task_controller_add(ctrl, TASK_POLL_LEVELS, POLL_TICK_MS, poll_levels_task, 0);
    task_controller_add(ctrl, TASK_POLL_SENSORS, POLL_TICK_MS, poll_sensors_task, 0);
    task_controller_add(ctrl, TASK_POLL_CHANNELS, POLL_TICK_MS, poll_channels_task, 0);
    // task_controller_add(ctrl, TASK_POLL_RTC, RTC_TICK_MS, poll_rtc_task, 0);
}

/* ============================================================================
 * Inbound
 * ============================================================================
 */

/* Shared convergence path used by both the channel_changed push and the
 * channel_state_read poll response. The push carries sensors too; the poll
 * doesn't. Sensor handling stays at the call site so this helper is purely
 * about the relay-channel word. */
static void apply_channel_observation(uint16_t curr_c) {
    channel_state = curr_c;

    /* Power-state confirmation: the main bit reflecting `power_target` is
     * what unblocks PENDING. ERROR also accepts late-arriving confirmations
     * as a recovery path (per spec — relay reports power on while we're in
     * ERROR transitions us to ON). */
    if (power_state == PWR_PENDING || power_state == PWR_ERROR) {
        uint8_t main_on = (curr_c & (uint16_t)(1u << RELAY_MAIN)) != 0;
        if (power_target == PWR_ON && main_on) {
            power_state = PWR_ON;
            power_pending_ticks = 0;
            recompute_target();
        } else if (power_target == PWR_OFF && !main_on) {
            power_state = PWR_OFF;
            power_pending_ticks = 0;
            recompute_target();
        }
    }

    button_fx_on_channel_state(curr_c);
}

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

    /* When the panel isn't fully ON, only L0 (Power) on the L board is
     * acted on — the user can recover from OFF / PENDING / ERROR but
     * can't toggle individual channels. */
    if (power_state != PWR_ON) {
        if (sender == COMM_ADDRESS_BUTTON_BOARD_L && button_id == 0) {
            ActionEffect eff = apply_action(&left_actions[0]);
            if (eff.mask != 0) {
                button_fx_notify_press(0, 0, eff.mask, eff.value);
            }
        }
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

void controller_on_channel_changed(uint8_t sender, uint16_t prev_c, uint16_t curr_c, uint8_t prev_s, uint8_t curr_s) {
    (void)sender;
    (void)prev_c;
    (void)prev_s;
    sensor_state = curr_s;
    apply_channel_observation(curr_c);
}

void controller_on_channel_state_response(const CommChannelState* state) {
    if (!state) {
        return;
    }
    /* Same convergence logic as the push path, but no sensor word here —
     * sensors are tracked via channel_changed pushes and the dedicated
     * sensors_read poll. */
    apply_channel_observation(state->channels);
    channels_age = 0;
}

/* ============================================================================
 * Queries
 * ============================================================================
 */

uint8_t controller_power_on(void) {
    return power_state == PWR_ON;
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
uint16_t controller_channel_state(void) {
    return channel_state;
}
uint8_t controller_nav_lights_active(void) {
    /* Inverse of nav_lights_to_relay_bits: pull the (possibly non-contiguous)
     * nav-relay bits out of the channel-state word and re-pack into the 5-bit
     * NAV_LIGHT_* layout the UI expects. */
    uint16_t phys = channel_state;
    uint8_t r = 0;
    if (phys & (uint16_t)(1u << RELAY_NAV_ANCHORING)) {
        r |= NAV_LIGHT_ANCHORING;
    }
    if (phys & (uint16_t)(1u << RELAY_NAV_TRICOLOR)) {
        r |= NAV_LIGHT_TRICOLOR;
    }
    if (phys & (uint16_t)(1u << RELAY_NAV_STEAMING)) {
        r |= NAV_LIGHT_STEAMING;
    }
    if (phys & (uint16_t)(1u << RELAY_NAV_BOW)) {
        r |= NAV_LIGHT_BOW;
    }
    if (phys & (uint16_t)(1u << RELAY_NAV_STERN)) {
        r |= NAV_LIGHT_STERN;
    }
    return r;
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
            return (uint8_t)(power_state == PWR_ON);
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
            /* L0 always-on transitions:
             *   ERROR     → OFF (acknowledge the failure, drop to dark)
             *   PENDING   → ignore (already mid-transition; user should wait)
             *   OFF       → PENDING(target=ON), saved intent restored on the
             *               way through so it's already correct when we land
             *   ON        → PENDING(target=OFF), snapshotting current intent */
            if (power_state == PWR_PENDING) {
                eff.mask = 0;
                return eff;
            }
            if (power_state == PWR_ERROR) {
                power_state = PWR_OFF;
                power_target = PWR_OFF;
                power_pending_ticks = 0;
            } else if (power_state == PWR_ON) {
                saved_relay_intent = relay_intent;
                saved_nav_mode = nav_mode;
                power_state = PWR_PENDING;
                power_target = PWR_OFF;
                power_pending_ticks = 0;
            } else { /* PWR_OFF */
                relay_intent = saved_relay_intent;
                nav_mode = saved_nav_mode;
                power_state = PWR_PENDING;
                power_target = PWR_ON;
                power_pending_ticks = 0;
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

/* Translate the 5-bit NAV_LIGHT_* mask returned by nav_lights_resolve into
 * the relay-word bit positions used on the wire. The two encodings could in
 * principle be aligned (nav lights are now contiguous at relay bits 3..7),
 * but keeping the explicit mapping isolates UI ordering from wiring order. */
static uint16_t nav_lights_to_relay_bits(uint8_t lights_mask) {
    uint16_t r = 0;
    if (lights_mask & NAV_LIGHT_ANCHORING) {
        r |= (uint16_t)(1u << RELAY_NAV_ANCHORING);
    }
    if (lights_mask & NAV_LIGHT_TRICOLOR) {
        r |= (uint16_t)(1u << RELAY_NAV_TRICOLOR);
    }
    if (lights_mask & NAV_LIGHT_STEAMING) {
        r |= (uint16_t)(1u << RELAY_NAV_STEAMING);
    }
    if (lights_mask & NAV_LIGHT_BOW) {
        r |= (uint16_t)(1u << RELAY_NAV_BOW);
    }
    if (lights_mask & NAV_LIGHT_STERN) {
        r |= (uint16_t)(1u << RELAY_NAV_STERN);
    }
    return r;
}

/* Projects (power_state, nav_mode, relay_intent) onto the 16-bit relay target.
 * Also latches nav_error for UI. The target reflects "what we want the bus
 * to be" — full pattern when ON or PENDING→ON, all-zero when OFF, ERROR, or
 * PENDING→OFF. */
static void recompute_target(void) {
    uint16_t t = 0;
    uint8_t err = 0;

    uint8_t want_on = (power_state == PWR_ON) ||
                      (power_state == PWR_PENDING && power_target == PWR_ON);
    if (want_on) {
        uint8_t enabled = config_get_nav_enabled_mask();
        NavResolution r = nav_lights_resolve((NavMode)nav_mode, enabled);
        err = r.error;

        /* Main contactor — gates downstream loads; on whenever we're powered. */
        t |= (uint16_t)(1u << RELAY_MAIN);
        t |= nav_lights_to_relay_bits(r.lights_mask);
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

    /* PENDING-state timeout: tick on every retry slot; trip to ERROR if the
     * switching board hasn't reflected the target main-bit state in time.
     * The transition to ERROR keeps the bus quiet — relay_dirty stays
     * cleared so we don't re-spam the relay_state write while the user
     * decides whether to retry (L0 → OFF) or wait for a late confirmation. */
    if (power_state == PWR_PENDING) {
        if (++power_pending_ticks >= POWER_PENDING_TIMEOUT_TICKS) {
            power_state = PWR_ERROR;
            power_pending_ticks = 0;
        }
    }

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

/* Pick a poll interval based on the current power state. Returns the new
 * interval; callers re-apply via task_controller_set_interval if it has
 * changed. PENDING uses the slow cadence too — we want the bus quiet while
 * the relay write is in flight. */
static uint16_t poll_interval_for_state(void) {
    return (power_state == PWR_ON) ? POLL_TICK_MS : POLL_TICK_SLOW_MS;
}

static void poll_battery_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    /* Battery + levels are still useful while the panel is "off" — the
     * inspector / config UI may want them. Sensors are gated to ON only:
     * they're consumers (e.g., bilge alarm) that don't need to wake while
     * the user has the panel idle. */
    if (batt_age < STALE_THRESHOLD) {
        batt_age++;
    }
    comm_send_battery_read();
}

static void poll_levels_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    if (levels_age < STALE_THRESHOLD) {
        levels_age++;
    }
    comm_send_levels_read();
}

static void poll_sensors_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    if (power_state != PWR_ON) {
        return;
    }
    if (sensors_age < STALE_THRESHOLD) {
        sensors_age++;
    }
    comm_send_sensors_read();
}

static void poll_channels_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    /* The switching board pushes a `channel_changed` message on every
     * transition, so the shadow is normally fresh without polling. The
     * read still earns its keep in two cases: it seeds `channel_state`
     * after a main-board reset (when the switching board hasn't changed
     * anything to push), and it papers over any push that gets lost on
     * the bus. */
    if (channels_age < STALE_THRESHOLD) {
        channels_age++;
    }
    comm_send_channel_state_read();
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
