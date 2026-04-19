#include "comm.h"

#include "config.h"
#include "controller.h"
#include "i2c.h"
#include "libcomm.h"

#include <xc.h>

static void on_rx(const uint8_t *data, uint8_t len);
static uint8_t on_read(const uint8_t *request, uint8_t request_len,
                       uint8_t *response, uint8_t response_max);

void comm_init(void) {
    i2c_set_rx_handler(on_rx);
    i2c_set_read_handler(on_read);
}

/* ISR context — handlers must stay short. */
static void on_rx(const uint8_t *data, uint8_t len) {
    if (len == 0)
        return;
    switch (data[0]) {
        case COMM_BUTTON_CHANGED:
            if (len == 1 + sizeof(CommButtonChanged)) {
                CommButtonChanged ev;
                comm_parse_button_changed(&data[1], &ev);
                controller_on_button_changed(ev.device_address, ev.prev_state,
                                             ev.current_state);
            }
            break;

        case COMM_RELAY_CHANGED:
            if (len == 1 + sizeof(CommRelayChanged)) {
                CommRelayChanged ev;
                comm_parse_relay_changed(&data[1], &ev);
                controller_on_relay_changed(ev.device_address, ev.prev_relays,
                                            ev.current_relays, ev.prev_sensors,
                                            ev.current_sensors);
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
            RESET();
            break;

        default:
            break;
    }
}

static uint8_t on_read(const uint8_t *request, uint8_t request_len,
                       uint8_t *response, uint8_t response_max) {
    if (request_len == 0 || response_max == 0)
        return 0;

    switch (request[0]) {
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
