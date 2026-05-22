#include "controller.h"

#include "button_fx.h"
#include "config.h"
#include "config_mode.h"
#include "display_text.h"
#include "indicator.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "nav_lights.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

/* ============================================================================
 * Architecture
 *
 *   L1 INTENT    toggle_power / toggle_relay / set_nav_light_mode
 *   L2 STATE     set_channels(uint16_t)               sole writer of g_relay_target
 *                apply_channel_observation(uint16_t)  sole writer of g_channel_state
 *   L3 BUS OUT   retry_task, on_relay_state_done
 *   L3 BUS IN    poll_*_task, on_*_response
 *
 * Functions only call adjacent layers. L1 intent never touches the bus or
 * button_fx directly; L2 set_channels owns the fan-out to button_fx /
 * indicator / dirty flag; L3 bus paths only ever call back into
 * apply_channel_observation. There is no separate "intent vs target"
 * representation — intent functions compute the new channel mask and
 * hand it to set_channels.
 *
 * No power state machine: g_on follows the *observed* main bit. Per-
 * button "pending → error" feedback is owned by button_fx; per-nav-
 * light feedback is owned by indicator. button_fx is push-driven by
 * set_channels / apply_channel_observation; the indicator is pull-driven
 * — its refresh task reads controller_nav_* each frame.
 * ============================================================================
 */

typedef enum {
    NAV_LIGHT_ERROR_NONE = 0,
    NAV_LIGHT_ERROR_CONFIG = 1 << 0, /* requested mode can't be realised with current enabled set */
} NavLightError;

typedef enum {
    NAV_LIGHT_MODE_OFF,
    NAV_LIGHT_MODE_STEAMING,
    NAV_LIGHT_MODE_RUNNING,
    NAV_LIGHT_MODE_ANCHORING,
} NavLightMode;

typedef enum {
    ACTION_NONE,
    ACTION_POWER,
    ACTION_RELAY,
    ACTION_MENU,
    ACTION_NAV,
} ActionType;

typedef struct {
    ActionType type;
    union {
        NavLightMode nav_light;
        Channel channel;
        MenuControl menu;
    };
} ActionEvent;

#define RETRY_TICK_MS 200u
#define POLL_TICK_MS 100u
#define POLL_TICK_SLOW_MS 5000u
#define STALE_THRESHOLD (10000u / POLL_TICK_MS) /* 10 s */

/* All five nav-light bits in CHANNEL_NAV_* layout. */
#define NAV_MASK_CHANNELS                                                                                              \
    ((uint16_t)(CHANNEL_NAV_ANCHORING | CHANNEL_NAV_TRICOLOR | CHANNEL_NAV_STEAMING | CHANNEL_NAV_BOW |                \
                CHANNEL_NAV_STERN))

/* ----- prototypes ----- */
static ActionEvent action_for_button(ButtonIndex button);
static uint16_t mask_for_button(ButtonIndex button);
static uint16_t nav_lights_to_channels(uint8_t nl_mask);
static uint16_t channels_for_nav_mode(NavLightMode mode, uint8_t* config_error_out);
static NavLights nav_lights_from_channels(uint16_t channels);

static void toggle_power(void);
static void toggle_relay(Channel channel);
static void set_nav_light_mode(NavLightMode mode);

static void set_channels(uint16_t new_target);
static void clear_channels(void);
static void apply_channel_observation(uint16_t observed);

static void retry_task(TaskId id, void* ctx);

static uint16_t poll_interval_for_state(void);
static void poll_battery_task(TaskId id, void* ctx);
static void poll_levels_task(TaskId id, void* ctx);
static void poll_sensors_task(TaskId id, void* ctx);
static void poll_channels_task(TaskId id, void* ctx);

/* ----- state ----- */
/* Single-writer discipline: g_relay_target is only written by set_channels;
 * g_channel_state and g_on are only written by apply_channel_observation. */
static uint8_t g_on;                   /* mirrors (g_channel_state & CHANNEL_MAIN) */
static uint16_t g_relay_target;        /* what we want the bus to be */
static uint16_t g_channel_state;       /* what we last observed on the bus */
static uint8_t g_nav_light_mode;       /* NavLightMode */
static uint8_t g_nav_light_error;      /* NavLightError bitmask */

static uint8_t g_relay_dirty;          /* target diverged from last successful send */
static uint8_t g_relay_inflight;
static uint16_t g_relay_inflight_value;

static uint16_t g_battery_mv;
static uint8_t g_levels[2];
static uint8_t g_sensor_state;
static uint16_t g_batt_age;
static uint16_t g_levels_age;
static uint16_t g_sensors_age;
static uint16_t g_channels_age;

/* Single in-flight UI operation. The menu can only have one menu action
 * pending at a time, so a single slot suffices. */
static struct {
    ControllerOpCompletion op_cb;
    ControllerReadCompletion read_cb;
    void* ctx;
} g_ui_op;

/* Cached for poll-task callbacks so they can adjust their own interval
 * when the power state changes. */
static TaskController* g_ctrl;

/* ============================================================================
 * Init
 * ============================================================================
 */

void controller_init(TaskController* ctrl) {
    g_ctrl = ctrl;
    g_on = 0;
    g_relay_target = 0;
    g_channel_state = 0;
    g_nav_light_mode = NAV_LIGHT_MODE_OFF;
    g_nav_light_error = NAV_LIGHT_ERROR_NONE;
    g_relay_dirty = 0;
    g_relay_inflight = 0;
    g_relay_inflight_value = 0;
    g_battery_mv = 0;
    g_levels[0] = 0;
    g_levels[1] = 0;
    g_sensor_state = 0;
    g_batt_age = STALE_THRESHOLD;
    g_levels_age = STALE_THRESHOLD;
    g_sensors_age = STALE_THRESHOLD;
    g_channels_age = STALE_THRESHOLD;

    task_controller_add(ctrl, TASK_COMM_RETRY, RETRY_TICK_MS, retry_task, 0);
    task_controller_add(ctrl, TASK_POLL_BATTERY, POLL_TICK_MS, poll_battery_task, 0);
    task_controller_add(ctrl, TASK_POLL_LEVELS, POLL_TICK_MS, poll_levels_task, 0);
    task_controller_add(ctrl, TASK_POLL_SENSORS, POLL_TICK_MS, poll_sensors_task, 0);
    task_controller_add(ctrl, TASK_POLL_CHANNELS, POLL_TICK_MS, poll_channels_task, 0);
}

void comm_on_button_changed_received(CommButtonChanged* event) {
    if (!event) {
        return;
    }
    /* Combine the I²C side address into the ButtonIndex encoding (bit 3
     * = side, low 3 bits = per-side index). */
    ButtonIndex button = event->button_id;
    switch (event->device_address) {
        case COMM_ADDRESS_BUTTON_BOARD_L:
            break;
        case COMM_ADDRESS_BUTTON_BOARD_R:
            button = (ButtonIndex)(button | 0x8u);
            break;
        default:
            return;
    }

    ActionEvent action = action_for_button(button);
    if (!config_mode_active() && !g_on && action.type != ACTION_POWER) {
        return;
    }

    switch (action.type) {
        case ACTION_POWER:
            toggle_power();
            break;
        case ACTION_RELAY:
            toggle_relay(action.channel);
            break;
        case ACTION_MENU:
            config_mode_on_action(action.menu);
            break;
        case ACTION_NAV:
            set_nav_light_mode(action.nav_light);
            break;
        case ACTION_NONE:
        default:
            break;
    }
}

void comm_on_channel_changed_received(CommChannelChanged* event) {
    if (!event) {
        return;
    }
    g_sensor_state = event->current_sensors;
    g_sensors_age = 0;
    apply_channel_observation(event->current_channels);
}

void comm_on_channel_state_read_response(CommChannelState* state) {
    if (!state) {
        return;
    }
    apply_channel_observation(state->channels);
}

/* ============================================================================
 * Button → action mapping
 * ============================================================================
 */

/* Config mode is a strict menu-navigation view: only the four left-side
 * buttons L1..L4 translate to a meaningful event; everything else is
 * deliberately swallowed so a misclick doesn't toggle a relay while the
 * operator is reconfiguring something. L0 stays inert here — exit is
 * via the dedicated RA7 config switch, not via the menu. */
static ActionEvent action_for_button(ButtonIndex button) {
    if (config_mode_active()) {
        switch (button) {
            case BUTTON_L1:
                return (ActionEvent){.type = ACTION_MENU, .menu = MENU_CONTROL_NEXT};
            case BUTTON_L2:
                return (ActionEvent){.type = ACTION_MENU, .menu = MENU_CONTROL_PREV};
            case BUTTON_L3:
                return (ActionEvent){.type = ACTION_MENU, .menu = MENU_CONTROL_ENTER};
            case BUTTON_L4:
                return (ActionEvent){.type = ACTION_MENU, .menu = MENU_CONTROL_EXIT};
            default:
                return (ActionEvent){.type = ACTION_NONE};
        }
    }

    /* Normal mode. Left: power, two helm relays, three nav-light modes,
     * inverter. Right: seven independent house-load relays. Nav buttons
     * map a *mode* (not a single relay) — set_nav_light_mode projects it
     * onto the appropriate set of CHANNEL_NAV_* bits. */
    switch (button) {
        case BUTTON_L0: return (ActionEvent){.type = ACTION_POWER};
        case BUTTON_L1: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_INSTRUMENTS};
        case BUTTON_L2: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_AUTOPILOT};
        case BUTTON_L3: return (ActionEvent){.type = ACTION_NAV, .nav_light = NAV_LIGHT_MODE_STEAMING};
        case BUTTON_L4: return (ActionEvent){.type = ACTION_NAV, .nav_light = NAV_LIGHT_MODE_RUNNING};
        case BUTTON_L5: return (ActionEvent){.type = ACTION_NAV, .nav_light = NAV_LIGHT_MODE_ANCHORING};
        case BUTTON_L6: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_INVERTER};
        case BUTTON_R0: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_WATER_PUMP};
        case BUTTON_R1: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_FRIDGE};
        case BUTTON_R2: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_DECK_LIGHTS};
        case BUTTON_R3: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_CABIN_LIGHTS};
        case BUTTON_R4: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_USB};
        case BUTTON_R5: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_AUX_1};
        case BUTTON_R6: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_AUX_2};
        default: return (ActionEvent){.type = ACTION_NONE};
    }
}

/* Which channel bit(s) the button represents, for button_fx's pending
 * mask. Nav buttons map to the whole nav band because one button
 * controls several lights at once. */
static uint16_t mask_for_button(ButtonIndex button) {
    switch (button) {
        case BUTTON_L0: return CHANNEL_MAIN;
        case BUTTON_L1: return CHANNEL_INSTRUMENTS;
        case BUTTON_L2: return CHANNEL_AUTOPILOT;
        case BUTTON_L3: return g_nav_light_mode == NAV_LIGHT_MODE_STEAMING ? NAV_MASK_CHANNELS : 0;
        case BUTTON_L4: return g_nav_light_mode == NAV_LIGHT_MODE_RUNNING ? NAV_MASK_CHANNELS : 0;
        case BUTTON_L5: return g_nav_light_mode == NAV_LIGHT_MODE_ANCHORING ? NAV_MASK_CHANNELS : 0;
        case BUTTON_L6: return CHANNEL_INVERTER;
        case BUTTON_R0: return CHANNEL_WATER_PUMP;
        case BUTTON_R1: return CHANNEL_FRIDGE;
        case BUTTON_R2: return CHANNEL_DECK_LIGHTS;
        case BUTTON_R3: return CHANNEL_CABIN_LIGHTS;
        case BUTTON_R4: return CHANNEL_USB;
        case BUTTON_R5: return CHANNEL_AUX_1;
        case BUTTON_R6: return CHANNEL_AUX_2;
        default: return 0;
    }
}

/* Map nav_lights_resolve's 5-bit NAV_LIGHT_* output to the corresponding
 * CHANNEL_NAV_* bits. Kept as one isolated function so the encoding-
 * crossing happens in exactly one place. */
static uint16_t nav_lights_to_channels(uint8_t nl_mask) {
    uint16_t r = 0;
    if (nl_mask & NAV_LIGHT_ANCHORING) { r |= CHANNEL_NAV_ANCHORING; }
    if (nl_mask & NAV_LIGHT_TRICOLOR)  { r |= CHANNEL_NAV_TRICOLOR; }
    if (nl_mask & NAV_LIGHT_STEAMING)  { r |= CHANNEL_NAV_STEAMING; }
    if (nl_mask & NAV_LIGHT_BOW)       { r |= CHANNEL_NAV_BOW; }
    if (nl_mask & NAV_LIGHT_STERN)     { r |= CHANNEL_NAV_STERN; }
    return r;
}

/* Translate a nav mode to the channel-bit pattern that realises it,
 * given the operator's enabled-lights config. `*config_error_out` is
 * set to 1 if the requested mode can't be realised with the available
 * lights (caller decides what to do with it). */
static uint16_t channels_for_nav_mode(NavLightMode mode, uint8_t* config_error_out) {
    if (mode == NAV_LIGHT_MODE_OFF) {
        if (config_error_out) {
            *config_error_out = 0;
        }
        return 0;
    }
    uint8_t enabled = config_get_nav_enabled_mask();
    /* The NavMode enum in nav_lights.h is laid out so OFF=0 lines up
     * with NAV_LIGHT_MODE_OFF=0; the other three values just have
     * different numeric ids. Pass through directly — same semantic. */
    NavMode m;
    switch (mode) {
        case NAV_LIGHT_MODE_STEAMING:   m = NAV_MODE_STEAMING; break;
        case NAV_LIGHT_MODE_RUNNING:    m = NAV_MODE_RUNNING; break;
        case NAV_LIGHT_MODE_ANCHORING:  m = NAV_MODE_ANCHORING; break;
        default:                        m = NAV_MODE_OFF; break;
    }
    NavResolution r = nav_lights_resolve(m, enabled);
    if (config_error_out) {
        *config_error_out = r.error ? 1u : 0u;
    }
    return nav_lights_to_channels(r.lights_mask);
}

/* Pack the five CHANNEL_NAV_* bits into the indicator's 5-bit NavLights
 * struct. Inverse of nav_lights_to_channels in spirit but with the
 * NavLights union layout. */
static NavLights nav_lights_from_channels(uint16_t channels) {
    NavLights nl;
    nl.raw = 0;
    nl.anchoring = (channels & CHANNEL_NAV_ANCHORING) ? 1u : 0u;
    nl.tricolor  = (channels & CHANNEL_NAV_TRICOLOR)  ? 1u : 0u;
    nl.steaming  = (channels & CHANNEL_NAV_STEAMING)  ? 1u : 0u;
    nl.bow       = (channels & CHANNEL_NAV_BOW)       ? 1u : 0u;
    nl.stern     = (channels & CHANNEL_NAV_STERN)     ? 1u : 0u;
    return nl;
}

/* ============================================================================
 * L1 intent
 * ============================================================================
 */

static void toggle_power(void) {
    g_on = !g_on;
    if (g_on) {
        /* Waking up: only main goes on. Nav lights and house loads stay
         * dark until the user picks them — predictable startup. */
        set_channels(CHANNEL_MAIN);
    } else {
        /* Going dark: drop every channel, reset nav mode so it doesn't
         * silently re-arm next time. */
        g_nav_light_mode = NAV_LIGHT_MODE_OFF;
        g_nav_light_error = NAV_LIGHT_ERROR_NONE;
        clear_channels();
    }
    display_text_set_active(g_on);
    indicator_set_active(g_on);
}

static void toggle_relay(Channel channel) {
    set_channels((uint16_t)(g_relay_target ^ (uint16_t)channel));
}

/* Same-mode press toggles off; different-mode press switches. The OFF
 * pseudo-mode collapses to "no nav lights" and clears the config error. */
static void set_nav_light_mode(NavLightMode mode) {
    NavLightMode new_mode;
    if (g_nav_light_mode == mode || mode == NAV_LIGHT_MODE_OFF) {
        new_mode = NAV_LIGHT_MODE_OFF;
    }
    else {
        new_mode = mode;
    }
    uint8_t cfg_err = 0;
    uint16_t new_nav_bits = channels_for_nav_mode(new_mode, &cfg_err);

    g_nav_light_mode = (uint8_t)new_mode;
    g_nav_light_error = cfg_err ? (uint8_t)NAV_LIGHT_ERROR_CONFIG : (uint8_t)NAV_LIGHT_ERROR_NONE;

    set_channels((uint16_t)((g_relay_target & ~NAV_MASK_CHANNELS) | new_nav_bits));
}

/* ============================================================================
 * L2 state mutators
 * ============================================================================
 */

/* Sole writer of g_relay_target. Fans out to button_fx (per-button
 * pending expectations), marks the bus dirty for retry_task, and asks
 * the indicator to refresh. */
static void set_channels(uint16_t new_target) {
    g_relay_target = new_target;
    g_relay_dirty = 1;
    for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
        uint16_t m = mask_for_button((ButtonIndex)b);
        button_fx_set((ButtonIndex)b, (uint16_t)(new_target & m), m);
    }
}

/* Sole writer of g_relay_target. Fans out to button_fx (per-button
 * pending expectations), marks the bus dirty for retry_task, and asks
 * the indicator to refresh. */
static void clear_channels(void) {
    for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
        button_fx_clear((ButtonIndex)b);
    }
}

/* Sole writer of g_channel_state. */
static void apply_channel_observation(uint16_t observed) {
    g_channel_state = observed;
    g_channels_age = 0;
    button_fx_on_channel_state((Channel)observed);
}

/* ============================================================================
 * L3 bus output — relay_state
 * ============================================================================
 */

/* Single-slot latest-wins: when set_channels bumps the target while a
 * write is in flight we don't queue a second one, just remember that
 * we're dirty. retry_task re-fires from the next tick. */
static void retry_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    if (!g_relay_dirty || g_relay_inflight) {
        return;
    }
    uint16_t snapshot;
    INTERRUPT_PUSH;
    snapshot = g_relay_target;
    INTERRUPT_POP;

    g_relay_inflight_value = snapshot;
    g_relay_inflight = 1;
    if (comm_send_relay_state(snapshot) != I2C_RESULT_OK) {
        g_relay_inflight = 0;
    }
}

void comm_on_relay_state_completion(I2cResult result, uint16_t relays) {
    (void)result;
    (void)relays;
    /* Clear dirty only if the value we just landed still matches the
     * current target — a producer could have bumped it between our
     * snapshot and the completion. */
    INTERRUPT_PUSH;
    if (g_relay_target == g_relay_inflight_value) {
        g_relay_dirty = 0;
    }
    INTERRUPT_POP;
    g_relay_inflight = 0;
}

/* ============================================================================
 * L3 polling — battery / levels / sensors / channels
 * ============================================================================
 */

/* Battery + levels are still useful while the panel is "off" (inspector
 * / config UI may want them), so they poll at the slow cadence in OFF /
 * PENDING. Sensors are consumer-facing (bilge alarm etc.) and are
 * skipped entirely when the panel isn't on. */
static uint16_t poll_interval_for_state(void) {
    return g_on ? POLL_TICK_MS : POLL_TICK_SLOW_MS;
}

static void poll_battery_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    if (g_batt_age < STALE_THRESHOLD) {
        g_batt_age++;
    }
    comm_send_battery_read();
}

static void poll_levels_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    if (g_levels_age < STALE_THRESHOLD) {
        g_levels_age++;
    }
    comm_send_levels_read();
}

static void poll_sensors_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    if (g_sensors_age < STALE_THRESHOLD) {
        g_sensors_age++;
    }
    comm_send_sensors_read();
}

/* The switching board pushes a `channel_changed` message on every
 * transition, so the shadow is normally fresh without polling. The poll
 * still earns its keep: it seeds g_channel_state after a main-board
 * reset (when the switching board hasn't changed anything to push), and
 * papers over any push lost on the bus. */
static void poll_channels_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, poll_interval_for_state());
    if (g_channels_age < STALE_THRESHOLD) {
        g_channels_age++;
    }
    comm_send_channel_state_read();
}

/* ============================================================================
 * L3 read responses
 * ============================================================================
 */

void comm_on_battery_read_response(CommBattery* battery) {
    if (battery) {
        g_battery_mv = battery->voltage;
        g_batt_age = 0;
    }
}

void comm_on_levels_read_response(CommLevels* lvl) {
    if (lvl) {
        g_levels[0] = lvl->level_0;
        g_levels[1] = lvl->level_1;
        g_levels_age = 0;
    }
}

void comm_on_sensors_read_response(CommSensors* sns) {
    if (sns) {
        g_sensor_state = sns->sensors;
        g_sensors_age = 0;
    }
}

/* ============================================================================
 * State queries
 * ============================================================================
 */

uint8_t controller_power_on(void) {
    return g_on;
}

uint16_t controller_battery_mv(void) {
    return g_battery_mv;
}

uint8_t controller_level(uint8_t i) {
    return (i < 2u) ? g_levels[i] : 0u;
}

uint8_t controller_sensors(void) {
    return g_sensor_state;
}

uint8_t controller_battery_stale(void) {
    return (uint8_t)(g_batt_age >= STALE_THRESHOLD);
}

uint8_t controller_levels_stale(void) {
    return (uint8_t)(g_levels_age >= STALE_THRESHOLD);
}

uint8_t controller_sensors_stale(void) {
    return (uint8_t)(g_sensors_age >= STALE_THRESHOLD);
}

/* Nav-light state queries — the indicator's refresh task pulls these
 * each frame to decide what to render. The four are O(1) bit-ops over
 * the existing globals; no allocation, no I²C. */
NavLights controller_nav_enabled(void) {
    return nav_lights_from_channels((uint16_t)(g_relay_target & NAV_MASK_CHANNELS));
}

NavLights controller_nav_pending(void) {
    uint16_t target_nav = (uint16_t)(g_relay_target & NAV_MASK_CHANNELS);
    uint16_t observed_nav = (uint16_t)(g_channel_state & NAV_MASK_CHANNELS);
    return nav_lights_from_channels((uint16_t)(target_nav & ~observed_nav));
}

/* Reserved for the per-light timeout that doesn't exist yet — always 0
 * for now. The indicator already routes errored bits via priority
 * cascade, so wiring a real source later is a single-line change here. */
NavLights controller_nav_errored(void) {
    return (NavLights){.raw = 0};
}

uint8_t controller_nav_config_error(void) {
    return (uint8_t)((g_nav_light_error & NAV_LIGHT_ERROR_CONFIG) ? 1u : 0u);
}

/* ============================================================================
 * UI ops — async menu actions that need to land safely or report back
 * ============================================================================
 */

void controller_read_config(uint8_t board_addr, uint8_t address, ControllerReadCompletion cb, void* ctx) {
    /* Reading our own config is in-RAM and synchronous; no point routing
     * through I2C to ourselves.  Fire the cb inline so callers don't need
     * a special path for local-vs-remote. */
    if (board_addr == COMM_ADDRESS_MAIN) {
        if (cb) {
            cb(1, config_read_byte(address), ctx);
        }
        return;
    }
    g_ui_op.read_cb = cb;
    g_ui_op.ctx = ctx;
    if (comm_send_config_read(board_addr, address) != I2C_RESULT_OK) {
        g_ui_op.read_cb = 0;
        if (cb) {
            cb(0, 0, ctx);
        }
    }
}

void comm_on_config_read_response(uint8_t addr, uint8_t* value) {
    (void)addr;
    ControllerReadCompletion cb = g_ui_op.read_cb;
    void* user_ctx = g_ui_op.ctx;
    g_ui_op.read_cb = 0;
    if (cb) {
        cb(value != 0, value ? *value : 0u, user_ctx);
    }
}

void controller_write_config(uint8_t board_addr, uint8_t address, uint8_t value, ControllerOpCompletion cb,
                             void* ctx) {
    /* Local writes go to the in-RAM shadow + deferred EEPROM queue
     * directly — no I2C self-loop.  ok=1 here means "queued"; the
     * EEPROM flush task drains it later, same trust model as the
     * remote case (where ok=1 means the peer ACKed but its own EEPROM
     * commit also runs asynchronously). */
    if (board_addr == COMM_ADDRESS_MAIN) {
        config_write_byte(address, value);
        if (cb) {
            cb(1, ctx);
        }
        return;
    }
    g_ui_op.op_cb = cb;
    g_ui_op.ctx = ctx;
    if (comm_send_config(board_addr, address, value) != I2C_RESULT_OK) {
        g_ui_op.op_cb = 0;
        if (cb) {
            cb(0, ctx);
        }
    }
}

void comm_on_config_completion(I2cResult result, uint8_t addr, uint8_t config_addr, uint8_t value) {
    (void)addr;
    (void)config_addr;
    (void)value;
    ControllerOpCompletion cb = g_ui_op.op_cb;
    void* user_ctx = g_ui_op.ctx;
    g_ui_op.op_cb = 0;
    if (cb) {
        cb(result == I2C_RESULT_OK, user_ctx);
    }
}
