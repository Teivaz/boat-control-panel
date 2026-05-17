#ifndef CONFIG_H
#define CONFIG_H

#include "task.h"

#include <stdint.h>

/* Switching-board-specific config address map (protocol space).
 * Universal 0x00..0x0F is owned by libcomm (see CommConfigAddress).
 *
 * Each ADC channel has a single-byte scale-factor calibration. The byte is
 * the value the channel reported when a known reference was applied:
 *   Levels: 100 Ω reference → byte = displayed Ω. Default 100.
 *   Battery: 12000 mV reference → byte = displayed mV / 100. Default 120.
 * adc.c's processors apply the inverse correction so the displayed value
 * matches the reference even with current-source / divider tolerance. */
#define CONFIG_ADDR_WATER_CAL 0x10
#define CONFIG_ADDR_FUEL_CAL 0x11
#define CONFIG_ADDR_BATTERY_CAL 0x12
#define CONFIG_ADDR_LEVEL_MODE 0x13   /* packed CommLevelMode byte; persists meter mode across reboot */

void config_init(TaskController* ctrl);

/* Both ISR-callable — invoked from the I2C on_rx / on_read handlers.
 * read serves directly from the in-RAM shadow; write enqueues onto the
 * flush queue drained from main context. */
uint8_t config_read_byte(uint8_t address);
void config_write_byte(uint8_t address, uint8_t value);

/* Convenience: read a 16-bit value stored as two consecutive bytes
 * (address_lo, address_lo+1), low byte first. */
uint16_t config_read_word(uint8_t address_lo);

#endif /* CONFIG_H */
