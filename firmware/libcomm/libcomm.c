#include "libcomm.h"

uint8_t comm_address(void) {
#ifdef DEVICE_TYPE_MAIN
    return COMM_ADDRESS_MAIN;
#elif defined DEVICE_TYPE_SWITCHING
    return COMM_ADDRESS_SWITCHING;
#elif defined DEVICE_TYPE_INPUT
    return PORTBbits.RB0 == 0 ? COMM_ADDRESS_BUTTON_BOARD_L : COMM_ADDRESS_BUTTON_BOARD_R;
#else
#error "Undefined device type. Define one of: DEVICE_TYPE_MAIN, DEVICE_TYPE_SWITCHING, DEVICE_TYPE_INPUT"
#endif
}

/* ============================================================================
 * button_effect (0x01)
 * ============================================================================ */

uint8_t comm_build_button_effect(CommMessage *msg, const CommButtonEffect *effect) {
    msg->id = COMM_BUTTON_EFFECT;
    msg->button_effect = *effect;
    return 1 + sizeof(CommButtonEffect);
}

void comm_parse_button_effect(const uint8_t *data, CommButtonEffect *effect) {
    effect->outputs_76 = data[0];
    effect->outputs_54 = data[1];
    effect->outputs_32 = data[2];
    effect->outputs_10 = data[3];
}

/* ============================================================================
 * button_changed (0x02)
 * ============================================================================ */

uint8_t comm_build_button_changed(CommMessage *msg, uint8_t prev_state, uint8_t current_state) {
    msg->id = COMM_BUTTON_CHANGED;
    msg->button_changed.device_address = comm_address();
    msg->button_changed.prev_state     = prev_state;
    msg->button_changed.current_state  = current_state;
    return 1 + sizeof(CommButtonChanged);
}

void comm_parse_button_changed(const uint8_t *data, CommButtonChanged *event) {
    event->device_address = data[0];
    event->prev_state     = data[1];
    event->current_state  = data[2];
}

/* ============================================================================
 * button_state_read (0x83)
 * ============================================================================ */

uint8_t comm_build_button_state_read(CommMessage *msg) {
    msg->id = COMM_BUTTON_STATE_READ;
    return 1;
}

void comm_parse_button_state_response(const uint8_t *data, CommButtonState *state) {
    state->current_state = data[0];
}

/* ============================================================================
 * button_trigger (0x04 / 0x84)
 * ============================================================================ */

uint8_t comm_build_button_trigger(CommMessage *msg, uint8_t button_id, CommTriggerConfig config) {
    msg->id = COMM_BUTTON_TRIGGER;
    msg->button_trigger.button_id = button_id & 0x07;
    msg->button_trigger.config = config;
    return 1 + sizeof(CommButtonTrigger);
}

uint8_t comm_build_button_trigger_read(CommMessage *msg, uint8_t button_id) {
    msg->id = COMM_BUTTON_TRIGGER_READ;
    msg->button_trigger.button_id = button_id & 0x07;
    return 1 + 1;
}

void comm_parse_button_trigger_write(const uint8_t *data, CommButtonTrigger *trigger) {
    trigger->button_id = data[0] & 0x07;
    *(uint8_t *)&trigger->config = data[1];
}

void comm_parse_button_trigger_response(const uint8_t *data, CommTriggerConfig *config) {
    *(uint8_t *)config = data[0];
}

/* TTTT x 10^EE multipliers for the MMEETTTT time field */
static const uint16_t comm_trigger_scale[4] = { 1, 10, 100, 1000 };

uint16_t comm_button_trigger_time_ms(CommTriggerConfig config) {
    return (uint16_t)config.time_mantissa * comm_trigger_scale[config.time_exponent];
}

CommTriggerConfig comm_button_trigger_make(CommButtonMode mode, uint16_t time_ms) {
    CommTriggerConfig c = { 0 };
    c.mode = mode & 0x03;
    if (time_ms > 15000) time_ms = 15000;
    uint8_t exp = 0;
    while (exp < 3 && time_ms > 15U * comm_trigger_scale[exp]) exp++;
    c.time_exponent = exp;
    c.time_mantissa = (uint8_t)(time_ms / comm_trigger_scale[exp]);
    return c;
}

/* ============================================================================
 * relay_state (0x05 / 0x85)
 * ============================================================================ */

uint8_t comm_build_relay_state(CommMessage *msg, uint16_t relays) {
    msg->id = COMM_RELAY_STATE;
    msg->relay_state.relays = relays;
    return 1 + sizeof(CommRelayState);
}

uint8_t comm_build_relay_state_read(CommMessage *msg) {
    msg->id = COMM_RELAY_STATE_READ;
    return 1;
}

void comm_parse_relay_state_write(const uint8_t *data, CommRelayState *state) {
    state->relays = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

void comm_parse_relay_state_response(const uint8_t *data, CommRelayState *state) {
    state->relays = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * relay_changed (0x06)
 * ============================================================================ */

uint8_t comm_build_relay_changed(CommMessage *msg,
                                 uint16_t prev_relays, uint16_t current_relays,
                                 uint8_t prev_sensors, uint8_t current_sensors) {
    msg->id = COMM_RELAY_CHANGED;
    msg->relay_changed.device_address  = comm_address();
    msg->relay_changed.prev_relays     = prev_relays;
    msg->relay_changed.current_relays  = current_relays;
    msg->relay_changed.prev_sensors    = prev_sensors;
    msg->relay_changed.current_sensors = current_sensors;
    return 1 + sizeof(CommRelayChanged);
}

void comm_parse_relay_changed(const uint8_t *data, CommRelayChanged *event) {
    event->device_address  = data[0];
    event->prev_relays     = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
    event->current_relays  = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    event->prev_sensors    = data[5];
    event->current_sensors = data[6];
}

/* ============================================================================
 * relay_mask (0x07 / 0x87)
 * ============================================================================ */

uint8_t comm_build_relay_mask(CommMessage *msg, uint16_t mask) {
    msg->id = COMM_RELAY_MASK;
    msg->relay_mask.mask = mask;
    return 1 + sizeof(CommRelayMask);
}

uint8_t comm_build_relay_mask_read(CommMessage *msg) {
    msg->id = COMM_RELAY_MASK_READ;
    return 1;
}

void comm_parse_relay_mask_write(const uint8_t *data, CommRelayMask *mask) {
    mask->mask = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

void comm_parse_relay_mask_response(const uint8_t *data, CommRelayMask *mask) {
    mask->mask = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * battery_read (0x88)
 * ============================================================================ */

uint8_t comm_build_battery_read(CommMessage *msg) {
    msg->id = COMM_BATTERY_READ;
    return 1;
}

void comm_parse_battery_response(const uint8_t *data, CommBattery *battery) {
    battery->voltage = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * levels_read (0x89)
 * ============================================================================ */

uint8_t comm_build_levels_read(CommMessage *msg) {
    msg->id = COMM_LEVELS_READ;
    return 1;
}

void comm_parse_levels_response(const uint8_t *data, CommLevels *levels) {
    levels->level_0 = data[0];
    levels->level_1 = data[1];
}

/* ============================================================================
 * level_mode (0x0A / 0x8A)
 * ============================================================================ */

uint8_t comm_build_level_mode(CommMessage *msg, CommMeterMode mode_0, CommMeterMode mode_1) {
    msg->id = COMM_LEVEL_MODE;
    msg->level_mode.mode_0 = (uint8_t)mode_0;
    msg->level_mode.mode_1 = (uint8_t)mode_1;
    return 1 + sizeof(CommLevelMode);
}

uint8_t comm_build_level_mode_read(CommMessage *msg) {
    msg->id = COMM_LEVEL_MODE_READ;
    return 1;
}

void comm_parse_level_mode_write(const uint8_t *data, CommLevelMode *mode) {
    *(uint8_t *)mode = data[0] & 0x0F;
}

void comm_parse_level_mode_response(const uint8_t *data, CommLevelMode *mode) {
    *(uint8_t *)mode = data[0] & 0x0F;
}

/* ============================================================================
 * sensors_read (0x8B)
 * ============================================================================ */

uint8_t comm_build_sensors_read(CommMessage *msg) {
    msg->id = COMM_SENSORS_READ;
    return 1;
}

void comm_parse_sensors_response(const uint8_t *data, CommSensors *sensors) {
    sensors->sensors = data[0] & 0x07;
}

/* ============================================================================
 * button_effect helpers
 * ============================================================================ */

void comm_button_effect_init(CommButtonEffect *effect) {
    effect->outputs_76 = 0;
    effect->outputs_54 = 0;
    effect->outputs_32 = 0;
    effect->outputs_10 = 0;
}

/* output layout: byte_index = (7 - output_index) / 2
 * even index -> lower nibble, odd index -> upper nibble */
int8_t comm_button_effect_set(CommButtonEffect *effect, uint8_t output_index, uint8_t value) {
    if (effect == NULL || output_index > 7 || value > 0xF) return -1;
    uint8_t *bytes = (uint8_t *)effect;
    uint8_t byte_index = (7 - output_index) / 2;
    if (output_index & 1) {
        bytes[byte_index] = (bytes[byte_index] & 0x0F) | (uint8_t)(value << 4);
    } else {
        bytes[byte_index] = (bytes[byte_index] & 0xF0) | value;
    }
    return 0;
}

int8_t comm_button_effect_get(const CommButtonEffect *effect, uint8_t output_index, uint8_t *value) {
    if (effect == NULL || output_index > 7 || value == NULL) return -1;
    const uint8_t *bytes = (const uint8_t *)effect;
    uint8_t byte_index = (7 - output_index) / 2;
    *value = (output_index & 1) ? (bytes[byte_index] >> 4) : (bytes[byte_index] & 0x0F);
    return 0;
}
