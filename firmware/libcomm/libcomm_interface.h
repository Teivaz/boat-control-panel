#ifndef LIBCOMM_INTERFACE_H
#define LIBCOMM_INTERFACE_H

/*
 * libcomm_interface.h — High-level protocol interface over I2C.
 *
 * This layer sits between the board application code and the raw I2C
 * driver + libcomm builders/parsers.  It provides:
 *
 *   1. Outbound send functions for every protocol command (write and
 *      read) — see libcomm_interface.c.
 *
 *   2. Adopter-implemented callbacks for:
 *        - Write completions (main-loop context, fired from i2c_poll)
 *        - Read responses    (main-loop context, fired from i2c_poll)
 *        - Incoming writes   (main-loop context, fired from i2c_poll
 *                             via the cold-rx handler)
 *        - Incoming read requests (ISR context)
 *
 * Both writes and reads use globally-defined adopter callbacks — the
 * board implements one comm_on_*_completion per outbound write and one
 * comm_on_*_read_response per outbound read.  Unused ones must still be
 * defined as empty stubs.
 *
 * Usage:
 *   Call comm_interface_init() once after i2c_init().  It registers
 *   the protocol dispatcher with the I2C driver as the cold-rx
 *   handler.  Then implement the comm_on_* callbacks your board needs.
 *
 * Read flow (requester side):
 *   comm_send_X_read(addr)  →  I2C write-then-read queued
 *                            →  i2c_poll fires the internal trampoline
 *                            →  comm_on_X_read_response(addr, parsed*)
 *
 * Write flow (sender):
 *   comm_send_X(addr, ...)  →  I2C write queued
 *                            →  i2c_poll fires the internal trampoline
 *                            →  comm_on_X_completion(result, addr, ...)
 *
 * Write flow (receiver):
 *   Master writes to us  →  driver buffers bytes, queues a cold-rx
 *                          completion  →  i2c_poll fires the
 *                          dispatcher  →  comm_on_X_received(parsed*)
 *
 * Note on read-from-this-device:
 *   The new I2C driver only supports a pre-loaded client-TX response
 *   (i2c_set_client_tx).  Parametric reads served from this device
 *   are not supported — boards that need them must update the client
 *   TX buffer ahead of time.
 *
 * Failure signalling:
 *   Write completions receive the I2cResult.  Read responses receive a
 *   NULL parsed-struct pointer when the transaction failed (bus error
 *   or response CRC/length mismatch).  Pointers passed to read-response
 *   callbacks are valid only during the call; copy out anything that
 *   must outlive the callback.
 */

#include "i2c.h"
#include "libcomm.h"

/* ============================================================================
 * Initialization
 * ============================================================================
 */

/* Register the protocol dispatcher as the I2C cold-rx handler.
 * Call once after i2c_init(). */
void comm_interface_init(void);

/* ============================================================================
 * Outbound write commands (library-implemented)
 *
 * Each builds the message via comm_build_* and submits a write-only I2C
 * transaction.  Returns I2C_RESULT_OK on successful enqueue.
 * TX data is copied into the I2C queue — caller-owned arguments (including
 * pointer parameters) are safe to drop immediately after the call returns.
 *
 * The matching comm_on_*_completion adopter callback fires from i2c_poll()
 * once the transaction lands or fails (including on final-retry failure).
 * ============================================================================
 */

I2cResult comm_send_reset(uint8_t addr);
I2cResult comm_send_config(uint8_t addr, uint8_t config_addr, uint8_t value);
I2cResult comm_send_button_effect(uint8_t addr, CommButtonEffect* effect);
I2cResult comm_send_button_changed(uint8_t button_id, uint8_t pressed, CommButtonMode mode);
I2cResult comm_send_button_trigger(uint8_t addr, uint8_t button_id, CommTriggerConfig config);
I2cResult comm_send_relay_state(uint16_t relays);
I2cResult comm_send_channel_changed(uint16_t prev_channels, uint16_t current_channels, uint8_t prev_sensors,
                                    uint8_t current_sensors);
I2cResult comm_send_level_mode(CommMeterMode mode_0, CommMeterMode mode_1);

/* ============================================================================
 * Outbound read commands (library-implemented)
 *
 * Each submits a write-then-read I2C transaction.  The response arrives
 * asynchronously via the matching comm_on_*_read_response callback in
 * main-loop context.  Returns I2C_RESULT_OK on successful enqueue.
 * On failure the response callback receives a NULL parsed-struct pointer.
 * ============================================================================
 */

I2cResult comm_send_button_state_read(uint8_t addr);
I2cResult comm_send_button_trigger_read(uint8_t addr, uint8_t button_id);
I2cResult comm_send_relay_state_read(void);
I2cResult comm_send_channel_state_read(void);
I2cResult comm_send_battery_read(void);
I2cResult comm_send_levels_read(void);
I2cResult comm_send_level_mode_read(void);
I2cResult comm_send_sensors_read(void);
I2cResult comm_send_config_read(uint8_t addr, uint8_t config_addr);

/* ============================================================================
 * Adopter-implemented: write completion handlers (main-loop context)
 *
 * Called from i2c_poll() when an outbound write transaction completes
 * (success or failure).  Arguments mirror the matching comm_send_*
 * call and are reconstructed from the queued TX bytes — they reflect
 * what was actually transmitted.
 *
 * The board must define all of these; use empty stubs for unneeded ones.
 * ============================================================================
 */

void comm_on_reset_completion(I2cResult result, uint8_t addr);
void comm_on_config_completion(I2cResult result, uint8_t addr, uint8_t config_addr, uint8_t value);
void comm_on_button_effect_completion(I2cResult result, uint8_t addr, CommButtonEffect* effect);
void comm_on_button_changed_completion(I2cResult result, uint8_t button_id, uint8_t pressed, CommButtonMode mode);
void comm_on_button_trigger_completion(I2cResult result, uint8_t addr, uint8_t button_id, CommTriggerConfig config);
void comm_on_relay_state_completion(I2cResult result, uint16_t relays);
void comm_on_channel_changed_completion(I2cResult result, uint16_t prev_channels, uint16_t current_channels,
                                        uint8_t prev_sensors, uint8_t current_sensors);
void comm_on_level_mode_completion(I2cResult result, CommMeterMode mode_0, CommMeterMode mode_1);

/* ============================================================================
 * Adopter-implemented: read response handlers (main-loop context)
 *
 * Always called from i2c_poll() when a read transaction settles, whether
 * it succeeded or failed.  `result` carries the outcome (I2C_RESULT_OK on
 * success, the bus error otherwise, I2C_RESULT_BAD_CRC on a length/CRC
 * mismatch); the parsed-struct pointer is NULL on any failure.
 *
 * The board must define all of these; use empty stubs for unneeded
 * ones.
 * ============================================================================
 */

void comm_on_button_state_read_response(I2cResult result, uint8_t addr, CommButtonState* state);
void comm_on_button_trigger_read_response(I2cResult result, uint8_t addr, CommTriggerConfig* config);
void comm_on_relay_state_read_response(I2cResult result, CommRelayState* state);
void comm_on_channel_state_read_response(I2cResult result, CommChannelState* state);
void comm_on_battery_read_response(I2cResult result, CommBattery* battery);
void comm_on_levels_read_response(I2cResult result, CommLevels* levels);
void comm_on_level_mode_read_response(I2cResult result, CommLevelMode* mode);
void comm_on_sensors_read_response(I2cResult result, CommSensors* sensors);
void comm_on_config_read_response(I2cResult result, uint8_t addr, uint8_t* value);

/* ============================================================================
 * Adopter-implemented: incoming write handlers (main-loop context)
 *
 * Called when another master writes a command to this device.  Fired
 * from i2c_poll() — safe to do non-trivial work, but be aware they run
 * before queued host operations get a chance to start, so heavy work
 * still benefits from being scheduled on a task.
 * ============================================================================
 */

void comm_on_reset(void);
void comm_on_config_received(CommConfig* config);
void comm_on_button_effect_received(CommButtonEffect* effect);
void comm_on_button_changed_received(CommButtonChanged* event);
void comm_on_button_trigger_received(CommButtonTrigger* trigger);
void comm_on_relay_state_received(CommRelayState* state);
void comm_on_channel_changed_received(CommChannelChanged* event);
void comm_on_level_mode_received(CommLevelMode* mode);

/* ============================================================================
 * Adopter-implemented: incoming read-request handlers (ISR context)
 *
 * Called from the I2C restart ISR when a master issues a write-then-read
 * to this device.  The handler must call comm_respond() to stage the
 * response payload (library appends CRC-8) before returning — the address
 * handler arms client TX DMA immediately after.  Must not block.
 * ============================================================================
 */

/* Stage a read response: copies the caller's payload, appends CRC-8, and
 * calls i2c_set_client_tx() with length `len + 1`.  Boards' read-request
 * handlers should use this rather than calling i2c_set_client_tx directly
 * so the wire-level framing (payload + CRC) stays consistent. */
I2cResult comm_respond(uint8_t* data, uint8_t len);

void comm_on_button_state_read_requested(void);
void comm_on_button_trigger_read_requested(uint8_t button_id);
void comm_on_relay_state_read_requested(void);
void comm_on_channel_state_read_requested(void);
void comm_on_battery_read_requested(void);
void comm_on_levels_read_requested(void);
void comm_on_level_mode_read_requested(void);
void comm_on_sensors_read_requested(void);
void comm_on_config_read_requested(uint8_t address);

#endif /* LIBCOMM_INTERFACE_H */
