#include "comm.h"

#include "config.h"
#include "controller.h"
#include "i2c.h"
#include "libcomm.h"
#include "sensors.h"

#include <xc.h>

static void on_rx(const uint8_t* data, uint8_t len);
static uint8_t on_read(const uint8_t* request, uint8_t request_len, uint8_t* response, uint8_t response_max);

void comm_init(void) {
    i2c_set_rx_handler(on_rx);
    i2c_set_read_handler(on_read);
}

uint8_t comm_address(void) {
    return COMM_ADDRESS_SWITCHING;
}

/* ISR context — handlers must stay short. */
static void on_rx(const uint8_t* data, uint8_t len) {
    if (len == 0) {
        return;
    }
    switch (data[0]) {
        case COMM_RELAY_STATE:
            if (len == 1 + sizeof(CommRelayState)) {
                CommRelayState s;
                comm_parse_relay_state_write(&data[1], &s);
                controller_set_relay_target(s.relays);
            }
            break;

        case COMM_RELAY_MASK:
            if (len == 1 + sizeof(CommRelayMask)) {
                CommRelayMask m;
                comm_parse_relay_mask_write(&data[1], &m);
                controller_set_relay_mask(m.mask);
            }
            break;

        case COMM_LEVEL_MODE:
            if (len == 1 + sizeof(CommLevelMode)) {
                controller_set_level_mode(data[1]);
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

static uint8_t on_read(const uint8_t* request, uint8_t request_len, uint8_t* response, uint8_t response_max) {
    if (request_len == 0 || response_max == 0) {
        return 0;
    }

    switch (request[0]) {
        case COMM_RELAY_STATE_READ: {
            uint16_t v = controller_relay_target();
            response[0] = (uint8_t)(v & 0xFF);
            response[1] = (uint8_t)(v >> 8);
            return 2;
        }

        case COMM_RELAY_MASK_READ: {
            uint16_t v = controller_relay_mask();
            response[0] = (uint8_t)(v & 0xFF);
            response[1] = (uint8_t)(v >> 8);
            return 2;
        }

        case COMM_BATTERY_READ: {
            uint16_t v = controller_battery_mv();
            response[0] = (uint8_t)(v & 0xFF);
            response[1] = (uint8_t)(v >> 8);
            return 2;
        }

        case COMM_LEVELS_READ:
            response[0] = controller_level(0);
            response[1] = controller_level(1);
            return 2;

        case COMM_LEVEL_MODE_READ:
            response[0] = controller_level_mode();
            return 1;

        case COMM_SENSORS_READ:
            response[0] = (uint8_t)(sensors_state() & 0x07);
            return 1;

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
