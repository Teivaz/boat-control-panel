#ifndef CONFIG_H
#define CONFIG_H

#include "task.h"

#include <stdint.h>

/* Switching-board-specific config address map (protocol space).
 * Universal 0x00..0x0F is owned by libcomm (see CommConfigAddress).
 *
 * The level meters and battery divider have per-board calibration that
 * compensates for harness resistance and divider tolerance. Stored as
 * raw 8-bit offsets applied to ADC counts before scaling. */
#define CONFIG_ADDR_LEVEL_OFFSET_WATER 0x10
#define CONFIG_ADDR_LEVEL_OFFSET_FUEL 0x11
#define CONFIG_ADDR_BATTERY_CAL 0x12

void config_init(TaskController* ctrl);
uint8_t config_read_byte(uint8_t address);
void config_write_byte(uint8_t address, uint8_t value);

#endif /* CONFIG_H */
