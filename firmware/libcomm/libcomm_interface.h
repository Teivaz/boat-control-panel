#ifndef LIBCOMM_INTERFACE_H
#define LIBCOMM_INTERFACE_H

/*
 * libcomm_interface.h — High-level protocol interface over I2C.
 *
 * This layer sits between the board application code and the raw I2C
 * driver + libcomm builders/parsers.  It provides:
 *
 *   1. Outbound send functions for every protocol command (write and
 *      read).  These are implemented in libcomm_interface.c.
 *
 *   2. Adopter-implemented callbacks for:
 *      - Read responses  (main-loop context, via i2c_poll)
 *      - Read requests   (ISR context, via I2C client read handler)
 *      - Incoming writes (ISR context, via I2C client rx handler)
 *
 * Usage:
 *   Call comm_interface_init() once after i2c_init().  It registers the
 *   protocol dispatchers with the I2C client automatically.  Then
 *   implement the comm_on_* callbacks your board needs — unused ones
 *   must still be defined (empty stubs are fine).
 *
 * Read flow (requester side):
 *   comm_send_X_read(addr)  →  I2C write-then-read queued
 *                            →  i2c_poll fires internal callback
 *                            →  comm_on_X_read_response(addr, parsed*)
 *
 * Read flow (responder side):
 *   Master sends read request  →  I2C client ISR fires dispatch_read
 *                              →  comm_on_X_read_request(out*) fills response
 *                              →  ISR shifts response bytes back to master
 *
 * Write flow (sender):
 *   comm_send_X(addr, ...)  →  I2C write-only queued (fire-and-forget)
 *
 * Write flow (receiver):
 *   Master sends write  →  I2C client ISR fires dispatch_rx
 *                       →  comm_on_X(parsed*) handles the command
 */

#include "i2c.h"
#include "libcomm.h"

/* ============================================================================
 * Initialization
 * ============================================================================
 */

/*
 * Register the protocol dispatchers as i2c_set_rx_handler /
 * i2c_set_read_handler.  Call once after i2c_init().
 */
void comm_interface_init(void);

/* ============================================================================
 * Outbound write commands (library-implemented)
 *
 * Each builds the message via comm_build_* and submits a write-only I2C
 * transaction.  Returns I2C_RESULT_OK on successful enqueue.
 * TX data is copied into the I2C queue — the CommMessage local is safe
 * to go out of scope immediately.
 *
 * cb/ctx are optional (pass 0, 0 for fire-and-forget).  When non-NULL
 * the completion fires from i2c_poll() after the write lands or fails.
 * ============================================================================
 */

/* reset (0x0F) — soft-reset the target device */
I2cResult comm_send_reset(uint8_t addr, I2cCompletion cb, void* ctx);

/* config (0x0E) — write one byte of device configuration */
I2cResult comm_send_config(uint8_t addr, uint8_t config_addr, uint8_t value, I2cCompletion cb, void* ctx);

/* button_effect (0x01) — set visual effects for 8 button outputs */
I2cResult comm_send_button_effect(uint8_t addr, const CommButtonEffect* effect, I2cCompletion cb, void* ctx);

/* button_changed (0x02) — notify main board of a button event.
 * Always sent to COMM_ADDRESS_MAIN; device_address filled from comm_address(). */
I2cResult comm_send_button_changed(uint8_t button_id, uint8_t pressed, CommButtonMode mode, I2cCompletion cb,
                                   void* ctx);

/* button_trigger (0x04) — write trigger config for one button */
I2cResult comm_send_button_trigger(uint8_t addr, uint8_t button_id, CommTriggerConfig config, I2cCompletion cb,
                                   void* ctx);

/* relay_state (0x05) — set the target state of all 16 relays.
 * Always sent to COMM_ADDRESS_SWITCHING (only one switching board). */
I2cResult comm_send_relay_state(uint16_t relays, I2cCompletion cb, void* ctx);

/* relay_changed (0x06) — notify main board of relay/sensor state change.
 * Always sent to COMM_ADDRESS_MAIN; device_address filled from comm_address(). */
I2cResult comm_send_relay_changed(uint16_t prev_relays, uint16_t current_relays, uint8_t prev_sensors,
                                  uint8_t current_sensors, I2cCompletion cb, void* ctx);

/* relay_mask (0x07) — set the relay event mask.
 * Always sent to COMM_ADDRESS_SWITCHING (only one switching board). */
I2cResult comm_send_relay_mask(uint16_t mask, I2cCompletion cb, void* ctx);

/* level_mode (0x0A) — set level meter operating modes.
 * Always sent to COMM_ADDRESS_SWITCHING (only one switching board). */
I2cResult comm_send_level_mode(CommMeterMode mode_0, CommMeterMode mode_1, I2cCompletion cb, void* ctx);

/* ============================================================================
 * Outbound read commands (library-implemented)
 *
 * Each submits a write-then-read I2C transaction.  The response arrives
 * asynchronously via the matching comm_on_*_response callback in main-
 * loop context.  A single shared RX buffer is used — safe because
 * i2c_poll fires the completion callback before starting the next op.
 *
 * Returns I2C_RESULT_OK on successful enqueue.
 * ============================================================================
 */

/* button_state_read (0x83) — read current button input register */
I2cResult comm_send_button_state_read(uint8_t addr);

/* button_trigger_read (0x84) — read trigger config for one button */
I2cResult comm_send_button_trigger_read(uint8_t addr, uint8_t button_id);

/* relay_state_read (0x85) — read target relay state.
 * Always sent to COMM_ADDRESS_SWITCHING. */
I2cResult comm_send_relay_state_read(void);

/* relay_mask_read (0x87) — read relay event mask.
 * Always sent to COMM_ADDRESS_SWITCHING. */
I2cResult comm_send_relay_mask_read(void);

/* battery_read (0x88) — read battery voltage.
 * Always sent to COMM_ADDRESS_SWITCHING. */
I2cResult comm_send_battery_read(void);

/* levels_read (0x89) — read both level meter values.
 * Always sent to COMM_ADDRESS_SWITCHING. */
I2cResult comm_send_levels_read(void);

/* level_mode_read (0x8A) — read level meter modes.
 * Always sent to COMM_ADDRESS_SWITCHING. */
I2cResult comm_send_level_mode_read(void);

/* sensors_read (0x8B) — read on/off sensor states.
 * Always sent to COMM_ADDRESS_SWITCHING. */
I2cResult comm_send_sensors_read(void);

/* config_read (0x8E) — read one byte of device configuration */
I2cResult comm_send_config_read(uint8_t addr, uint8_t config_addr);

/* ============================================================================
 * Adopter-implemented: read response handlers (main-loop context)
 *
 * Called from i2c_poll() when a read response arrives from a remote
 * device.  On I2C error the struct pointer is NULL.
 *
 * The board must define all of these; use empty stubs for unneeded ones.
 * ============================================================================
 */

void comm_on_button_state_read_response(uint8_t addr, CommButtonState* state);
void comm_on_button_trigger_read_response(uint8_t addr, CommTriggerConfig* config);
void comm_on_relay_state_read_response(CommRelayState* state);
void comm_on_relay_mask_read_response(CommRelayMask* mask);
void comm_on_battery_read_response(CommBattery* battery);
void comm_on_levels_read_response(CommLevels* levels);
void comm_on_level_mode_read_response(CommLevelMode* mode);
void comm_on_sensors_read_response(CommSensors* sensors);
void comm_on_config_read_response(uint8_t addr, uint8_t* value);

/* ============================================================================
 * Adopter-implemented: read request handlers (ISR context)
 *
 * Called when another master issues a read to this device.  Fill the
 * output struct and return 0 on success or 1 on error (causes zero-
 * length response / NACK).
 *
 * Runs under clock stretching — keep it fast.
 * ============================================================================
 */

uint8_t comm_on_button_state_read_request(CommButtonState* state);
uint8_t comm_on_button_trigger_read_request(uint8_t button_id, CommTriggerConfig* config);
uint8_t comm_on_relay_state_read_request(CommRelayState* state);
uint8_t comm_on_relay_mask_read_request(CommRelayMask* mask);
uint8_t comm_on_battery_read_request(CommBattery* battery);
uint8_t comm_on_levels_read_request(CommLevels* levels);
uint8_t comm_on_level_mode_read_request(CommLevelMode* mode);
uint8_t comm_on_sensors_read_request(CommSensors* sensors);
uint8_t comm_on_config_read_request(uint8_t address, uint8_t* value);

/* ============================================================================
 * Adopter-implemented: incoming write handlers (ISR context)
 *
 * Called when another master writes a command to this device.
 * Runs in ISR context — must be short, no blocking.  Use
 * run_in_main_loop() to defer heavier work.
 * ============================================================================
 */

void comm_on_reset(void);
void comm_on_config_received(const CommConfig* config);
void comm_on_button_effect_received(const CommButtonEffect* effect);
void comm_on_button_changed_received(const CommButtonChanged* event);
void comm_on_button_trigger_received(const CommButtonTrigger* trigger);
void comm_on_relay_state_received(const CommRelayState* state);
void comm_on_relay_changed_received(const CommRelayChanged* event);
void comm_on_relay_mask_received(const CommRelayMask* mask);
void comm_on_level_mode_received(const CommLevelMode* mode);

/// internal

// This function is invoked when I2C wants to read from client
// Invoked from interrupt
void _comm_on_read(uint8_t index);


#endif /* LIBCOMM_INTERFACE_H */
