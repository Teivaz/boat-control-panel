#include "config.h"

#include "libcomm.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

#define HW_REVISION 0x01
#define SW_REVISION 0x01

/* Data EEPROM lives at NVM address 0x380000. Blank reads as 0xFF, so a magic
 * header seeds defaults on a virgin device.
 *
 * Storage layout (internal — protocol addresses are remapped by
 * eeprom_offset_for):
 *   0x00..0x01   magic header
 *   OFF_WATER    1 byte — water-meter wire offset (protocol 0x10)
 *   OFF_FUEL     1 byte — fuel-meter wire offset  (protocol 0x11)
 *   OFF_BATT     1 byte — battery divider trim    (protocol 0x12) */
#define EEPROM_ADDR_U 0x38
#define CONFIG_MAGIC_LO 0x5A
#define CONFIG_MAGIC_HI 0xA5

#define OFF_MAGIC_LO 0x00
#define OFF_MAGIC_HI 0x01
#define OFF_WATER 0x02
#define OFF_FUEL 0x03
#define OFF_BATT 0x04
#define OFF_NONE 0xFF

#define WRITE_QUEUE_SIZE 4
#define WRITE_QUEUE_MASK (WRITE_QUEUE_SIZE - 1)

typedef struct {
    uint8_t address;
    uint8_t value;
} WriteEntry;

static volatile WriteEntry wq[WRITE_QUEUE_SIZE];
static volatile uint8_t wq_head;
static volatile uint8_t wq_tail;
static volatile uint8_t wq_task_scheduled;
static TaskController* ctrl_ref;

static uint8_t nvm_read(uint8_t offset);
static void nvm_write(uint8_t offset, uint8_t value);
static uint8_t eeprom_offset_for(uint8_t address);
static uint8_t queue_lookup(uint8_t address, uint8_t* out);
static void flush_task(TaskId id, void* ctx);
static void write_defaults(void);

void config_init(TaskController* ctrl) {
    wq_head = wq_tail = 0;
    wq_task_scheduled = 0;
    ctrl_ref = ctrl;

    if (nvm_read(OFF_MAGIC_LO) != CONFIG_MAGIC_LO || nvm_read(OFF_MAGIC_HI) != CONFIG_MAGIC_HI) {
        write_defaults();
        nvm_write(OFF_MAGIC_LO, CONFIG_MAGIC_LO);
        nvm_write(OFF_MAGIC_HI, CONFIG_MAGIC_HI);
    }
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
    if (queue_lookup(address, &pending)) {
        return pending;
    }
    uint8_t offset = eeprom_offset_for(address);
    return offset == OFF_NONE ? 0xFF : nvm_read(offset);
}

/* ISR-safe. On first pending write spawn the flush task; subsequent writes
 * dedup against the queue so a burst of updates to the same address collapses
 * to a single NVM program. Drops silently when the queue is full — config
 * writes are idempotent so the host can retry. */
void config_write_byte(uint8_t address, uint8_t value) {
    if (eeprom_offset_for(address) == OFF_NONE) {
        return;
    }
    INTERRUPT_PUSH;
    uint8_t merged = 0;
    for (uint8_t i = wq_head; i != wq_tail; i = (uint8_t)((i + 1) & WRITE_QUEUE_MASK)) {
        if (wq[i].address == address) {
            wq[i].value = value;
            merged = 1;
            break;
        }
    }
    if (!merged) {
        uint8_t next = (uint8_t)((wq_tail + 1) & WRITE_QUEUE_MASK);
        if (next != wq_head) {
            wq[wq_tail].address = address;
            wq[wq_tail].value = value;
            wq_tail = next;
        }
    }
    if (!wq_task_scheduled && wq_head != wq_tail) {
        task_controller_add(ctrl_ref, TASK_CONFIG_FLUSH, TASK_MIN_MS, flush_task, 0);
        wq_task_scheduled = 1;
    }
    INTERRUPT_POP;
}

static uint8_t eeprom_offset_for(uint8_t address) {
    switch (address) {
        case CONFIG_ADDR_LEVEL_OFFSET_WATER:
            return OFF_WATER;
        case CONFIG_ADDR_LEVEL_OFFSET_FUEL:
            return OFF_FUEL;
        case CONFIG_ADDR_BATTERY_CAL:
            return OFF_BATT;
    }
    return OFF_NONE;
}

static uint8_t queue_lookup(uint8_t address, uint8_t* out) {
    uint8_t tail = wq_tail;
    uint8_t found = 0;
    for (uint8_t i = wq_head; i != tail; i = (uint8_t)((i + 1) & WRITE_QUEUE_MASK)) {
        if (wq[i].address == address) {
            *out = wq[i].value;
            found = 1;
        }
    }
    return found;
}

/* Drain the whole queue then remove the task. wq_task_scheduled is cleared
 * under INTERRUPT_PUSH/POP while the queue is observed empty, so a producer
 * that enqueues after our check is forced to respawn the task. */
static void flush_task(TaskId id, void* ctx) {
    (void)ctx;
    while (1) {
        INTERRUPT_PUSH;
        if (wq_head == wq_tail) {
            wq_task_scheduled = 0;
            task_controller_remove(ctrl_ref, id);
            INTERRUPT_POP;
            return;
        }
        uint8_t addr = wq[wq_head].address;
        uint8_t value = wq[wq_head].value;
        INTERRUPT_POP;

        uint8_t offset = eeprom_offset_for(addr);
        if (offset != OFF_NONE) {
            nvm_write(offset, value);
        }

        INTERRUPT_PUSH_NDECL;
        wq_head = (uint8_t)((wq_head + 1) & WRITE_QUEUE_MASK);
        INTERRUPT_POP;
    }
}

static uint8_t nvm_read(uint8_t offset) {
    while (NVMCON0bits.GO);
    NVMADRU = EEPROM_ADDR_U;
    NVMADRH = 0x00;
    NVMADRL = offset;
    NVMCON1bits.CMD = 0x0;
    NVMCON0bits.GO = 1;
    while (NVMCON0bits.GO);
    return NVMDATL;
}

static void nvm_write(uint8_t offset, uint8_t value) {
    if (nvm_read(offset) == value) {
        return;
    }

    NVMADRU = EEPROM_ADDR_U;
    NVMADRH = 0x00;
    NVMADRL = offset;
    NVMDATL = value;
    NVMCON1bits.CMD = 0x3;

    INTERRUPT_PUSH;
    NVMLOCK = 0x55;
    NVMLOCK = 0xAA;
    NVMCON0bits.GO = 1;
    INTERRUPT_POP;

    while (NVMCON0bits.GO);
}

static void write_defaults(void) {
    nvm_write(OFF_WATER, 0);
    nvm_write(OFF_FUEL, 0);
    nvm_write(OFF_BATT, 0);
}
