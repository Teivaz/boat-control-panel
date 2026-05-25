/*
 * libcomm_interface.c — Protocol interface implementation.
 *
 * See libcomm_interface.h for the design overview and callback contracts.
 *
 * Both writes and reads use static trampolines as their I2cCompletion.
 * Each trampoline reconstructs the typed arguments from the queued TX
 * bytes (and, for reads, parses the response bytes) and dispatches to
 * the adopter's comm_on_*_completion / comm_on_*_read_response callback.
 */

#include "libcomm_interface.h"

#include <stdint.h>

/* Validate a read-response frame: the last byte must be the CRC-8 over the
 * preceding `expected` payload bytes.  Returns 1 if the frame is the right
 * length and the CRC matches. */
static uint8_t response_crc_ok(uint8_t* rx, uint8_t rx_len, uint8_t expected_payload) {
    if (rx_len != (uint8_t)(expected_payload + 1u)) {
        return 0;
    }
    return (uint8_t)(comm_crc8(rx, expected_payload) == rx[expected_payload]);
}

/* ── Write trampolines ─────────────────────────────────────────────────
 *
 * tx[0] is the command id, tx[1..tx_len-2] is the payload, tx[tx_len-1]
 * is the CRC.  Each trampoline parses the payload back into typed args
 * and dispatches to the adopter's comm_on_*_completion callback.
 * ──────────────────────────────────────────────────────────────────────── */

static void on_reset_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx, uint8_t rx_len) {
    (void)tx;
    (void)tx_len;
    (void)rx;
    (void)rx_len;
    comm_on_reset_completion(result, addr);
}

static void on_config_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx, uint8_t rx_len) {
    (void)rx;
    (void)rx_len;
    if (tx_len < 3) {
        comm_on_config_completion(result, addr, 0, 0);
        return;
    }
    CommConfig config;
    comm_parse_config_write(tx + 1, &config);
    comm_on_config_completion(result, addr, config.address, config.value);
}

static void on_button_effect_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                  uint8_t rx_len) {
    (void)rx;
    (void)rx_len;
    if (tx_len < 5) {
        comm_on_button_effect_completion(result, addr, 0);
        return;
    }
    CommButtonEffect effect;
    comm_parse_button_effect(tx + 1, &effect);
    comm_on_button_effect_completion(result, addr, &effect);
}

static void on_button_changed_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                   uint8_t rx_len) {
    (void)addr;
    (void)rx;
    (void)rx_len;
    if (tx_len < 3) {
        comm_on_button_changed_completion(result, 0, 0, COMM_BUTTON_MODE_UNKNOWN);
        return;
    }
    CommButtonChanged event;
    comm_parse_button_changed(tx + 1, &event);
    comm_on_button_changed_completion(result, event.button_id, event.pressed, (CommButtonMode)event.mode);
}

static void on_button_trigger_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                   uint8_t rx_len) {
    (void)rx;
    (void)rx_len;
    if (tx_len < 3) {
        CommTriggerConfig empty = {0};
        comm_on_button_trigger_completion(result, addr, 0, empty);
        return;
    }
    CommButtonTrigger trigger;
    comm_parse_button_trigger_write(tx + 1, &trigger);
    comm_on_button_trigger_completion(result, addr, trigger.button_id, trigger.config);
}

static void on_relay_state_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                uint8_t rx_len) {
    (void)addr;
    (void)rx;
    (void)rx_len;
    if (tx_len < 3) {
        comm_on_relay_state_completion(result, 0);
        return;
    }
    CommRelayState state;
    comm_parse_relay_state_write(tx + 1, &state);
    comm_on_relay_state_completion(result, state.relays);
}

static void on_channel_changed_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                    uint8_t rx_len) {
    (void)addr;
    (void)rx;
    (void)rx_len;
    if (tx_len < 8) {
        comm_on_channel_changed_completion(result, 0, 0, 0, 0);
        return;
    }
    CommChannelChanged event;
    comm_parse_channel_changed(tx + 1, &event);
    comm_on_channel_changed_completion(result, event.prev_channels, event.current_channels, event.prev_sensors,
                                       event.current_sensors);
}

static void on_level_mode_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                               uint8_t rx_len) {
    (void)addr;
    (void)rx;
    (void)rx_len;
    if (tx_len < 2) {
        comm_on_level_mode_completion(result, COMM_METER_MODE_CALIBRATION, COMM_METER_MODE_CALIBRATION);
        return;
    }
    CommLevelMode mode;
    comm_parse_level_mode_write(tx + 1, &mode);
    comm_on_level_mode_completion(result, (CommMeterMode)mode.mode_0, (CommMeterMode)mode.mode_1);
}

/* ── Read trampolines ──────────────────────────────────────────────────
 *
 * Each is an I2cCompletion fired from i2c_poll() (main-loop context).
 * It validates the response frame and forwards to the adopter's
 * comm_on_*_read_response callback.  On any failure (I2cResult, length,
 * CRC) the adopter receives a NULL parsed-struct pointer.
 * ──────────────────────────────────────────────────────────────────────── */

static void on_button_state_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                      uint8_t rx_len) {
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommButtonState))) {
        comm_on_button_state_read_response(addr, 0);
        return;
    }
    CommButtonState state;
    comm_parse_button_state_response(rx, &state);
    comm_on_button_state_read_response(addr, &state);
}

static void on_button_trigger_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                        uint8_t rx_len) {
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommTriggerConfig))) {
        comm_on_button_trigger_read_response(addr, 0);
        return;
    }
    CommTriggerConfig config;
    comm_parse_button_trigger_response(rx, &config);
    comm_on_button_trigger_read_response(addr, &config);
}

static void on_relay_state_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                     uint8_t rx_len) {
    (void)addr;
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommRelayState))) {
        comm_on_relay_state_read_response(0);
        return;
    }
    CommRelayState state;
    comm_parse_relay_state_response(rx, &state);
    comm_on_relay_state_read_response(&state);
}

static void on_channel_state_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                       uint8_t rx_len) {
    (void)addr;
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommChannelState))) {
        comm_on_channel_state_read_response(0);
        return;
    }
    CommChannelState state;
    comm_parse_channel_state_response(rx, &state);
    comm_on_channel_state_read_response(&state);
}

static void on_battery_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                 uint8_t rx_len) {
    (void)addr;
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommBattery))) {
        comm_on_battery_read_response(0);
        return;
    }
    CommBattery battery;
    comm_parse_battery_response(rx, &battery);
    comm_on_battery_read_response(&battery);
}

static void on_levels_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                uint8_t rx_len) {
    (void)addr;
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommLevels))) {
        comm_on_levels_read_response(0);
        return;
    }
    CommLevels levels;
    comm_parse_levels_response(rx, &levels);
    comm_on_levels_read_response(&levels);
}

static void on_level_mode_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                    uint8_t rx_len) {
    (void)addr;
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommLevelMode))) {
        comm_on_level_mode_read_response(0);
        return;
    }
    CommLevelMode mode;
    comm_parse_level_mode_response(rx, &mode);
    comm_on_level_mode_read_response(&mode);
}

static void on_sensors_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                 uint8_t rx_len) {
    (void)addr;
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK || !response_crc_ok(rx, rx_len, (uint8_t)sizeof(CommSensors))) {
        comm_on_sensors_read_response(0);
        return;
    }
    CommSensors sensors;
    comm_parse_sensors_response(rx, &sensors);
    comm_on_sensors_read_response(&sensors);
}

static void on_config_read_done(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                                uint8_t rx_len) {
    (void)tx;
    (void)tx_len;
    if (result != I2C_RESULT_OK) {
        comm_on_config_read_response(result, addr, 0);
        return;
    }
    if (!response_crc_ok(rx, rx_len, 1 /* value byte */)) {
        comm_on_config_read_response(I2C_RESULT_BAD_CRC, addr, 0);
        return;
    }
    uint8_t value;
    comm_parse_config_response(rx, &value);
    comm_on_config_read_response(I2C_RESULT_OK, addr, &value);
}

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

    uint8_t* payload = data + 1;
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

static void cold_rx_dispatch(I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* data, uint8_t len) {
    (void)result;
    (void)addr;
    (void)tx;
    (void)tx_len;
    if (!comm_can_parse(data, len)) {
        return;
    }

    uint8_t id = data[0];
    uint8_t* payload = data + 1;
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

I2cResult comm_send_reset(uint8_t addr) {
    CommMessage msg;
    uint8_t len = comm_build_reset(&msg);
    return i2c_submit(addr, (uint8_t*)&msg, len, 0, on_reset_done);
}

I2cResult comm_send_config(uint8_t addr, uint8_t config_addr, uint8_t value) {
    CommMessage msg;
    uint8_t len = comm_build_config(&msg, config_addr, value);
    return i2c_submit(addr, (uint8_t*)&msg, len, 0, on_config_done);
}

I2cResult comm_send_button_effect(uint8_t addr, CommButtonEffect* effect) {
    CommMessage msg;
    uint8_t len = comm_build_button_effect(&msg, effect);
    return i2c_submit(addr, (uint8_t*)&msg, len, 0, on_button_effect_done);
}

I2cResult comm_send_button_changed(uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
    CommMessage msg;
    uint8_t len = comm_build_button_changed(&msg, button_id, pressed, mode);
    return i2c_submit(COMM_ADDRESS_MAIN, (uint8_t*)&msg, len, 0, on_button_changed_done);
}

I2cResult comm_send_button_trigger(uint8_t addr, uint8_t button_id, CommTriggerConfig config) {
    CommMessage msg;
    uint8_t len = comm_build_button_trigger(&msg, button_id, config);
    return i2c_submit(addr, (uint8_t*)&msg, len, 0, on_button_trigger_done);
}

I2cResult comm_send_relay_state(uint16_t relays) {
    CommMessage msg;
    uint8_t len = comm_build_relay_state(&msg, relays);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, len, 0, on_relay_state_done);
}

I2cResult comm_send_channel_changed(uint16_t prev_channels, uint16_t current_channels, uint8_t prev_sensors,
                                    uint8_t current_sensors) {
    CommMessage msg;
    uint8_t len = comm_build_channel_changed(&msg, prev_channels, current_channels, prev_sensors, current_sensors);
    return i2c_submit(COMM_ADDRESS_MAIN, (uint8_t*)&msg, len, 0, on_channel_changed_done);
}

I2cResult comm_send_level_mode(CommMeterMode mode_0, CommMeterMode mode_1) {
    CommMessage msg;
    uint8_t len = comm_build_level_mode(&msg, mode_0, mode_1);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, len, 0, on_level_mode_done);
}

/* ── Outbound read commands ────────────────────────────────────────────
 *
 * Each builds the write-phase message (command id + optional params)
 * and submits a write-then-read I2C transaction.  The driver owns the
 * RX buffer; the trampoline forwards the parsed response to the
 * adopter's comm_on_*_read_response callback.
 * ──────────────────────────────────────────────────────────────────────── */

I2cResult comm_send_button_state_read(uint8_t addr) {
    CommMessage msg;
    uint8_t tx_len = comm_build_button_state_read(&msg);
    /* rx_len = payload(1) + CRC(1) */
    return i2c_submit(addr, (uint8_t*)&msg, tx_len, 2, on_button_state_read_done);
}

I2cResult comm_send_button_trigger_read(uint8_t addr, uint8_t button_id) {
    CommMessage msg;
    uint8_t tx_len = comm_build_button_trigger_read(&msg, button_id);
    /* rx_len = payload(1) + CRC(1) */
    return i2c_submit(addr, (uint8_t*)&msg, tx_len, 2, on_button_trigger_read_done);
}

I2cResult comm_send_relay_state_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_relay_state_read(&msg);
    /* rx_len = payload(2) + CRC(1) */
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, tx_len, 3, on_relay_state_read_done);
}

I2cResult comm_send_channel_state_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_channel_state_read(&msg);
    /* rx_len = payload(2) + CRC(1) */
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, tx_len, 3, on_channel_state_read_done);
}

I2cResult comm_send_battery_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_battery_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, tx_len, 3, on_battery_read_done);
}

I2cResult comm_send_levels_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_levels_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, tx_len, 3, on_levels_read_done);
}

I2cResult comm_send_level_mode_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_level_mode_read(&msg);
    /* rx_len = payload(1) + CRC(1) */
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, tx_len, 2, on_level_mode_read_done);
}

I2cResult comm_send_sensors_read(void) {
    CommMessage msg;
    uint8_t tx_len = comm_build_sensors_read(&msg);
    return i2c_submit(COMM_ADDRESS_SWITCHING, (uint8_t*)&msg, tx_len, 2, on_sensors_read_done);
}

I2cResult comm_send_config_read(uint8_t addr, uint8_t config_addr) {
    CommMessage msg;
    uint8_t tx_len = comm_build_config_read(&msg, config_addr);
    return i2c_submit(addr, (uint8_t*)&msg, tx_len, 2, on_config_read_done);
}

/* ── Response staging ──────────────────────────────────────────────────
 *
 * Boards' read-request handlers use this to stage their reply: the helper
 * copies `len` bytes of payload into a local frame buffer, appends CRC-8,
 * and hands the framed buffer + len+1 to the driver.  Keeps wire framing
 * out of per-board callbacks. */

I2cResult comm_respond(uint8_t* data, uint8_t len) {
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
