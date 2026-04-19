#ifndef CONFIG_MODE_H
#define CONFIG_MODE_H

#include "task.h"

#include <stdint.h>

/* RA7 is wired to a momentary / toggle switch pulling the input low when the
 * user wants to enter configuration. While active, the normal button
 * dispatch is bypassed: the five left-panel nav-light buttons become bit
 * toggles for CONFIG_ADDR_NAV_ENABLED_MASK, and the right-panel buttons
 * move the on-screen cursor between the five slots. Exiting config mode
 * commits the final mask via config_write_byte (EEPROM-backed). */

void config_mode_init(TaskController* ctrl);

/* True while the user is in configuration mode. Read by dispatch / UI layers
 * to suppress normal behaviour. */
uint8_t config_mode_active(void);

/* Index (0..4) of the currently focused nav-light slot in config mode.
 * Returns 0 when config mode is inactive. */
uint8_t config_mode_cursor(void);

/* Invoked from the button-change dispatch when config mode is active.
 * `side` is the originating button board I2C address; `rising` is the mask
 * of buttons whose state flipped 0 -> 1 this event. */
void config_mode_on_buttons(uint8_t side, uint8_t rising);

#endif /* CONFIG_MODE_H */
