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

void comm_on_relay_state_received(CommRelayState* state) {
    controller_set_relay_target(state->relays);
}

void comm_on_level_mode_received(CommLevelMode* mode) {
    /* Update RAM and persist so the mode survives a reboot. config_write_byte
     * dedups against pending writes, so a flurry of repeated writes coalesces
     * to a single NVM program. */
    uint8_t byte = (uint8_t)(*(uint8_t*)mode & 0x0F);
    controller_set_level_mode(byte);
    config_write_byte(CONFIG_ADDR_LEVEL_MODE, byte);
}

void comm_on_config_received(CommConfig* config) {
    config_write_byte(config->address, config->value);
}

void comm_on_reset(void) {
    RESET();
}

void comm_on_button_effect_received(CommButtonEffect* effect) {
    (void)effect;
}
void comm_on_button_changed_received(CommButtonChanged* event) {
    (void)event;
}
void comm_on_button_trigger_received(CommButtonTrigger* trigger) {
    (void)trigger;
}
void comm_on_channel_changed_received(CommChannelChanged* event) {
    (void)event;
}

/* ============================================================================
 * Adopter callbacks: read-request handlers (ISR context)
 * ============================================================================
 */

void comm_on_relay_state_read_requested(void) {
    uint16_t v = controller_relay_target();
    uint8_t buf[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    comm_respond(buf, 2);
}

void comm_on_channel_state_read_requested(void) {
    uint16_t v = controller_channel_state();
    uint8_t buf[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    comm_respond(buf, 2);
}

void comm_on_battery_read_requested(void) {
    uint16_t v = controller_battery_mv();
    uint8_t buf[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    comm_respond(buf, 2);
}

void comm_on_levels_read_requested(void) {
    uint8_t buf[2] = {controller_level(0), controller_level(1)};
    comm_respond(buf, 2);
}

void comm_on_level_mode_read_requested(void) {
    uint8_t v = controller_level_mode();
    comm_respond(&v, 1);
}

void comm_on_sensors_read_requested(void) {
    uint8_t v = (uint8_t)(sensors_state() & 0x07);
    comm_respond(&v, 1);
}

void comm_on_config_read_requested(uint8_t address) {
    uint8_t v = config_read_byte(address);
    comm_respond(&v, 1);
}

void comm_on_button_state_read_requested(void) {
}
void comm_on_button_trigger_read_requested(uint8_t button_id) {
    (void)button_id;
}

/* ============================================================================
 * Adopter callbacks: write completion handlers (main-loop context)
 *
 * Switching board only initiates channel_changed writes and does not
 * react to the completion.  All others are unused.
 * ============================================================================
 */

void comm_on_reset_completion(I2cResult result, uint8_t addr) {
    (void)result;
    (void)addr;
}
void comm_on_config_completion(I2cResult result, uint8_t addr, uint8_t config_addr, uint8_t value) {
    (void)result;
    (void)addr;
    (void)config_addr;
    (void)value;
}
void comm_on_button_effect_completion(I2cResult result, uint8_t addr, CommButtonEffect* effect) {
    (void)result;
    (void)addr;
    (void)effect;
}
void comm_on_button_changed_completion(I2cResult result, uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
    (void)result;
    (void)button_id;
    (void)pressed;
    (void)mode;
}
void comm_on_button_trigger_completion(I2cResult result, uint8_t addr, uint8_t button_id, CommTriggerConfig config) {
    (void)result;
    (void)addr;
    (void)button_id;
    (void)config;
}
void comm_on_relay_state_completion(I2cResult result, uint16_t relays) {
    (void)result;
    (void)relays;
}
void comm_on_channel_changed_completion(I2cResult result, uint16_t prev_channels, uint16_t current_channels,
                                        uint8_t prev_sensors, uint8_t current_sensors) {
    (void)result;
    (void)prev_channels;
    (void)current_channels;
    (void)prev_sensors;
    (void)current_sensors;
}
void comm_on_level_mode_completion(I2cResult result, CommMeterMode mode_0, CommMeterMode mode_1) {
    (void)result;
    (void)mode_0;
    (void)mode_1;
}

/* ============================================================================
 * Adopter callbacks: read response handlers (main-loop context)
 *
 * Switching board does not initiate reads — empty stubs.
 * ============================================================================
 */

void comm_on_button_state_read_response(I2cResult result, uint8_t addr, CommButtonState* state) {
    (void)result;
    (void)addr;
    (void)state;
}
void comm_on_button_trigger_read_response(I2cResult result, uint8_t addr, CommTriggerConfig* config) {
    (void)result;
    (void)addr;
    (void)config;
}
void comm_on_relay_state_read_response(I2cResult result, CommRelayState* state) {
    (void)result;
    (void)state;
}
void comm_on_channel_state_read_response(I2cResult result, CommChannelState* state) {
    (void)result;
    (void)state;
}
void comm_on_battery_read_response(I2cResult result, CommBattery* battery) {
    (void)result;
    (void)battery;
}
void comm_on_levels_read_response(I2cResult result, CommLevels* levels) {
    (void)result;
    (void)levels;
}
void comm_on_level_mode_read_response(I2cResult result, CommLevelMode* mode) {
    (void)result;
    (void)mode;
}
void comm_on_sensors_read_response(I2cResult result, CommSensors* sensors) {
    (void)result;
    (void)sensors;
}
void comm_on_config_read_response(I2cResult result, uint8_t addr, uint8_t* value) {
    (void)result;
    (void)addr;
    (void)value;
}
