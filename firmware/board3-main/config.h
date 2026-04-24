#ifndef CONFIG_H
#define CONFIG_H

#include "task.h"

#include <stdint.h>

/* Main-board-specific config address map (protocol space).
 * Universal 0x00..0x0F is owned by the module (see CommConfigAddress). */
#define CONFIG_ADDR_NAV_ENABLED_MASK 0x10

/* 5-bit mask: bit N = 1 means light N is physically present and usable.
 *   bit 0 anchoring
 *   bit 1 tricolor
 *   bit 2 steaming
 *   bit 3 bow
 *   bit 4 stern */
#define NAV_LIGHT_ANCHORING 0x01
#define NAV_LIGHT_TRICOLOR 0x02
#define NAV_LIGHT_STEAMING 0x04
#define NAV_LIGHT_BOW 0x08
#define NAV_LIGHT_STERN 0x10
#define NAV_LIGHT_ALL 0x1F

void config_init(TaskController* ctrl);

/* Both ISR-callable — invoked from the I2C on_rx / on_read handlers.
 * read serves directly from the in-RAM shadow; write enqueues onto the
 * flush queue drained from main context. */
uint8_t config_read_byte(uint8_t address);
void config_write_byte(uint8_t address, uint8_t value);

uint8_t config_get_nav_enabled_mask(void);

#endif /* CONFIG_H */
