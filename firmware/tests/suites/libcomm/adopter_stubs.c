#include "adopter_stubs.h"

#include <string.h>

CallbackRecord adopter_log[ADOPTER_LOG_MAX];
uint8_t adopter_log_count;
uint8_t adopter_response[8];
uint8_t adopter_response_len;

void adopter_reset(void) {
    memset(adopter_log, 0, sizeof(adopter_log));
    adopter_log_count = 0;
    memset(adopter_response, 0, sizeof(adopter_response));
    adopter_response_len = 0;
}

static CallbackRecord* rec(CallbackKind kind) {
    static CallbackRecord discard;
    if (adopter_log_count >= ADOPTER_LOG_MAX) {
        memset(&discard, 0, sizeof(discard));
        return &discard;
    }
    CallbackRecord* r = &adopter_log[adopter_log_count++];
    memset(r, 0, sizeof(*r));
    r->kind = kind;
    return r;
}

const CallbackRecord* adopter_find(CallbackKind k) {
    for (uint8_t i = 0; i < adopter_log_count; i++) {
        if (adopter_log[i].kind == k) {
            return &adopter_log[i];
        }
    }
    return 0;
}

uint8_t adopter_count_of(CallbackKind k) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < adopter_log_count; i++) {
        if (adopter_log[i].kind == k) {
            n++;
        }
    }
    return n;
}

/* comm_address() is declared in libcomm.h but implemented by each board (the
 * button boards derive it from a strap pin), so the library on its own does
 * not link.  This binary stands in for the main board. */
uint8_t comm_address(void) {
    return COMM_ADDRESS_MAIN;
}

/* ── Incoming writes ──────────────────────────────────────────────────── */

void comm_on_reset(void) {
    rec(CB_RESET);
}

void comm_on_config_received(CommConfig* config) {
    CallbackRecord* r = rec(CB_CONFIG_RECEIVED);
    r->had_payload = config != 0;
    if (config) {
        r->u8[0] = config->address;
        r->u8[1] = config->value;
    }
}

void comm_on_button_effect_received(CommButtonEffect* effect) {
    CallbackRecord* r = rec(CB_BUTTON_EFFECT_RECEIVED);
    r->had_payload = effect != 0;
    if (effect) {
        memcpy(r->u8, effect, sizeof(*effect));
    }
}

void comm_on_button_changed_received(CommButtonChanged* event) {
    CallbackRecord* r = rec(CB_BUTTON_CHANGED_RECEIVED);
    r->had_payload = event != 0;
    if (event) {
        r->addr = event->device_address;
        r->u8[0] = event->button_id;
        r->u8[1] = event->pressed;
        r->u8[2] = event->mode;
    }
}

void comm_on_button_trigger_received(CommButtonTrigger* trigger) {
    CallbackRecord* r = rec(CB_BUTTON_TRIGGER_RECEIVED);
    r->had_payload = trigger != 0;
    if (trigger) {
        r->u8[0] = trigger->button_id;
        r->u8[1] = *(uint8_t*)&trigger->config;
    }
}

void comm_on_relay_state_received(CommRelayState* state) {
    CallbackRecord* r = rec(CB_RELAY_STATE_RECEIVED);
    r->had_payload = state != 0;
    if (state) {
        r->u16[0] = state->relays;
    }
}

void comm_on_channel_changed_received(CommChannelChanged* event) {
    CallbackRecord* r = rec(CB_CHANNEL_CHANGED_RECEIVED);
    r->had_payload = event != 0;
    if (event) {
        r->addr = event->device_address;
        r->u16[0] = event->prev_channels;
        r->u16[1] = event->current_channels;
        r->u8[0] = event->prev_sensors;
        r->u8[1] = event->current_sensors;
    }
}

void comm_on_level_mode_received(CommLevelMode* mode) {
    CallbackRecord* r = rec(CB_LEVEL_MODE_RECEIVED);
    r->had_payload = mode != 0;
    if (mode) {
        r->u8[0] = mode->mode_0;
        r->u8[1] = mode->mode_1;
    }
}

/* ── Write completions ────────────────────────────────────────────────── */

void comm_on_reset_completion(I2cResult result, uint8_t addr) {
    CallbackRecord* r = rec(CB_RESET_COMPLETION);
    r->result = result;
    r->addr = addr;
}

void comm_on_config_completion(I2cResult result, uint8_t addr, uint8_t config_addr, uint8_t value) {
    CallbackRecord* r = rec(CB_CONFIG_COMPLETION);
    r->result = result;
    r->addr = addr;
    r->u8[0] = config_addr;
    r->u8[1] = value;
}

void comm_on_button_effect_completion(I2cResult result, uint8_t addr, CommButtonEffect* effect) {
    CallbackRecord* r = rec(CB_BUTTON_EFFECT_COMPLETION);
    r->result = result;
    r->addr = addr;
    r->had_payload = effect != 0;
    if (effect) {
        memcpy(r->u8, effect, sizeof(*effect));
    }
}

void comm_on_button_changed_completion(I2cResult result, uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
    CallbackRecord* r = rec(CB_BUTTON_CHANGED_COMPLETION);
    r->result = result;
    r->u8[0] = button_id;
    r->u8[1] = pressed;
    r->u8[2] = (uint8_t)mode;
}

void comm_on_button_trigger_completion(I2cResult result, uint8_t addr, uint8_t button_id, CommTriggerConfig config) {
    CallbackRecord* r = rec(CB_BUTTON_TRIGGER_COMPLETION);
    r->result = result;
    r->addr = addr;
    r->u8[0] = button_id;
    r->u8[1] = *(uint8_t*)&config;
}

void comm_on_relay_state_completion(I2cResult result, uint16_t relays) {
    CallbackRecord* r = rec(CB_RELAY_STATE_COMPLETION);
    r->result = result;
    r->u16[0] = relays;
}

void comm_on_channel_changed_completion(I2cResult result, uint16_t prev_channels, uint16_t current_channels,
                                        uint8_t prev_sensors, uint8_t current_sensors) {
    CallbackRecord* r = rec(CB_CHANNEL_CHANGED_COMPLETION);
    r->result = result;
    r->u16[0] = prev_channels;
    r->u16[1] = current_channels;
    r->u8[0] = prev_sensors;
    r->u8[1] = current_sensors;
}

void comm_on_level_mode_completion(I2cResult result, CommMeterMode mode_0, CommMeterMode mode_1) {
    CallbackRecord* r = rec(CB_LEVEL_MODE_COMPLETION);
    r->result = result;
    r->u8[0] = (uint8_t)mode_0;
    r->u8[1] = (uint8_t)mode_1;
}

/* ── Read responses ───────────────────────────────────────────────────── */

void comm_on_button_state_read_response(I2cResult result, uint8_t addr, CommButtonState* state) {
    CallbackRecord* r = rec(CB_BUTTON_STATE_RESPONSE);
    r->result = result;
    r->addr = addr;
    r->had_payload = state != 0;
    if (state) {
        r->u8[0] = state->current_state;
    }
}

void comm_on_button_trigger_read_response(I2cResult result, uint8_t addr, CommTriggerConfig* config) {
    CallbackRecord* r = rec(CB_BUTTON_TRIGGER_RESPONSE);
    r->result = result;
    r->addr = addr;
    r->had_payload = config != 0;
    if (config) {
        r->u8[0] = *(uint8_t*)config;
    }
}

void comm_on_relay_state_read_response(I2cResult result, CommRelayState* state) {
    CallbackRecord* r = rec(CB_RELAY_STATE_RESPONSE);
    r->result = result;
    r->had_payload = state != 0;
    if (state) {
        r->u16[0] = state->relays;
    }
}

void comm_on_channel_state_read_response(I2cResult result, CommChannelState* state) {
    CallbackRecord* r = rec(CB_CHANNEL_STATE_RESPONSE);
    r->result = result;
    r->had_payload = state != 0;
    if (state) {
        r->u16[0] = state->channels;
    }
}

void comm_on_battery_read_response(I2cResult result, CommBattery* battery) {
    CallbackRecord* r = rec(CB_BATTERY_RESPONSE);
    r->result = result;
    r->had_payload = battery != 0;
    if (battery) {
        r->u16[0] = battery->voltage;
    }
}

void comm_on_levels_read_response(I2cResult result, CommLevels* levels) {
    CallbackRecord* r = rec(CB_LEVELS_RESPONSE);
    r->result = result;
    r->had_payload = levels != 0;
    if (levels) {
        r->u8[0] = levels->level_0;
        r->u8[1] = levels->level_1;
    }
}

void comm_on_level_mode_read_response(I2cResult result, CommLevelMode* mode) {
    CallbackRecord* r = rec(CB_LEVEL_MODE_RESPONSE);
    r->result = result;
    r->had_payload = mode != 0;
    if (mode) {
        r->u8[0] = mode->mode_0;
        r->u8[1] = mode->mode_1;
    }
}

void comm_on_sensors_read_response(I2cResult result, CommSensors* sensors) {
    CallbackRecord* r = rec(CB_SENSORS_RESPONSE);
    r->result = result;
    r->had_payload = sensors != 0;
    if (sensors) {
        r->u8[0] = sensors->sensors;
    }
}

void comm_on_config_read_response(I2cResult result, uint8_t addr, uint8_t* value) {
    CallbackRecord* r = rec(CB_CONFIG_RESPONSE);
    r->result = result;
    r->addr = addr;
    r->had_payload = value != 0;
    if (value) {
        r->u8[0] = *value;
    }
}

/* ── Read requests (ISR context on the device) ────────────────────────── */

static void respond(void) {
    if (adopter_response_len) {
        comm_respond(adopter_response, adopter_response_len);
    }
}

void comm_on_button_state_read_requested(void) {
    rec(CB_BUTTON_STATE_REQUESTED);
    respond();
}

void comm_on_button_trigger_read_requested(uint8_t button_id) {
    rec(CB_BUTTON_TRIGGER_REQUESTED)->u8[0] = button_id;
    respond();
}

void comm_on_relay_state_read_requested(void) {
    rec(CB_RELAY_STATE_REQUESTED);
    respond();
}

void comm_on_channel_state_read_requested(void) {
    rec(CB_CHANNEL_STATE_REQUESTED);
    respond();
}

void comm_on_battery_read_requested(void) {
    rec(CB_BATTERY_REQUESTED);
    respond();
}

void comm_on_levels_read_requested(void) {
    rec(CB_LEVELS_REQUESTED);
    respond();
}

void comm_on_level_mode_read_requested(void) {
    rec(CB_LEVEL_MODE_REQUESTED);
    respond();
}

void comm_on_sensors_read_requested(void) {
    rec(CB_SENSORS_REQUESTED);
    respond();
}

void comm_on_config_read_requested(uint8_t address) {
    rec(CB_CONFIG_REQUESTED)->u8[0] = address;
    respond();
}
