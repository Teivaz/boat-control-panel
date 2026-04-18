#include "config.h"

#include <xc.h>

/* Bumped on any hardware/firmware revision; monotonic, unsigned 8-bit. */
#define HW_REVISION  0x01
#define SW_REVISION  0x01

/* Data EEPROM on PIC18F27Q84 lives at NVM address 0x380000. Byte-addressable;
 * byte writes take ~4 ms. Blank contents read as 0xFF, so the module uses a
 * magic header to detect a virgin device and seeds zero defaults.
 *
 * Storage layout (internal — protocol addresses are remapped by
 * eeprom_offset_for). Payload bytes are stored in the same on-wire format
 * as the corresponding protocol messages, so the contents can be mirrored
 * to/from button_trigger / button_effect without reshaping:
 *   0x00..0x01                   magic header
 *   OFF_BUTTONS (BUTTON_COUNT)   array of CommTriggerConfig, one per button
 *   OFF_EFFECTS (4 bytes)        CommButtonEffect — nibbles packed 2 per byte */
#define EEPROM_ADDR_U     0x38
#define CONFIG_MAGIC_LO   0x5A
#define CONFIG_MAGIC_HI   0xA5

#define OFF_MAGIC_LO      0x00
#define OFF_MAGIC_HI      0x01
#define OFF_BUTTONS       0x02
#define OFF_EFFECTS       (OFF_BUTTONS + BUTTON_COUNT * sizeof(CommTriggerConfig))
#define OFF_NONE          0xFF

static uint8_t nvm_read       (uint8_t offset);
static void    nvm_write      (uint8_t offset, uint8_t value);
static uint8_t eeprom_offset_for (uint8_t address);
static uint8_t effect_byte_index (uint8_t led_id);
static void write_default_config(void);

void config_init(void) {
    if (nvm_read(OFF_MAGIC_LO) == CONFIG_MAGIC_LO &&
        nvm_read(OFF_MAGIC_HI) == CONFIG_MAGIC_HI) return;
    write_default_config();
    /* Magic written last so a reset mid-init re-triggers seeding. */
    nvm_write(OFF_MAGIC_LO, CONFIG_MAGIC_LO);
    nvm_write(OFF_MAGIC_HI, CONFIG_MAGIC_HI);
}

uint8_t config_read_byte(uint8_t address) {
    switch (address) {
        case COMM_CONFIG_DEVICE_ID:   return comm_address();
        case COMM_CONFIG_HW_REVISION: return HW_REVISION;
        case COMM_CONFIG_SW_REVISION: return SW_REVISION;
    }
    uint8_t offset = eeprom_offset_for(address);
    return offset == OFF_NONE ? 0xFF : nvm_read(offset);
}

void config_write_byte(uint8_t address, uint8_t value) {
    uint8_t offset = eeprom_offset_for(address);
    if (offset != OFF_NONE) nvm_write(offset, value);
}

CommTriggerConfig config_get_button(uint8_t button_id) {
    CommTriggerConfig cfg = { 0 };
    if (button_id < BUTTON_COUNT) {
        *(uint8_t *)&cfg = config_read_byte(CONFIG_ADDR_BUTTON_TRIGGER + button_id);
    }
    return cfg;
}

void config_set_button(uint8_t button_id, CommTriggerConfig cfg) {
    if (button_id < BUTTON_COUNT) {
        config_write_byte(CONFIG_ADDR_BUTTON_TRIGGER + button_id, *(uint8_t *)&cfg);
    }
}

CommButtonOutputEffect config_get_effect(uint8_t led_id) {
    CommButtonOutputEffect eff = { 0 };
    if (led_id < LED_EFFECT_COUNT) {
        uint8_t byte = config_read_byte(CONFIG_ADDR_LED_EFFECT + effect_byte_index(led_id));
        eff.raw = (led_id & 1) ? (byte >> 4) : (byte & 0x0F);
    }
    return eff;
}

void config_set_effect(uint8_t led_id, CommButtonOutputEffect eff) {
    if (led_id >= LED_EFFECT_COUNT) return;
    uint8_t addr = (uint8_t)(CONFIG_ADDR_LED_EFFECT + effect_byte_index(led_id));
    uint8_t byte = config_read_byte(addr);
    uint8_t nib  = eff.raw & 0x0F;
    byte = (led_id & 1) ? (uint8_t)((byte & 0x0F) | (nib << 4))
                        : (uint8_t)((byte & 0xF0) | nib);
    config_write_byte(addr, byte);
}

/* CommButtonEffect packs output N's nibble in byte (7 - N) / 2.
 * odd N -> upper nibble, even N -> lower nibble. */
static uint8_t effect_byte_index(uint8_t led_id) {
    return (uint8_t)((7 - led_id) / 2);
}

static uint8_t eeprom_offset_for(uint8_t address) {
    if (address >= CONFIG_ADDR_BUTTON_TRIGGER &&
        address <  CONFIG_ADDR_BUTTON_TRIGGER + BUTTON_COUNT) {
        return (uint8_t)(OFF_BUTTONS + (address - CONFIG_ADDR_BUTTON_TRIGGER));
    }
    if (address >= CONFIG_ADDR_LED_EFFECT &&
        address <  CONFIG_ADDR_LED_EFFECT + sizeof(CommButtonEffect)) {
        return (uint8_t)(OFF_EFFECTS + (address - CONFIG_ADDR_LED_EFFECT));
    }
    return OFF_NONE;
}

static uint8_t nvm_read(uint8_t offset) {
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
static void nvm_write(uint8_t offset, uint8_t value) {
    if (nvm_read(offset) == value) return;

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

void write_default_config(void) {
    // Default mode is 1ms hold time
    CommTriggerConfig default_trigger = comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1);
    if (comm_address() == COMM_ADDRESS_BUTTON_BOARD_L) {
        // Button 0 on the left board has 1.5s hold time
        CommTriggerConfig trigger = comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1500);
        config_write_byte(OFF_BUTTONS + 0, *(uint8_t*)&trigger);
    } else {
        config_write_byte(OFF_BUTTONS + 0, *(uint8_t*)&default_trigger);
    }
    config_write_byte(OFF_BUTTONS + 1, *(uint8_t*)&default_trigger);
    config_write_byte(OFF_BUTTONS + 2, *(uint8_t*)&default_trigger);
    config_write_byte(OFF_BUTTONS + 3, *(uint8_t*)&default_trigger);
    config_write_byte(OFF_BUTTONS + 4, *(uint8_t*)&default_trigger);
    config_write_byte(OFF_BUTTONS + 5, *(uint8_t*)&default_trigger);
    config_write_byte(OFF_BUTTONS + 6, *(uint8_t*)&default_trigger);

    CommButtonOutputEffect effect = { .color=COMM_EFFECT_COLOR_WHITE, .mode = COMM_EFFECT_MODE_DISABLED };
    uint8_t effect_pair = effect.raw & 0x0F;
    effect_pair |= (effect.raw << 4) & 0xF0;
    for (uint8_t i = 0; i < sizeof(CommButtonEffect); i++) {
         config_write_byte(OFF_EFFECTS + i, effect_pair);
    }
}
