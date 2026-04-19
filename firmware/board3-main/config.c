#include "config.h"
#include "libcomm.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

#define HW_REVISION  0x01
#define SW_REVISION  0x01

/* Data EEPROM lives at NVM address 0x380000. Blank reads as 0xFF, so a magic
 * header seeds defaults on a virgin device.
 *
 * Storage layout (internal — protocol addresses are remapped by
 * eeprom_offset_for):
 *   0x00..0x01   magic header
 *   OFF_NAV      1 byte — nav light enabled mask (mirrors protocol 0x10) */
#define EEPROM_ADDR_U     0x38
#define CONFIG_MAGIC_LO   0x5A
#define CONFIG_MAGIC_HI   0xA5

#define OFF_MAGIC_LO      0x00
#define OFF_MAGIC_HI      0x01
#define OFF_NAV           0x02
#define OFF_NONE          0xFF

#define WRITE_QUEUE_SIZE   4
#define WRITE_QUEUE_MASK   (WRITE_QUEUE_SIZE - 1)
#define FLUSH_TICK_MS      20

typedef struct {
    uint8_t address;
    uint8_t value;
} WriteEntry;

static volatile WriteEntry wq[WRITE_QUEUE_SIZE];
static volatile uint8_t    wq_head;
static volatile uint8_t    wq_tail;

static uint8_t nvm_read          (uint8_t offset);
static void    nvm_write         (uint8_t offset, uint8_t value);
static uint8_t eeprom_offset_for (uint8_t address);
static uint8_t queue_lookup      (uint8_t address, uint8_t *out);
static void    flush_task        (TaskId id, void *ctx);
static void    write_defaults    (void);

void config_init(TaskController *ctrl) {
    wq_head = wq_tail = 0;

    if (nvm_read(OFF_MAGIC_LO) != CONFIG_MAGIC_LO ||
        nvm_read(OFF_MAGIC_HI) != CONFIG_MAGIC_HI) {
        write_defaults();
        nvm_write(OFF_MAGIC_LO, CONFIG_MAGIC_LO);
        nvm_write(OFF_MAGIC_HI, CONFIG_MAGIC_HI);
    }

    task_controller_add(ctrl, TASK_CONFIG_FLUSH, FLUSH_TICK_MS, flush_task, 0);
}

uint8_t config_read_byte(uint8_t address) {
    switch (address) {
        case COMM_CONFIG_DEVICE_ID:   return comm_address();
        case COMM_CONFIG_HW_REVISION: return HW_REVISION;
        case COMM_CONFIG_SW_REVISION: return SW_REVISION;
    }
    uint8_t pending;
    if (queue_lookup(address, &pending)) return pending;
    uint8_t offset = eeprom_offset_for(address);
    return offset == OFF_NONE ? 0xFF : nvm_read(offset);
}

/* ISR-safe enqueue. Drops on full; config writes are idempotent. */
void config_write_byte(uint8_t address, uint8_t value) {
    if (eeprom_offset_for(address) == OFF_NONE) return;
    INTERRUPT_PUSH;
    uint8_t next = (uint8_t)((wq_tail + 1) & WRITE_QUEUE_MASK);
    if (next != wq_head) {
        wq[wq_tail].address = address;
        wq[wq_tail].value   = value;
        wq_tail = next;
    }
    INTERRUPT_POP;
}

uint8_t config_get_nav_enabled_mask(void) {
    uint8_t v = config_read_byte(CONFIG_ADDR_NAV_ENABLED_MASK);
    return (uint8_t)(v & NAV_LIGHT_ALL);
}

static uint8_t eeprom_offset_for(uint8_t address) {
    if (address == CONFIG_ADDR_NAV_ENABLED_MASK) return OFF_NAV;
    return OFF_NONE;
}

static uint8_t queue_lookup(uint8_t address, uint8_t *out) {
    uint8_t tail  = wq_tail;
    uint8_t found = 0;
    for (uint8_t i = wq_head; i != tail; i = (uint8_t)((i + 1) & WRITE_QUEUE_MASK)) {
        if (wq[i].address == address) {
            *out  = wq[i].value;
            found = 1;
        }
    }
    return found;
}

static void flush_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    if (wq_head == wq_tail) return;
    uint8_t addr   = wq[wq_head].address;
    uint8_t value  = wq[wq_head].value;
    uint8_t offset = eeprom_offset_for(addr);
    if (offset != OFF_NONE) nvm_write(offset, value);
    wq_head = (uint8_t)((wq_head + 1) & WRITE_QUEUE_MASK);
}

static uint8_t nvm_read(uint8_t offset) {
    while (NVMCON0bits.GO);
    NVMADRU = EEPROM_ADDR_U;
    NVMADRH = 0x00;
    NVMADRL = offset;
    NVMCON1bits.CMD = 0x0;
    NVMCON0bits.GO  = 1;
    while (NVMCON0bits.GO);
    return NVMDATL;
}

static void nvm_write(uint8_t offset, uint8_t value) {
    if (nvm_read(offset) == value) return;

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
    nvm_write(OFF_NAV, NAV_LIGHT_ALL);
}
