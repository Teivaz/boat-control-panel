#ifndef BUTTON_FX_H
#define BUTTON_FX_H

#include "task.h"

#include <stdint.h>

/* Per-button feedback overlay on top of the button-board RGB LEDs.
 *
 * Lifecycle per button press:
 *   1. button_fx_notify_press() switches the slot to PENDING -> pulsating.
 *   2. As relay_changed events roll in, the expected (mask, value) is
 *      matched against relay_physical; when it matches, the slot clears
 *      back to IDLE and the base effect takes over.
 *   3. If the expected state does not arrive within BUTTON_FX_TIMEOUT_MS,
 *      the slot flips to ERROR -> flashing red. Controllers clear the
 *      error by calling button_fx_clear(); the base effect then resumes. */

#define BUTTON_FX_SIDES 2
#define BUTTON_FX_BUTTONS_PER_SIDE 7

void button_fx_init(TaskController* ctrl);

/* ISR-context inspection / control from the I2C dispatch path. */
uint8_t button_fx_is_error(uint8_t side, uint8_t button_idx);
void button_fx_clear(uint8_t side, uint8_t button_idx);
void button_fx_notify_press(uint8_t side, uint8_t button_idx, uint16_t expected_mask, uint16_t expected_value);
void button_fx_on_relay_physical(uint16_t physical);

#endif /* BUTTON_FX_H */
