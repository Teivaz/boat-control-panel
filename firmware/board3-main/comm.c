#include "comm.h"

#include "config.h"
#include "controller.h"
#include "libcomm.h"
#include "libcomm_interface.h"

#include <xc.h>

uint8_t comm_address(void) {
    return COMM_ADDRESS_MAIN;
}

void comm_init(void) {
    /* Dispatch is handled by comm_interface_init() in main.c.
     * This function remains for any board-specific post-init if needed. */
}

/* ============================================================================
 * Adopter callbacks: incoming write handlers (main-loop context)
 * ============================================================================
 */

void comm_on_button_changed_received(const CommButtonChanged* event) {
    controller_on_button_changed(event->device_address, event->button_id, event->pressed, (CommButtonMode)event->mode);
}

void comm_on_relay_changed_received(const CommRelayChanged* event) {
    controller_on_relay_changed(event->device_address, event->prev_relays, event->current_relays, event->prev_sensors,
                                event->current_sensors);
}

void comm_on_config_received(const CommConfig* config) {
    config_write_byte(config->address, config->value);
}

void comm_on_reset(void) {
    RESET();
}

/* Main board does not receive these commands — empty stubs. */
void comm_on_button_effect_received(const CommButtonEffect* effect) {
    (void)effect;
}
void comm_on_button_trigger_received(const CommButtonTrigger* trigger) {
    (void)trigger;
}
void comm_on_relay_state_received(const CommRelayState* state) {
    (void)state;
}
void comm_on_relay_mask_received(const CommRelayMask* mask) {
    (void)mask;
}
void comm_on_level_mode_received(const CommLevelMode* mode) {
    (void)mode;
}

/* Main board does not serve these reads — empty stubs. */
void comm_on_button_state_read_requested(void) {
}
void comm_on_button_trigger_read_requested(uint8_t button_id) {
    (void)button_id;
}
void comm_on_config_read_requested(uint8_t address) {
    (void)address;
}
void comm_on_relay_state_read_requested(void) {
}
void comm_on_relay_mask_read_requested(void) {
}
void comm_on_battery_read_requested(void) {
}
void comm_on_levels_read_requested(void) {
}
void comm_on_level_mode_read_requested(void) {
}
void comm_on_sensors_read_requested(void) {
}

/* ============================================================================
 * Adopter callbacks: read response handlers (main-loop context)
 *
 * Battery, levels, sensors, and config_read are forwarded to the
 * controller which latches the values into its shadow state.
 * ============================================================================
 */

void comm_on_battery_read_response(CommBattery* battery) {
    controller_on_battery_response(battery);
}
void comm_on_levels_read_response(CommLevels* levels) {
    controller_on_levels_response(levels);
}
void comm_on_sensors_read_response(CommSensors* sensors) {
    controller_on_sensors_response(sensors);
}
void comm_on_config_read_response(uint8_t addr, uint8_t* value) {
    (void)addr;
    controller_on_config_read_response(value);
}

/* Unused on the main board. */
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
void comm_on_level_mode_read_response(CommLevelMode* mode) {
    (void)mode;
}
