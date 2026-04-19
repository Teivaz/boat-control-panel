#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>
#include "task.h"

/* Switching-board orchestration:
 *   - Translates protocol-bit relay_state writes into wire-bit shift-register
 *     output, asserting the "main" power-master rail when any relay is on.
 *   - Polls the mux to detect physical relay divergence and pushes
 *     relay_changed (filtered by the configured relay_mask).
 *   - Maintains battery / level / sensor shadows so I2C read handlers can
 *     respond synchronously from ISR context. */
void controller_init(TaskController *ctrl);

/* Inbound dispatch hooks — invoked from I2C ISR context. */
void controller_set_relay_target(uint16_t target);
void controller_set_relay_mask  (uint16_t mask);
void controller_set_level_mode  (uint8_t  mode_byte);

/* Cached state queries (used by I2C read handlers in ISR context). */
uint16_t controller_relay_target  (void);
uint16_t controller_relay_physical(void);
uint16_t controller_relay_mask    (void);
uint8_t  controller_level_mode    (void);
uint16_t controller_battery_mv    (void);
uint8_t  controller_level         (uint8_t meter_index);  /* 0 = water, 1 = fuel */

#endif /* CONTROLLER_H */
