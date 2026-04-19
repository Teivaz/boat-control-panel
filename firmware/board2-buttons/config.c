#include "config.h"

#include "task.h"
#include "task_ids.h"

#include <xc.h>

/* Bumped on any hardware/firmware revision; monotonic, unsigned 8-bit. */
#define HW_REVISION 0x01
#define SW_REVISION 0x01

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
 *   OFF_EFFECTS (4 bytes)        CommButtonEffect — nibbles packed 2 per byte
 */
#define EEPROM_ADDR_U 0x38
#define CONFIG_MAGIC_LO 0x5A
#define CONFIG_MAGIC_HI 0xA5

#define OFF_MAGIC_LO 0x00
#define OFF_MAGIC_HI 0x01
#define OFF_BUTTONS 0x02
#define OFF_EFFECTS (OFF_BUTTONS + BUTTON_COUNT * sizeof(CommTriggerConfig))
#define OFF_NONE 0xFF

/* Deferred-write queue. Producer: I2C ISR via config_write_byte.
 * Consumer: flush_task (main context). Power-of-two so wrap is a mask. Size
 * 4 is plenty — config writes are one-byte-per-command from the host. */
#define WRITE_QUEUE_SIZE 4
#define WRITE_QUEUE_MASK (WRITE_QUEUE_SIZE - 1)
#define FLUSH_TICK_MS 20

typedef struct {
    uint8_t address;
    uint8_t value;
} WriteEntry;

static volatile WriteEntry wq[WRITE_QUEUE_SIZE];
static volatile uint8_t wq_head; /* consumer (flush_task) */
static volatile uint8_t wq_tail; /* producer (config_write_byte) */

static uint8_t nvm_read(uint8_t offset);
static void nvm_write(uint8_t offset, uint8_t value);
static uint8_t eeprom_offset_for(uint8_t address);
static uint8_t effect_byte_index(uint8_t led_id);
static uint8_t queue_lookup(uint8_t address, uint8_t *out);
static void flush_task(TaskId id, void *ctx);
static void write_default_config(void);

void config_init(TaskController *ctrl) {
    wq_head = wq_tail = 0;

    if (nvm_read(OFF_MAGIC_LO) != CONFIG_MAGIC_LO ||
        nvm_read(OFF_MAGIC_HI) != CONFIG_MAGIC_HI) {
        write_default_config();
        /* Magic written last so a reset mid-init re-triggers seeding. */
        nvm_write(OFF_MAGIC_LO, CONFIG_MAGIC_LO);
        nvm_write(OFF_MAGIC_HI, CONFIG_MAGIC_HI);
    }

    task_controller_add(ctrl, TASK_CONFIG_FLUSH, FLUSH_TICK_MS, flush_task, 0);
}

uint8_t config_read_byte(uint8_t address) {
    switch (address) {
        case COMM_CONFIG_DEVICE_ID:
            return comm_address();
        case COMM_CONFIG_HW_REVISION:
            return HW_REVISION;
        case COMM_CONFIG_SW_REVISION:
            return SW_REVISION;
    }
    uint8_t pending;
    if (queue_lookup(address, &pending))
        return pending;
    uint8_t offset = eeprom_offset_for(address);
    return offset == OFF_NONE ? 0xFF : nvm_read(offset);
}

/* ISR-safe: enqueue only. The actual EEPROM program happens in main context
 * via flush_task. Drops on full — config writes are idempotent so the host
 * can retry. */
void config_write_byte(uint8_t address, uint8_t value) {
    if (eeprom_offset_for(address) == OFF_NONE)
        return;
    INTERRUPT_PUSH;
    uint8_t next = (uint8_t) ((wq_tail + 1) & WRITE_QUEUE_MASK);
    if (next != wq_head) {
        wq[wq_tail].address = address;
        wq[wq_tail].value = value;
        wq_tail = next;
    }
    INTERRUPT_POP;
}

CommTriggerConfig config_get_button(uint8_t button_id) {
    CommTriggerConfig cfg = {0};
    if (button_id < BUTTON_COUNT) {
        *(uint8_t *) &cfg =
            config_read_byte(CONFIG_ADDR_BUTTON_TRIGGER + button_id);
    }
    return cfg;
}

void config_set_button(uint8_t button_id, CommTriggerConfig cfg) {
    if (button_id < BUTTON_COUNT) {
        config_write_byte(CONFIG_ADDR_BUTTON_TRIGGER + button_id,
                          *(uint8_t *) &cfg);
    }
}

CommButtonOutputEffect config_get_effect(uint8_t led_id) {
    CommButtonOutputEffect eff = {0};
    if (led_id < LED_EFFECT_COUNT) {
        uint8_t byte = config_read_byte(CONFIG_ADDR_LED_EFFECT +
                                        effect_byte_index(led_id));
        eff.raw = (led_id & 1) ? (byte >> 4) : (byte & 0x0F);
    }
    return eff;
}

void config_set_effect(uint8_t led_id, CommButtonOutputEffect eff) {
    if (led_id >= LED_EFFECT_COUNT)
        return;
    uint8_t addr =
        (uint8_t) (CONFIG_ADDR_LED_EFFECT + effect_byte_index(led_id));
    uint8_t byte = config_read_byte(addr);
    uint8_t nib = eff.raw & 0x0F;
    byte = (led_id & 1) ? (uint8_t) ((byte & 0x0F) | (nib << 4))
                        : (uint8_t) ((byte & 0xF0) | nib);
    config_write_byte(addr, byte);
}

/* CommButtonEffect packs output N's nibble in byte (7 - N) / 2.
 * odd N -> upper nibble, even N -> lower nibble. */
static uint8_t effect_byte_index(uint8_t led_id) {
    return (uint8_t) ((7 - led_id) / 2);
}

static uint8_t eeprom_offset_for(uint8_t address) {
    if (address >= CONFIG_ADDR_BUTTON_TRIGGER &&
        address < CONFIG_ADDR_BUTTON_TRIGGER + BUTTON_COUNT) {
        return (uint8_t) (OFF_BUTTONS + (address - CONFIG_ADDR_BUTTON_TRIGGER));
    }
    if (address >= CONFIG_ADDR_LED_EFFECT &&
        address < CONFIG_ADDR_LED_EFFECT + sizeof(CommButtonEffect)) {
        return (uint8_t) (OFF_EFFECTS + (address - CONFIG_ADDR_LED_EFFECT));
    }
    return OFF_NONE;
}

/* Walk head..tail oldest-first; latest matching value wins so a
 * write-then-write to the same address reads back the newest pending value.
 * Snapshot tail once to keep the bound stable if a producer preempts. */
static uint8_t queue_lookup(uint8_t address, uint8_t *out) {
    uint8_t tail = wq_tail;
    uint8_t found = 0;
    for (uint8_t i = wq_head; i != tail;
         i = (uint8_t) ((i + 1) & WRITE_QUEUE_MASK)) {
        if (wq[i].address == address) {
            *out = wq[i].value;
            found = 1;
        }
    }
    return found;
}

/* Drain one entry per tick — each nvm_write blocks ~4 ms on the cell
 * program, so the 20 ms interval keeps main context responsive. Increment
 * head AFTER the persist completes so a racing ISR read still sees the
 * pending value in the queue instead of stale EEPROM. */
static void flush_task(TaskId id, void *ctx) {
    (void) id;
    (void) ctx;
    if (wq_head == wq_tail)
        return;
    uint8_t addr = wq[wq_head].address;
    uint8_t value = wq[wq_head].value;
    uint8_t offset = eeprom_offset_for(addr);
    if (offset != OFF_NONE)
        nvm_write(offset, value);
    wq_head = (uint8_t) ((wq_head + 1) & WRITE_QUEUE_MASK);
}

/* Wait for any in-flight NVM op before starting — an ISR read that races
 * with the main-context flush would otherwise overwrite NVMADR* partway
 * through a cell program. Worst-case ISR extension is one write (~4 ms),
 * bounded and rare (both sides must touch config simultaneously). */
static uint8_t nvm_read(uint8_t offset) {
    while (NVMCON0bits.GO)
        ;
    NVMADRU = EEPROM_ADDR_U;
    NVMADRH = 0x00;
    NVMADRL = offset;
    NVMCON1bits.CMD = 0x0; /* read byte */
    NVMCON0bits.GO = 1;
    while (NVMCON0bits.GO)
        ;
    return NVMDATL;
}

/* Byte write requires the NVMLOCK unlock sequence to be atomic with GO=1;
 * any interrupt between the two unlock writes aborts the operation. */
static void nvm_write(uint8_t offset, uint8_t value) {
    if (nvm_read(offset) == value)
        return;

    NVMADRU = EEPROM_ADDR_U;
    NVMADRH = 0x00;
    NVMADRL = offset;
    NVMDATL = value;
    NVMCON1bits.CMD = 0x3; /* write byte */

    INTERRUPT_PUSH;
    NVMLOCK = 0x55;
    NVMLOCK = 0xAA;
    NVMCON0bits.GO = 1;
    INTERRUPT_POP;

    while (NVMCON0bits.GO)
        ;
}

/* Runs on a virgin device (magic header missing). Bypasses the write queue
 * — the scheduler hasn't started yet, and nvm_write is safe to block in
 * init context. Writes internal offsets directly. */
static void write_default_config(void) {
    CommTriggerConfig default_trigger =
        comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1);
    if (comm_address() == COMM_ADDRESS_BUTTON_BOARD_L) {
        /* Button 0 on the left board has a 1.5 s hold time. */
        CommTriggerConfig trigger =
            comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1500);
        nvm_write(OFF_BUTTONS + 0, *(uint8_t *) &trigger);
    } else {
        nvm_write(OFF_BUTTONS + 0, *(uint8_t *) &default_trigger);
    }
    for (uint8_t i = 1; i < BUTTON_COUNT; i++) {
        nvm_write(OFF_BUTTONS + i, *(uint8_t *) &default_trigger);
    }

    CommButtonOutputEffect effect = {.color = COMM_EFFECT_COLOR_WHITE,
                                     .mode = COMM_EFFECT_MODE_DISABLED};
    uint8_t pair = (uint8_t) ((effect.raw & 0x0F) | ((effect.raw & 0x0F) << 4));
    for (uint8_t i = 0; i < sizeof(CommButtonEffect); i++) {
        nvm_write(OFF_EFFECTS + i, pair);
    }
}
