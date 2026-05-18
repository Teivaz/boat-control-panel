/*
 * libcomm_interface.c — Protocol interface implementation.
 *
 * See libcomm_interface.h for the design overview and callback contracts.
 */

#include "libcomm_interface.h"

#include <stdint.h>

/* Validate a read-response frame: the last byte must be the CRC-8 over the
 * preceding `expected` payload bytes.  Returns 1 if the frame is the right
 * length and the CRC matches. */
static uint8_t response_crc_ok(const uint8_t* rx, uint8_t rx_len, uint8_t expected_payload) {
    if (rx_len != (uint8_t)(expected_payload + 1u)) {
        return 0;
    }
    return (uint8_t)(comm_crc8(rx, expected_payload) == rx[expected_payload]);
}

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

static void on_button_state_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    uint8_t addr = (uint8_t)(uintptr_t)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommButtonState))) {
        comm_on_button_state_read_response(addr, 0);
        return;
    }
    CommButtonState state;
    comm_parse_button_state_response(rx_buf, &state);
    comm_on_button_state_read_response(addr, &state);
}

static void on_button_trigger_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    uint8_t addr = (uint8_t)(uintptr_t)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommTriggerConfig))) {
        comm_on_button_trigger_read_response(addr, 0);
        return;
    }
    CommTriggerConfig config;
    comm_parse_button_trigger_response(rx_buf, &config);
    comm_on_button_trigger_read_response(addr, &config);
}

static void on_relay_state_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommRelayState))) {
        comm_on_relay_state_read_response(0);
        return;
    }
    CommRelayState state;
    comm_parse_relay_state_response(rx_buf, &state);
    comm_on_relay_state_read_response(&state);
}

static void on_channel_state_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommChannelState))) {
        comm_on_channel_state_read_response(0);
        return;
    }
    CommChannelState state;
    comm_parse_channel_state_response(rx_buf, &state);
    comm_on_channel_state_read_response(&state);
}

static void on_battery_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommBattery))) {
        comm_on_battery_read_response(0);
        return;
    }
    CommBattery battery;
    comm_parse_battery_response(rx_buf, &battery);
    comm_on_battery_read_response(&battery);
}

static void on_levels_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommLevels))) {
        comm_on_levels_read_response(0);
        return;
    }
    CommLevels levels;
    comm_parse_levels_response(rx_buf, &levels);
    comm_on_levels_read_response(&levels);
}

static void on_level_mode_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommLevelMode))) {
        comm_on_level_mode_read_response(0);
        return;
    }
    CommLevelMode mode;
    comm_parse_level_mode_response(rx_buf, &mode);
    comm_on_level_mode_read_response(&mode);
}

static void on_sensors_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    (void)ctx;
    if (!response_crc_ok(rx_buf, rx_len, (uint8_t)sizeof(CommSensors))) {
        comm_on_sensors_read_response(0);
        return;
    }
    CommSensors sensors;
    comm_parse_sensors_response(rx_buf, &sensors);
    comm_on_sensors_read_response(&sensors);
}

static void on_config_read_done(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
    (void)result;
    uint8_t addr = (uint8_t)(uintptr_t)ctx;
    if (!response_crc_ok(rx_buf, rx_len, 1 /* value byte */)) {
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

/* ── Cold-RX dispatchers ───────────────────────────────────────────────
 *
 * Two entry points from the I2C driver:
 *   sync_cold_rx_dispatch — fires in ISR context the moment a client-RX
 *     transaction completes (before the next address byte).  Handles read
 *     commands synchronously so the response is staged via
 *     i2c_set_client_tx() before the read-phase address arrives.  Returns
 *     0 when it has handled the message; the driver then skips the async
 *     queue.  Returns 1 to let the write dispatcher handle it from the
 *     main loop.
 *   cold_rx_dispatch — fires from i2c_poll() in main-loop context for
 *     messages the sync path declined to handle (all non-read commands).
 *     data[0] is the command id, data[1..len-1] is the payload.
 * ──────────────────────────────────────────────────────────────────────── */

static uint8_t sync_cold_rx_dispatch(uint8_t* data, uint8_t len) {
    if (!comm_can_parse(data, len)) {
        return 1;
    }
    uint8_t id = data[0];
    /* Read commands (MSB set) must stage a reply before the master's
     * read-phase address triggers client TX.  Anything else is deferred. */
    if ((id & 0x80) == 0) {
        return 1;
    }

    const uint8_t* payload = data + 1;
    uint8_t plen = len - 1;

    switch (id) {
        case COMM_BUTTON_STATE_READ:
            comm_on_button_state_read_requested();
            break;

        case COMM_BUTTON_TRIGGER_READ:
            if (plen >= 1) {
                comm_on_button_trigger_read_requested(payload[0] & 0x07);
            }
            break;

        case COMM_CONFIG_READ:
            if (plen >= 1) {
                comm_on_config_read_requested(payload[0]);
            }
            break;

        case COMM_RELAY_STATE_READ:
            comm_on_relay_state_read_requested();
            break;

        case COMM_CHANNEL_STATE_READ:
            comm_on_channel_state_read_requested();
            break;

        case COMM_BATTERY_READ:
            comm_on_battery_read_requested();
            break;

        case COMM_LEVELS_READ:
            comm_on_levels_read_requested();
            break;

        case COMM_LEVEL_MODE_READ:
            comm_on_level_mode_read_requested();
            break;

        case COMM_SENSORS_READ:
            comm_on_sensors_read_requested();
            break;

        default:
            return 1;
    }
    return 0;
}

static void cold_rx_dispatch(I2cResult result, uint8_t* data, uint8_t len, void* ctx) {
    (void)result;
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

        case COMM_CHANNEL_CHANGED:
            if (plen >= 7) {
                CommChannelChanged event;
                comm_parse_channel_changed(payload, &event);
                comm_on_channel_changed_received(&event);
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
    i2c_set_sync_cold_rx_handler(sync_cold_rx_dispatch);
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

I2cResult comm_send_channel_changed(uint16_t prev_channels, uint16_t current_channels, uint8_t prev_sensors,
                                    uint8_t current_sensors, I2cCompletion cb, void* ctx) {
    CommMessage msg;
    uint8_t len = comm_build_channel_changed(&msg, prev_channels, current_channels, prev_sensors, current_sensors);
    return send_write(COMM_ADDRESS_MAIN, &msg, len, cb, ctx);
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
    /* rx_len = payload(1) + CRC(1) */
    return i2c_submit(addr, (const uint8_t*)&msg, tx_len, 2, on_button_state_read_done, (void*)(uintptr_t)addr);
}

I2cResult comm_send_button_trigger_read(uint8_t addr, uint8_t button_id) {
    CommMessage msg;
    uint8_t tx_len = comm_build_button_trigger_read(&msg, button_id);
    /* rx_len = payload(1) + CRC(1) */
    return i2c_submit(addr, (const uint8_t*)&msg, tx_len, 2, on_button_trigger_read_done, (void*)(uintptr_t)addr);
}

I2cResult comm_send_relay_state_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_relay_state_read(&msg);
    /* rx_len = payload(2) + CRC(1) */
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 3, on_relay_state_read_done, 0);
}

I2cResult comm_send_channel_state_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_channel_state_read(&msg);
    /* rx_len = payload(2) + CRC(1) */
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 3, on_channel_state_read_done, 0);
}

I2cResult comm_send_battery_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_battery_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 3, on_battery_read_done, 0);
}

I2cResult comm_send_levels_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_levels_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 3, on_levels_read_done, 0);
}

I2cResult comm_send_level_mode_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_level_mode_read(&msg);
    /* rx_len = payload(1) + CRC(1) */
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 2, on_level_mode_read_done, 0);
}

I2cResult comm_send_sensors_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_sensors_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (const uint8_t*)&msg, tx_len, 2, on_sensors_read_done, 0);
}

I2cResult comm_send_config_read(uint8_t addr, uint8_t config_addr) {
    CommMessage msg;
    uint8_t tx_len = comm_build_config_read(&msg, config_addr);
    return i2c_submit(addr, (const uint8_t*)&msg, tx_len, 2, on_config_read_done, (void*)(uintptr_t)addr);
}

/* ── Response staging ──────────────────────────────────────────────────
 *
 * Boards' read-request handlers use this to stage their reply: the helper
 * copies `len` bytes of payload into a local frame buffer, appends CRC-8,
 * and hands the framed buffer + len+1 to the driver.  Keeps wire framing
 * out of per-board callbacks. */

I2cResult comm_respond(const uint8_t* data, uint8_t len) {
    if (data == 0 || (uint16_t)len + 1u > (uint16_t)I2C_TX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    uint8_t buf[I2C_TX_MAX];
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = data[i];
    }
    buf[len] = comm_crc8(data, len);
    return i2c_set_client_tx(buf, (uint8_t)(len + 1u));
}
