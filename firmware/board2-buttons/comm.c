#include "comm.h"
#include "i2c.h"
#include "libcomm.h"
#include "button.h"
#include "led_effect.h"
#include "config.h"
#include "input.h"

#include <xc.h>

static void    on_rx  (const uint8_t *data, uint8_t len);
static uint8_t on_read(const uint8_t *request, uint8_t request_len,
                       uint8_t *response, uint8_t response_max);
static void    apply_button_effect(const CommButtonEffect *eff);

void comm_init(void) {
    i2c_set_rx_handler(on_rx);
    i2c_set_read_handler(on_read);
}

void comm_send_button_changed(uint8_t prev_state, uint8_t current_state) {
    CommMessage msg;
    uint8_t     len = comm_build_button_changed(&msg, prev_state, current_state);
    (void)i2c_transmit(COMM_ADDRESS_MAIN, (const uint8_t *)&msg, len);
}

/* ============================================================================
 * Inbound dispatch (ISR context)
 *
 * Handlers must stay short — any work beyond a few microseconds stretches
 * the I2C clock. EEPROM-backed writes are already gated by a no-op check
 * in config.c; an actual cell program (~4 ms) only happens when the byte
 * changes, which is infrequent for `config` writes.
 * ============================================================================ */

static void on_rx(const uint8_t *data, uint8_t len) {
    if (len == 0) return;
    switch (data[0]) {
        case COMM_BUTTON_EFFECT:
            if (len == 1 + sizeof(CommButtonEffect)) {
                CommButtonEffect eff;
                comm_parse_button_effect(&data[1], &eff);
                apply_button_effect(&eff);
            }
            break;

        case COMM_BUTTON_TRIGGER:
            if (len == 1 + sizeof(CommButtonTrigger)) {
                CommButtonTrigger trg;
                comm_parse_button_trigger_write(&data[1], &trg);
                button_set_trigger(trg.button_id, trg.config);
            }
            break;

        case COMM_CONFIG:
            if (len == 1 + sizeof(CommConfig)) {
                CommConfig cfg;
                comm_parse_config_write(&data[1], &cfg);
                config_write_byte(cfg.address, cfg.value);
            }
            break;

        case COMM_RESET:
            RESET();                    /* software reset; does not return */
            break;

        default:
            break;
    }
}

static uint8_t on_read(const uint8_t *request, uint8_t request_len,
                       uint8_t *response, uint8_t response_max) {
    if (request_len == 0 || response_max == 0) return 0;

    switch (request[0]) {
        case COMM_BUTTON_STATE_READ:
            response[0] = input_state_current().integer;
            return 1;

        case COMM_BUTTON_TRIGGER_READ:
            if (request_len >= 2) {
                CommTriggerConfig cfg = button_get_trigger(request[1] & 0x07);
                response[0] = *(uint8_t *)&cfg;
                return 1;
            }
            break;

        case COMM_CONFIG_READ:
            if (request_len >= 2) {
                response[0] = config_read_byte(request[1]);
                return 1;
            }
            break;

        default:
            break;
    }
    return 0;
}

static void apply_button_effect(const CommButtonEffect *eff) {
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        CommButtonOutputEffect out;
        if (comm_button_effect_get(eff, i, &out) == 0) {
            led_effect_set(i, out);
        }
    }
}
