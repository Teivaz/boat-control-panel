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
 *        - Read responses  (main-loop context, fired from i2c_poll)
 *        - Incoming writes (main-loop context, fired from i2c_poll
 *          via the cold-rx handler)
 *
 * Usage:
 *   Call comm_interface_init() once after i2c_init().  It registers
 *   the protocol dispatcher with the I2C driver as the cold-rx
 *   handler.  Then implement the comm_on_* callbacks your board needs;
 *   unused ones must still be defined as empty stubs.
 *
 * Read flow (requester side):
 *   comm_send_X_read(addr)  →  I2C write-then-read queued
 *                            →  i2c_poll fires the internal callback
 *                            →  comm_on_X_read_response(addr, parsed*)
 *
 * Write flow (sender):
 *   comm_send_X(addr, ...)  →  I2C write queued
 *
 * Write flow (receiver):
 *   Master writes to us  →  driver buffers bytes, queues a cold-rx
 *                          completion  →  i2c_poll fires the
 *                          dispatcher  →  comm_on_X(parsed*)
 *
 * Note on read-from-this-device:
 *   The new I2C driver only supports a pre-loaded client-TX response
 *   (i2c_set_client_tx).  Parametric reads served from this device
 *   are not supported — boards that need them must update the client
 *   TX buffer ahead of time.
 *
 * Failure signalling:
 *   The completion callback has no explicit status.  By convention,
 *   a read response with rx_len == 0 indicates failure; the
 *   corresponding comm_on_*_response callback receives a NULL pointer.
 *   Write-only completions fire only on success — callers should rely
 *   on application-level retry/timeout instead of expecting a failure
 *   callback.
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
 * TX data is copied into the I2C queue — the CommMessage local is safe
 * to go out of scope immediately.
 *
 * cb/ctx are optional (pass 0, 0 for fire-and-forget).  When non-NULL
 * the completion fires from i2c_poll() after the write lands.  No
 * callback is invoked on a final retry failure — callers needing to
 * detect that must apply their own timeout/retry policy.
 * ============================================================================
 */

I2cResult comm_send_reset(uint8_t addr, I2cCompletion cb, void* ctx);
I2cResult comm_send_config(uint8_t addr, uint8_t config_addr, uint8_t value, I2cCompletion cb, void* ctx);
I2cResult comm_send_button_effect(uint8_t addr, const CommButtonEffect* effect, I2cCompletion cb, void* ctx);
I2cResult comm_send_button_changed(uint8_t button_id, uint8_t pressed, CommButtonMode mode, I2cCompletion cb,
                                   void* ctx);
I2cResult comm_send_button_trigger(uint8_t addr, uint8_t button_id, CommTriggerConfig config, I2cCompletion cb,
                                   void* ctx);
I2cResult comm_send_relay_state(uint16_t relays, I2cCompletion cb, void* ctx);
I2cResult comm_send_relay_changed(uint16_t prev_relays, uint16_t current_relays, uint8_t prev_sensors,
                                  uint8_t current_sensors, I2cCompletion cb, void* ctx);
I2cResult comm_send_relay_mask(uint16_t mask, I2cCompletion cb, void* ctx);
I2cResult comm_send_level_mode(CommMeterMode mode_0, CommMeterMode mode_1, I2cCompletion cb, void* ctx);

/* ============================================================================
 * Outbound read commands (library-implemented)
 *
 * Each submits a write-then-read I2C transaction.  The response arrives
 * asynchronously via the matching comm_on_*_response callback in main-
 * loop context.  Returns I2C_RESULT_OK on successful enqueue.
 * On failure the response callback receives a NULL parsed-struct
 * pointer.
 * ============================================================================
 */

I2cResult comm_send_button_state_read(uint8_t addr);
I2cResult comm_send_button_trigger_read(uint8_t addr, uint8_t button_id);
I2cResult comm_send_relay_state_read(void);
I2cResult comm_send_relay_mask_read(void);
I2cResult comm_send_battery_read(void);
I2cResult comm_send_levels_read(void);
I2cResult comm_send_level_mode_read(void);
I2cResult comm_send_sensors_read(void);
I2cResult comm_send_config_read(uint8_t addr, uint8_t config_addr);

/* ============================================================================
 * Adopter-implemented: read response handlers (main-loop context)
 *
 * Called from i2c_poll() when a read response arrives.  On I2C error
 * the parsed-struct pointer is NULL.
 *
 * The board must define all of these; use empty stubs for unneeded
 * ones.
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
 * Adopter-implemented: incoming write handlers (main-loop context)
 *
 * Called when another master writes a command to this device.  Fired
 * from i2c_poll() — safe to do non-trivial work, but be aware they run
 * before queued host operations get a chance to start, so heavy work
 * still benefits from being scheduled on a task.
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

#endif /* LIBCOMM_INTERFACE_H */
