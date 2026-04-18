#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "libcomm.h"
#include "button.h"
#include "led_effect.h"

/* Device-specific configuration address map (protocol space). Universal
 * addresses 0x00..0x0F are owned by the module (see CommConfigAddress).
 * Payload bytes mirror the on-wire format of their corresponding messages:
 *   BUTTON_TRIGGER  BUTTON_COUNT   bytes, one CommTriggerConfig each
 *   LED_EFFECT      4              bytes, one CommButtonEffect (packed nibbles) */
#define CONFIG_ADDR_BUTTON_TRIGGER   0x10
#define CONFIG_ADDR_LED_EFFECT       (CONFIG_ADDR_BUTTON_TRIGGER + BUTTON_COUNT * sizeof(CommTriggerConfig))

void config_init(void);

/* Protocol-level byte access. Reads from unmapped addresses return 0xFF;
 * writes to read-only/unmapped addresses are silently ignored. */
uint8_t config_read_byte (uint8_t address);
void    config_write_byte(uint8_t address, uint8_t value);

/* Typed accessors that go through the same protocol address space. */
CommTriggerConfig      config_get_button(uint8_t button_id);
void                   config_set_button(uint8_t button_id, CommTriggerConfig cfg);

CommButtonOutputEffect config_get_effect(uint8_t led_id);
void                   config_set_effect(uint8_t led_id, CommButtonOutputEffect eff);

#endif /* CONFIG_H */
