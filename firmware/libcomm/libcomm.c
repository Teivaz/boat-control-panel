#include "libcomm.h"

uint8_t comm_can_parse(const uint8_t* data, uint8_t len) {
    if (len == 0) {
        return 0;
    }
    switch (data[0]) {
        case COMM_BUTTON_EFFECT:
            return len == 1 + sizeof(CommButtonEffect);
        case COMM_BUTTON_CHANGED:
            return len == 1 + sizeof(CommButtonChanged);
        case COMM_BUTTON_TRIGGER:
            return len == 1 + sizeof(CommButtonTrigger);
        case COMM_RELAY_STATE:
            return len == 1 + sizeof(CommRelayState);
        case COMM_RELAY_CHANGED:
            return len == 1 + sizeof(CommRelayChanged);
        case COMM_RELAY_MASK:
            return len == 1 + sizeof(CommRelayMask);
        case COMM_LEVEL_MODE:
            return len == 1 + sizeof(CommLevelMode);
        case COMM_CONFIG:
            return len == 1 + sizeof(CommConfig);
        case COMM_RESET:
            return len == 1;
        case COMM_BUTTON_STATE_READ:
            return len == 1;
        case COMM_BUTTON_TRIGGER_READ:
            return len == 2;
        case COMM_RELAY_STATE_READ:
            return len == 1;
        case COMM_RELAY_MASK_READ:
            return len == 1;
        case COMM_BATTERY_READ:
            return len == 1;
        case COMM_LEVELS_READ:
            return len == 1;
        case COMM_LEVEL_MODE_READ:
            return len == 1;
        case COMM_SENSORS_READ:
            return len == 1;
        case COMM_CONFIG_READ:
            return len == 2;
        default:
            return 0;
    }
}

/* ============================================================================
 * button_effect (0x01)
 * ============================================================================
 */

uint8_t comm_build_button_effect(CommMessage* msg, const CommButtonEffect* effect) {
    msg->id = COMM_BUTTON_EFFECT;
    msg->button_effect = *effect;
    return 1 + sizeof(CommButtonEffect);
}

void comm_parse_button_effect(const uint8_t* data, CommButtonEffect* effect) {
    effect->outputs_76 = data[0];
    effect->outputs_54 = data[1];
    effect->outputs_32 = data[2];
    effect->outputs_10 = data[3];
}

/* ============================================================================
 * button_changed (0x02)
 * ============================================================================
 */

uint8_t comm_build_button_changed(CommMessage* msg, uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
    msg->id = COMM_BUTTON_CHANGED;
    msg->button_changed.device_address = comm_address();
    msg->button_changed.button_id = button_id & 0x07;
    msg->button_changed.pressed = pressed & 0x01;
    msg->button_changed.mode = (uint8_t)mode & 0x03;
    return 1 + sizeof(CommButtonChanged);
}

void comm_parse_button_changed(const uint8_t* data, CommButtonChanged* event) {
    event->device_address = data[0];
    event->button_id = data[1] & 0x07;
    event->pressed = (data[1] >> 3) & 0x01;
    event->mode = (data[1] >> 4) & 0x03;
}

/* ============================================================================
 * button_state_read (0x83)
 * ============================================================================
 */

uint8_t comm_build_button_state_read(CommMessage* msg) {
    msg->id = COMM_BUTTON_STATE_READ;
    return 1;
}

void comm_parse_button_state_response(const uint8_t* data, CommButtonState* state) {
    state->current_state = data[0];
}

/* ============================================================================
 * button_trigger (0x04 / 0x84)
 * ============================================================================
 */

uint8_t comm_build_button_trigger(CommMessage* msg, uint8_t button_id, CommTriggerConfig config) {
    msg->id = COMM_BUTTON_TRIGGER;
    msg->button_trigger.button_id = button_id & 0x07;
    msg->button_trigger.config = config;
    return 1 + sizeof(CommButtonTrigger);
}

uint8_t comm_build_button_trigger_read(CommMessage* msg, uint8_t button_id) {
    msg->id = COMM_BUTTON_TRIGGER_READ;
    msg->button_trigger.button_id = button_id & 0x07;
    return 1 + 1;
}

void comm_parse_button_trigger_write(const uint8_t* data, CommButtonTrigger* trigger) {
    trigger->button_id = data[0] & 0x07;
    *(uint8_t*)&trigger->config = data[1];
}

void comm_parse_button_trigger_response(const uint8_t* data, CommTriggerConfig* config) {
    *(uint8_t*)config = data[0];
}

/* TTTT x 10^EE multipliers for the MMEETTTT time field */
static const uint16_t comm_trigger_scale[4] = {1, 10, 100, 1000};

uint16_t comm_button_trigger_time_ms(CommTriggerConfig config) {
    return (uint16_t)config.time_mantissa * comm_trigger_scale[config.time_exponent];
}

CommTriggerConfig comm_button_trigger_make(CommButtonMode mode, uint16_t time_ms) {
    CommTriggerConfig c = {0};
    c.mode = mode & 0x03;
    if (time_ms > 15000) {
        time_ms = 15000;
    }
    uint8_t exp = 0;
    while (exp < 3 && time_ms > 15U * comm_trigger_scale[exp]) {
        exp++;
    }
    c.time_exponent = exp;
    c.time_mantissa = (uint8_t)(time_ms / comm_trigger_scale[exp]);
    return c;
}

/* ============================================================================
 * relay_state (0x05 / 0x85)
 * ============================================================================
 */

uint8_t comm_build_relay_state(CommMessage* msg, uint16_t relays) {
    msg->id = COMM_RELAY_STATE;
    msg->relay_state.relays = relays;
    return 1 + sizeof(CommRelayState);
}

uint8_t comm_build_relay_state_read(CommMessage* msg) {
    msg->id = COMM_RELAY_STATE_READ;
    return 1;
}

void comm_parse_relay_state_write(const uint8_t* data, CommRelayState* state) {
    state->relays = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

void comm_parse_relay_state_response(const uint8_t* data, CommRelayState* state) {
    state->relays = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * relay_changed (0x06)
 * ============================================================================
 */

uint8_t comm_build_relay_changed(CommMessage* msg, uint16_t prev_relays, uint16_t current_relays, uint8_t prev_sensors,
                                 uint8_t current_sensors) {
    msg->id = COMM_RELAY_CHANGED;
    msg->relay_changed.device_address = comm_address();
    msg->relay_changed.prev_relays = prev_relays;
    msg->relay_changed.current_relays = current_relays;
    msg->relay_changed.prev_sensors = prev_sensors;
    msg->relay_changed.current_sensors = current_sensors;
    return 1 + sizeof(CommRelayChanged);
}

void comm_parse_relay_changed(const uint8_t* data, CommRelayChanged* event) {
    event->device_address = data[0];
    event->prev_relays = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
    event->current_relays = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    event->prev_sensors = data[5];
    event->current_sensors = data[6];
}

/* ============================================================================
 * relay_mask (0x07 / 0x87)
 * ============================================================================
 */

uint8_t comm_build_relay_mask(CommMessage* msg, uint16_t mask) {
    msg->id = COMM_RELAY_MASK;
    msg->relay_mask.mask = mask;
    return 1 + sizeof(CommRelayMask);
}

uint8_t comm_build_relay_mask_read(CommMessage* msg) {
    msg->id = COMM_RELAY_MASK_READ;
    return 1;
}

void comm_parse_relay_mask_write(const uint8_t* data, CommRelayMask* mask) {
    mask->mask = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

void comm_parse_relay_mask_response(const uint8_t* data, CommRelayMask* mask) {
    mask->mask = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * battery_read (0x88)
 * ============================================================================
 */

uint8_t comm_build_battery_read(CommMessage* msg) {
    msg->id = COMM_BATTERY_READ;
    return 1;
}

void comm_parse_battery_response(const uint8_t* data, CommBattery* battery) {
    battery->voltage = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * levels_read (0x89)
 * ============================================================================
 */

uint8_t comm_build_levels_read(CommMessage* msg) {
    msg->id = COMM_LEVELS_READ;
    return 1;
}

void comm_parse_levels_response(const uint8_t* data, CommLevels* levels) {
    levels->level_0 = data[0];
    levels->level_1 = data[1];
}

/* ============================================================================
 * level_mode (0x0A / 0x8A)
 * ============================================================================
 */

uint8_t comm_build_level_mode(CommMessage* msg, CommMeterMode mode_0, CommMeterMode mode_1) {
    msg->id = COMM_LEVEL_MODE;
    msg->level_mode.mode_0 = (uint8_t)mode_0;
    msg->level_mode.mode_1 = (uint8_t)mode_1;
    return 1 + sizeof(CommLevelMode);
}

uint8_t comm_build_level_mode_read(CommMessage* msg) {
    msg->id = COMM_LEVEL_MODE_READ;
    return 1;
}

void comm_parse_level_mode_write(const uint8_t* data, CommLevelMode* mode) {
    *(uint8_t*)mode = data[0] & 0x0F;
}

void comm_parse_level_mode_response(const uint8_t* data, CommLevelMode* mode) {
    *(uint8_t*)mode = data[0] & 0x0F;
}

/* ============================================================================
 * sensors_read (0x8B)
 * ============================================================================
 */

uint8_t comm_build_sensors_read(CommMessage* msg) {
    msg->id = COMM_SENSORS_READ;
    return 1;
}

void comm_parse_sensors_response(const uint8_t* data, CommSensors* sensors) {
    sensors->sensors = data[0] & 0x07;
}

/* ============================================================================
 * reset (0x0F) / config (0x0E / 0x8E)
 * ============================================================================
 */

uint8_t comm_build_reset(CommMessage* msg) {
    msg->id = COMM_RESET;
    return 1;
}

uint8_t comm_build_config(CommMessage* msg, uint8_t address, uint8_t value) {
    msg->id = COMM_CONFIG;
    msg->config.address = address;
    msg->config.value = value;
    return 1 + sizeof(CommConfig);
}

uint8_t comm_build_config_read(CommMessage* msg, uint8_t address) {
    msg->id = COMM_CONFIG_READ;
    msg->config.address = address;
    return 1 + 1;
}

void comm_parse_config_write(const uint8_t* data, CommConfig* config) {
    config->address = data[0];
    config->value = data[1];
}

void comm_parse_config_read_request(const uint8_t* data, uint8_t* address) {
    *address = data[0];
}

void comm_parse_config_response(const uint8_t* data, uint8_t* value) {
    *value = data[0];
}

/* ============================================================================
 * button_effect helpers
 * ============================================================================
 */

void comm_button_effect_init(CommButtonEffect* effect) {
    effect->outputs_76 = 0;
    effect->outputs_54 = 0;
    effect->outputs_32 = 0;
    effect->outputs_10 = 0;
}

/* output layout: byte_index = (7 - output_index) / 2
 * even index -> lower nibble, odd index -> upper nibble */
int8_t comm_button_effect_set(CommButtonEffect* effect, uint8_t output_index, CommButtonOutputEffect value) {
    if (effect == 0 || output_index > 7) {
        return -1;
    }
    uint8_t* bytes = (uint8_t*)effect;
    uint8_t byte_index = (7 - output_index) / 2;
    uint8_t nibble = value.raw & 0x0F;
    if (output_index & 1) {
        bytes[byte_index] = (bytes[byte_index] & 0x0F) | (uint8_t)(nibble << 4);
    } else {
        bytes[byte_index] = (bytes[byte_index] & 0xF0) | nibble;
    }
    return 0;
}

int8_t comm_button_effect_get(const CommButtonEffect* effect, uint8_t output_index, CommButtonOutputEffect* value) {
    if (effect == 0 || output_index > 7 || value == 0) {
        return -1;
    }
    const uint8_t* bytes = (const uint8_t*)effect;
    uint8_t byte_index = (7 - output_index) / 2;
    value->raw = (output_index & 1) ? (bytes[byte_index] >> 4) : (bytes[byte_index] & 0x0F);
    return 0;
}
