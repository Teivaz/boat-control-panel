#include "libcomm.h"

/* Write the trailing CRC and return the total byte count.  The caller hands
 * us the payload length (without id and without CRC); we finalise as
 *   [id] [payload...] [crc] — crc over id + payload.
 *
 * Hot-path sizes are unrolled so XC8 emits straight-line PFM reads with no
 * loop-counter overhead — the total byte span (id + payload) is always a
 * compile-time constant at the builder call sites.  crc8_table is defined
 * in crc.c and declared extern via crc.h (pulled in by libcomm.h). */
static uint8_t comm_finalize(CommMessage* msg, uint8_t payload_len) {
    const uint8_t n = (uint8_t)(1u + payload_len);
    const uint8_t* b = (const uint8_t*)msg;
    uint8_t crc;
    switch (n) {
        case 2: /* 1-byte payload messages: button_trigger_read, level_mode, config_read */
            crc = crc8_table[0xFFu ^ b[0]];
            crc = crc8_table[crc ^ b[1]];
            break;
        case 3: /* 2-byte payload: button_changed, button_trigger, relay_state, config */
            crc = crc8_table[0xFFu ^ b[0]];
            crc = crc8_table[crc ^ b[1]];
            crc = crc8_table[crc ^ b[2]];
            break;
        case 5: /* 4-byte payload: button_effect */
            crc = crc8_table[0xFFu ^ b[0]];
            crc = crc8_table[crc ^ b[1]];
            crc = crc8_table[crc ^ b[2]];
            crc = crc8_table[crc ^ b[3]];
            crc = crc8_table[crc ^ b[4]];
            break;
        case 8: /* 7-byte payload: channel_changed */
            crc = crc8_table[0xFFu ^ b[0]];
            crc = crc8_table[crc ^ b[1]];
            crc = crc8_table[crc ^ b[2]];
            crc = crc8_table[crc ^ b[3]];
            crc = crc8_table[crc ^ b[4]];
            crc = crc8_table[crc ^ b[5]];
            crc = crc8_table[crc ^ b[6]];
            crc = crc8_table[crc ^ b[7]];
            break;
        default: /* Fallback for any future sizes; generic loop. */
            crc = comm_crc8(b, n);
            break;
    }
    ((uint8_t*)msg)[n] = crc;
    return (uint8_t)(n + 1u);
}

/* Precomputed CRC-8 (poly 0x07, init 0xFF) for every single-byte command so
 * the runtime CRC loop is skipped for the bus's busiest read-pollers.  If a
 * new id-only command is added, regenerate these with:
 *   python3 -c "d=0xID; c=d^0xFF
 *     for _ in range(8): c = ((c<<1)^0x07)&0xFF if c&0x80 else (c<<1)&0xFF
 *     print(hex(c))"
 * or by looking up comm_crc8(&id, 1) at runtime once. */
#define CRC8_RESET              0xDE  /* crc8(0x0F) */
#define CRC8_BUTTON_STATE_READ  0x73  /* crc8(0x83) */
#define CRC8_RELAY_STATE_READ   0x61  /* crc8(0x85) */
#define CRC8_CHANNEL_STATE_READ 0x6F  /* crc8(0x87) */
#define CRC8_BATTERY_READ       0x42  /* crc8(0x88) */
#define CRC8_LEVELS_READ        0x45  /* crc8(0x89) */
#define CRC8_LEVEL_MODE_READ    0x4C  /* crc8(0x8A) */
#define CRC8_SENSORS_READ       0x4B  /* crc8(0x8B) */

/* Fast-path for single-byte (id-only) commands: skips the CRC bit-loop and
 * writes the precomputed value directly.  Layout on the wire is [id][crc]. */
static uint8_t comm_finalize_precomputed(CommMessage* msg, uint8_t crc) {
    msg->raw[0] = crc;
    return 2;
}

/* Payload size (without id, without CRC) for each command id, or 0xFF for
 * an unknown id.  Keeps the size table in one place so the CRC check can
 * validate both length and crc generically. */
static uint8_t expected_body_len(uint8_t id) {
    switch (id) {
        case COMM_BUTTON_EFFECT:        return (uint8_t)sizeof(CommButtonEffect);
        case COMM_BUTTON_CHANGED:       return (uint8_t)sizeof(CommButtonChanged);
        case COMM_BUTTON_TRIGGER:       return (uint8_t)sizeof(CommButtonTrigger);
        case COMM_RELAY_STATE:          return (uint8_t)sizeof(CommRelayState);
        case COMM_CHANNEL_CHANGED:      return (uint8_t)sizeof(CommChannelChanged);
        case COMM_LEVEL_MODE:           return (uint8_t)sizeof(CommLevelMode);
        case COMM_CONFIG:               return (uint8_t)sizeof(CommConfig);
        case COMM_RESET:                return 0;
        case COMM_BUTTON_STATE_READ:    return 0;
        case COMM_BUTTON_TRIGGER_READ:  return 1; /* button_id byte */
        case COMM_RELAY_STATE_READ:     return 0;
        case COMM_CHANNEL_STATE_READ:   return 0;
        case COMM_BATTERY_READ:         return 0;
        case COMM_LEVELS_READ:          return 0;
        case COMM_LEVEL_MODE_READ:      return 0;
        case COMM_SENSORS_READ:         return 0;
        case COMM_CONFIG_READ:          return 1; /* address byte */
        case COMM_TEST_ECHO:            return (uint8_t)sizeof(CommTestEcho);
        case COMM_TEST_ECHO_RESPONSE:   return (uint8_t)sizeof(CommTestEcho);
        case COMM_TEST_READ:            return 1; /* value byte (write phase) */
        default:                        return 0xFF;
    }
}

uint8_t comm_can_parse(uint8_t* data, uint8_t len) {
    /* Shape: [id] [body...] [crc]. Minimum is 2 bytes (id + crc). */
    if (len < 2) {
        return 0;
    }
    uint8_t body = expected_body_len(data[0]);
    if (body == 0xFF) {
        return 0;
    }
    if (len != (uint8_t)(1u + body + 1u)) {
        return 0;
    }
    return (uint8_t)(comm_crc8(data, (uint8_t)(len - 1u)) == data[len - 1u]);
}

/* ============================================================================
 * button_effect (0x01)
 * ============================================================================
 */

uint8_t comm_build_button_effect(CommMessage* msg, CommButtonEffect* effect) {
    msg->id = COMM_BUTTON_EFFECT;
    msg->button_effect = *effect;
    return comm_finalize(msg, (uint8_t)sizeof(CommButtonEffect));
}

void comm_parse_button_effect(uint8_t* data, CommButtonEffect* effect) {
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
    return comm_finalize(msg, (uint8_t)sizeof(CommButtonChanged));
}

void comm_parse_button_changed(uint8_t* data, CommButtonChanged* event) {
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
    return comm_finalize_precomputed(msg, CRC8_BUTTON_STATE_READ);
}

void comm_parse_button_state_response(uint8_t* data, CommButtonState* state) {
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
    return comm_finalize(msg, (uint8_t)sizeof(CommButtonTrigger));
}

uint8_t comm_build_button_trigger_read(CommMessage* msg, uint8_t button_id) {
    msg->id = COMM_BUTTON_TRIGGER_READ;
    msg->button_trigger.button_id = button_id & 0x07;
    return comm_finalize(msg, 1);
}

void comm_parse_button_trigger_write(uint8_t* data, CommButtonTrigger* trigger) {
    trigger->button_id = data[0] & 0x07;
    *(uint8_t*)&trigger->config = data[1];
}

void comm_parse_button_trigger_response(uint8_t* data, CommTriggerConfig* config) {
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
    return comm_finalize(msg, (uint8_t)sizeof(CommRelayState));
}

uint8_t comm_build_relay_state_read(CommMessage* msg) {
    msg->id = COMM_RELAY_STATE_READ;
    return comm_finalize_precomputed(msg, CRC8_RELAY_STATE_READ);
}

void comm_parse_relay_state_write(uint8_t* data, CommRelayState* state) {
    state->relays = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

void comm_parse_relay_state_response(uint8_t* data, CommRelayState* state) {
    state->relays = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * channel_changed (0x06)
 * ============================================================================
 */

uint8_t comm_build_channel_changed(CommMessage* msg, uint16_t prev_channels, uint16_t current_channels,
                                   uint8_t prev_sensors, uint8_t current_sensors) {
    msg->id = COMM_CHANNEL_CHANGED;
    msg->channel_changed.device_address = comm_address();
    msg->channel_changed.prev_channels = prev_channels;
    msg->channel_changed.current_channels = current_channels;
    msg->channel_changed.prev_sensors = prev_sensors;
    msg->channel_changed.current_sensors = current_sensors;
    return comm_finalize(msg, (uint8_t)sizeof(CommChannelChanged));
}

void comm_parse_channel_changed(uint8_t* data, CommChannelChanged* event) {
    event->device_address = data[0];
    event->prev_channels = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
    event->current_channels = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    event->prev_sensors = data[5];
    event->current_sensors = data[6];
}

/* ============================================================================
 * channel_state_read (0x87)
 * ============================================================================
 */

uint8_t comm_build_channel_state_read(CommMessage* msg) {
    msg->id = COMM_CHANNEL_STATE_READ;
    return comm_finalize_precomputed(msg, CRC8_CHANNEL_STATE_READ);
}

void comm_parse_channel_state_response(uint8_t* data, CommChannelState* state) {
    state->channels = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}


/* ============================================================================
 * battery_read (0x88)
 * ============================================================================
 */

uint8_t comm_build_battery_read(CommMessage* msg) {
    msg->id = COMM_BATTERY_READ;
    return comm_finalize_precomputed(msg, CRC8_BATTERY_READ);
}

void comm_parse_battery_response(uint8_t* data, CommBattery* battery) {
    battery->voltage = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/* ============================================================================
 * levels_read (0x89)
 * ============================================================================
 */

uint8_t comm_build_levels_read(CommMessage* msg) {
    msg->id = COMM_LEVELS_READ;
    return comm_finalize_precomputed(msg, CRC8_LEVELS_READ);
}

void comm_parse_levels_response(uint8_t* data, CommLevels* levels) {
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
    return comm_finalize(msg, (uint8_t)sizeof(CommLevelMode));
}

uint8_t comm_build_level_mode_read(CommMessage* msg) {
    msg->id = COMM_LEVEL_MODE_READ;
    return comm_finalize_precomputed(msg, CRC8_LEVEL_MODE_READ);
}

void comm_parse_level_mode_write(uint8_t* data, CommLevelMode* mode) {
    *(uint8_t*)mode = data[0] & 0x0F;
}

void comm_parse_level_mode_response(uint8_t* data, CommLevelMode* mode) {
    *(uint8_t*)mode = data[0] & 0x0F;
}

/* ============================================================================
 * sensors_read (0x8B)
 * ============================================================================
 */

uint8_t comm_build_sensors_read(CommMessage* msg) {
    msg->id = COMM_SENSORS_READ;
    return comm_finalize_precomputed(msg, CRC8_SENSORS_READ);
}

void comm_parse_sensors_response(uint8_t* data, CommSensors* sensors) {
    sensors->sensors = data[0] & 0x07;
}

/* ============================================================================
 * reset (0x0F) / config (0x0E / 0x8E)
 * ============================================================================
 */

uint8_t comm_build_reset(CommMessage* msg) {
    msg->id = COMM_RESET;
    return comm_finalize_precomputed(msg, CRC8_RESET);
}

uint8_t comm_build_config(CommMessage* msg, uint8_t address, uint8_t value) {
    msg->id = COMM_CONFIG;
    msg->config.address = address;
    msg->config.value = value;
    return comm_finalize(msg, (uint8_t)sizeof(CommConfig));
}

uint8_t comm_build_config_read(CommMessage* msg, uint8_t address) {
    msg->id = COMM_CONFIG_READ;
    msg->config.address = address;
    return comm_finalize(msg, 1);
}

void comm_parse_config_write(uint8_t* data, CommConfig* config) {
    config->address = data[0];
    config->value = data[1];
}

void comm_parse_config_read_request(uint8_t* data, uint8_t* address) {
    *address = data[0];
}

void comm_parse_config_response(uint8_t* data, uint8_t* value) {
    *value = data[0];
}

/* ============================================================================
 * test_echo (0x0C) / test_echo_response (0x0D) / test_read (0x8C) — diagnostics
 * ============================================================================
 */

uint8_t comm_build_test_echo(CommMessage* msg, uint8_t address, uint8_t value) {
    msg->id = COMM_TEST_ECHO;
    msg->test_echo.address = address;
    msg->test_echo.value = value;
    return comm_finalize(msg, (uint8_t)sizeof(CommTestEcho));
}

uint8_t comm_build_test_echo_response(CommMessage* msg, uint8_t address, uint8_t value) {
    msg->id = COMM_TEST_ECHO_RESPONSE;
    msg->test_echo.address = address;
    msg->test_echo.value = value;
    return comm_finalize(msg, (uint8_t)sizeof(CommTestEcho));
}

void comm_parse_test_echo(uint8_t* data, CommTestEcho* echo) {
    echo->address = data[0];
    echo->value = data[1];
}

uint8_t comm_build_test_read(CommMessage* msg, uint8_t value) {
    msg->id = COMM_TEST_READ;
    /* raw[] is the payload union, so raw[0] is the first byte *after* the id —
     * the same slot msg->config.address and msg->button_trigger.button_id
     * occupy.  Writing raw[1] put the value one byte too far and
     * comm_finalize then overwrote it with the CRC. */
    msg->raw[0] = value; /* single write-phase value byte */
    return comm_finalize(msg, 1);
}

void comm_parse_test_read_request(uint8_t* data, uint8_t* value) {
    *value = data[0];
}

void comm_parse_test_read_response(uint8_t* data, uint8_t* value) {
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

int8_t comm_button_effect_get(CommButtonEffect* effect, uint8_t output_index, CommButtonOutputEffect* value) {
    if (effect == 0 || output_index > 7 || value == 0) {
        return -1;
    }
    const uint8_t* bytes = (const uint8_t*)effect;
    uint8_t byte_index = (7 - output_index) / 2;
    value->raw = (output_index & 1) ? (bytes[byte_index] >> 4) : (bytes[byte_index] & 0x0F);
    return 0;
}
