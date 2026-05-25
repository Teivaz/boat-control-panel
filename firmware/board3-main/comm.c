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

void comm_on_config_received(CommConfig* config) {
    config_write_byte(config->address, config->value);
}

void comm_on_reset(void) {
    RESET();
}

/* Main board does not receive these commands — empty stubs. */
void comm_on_button_effect_received(CommButtonEffect* effect) {
    (void)effect;
}
void comm_on_button_trigger_received(CommButtonTrigger* trigger) {
    (void)trigger;
}
void comm_on_relay_state_received(CommRelayState* state) {
    (void)state;
}
void comm_on_level_mode_received(CommLevelMode* mode) {
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
void comm_on_channel_state_read_requested(void) {
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
 * Adopter callbacks: write completion handlers (main-loop context)
 *
 * relay_state, config, and button_effect completions are handled in
 * controller.c / button_fx.c.  The remaining ones are unused on the
 * main board.
 * ============================================================================
 */

void comm_on_reset_completion(I2cResult result, uint8_t addr) {
    (void)result;
    (void)addr;
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
 * Battery, levels, sensors, channel_state, and config_read are handled
 * in controller.c.  The remaining ones are unused on the main board.
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
void comm_on_level_mode_read_response(I2cResult result, CommLevelMode* mode) {
    (void)result;
    (void)mode;
}
