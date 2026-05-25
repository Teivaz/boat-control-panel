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

typedef enum {
    NAV_LIGHT_ERROR_NONE = 0,
    NAV_LIGHT_ERROR_CONFIG = 1 << 0, /* requested mode can't be realised with current enabled set */
} NavLightError;

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
        NavLightsMode nav_light;
        Channel channel;
        MenuControl menu;
    };
} ActionEvent;

/* All five nav-light bits in CHANNEL_NAV_* layout. */
#define NAV_MASK_CHANNELS                                                                                              \
    ((uint16_t)(CHANNEL_NAV_ANCHORING | CHANNEL_NAV_TRICOLOR | CHANNEL_NAV_STEAMING | CHANNEL_NAV_BOW |                \
                CHANNEL_NAV_STERN))

/* ----- prototypes ----- */
static ActionEvent action_for_button(ButtonIndex button);
static uint16_t mask_for_button(ButtonIndex button);
static uint16_t nav_lights_to_channels(NavLights nl_mask);
static uint16_t channels_for_nav_mode(NavLightsMode mode, uint8_t* config_error_out);
static NavLights nav_lights_from_channels(uint16_t channels);

static void toggle_power(void);
static void toggle_relay(Channel channel);
static void set_nav_light_mode(NavLightsMode mode);

static void set_channels(uint16_t new_target);
static void clear_channels(void);
static void apply_channel_observation(uint16_t observed);

static void retry_task(TaskId id, void* ctx);

static void poll_battery_task(TaskId id, void* ctx);
static void poll_levels_task(TaskId id, void* ctx);
static void poll_sensors_task(TaskId id, void* ctx);
static void poll_channels_task(TaskId id, void* ctx);

/* ----- state ----- */
/* Single-writer discipline: g_relay_target is only written by set_channels;
 * g_channel_state and g_on are only written by apply_channel_observation. */
static uint8_t g_on;                   /* mirrors (g_channel_state & CHANNEL_MAIN) */
static uint16_t g_relay_target;        /* what we want the bus to be */
static uint16_t g_relay_state;        /* what we want the bus to be */
static uint16_t g_channel_state;       /* what we last observed on the bus */
static NavLightsMode g_nav_light_mode; /* NavLightsMode */
static uint8_t g_nav_light_error;      /* NavLightError bitmask */

static uint8_t g_relay_inflight;

static uint16_t g_battery_mv;
static uint8_t g_levels[2];
static uint8_t g_sensor_state;

/* Cached for poll-task callbacks so they can adjust their own interval
 * when the power state changes. */
static TaskController* g_ctrl;

static uint16_t interval_for_task(uint8_t task_id) {
    return 2000u + (uint8_t)task_id * 100;
    switch (task_id) {
        case TASK_COMM_RETRY: return 19u;
        case TASK_POLL_BATTERY: return g_on ? 23u : 211u;
        case TASK_POLL_LEVELS: return g_on ? 29u : 239u;
        case TASK_POLL_SENSORS: return g_on ? 31u : 257u;
        case TASK_POLL_CHANNELS: return g_on ? 37u : 181u;
        default: return 41u;
    }
}

/* ============================================================================
 * Init
 * ============================================================================
 */

void controller_init(TaskController* ctrl) {
    g_ctrl = ctrl;
    g_on = 0;
    g_relay_target = 0;
    g_relay_state = 0;
    g_channel_state = 0;
    g_nav_light_mode = NAV_LIGHTS_MODE_OFF;
    g_nav_light_error = NAV_LIGHT_ERROR_NONE;
    g_relay_inflight = 0;
    g_battery_mv = 0;
    g_levels[0] = 0;
    g_levels[1] = 0;
    g_sensor_state = 0;

    // task_controller_add(ctrl, TASK_COMM_RETRY, interval_for_task(TASK_COMM_RETRY), retry_task, 0);
    // task_controller_add(ctrl, TASK_POLL_BATTERY,  interval_for_task(TASK_POLL_BATTERY), poll_battery_task,  0);
    // task_controller_add(ctrl, TASK_POLL_LEVELS,   interval_for_task(TASK_POLL_LEVELS), poll_levels_task,   0);
    // task_controller_add(ctrl, TASK_POLL_SENSORS,  interval_for_task(TASK_POLL_SENSORS), poll_sensors_task,  0);
    // task_controller_add(ctrl, TASK_POLL_CHANNELS, interval_for_task(TASK_POLL_CHANNELS), poll_channels_task, 0);
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
    if (button == BUTTON_L0) {
        return (ActionEvent){.type = ACTION_POWER};
    }

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

    if (!g_on) {
        return (ActionEvent){.type = ACTION_NONE};
    }

    /* Normal mode. Left: power, two helm relays, three nav-light modes,
     * inverter. Right: seven independent house-load relays. Nav buttons
     * map a *mode* (not a single relay) — set_nav_light_mode projects it
     * onto the appropriate set of CHANNEL_NAV_* bits. */
    switch (button) {
        case BUTTON_L1: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_INSTRUMENTS};
        case BUTTON_L2: return (ActionEvent){.type = ACTION_RELAY, .channel = CHANNEL_AUTOPILOT};
        case BUTTON_L3: return (ActionEvent){.type = ACTION_NAV, .nav_light = NAV_LIGHTS_MODE_STEAMING};
        case BUTTON_L4: return (ActionEvent){.type = ACTION_NAV, .nav_light = NAV_LIGHTS_MODE_RUNNING};
        case BUTTON_L5: return (ActionEvent){.type = ACTION_NAV, .nav_light = NAV_LIGHTS_MODE_ANCHORING};
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
        case BUTTON_L3: return g_nav_light_mode == NAV_LIGHTS_MODE_STEAMING ? NAV_MASK_CHANNELS : 0;
        case BUTTON_L4: return g_nav_light_mode == NAV_LIGHTS_MODE_RUNNING ? NAV_MASK_CHANNELS : 0;
        case BUTTON_L5: return g_nav_light_mode == NAV_LIGHTS_MODE_ANCHORING ? NAV_MASK_CHANNELS : 0;
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
static uint16_t nav_lights_to_channels(NavLights nav_lights) {
    uint16_t r = 0;
    if (nav_lights.anchoring) { r |= CHANNEL_NAV_ANCHORING; }
    if (nav_lights.tricolor)  { r |= CHANNEL_NAV_TRICOLOR; }
    if (nav_lights.steaming)  { r |= CHANNEL_NAV_STEAMING; }
    if (nav_lights.bow)       { r |= CHANNEL_NAV_BOW; }
    if (nav_lights.stern)     { r |= CHANNEL_NAV_STERN; }
    return r;
}

/* Translate a nav mode to the channel-bit pattern that realises it,
 * given the operator's enabled-lights config. `*config_error_out` is
 * set to 1 if the requested mode can't be realised with the available
 * lights (caller decides what to do with it). */
static uint16_t channels_for_nav_mode(NavLightsMode mode, uint8_t* config_error_out) {
    if (mode == NAV_LIGHTS_MODE_OFF) {
        if (config_error_out) {
            *config_error_out = 0;
        }
        return 0;
    }
    NavLights available = {.raw=config_get_nav_enabled_mask()};
    NavResolution r = nav_lights_resolve(mode, available);
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
        g_nav_light_mode = NAV_LIGHTS_MODE_OFF;
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
static void set_nav_light_mode(NavLightsMode mode) {
    if (g_nav_light_mode == mode || mode == NAV_LIGHTS_MODE_OFF) {
        g_nav_light_mode = NAV_LIGHTS_MODE_OFF;
    }
    else {
        g_nav_light_mode = mode;
    }
    uint8_t cfg_err = 0;
    uint16_t new_nav_bits = channels_for_nav_mode(g_nav_light_mode, &cfg_err);

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
    for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
        uint16_t m = mask_for_button((ButtonIndex)b);
        button_fx_set((ButtonIndex)b, new_target, m);
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
    if (g_relay_inflight) {
        return;
    }
    if (g_relay_target == g_relay_state) {
        return;
    }
    if (comm_send_relay_state(g_relay_target) == I2C_RESULT_OK) {
        g_relay_inflight = 1;
    }
}

void comm_on_relay_state_completion(I2cResult result, uint16_t relays) {
    g_relay_inflight = 0;
    if (result == I2C_RESULT_OK) {
        g_relay_state = relays;
    }
}

/* ============================================================================
 * L3 polling — battery / levels / sensors / channels
 * ============================================================================
 */

static void poll_battery_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, interval_for_task(id));
    comm_send_battery_read();
}

static void poll_levels_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, interval_for_task(id));
    comm_send_levels_read();
}

static void poll_sensors_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, interval_for_task(id));
    comm_send_sensors_read();
}

/* The switching board pushes a `channel_changed` message on every
 * transition, so the shadow is normally fresh without polling. The poll
 * still earns its keep: it seeds g_channel_state after a main-board
 * reset (when the switching board hasn't changed anything to push), and
 * papers over any push lost on the bus. */
static void poll_channels_task(TaskId id, void* ctx) {
    (void)ctx;
    task_controller_set_interval(g_ctrl, id, interval_for_task(id));
    comm_send_channel_state_read();
}

/* ============================================================================
 * L3 read responses
 * ============================================================================
 */

void comm_on_battery_read_response(CommBattery* battery) {
    if (battery) {
        g_battery_mv = battery->voltage;
    }
}

void comm_on_levels_read_response(CommLevels* lvl) {
    if (lvl) {
        g_levels[0] = lvl->level_0;
        g_levels[1] = lvl->level_1;
    }
}

void comm_on_sensors_read_response(CommSensors* sns) {
    if (sns) {
        g_sensor_state = sns->sensors;
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
