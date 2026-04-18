#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "libcomm.h"

/* Persistent defaults for button triggers and LED effects. Stored in
 * data EEPROM; survives power cycles. Blank devices are initialized to
 * zeros (UNKNOWN trigger, WHITE/DISABLED effect) on first boot. */
void config_init(void);

CommTriggerConfig      config_get_button(uint8_t button_id);
void                   config_set_button(uint8_t button_id, CommTriggerConfig cfg);

CommButtonOutputEffect config_get_effect(uint8_t led_id);
void                   config_set_effect(uint8_t led_id, CommButtonOutputEffect eff);

#endif /* CONFIG_H */
