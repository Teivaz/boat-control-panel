#include "config.h"
#include "button.h"
#include "led_effect.h"

#include <xc.h>

/* Data EEPROM on PIC18F27Q84 lives at NVM address 0x380000. Byte-addressable;
 * byte writes take ~4 ms. Blank contents read as 0xFF, so the module uses a
 * magic header to detect a virgin device and seeds zero defaults. */
#define EEPROM_ADDR_U     0x38

#define CONFIG_MAGIC_LO   0x5A
#define CONFIG_MAGIC_HI   0xA5

#define OFF_MAGIC_LO      0x00
#define OFF_MAGIC_HI      0x01
#define OFF_BUTTONS       0x02
#define OFF_EFFECTS       (OFF_BUTTONS + BUTTON_COUNT)

static uint8_t eeprom_read (uint8_t offset);
static void    eeprom_write(uint8_t offset, uint8_t value);

void config_init(void) {
    if (eeprom_read(OFF_MAGIC_LO) == CONFIG_MAGIC_LO &&
        eeprom_read(OFF_MAGIC_HI) == CONFIG_MAGIC_HI) return;

    for (uint8_t i = 0; i < BUTTON_COUNT;     i++) eeprom_write(OFF_BUTTONS + i, 0);
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) eeprom_write(OFF_EFFECTS + i, 0);
    /* Magic written last so a reset mid-init re-triggers seeding. */
    eeprom_write(OFF_MAGIC_LO, CONFIG_MAGIC_LO);
    eeprom_write(OFF_MAGIC_HI, CONFIG_MAGIC_HI);
}

CommTriggerConfig config_get_button(uint8_t button_id) {
    CommTriggerConfig cfg = { 0 };
    if (button_id < BUTTON_COUNT) {
        *(uint8_t *)&cfg = eeprom_read(OFF_BUTTONS + button_id);
    }
    return cfg;
}

void config_set_button(uint8_t button_id, CommTriggerConfig cfg) {
    if (button_id < BUTTON_COUNT) {
        eeprom_write(OFF_BUTTONS + button_id, *(uint8_t *)&cfg);
    }
}

CommButtonOutputEffect config_get_effect(uint8_t led_id) {
    CommButtonOutputEffect eff = { 0 };
    if (led_id < LED_EFFECT_COUNT) {
        eff.raw = eeprom_read(OFF_EFFECTS + led_id);
    }
    return eff;
}

void config_set_effect(uint8_t led_id, CommButtonOutputEffect eff) {
    if (led_id < LED_EFFECT_COUNT) {
        eeprom_write(OFF_EFFECTS + led_id, eff.raw);
    }
}

static uint8_t eeprom_read(uint8_t offset) {
    NVMADRU = EEPROM_ADDR_U;
    NVMADRH = 0x00;
    NVMADRL = offset;
    NVMCON1bits.CMD = 0x0;          /* read byte */
    NVMCON0bits.GO  = 1;
    while (NVMCON0bits.GO);
    return NVMDATL;
}

/* Byte write requires the NVMLOCK unlock sequence to be atomic with GO=1;
 * any interrupt between the two unlock writes aborts the operation. */
static void eeprom_write(uint8_t offset, uint8_t value) {
    if(eeprom_read(offset) == value) {
        return;
    }
    NVMADRU = EEPROM_ADDR_U;
    NVMADRH = 0x00;
    NVMADRL = offset;
    NVMDATL = value;
    NVMCON1bits.CMD = 0x3;          /* write byte */

    INTERRUPT_PUSH;
    NVMLOCK = 0x55;
    NVMLOCK = 0xAA;
    NVMCON0bits.GO = 1;
    INTERRUPT_POP;

    while (NVMCON0bits.GO);
}
