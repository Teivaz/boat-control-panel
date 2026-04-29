/*
 * libcomm_interface.c — Protocol interface implementation.
 *
 * See libcomm_interface.h for the design overview and callback contracts.
 */

#include "libcomm_interface.h"

#include <stdint.h>

/* ── Internal: read completion callbacks ───────────────────────────────
 *
 * Each is an I2cCompletion fired from i2c_poll() (main-loop context).
 * It parses the raw response bytes and forwards to the adopter's
 * comm_on_*_response callback.  rx_len == 0 means the host transaction
 * failed (or no bytes came back); the adopter receives a NULL pointer.
 *
 * For commands whose response handler reports the source address, the
 * address is passed through cb_ctx (uint8_t widened to void*).
 * ──────────────────────────────────────────────────────────────────────── */

static void on_button_state_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    uint8_t addr = (uint8_t)(uintptr_t)ctx;
    if (rx_len == 0) {
        comm_on_button_state_read_response(addr, 0);
        return;
    }
    CommButtonState state;
    comm_parse_button_state_response(rx_buf, &state);
    comm_on_button_state_read_response(addr, &state);
}

static void on_button_trigger_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    uint8_t addr = (uint8_t)(uintptr_t)ctx;
    if (rx_len == 0) {
        comm_on_button_trigger_read_response(addr, 0);
        return;
    }
    CommTriggerConfig config;
    comm_parse_button_trigger_response(rx_buf, &config);
    comm_on_button_trigger_read_response(addr, &config);
}

static void on_relay_state_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)ctx;
    if (rx_len == 0) {
        comm_on_relay_state_read_response(0);
        return;
    }
    CommRelayState state;
    comm_parse_relay_state_response(rx_buf, &state);
    comm_on_relay_state_read_response(&state);
}

static void on_relay_mask_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)ctx;
    if (rx_len == 0) {
        comm_on_relay_mask_read_response(0);
        return;
    }
    CommRelayMask mask;
    comm_parse_relay_mask_response(rx_buf, &mask);
    comm_on_relay_mask_read_response(&mask);
}

static void on_battery_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)ctx;
    if (rx_len == 0) {
        comm_on_battery_read_response(0);
        return;
    }
    CommBattery battery;
    comm_parse_battery_response(rx_buf, &battery);
    comm_on_battery_read_response(&battery);
}

static void on_levels_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)ctx;
    if (rx_len == 0) {
        comm_on_levels_read_response(0);
        return;
    }
    CommLevels levels;
    comm_parse_levels_response(rx_buf, &levels);
    comm_on_levels_read_response(&levels);
}

static void on_level_mode_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)ctx;
    if (rx_len == 0) {
        comm_on_level_mode_read_response(0);
        return;
    }
    CommLevelMode mode;
    comm_parse_level_mode_response(rx_buf, &mode);
    comm_on_level_mode_read_response(&mode);
}

static void on_sensors_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)ctx;
    if (rx_len == 0) {
        comm_on_sensors_read_response(0);
        return;
    }
    CommSensors sensors;
    comm_parse_sensors_response(rx_buf, &sensors);
    comm_on_sensors_read_response(&sensors);
}

static void on_config_read_done(uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    uint8_t addr = (uint8_t)(uintptr_t)ctx;
    if (rx_len == 0) {
        comm_on_config_read_response(addr, 0);
        return;
    }
    uint8_t value;
    comm_parse_config_response(rx_buf, &value);
    comm_on_config_read_response(addr, &value);
}

/* ── Cold-RX dispatcher ────────────────────────────────────────────────
 *
 * Registered with i2c_set_cold_rx_handler().  The driver delivers each
 * complete inbound write here from i2c_poll() (main-loop context).
 * data[0] is the command id, data[1..len-1] is the payload.
 * ──────────────────────────────────────────────────────────────────────── */

static void cold_rx_dispatch(uint8_t* data, uint8_t len, void* ctx) {
    (void)ctx;
    if (!comm_can_parse(data, len)) {
        return;
    }

    uint8_t id = data[0];
    const uint8_t* payload = data + 1;
    uint8_t plen = len - 1;

    switch (id) {
        case COMM_RESET:
            comm_on_reset();
            break;

        case COMM_CONFIG:
            if (plen >= 2) {
                CommConfig config;
                comm_parse_config_write(payload, &config);
                comm_on_config_received(&config);
            }
            break;

        case COMM_BUTTON_EFFECT:
            if (plen >= 4) {
                CommButtonEffect effect;
                comm_parse_button_effect(payload, &effect);
                comm_on_button_effect_received(&effect);
            }
            break;

        case COMM_BUTTON_CHANGED:
            if (plen >= 2) {
                CommButtonChanged event;
                comm_parse_button_changed(payload, &event);
                comm_on_button_changed_received(&event);
            }
            break;

        case COMM_BUTTON_TRIGGER:
            if (plen >= 2) {
                CommButtonTrigger trigger;
                comm_parse_button_trigger_write(payload, &trigger);
                comm_on_button_trigger_received(&trigger);
            }
            break;

        case COMM_RELAY_STATE:
            if (plen >= 2) {
                CommRelayState state;
                comm_parse_relay_state_write(payload, &state);
                comm_on_relay_state_received(&state);
            }
            break;

        case COMM_RELAY_CHANGED:
            if (plen >= 7) {
                CommRelayChanged event;
                comm_parse_relay_changed(payload, &event);
                comm_on_relay_changed_received(&event);
            }
            break;

        case COMM_RELAY_MASK:
            if (plen >= 2) {
                CommRelayMask mask;
                comm_parse_relay_mask_write(payload, &mask);
                comm_on_relay_mask_received(&mask);
            }
            break;

        case COMM_LEVEL_MODE:
            if (plen >= 1) {
                CommLevelMode mode;
                comm_parse_level_mode_write(payload, &mode);
                comm_on_level_mode_received(&mode);
            }
            break;

        default:
            break;
    }
}

/* ── Initialization ────────────────────────────────────────────────────── */

void comm_interface_init(void) {
    i2c_set_cold_rx_handler(cold_rx_dispatch);
}

/* ── Outbound write commands ───────────────────────────────────────────── */

static I2cResult send_write(uint8_t addr, CommMessage* msg, uint8_t len, I2cCompletion cb, void* ctx) {
    return i2c_submit(addr, (const uint8_t*)msg, len, 0, cb, ctx);
}

I2cResult comm_send_reset(uint8_t addr, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_reset(&msg);
    return send_write(addr, &msg, len, cb, ctx);
}

I2cResult comm_send_config(uint8_t addr, uint8_t config_addr, uint8_t value, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_config(&msg, config_addr, value);
    return send_write(addr, &msg, len, cb, ctx);
}

I2cResult comm_send_button_effect(uint8_t addr, const CommButtonEffect* effect, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_button_effect(&msg, effect);
    return send_write(addr, &msg, len, cb, ctx);
}

I2cResult comm_send_button_changed(uint8_t button_id, uint8_t pressed, CommButtonMode mode, I2cCompletion cb,
                                   void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_button_changed(&msg, button_id, pressed, mode);
    return send_write(COMM_ADDRESS_MAIN, &msg, len, cb, ctx);
}

I2cResult comm_send_button_trigger(uint8_t addr, uint8_t button_id, CommTriggerConfig config, I2cCompletion cb,
                                   void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_button_trigger(&msg, button_id, config);
    return send_write(addr, &msg, len, cb, ctx);
}

I2cResult comm_send_relay_state(uint16_t relays, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_relay_state(&msg, relays);
    return send_write(COMM_ADDRESS_SWITCHING, &msg, len, cb, ctx);
}

I2cResult comm_send_relay_changed(uint16_t prev_relays, uint16_t current_relays, uint8_t prev_sensors,
                                  uint8_t current_sensors, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_relay_changed(&msg, prev_relays, current_relays, prev_sensors, current_sensors);
    return send_write(COMM_ADDRESS_MAIN, &msg, len, cb, ctx);
}

I2cResult comm_send_relay_mask(uint16_t mask, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_relay_mask(&msg, mask);
    return send_write(COMM_ADDRESS_SWITCHING, &msg, len, cb, ctx);
}

I2cResult comm_send_level_mode(CommMeterMode mode_0, CommMeterMode mode_1, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_level_mode(&msg, mode_0, mode_1);
    return send_write(COMM_ADDRESS_SWITCHING, &msg, len, cb, ctx);
}

/* ── Outbound read commands ────────────────────────────────────────────
 *
 * Each builds the write-phase message (command id + optional params)
 * and submits a write-then-read I2C transaction.  The driver owns the
 * RX buffer; the internal completion forwards the parsed response.
 * ──────────────────────────────────────────────────────────────────────── */

I2cResult comm_send_button_state_read(uint8_t addr) {
    CommMessage msg;
    uint8_t tx_len = comm_build_button_state_read(&msg);
    return i2c_submit(addr, (const uint8_t*)&msg, tx_len, 1, on_button_state_read_done, (void*)(uintptr_t)addr);
}

I2cResult comm_send_button_trigger_read(uint8_t addr, uint8_t button_id) {
    CommMessage msg;
    uint8_t tx_len = comm_build_button_trigger_read(&msg, button_id);
    return i2c_submit(addr, (const uint8_t*)&msg, tx_len, 1, on_button_trigger_read_done, (void*)(uintptr_t)addr);
}

I2cResult comm_send_relay_state_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_relay_state_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 2, on_relay_state_read_done, 0);
}

I2cResult comm_send_relay_mask_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_relay_mask_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 2, on_relay_mask_read_done, 0);
}

I2cResult comm_send_battery_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_battery_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 2, on_battery_read_done, 0);
}

I2cResult comm_send_levels_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_levels_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 2, on_levels_read_done, 0);
}

I2cResult comm_send_level_mode_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_level_mode_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 1, on_level_mode_read_done, 0);
}

I2cResult comm_send_sensors_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_sensors_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 1, on_sensors_read_done, 0);
}

I2cResult comm_send_config_read(uint8_t addr, uint8_t config_addr) {
    CommMessage msg;
    uint8_t tx_len = comm_build_config_read(&msg, config_addr);
    return i2c_submit(addr, (const uint8_t*)&msg, tx_len, 1, on_config_read_done, (void*)(uintptr_t)addr);
}
