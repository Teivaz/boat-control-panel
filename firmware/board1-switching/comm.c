#include "comm.h"

#include "config.h"
#include "controller.h"
#include "i2c.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "sensors.h"

#include <xc.h>

uint8_t comm_address(void) {
    return COMM_ADDRESS_SWITCHING;
}

void comm_init(void) {
}

/* ============================================================================
 * Adopter callbacks: incoming write handlers (main-loop context)
 * ============================================================================
 */

void comm_on_relay_state_received(const CommRelayState* state) {
    controller_set_relay_target(state->relays);
}

void comm_on_relay_mask_received(const CommRelayMask* mask) {
    controller_set_relay_mask(mask->mask);
}

void comm_on_level_mode_received(const CommLevelMode* mode) {
    controller_set_level_mode(*(const uint8_t*)mode & 0x0F);
}

void comm_on_config_received(const CommConfig* config) {
    config_write_byte(config->address, config->value);
}

void comm_on_reset(void) {
    RESET();
}

void comm_on_button_effect_received(const CommButtonEffect* effect) {
    (void)effect;
}
void comm_on_button_changed_received(const CommButtonChanged* event) {
    (void)event;
}
void comm_on_button_trigger_received(const CommButtonTrigger* trigger) {
    (void)trigger;
}
void comm_on_relay_changed_received(const CommRelayChanged* event) {
    (void)event;
}

/* ============================================================================
 * Adopter callbacks: read-request handlers (ISR context)
 * ============================================================================
 */

void comm_on_relay_state_read_requested(void) {
    uint16_t v = controller_relay_target();
    uint8_t buf[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    i2c_set_client_tx(buf, 2);
}

void comm_on_relay_mask_read_requested(void) {
    uint16_t v = controller_relay_mask();
    uint8_t buf[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    i2c_set_client_tx(buf, 2);
}

void comm_on_battery_read_requested(void) {
    uint16_t v = controller_battery_mv();
    uint8_t buf[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    i2c_set_client_tx(buf, 2);
}

void comm_on_levels_read_requested(void) {
    uint8_t buf[2] = {controller_level(0), controller_level(1)};
    i2c_set_client_tx(buf, 2);
}

void comm_on_level_mode_read_requested(void) {
    uint8_t v = controller_level_mode();
    i2c_set_client_tx(&v, 1);
}

void comm_on_sensors_read_requested(void) {
    uint8_t v = (uint8_t)(sensors_state() & 0x07);
    i2c_set_client_tx(&v, 1);
}

void comm_on_config_read_requested(uint8_t address) {
    uint8_t v = config_read_byte(address);
    i2c_set_client_tx(&v, 1);
}

void comm_on_button_state_read_requested(void) {
}
void comm_on_button_trigger_read_requested(uint8_t button_id) {
    (void)button_id;
}

/* ============================================================================
 * Adopter callbacks: read response handlers (main-loop context)
 *
 * Switching board does not initiate reads — empty stubs.
 * ============================================================================
 */

void comm_on_button_state_read_response(uint8_t addr, CommButtonState* state) {
    (void)addr;
    (void)state;
}
void comm_on_button_trigger_read_response(uint8_t addr, CommTriggerConfig* config) {
    (void)addr;
    (void)config;
}
void comm_on_relay_state_read_response(CommRelayState* state) {
    (void)state;
}
void comm_on_relay_mask_read_response(CommRelayMask* mask) {
    (void)mask;
}
void comm_on_battery_read_response(CommBattery* battery) {
    (void)battery;
}
void comm_on_levels_read_response(CommLevels* levels) {
    (void)levels;
}
void comm_on_level_mode_read_response(CommLevelMode* mode) {
    (void)mode;
}
void comm_on_sensors_read_response(CommSensors* sensors) {
    (void)sensors;
}
void comm_on_config_read_response(uint8_t addr, uint8_t* value) {
    (void)addr;
    (void)value;
}
