#include "pic_mock.h"

#include <string.h>

uint8_t pic_sfr[PIC_SFR_SIZE];
uint8_t pic_dma[PIC_DMA_CHANNELS][PIC_DMA_BANK_SPAN];
uint8_t pic_eeprom[PIC_EEPROM_SIZE];

unsigned pic_eeprom_writes;
int pic_eeprom_write_fails;
unsigned pic_reset_count;

static void pic_flash_clear(void);

/* NVMCON0 bit 0 is GO, NVMCON1 bits 2:0 are CMD.  Only the two commands the
 * firmware issues are modelled. */
#define NVMCON0_GO   0x01u
#define NVMCMD_READ  0x0u
#define NVMCMD_WRITE 0x3u

/*
 * Complete any NVM operation the firmware has kicked off.
 *
 * On the device the byte program takes ~4 ms and the firmware spins on GO.
 * Here the operation lands the instant anything touches a register, which is
 * what makes `while (NVMCON0bits.GO);` terminate: the read that evaluates the
 * condition is itself what clears the bit.
 *
 * Only the 0x38xxxx data-EEPROM region is backed.  A program aimed anywhere
 * else (program flash) completes with no effect, which is the safe behaviour
 * for a test that strays outside the region config.c uses.
 */
static void nvm_tick(void) {
    if (!(pic_sfr[PIC_NVMCON0_ADDR] & NVMCON0_GO)) {
        return;
    }
    pic_sfr[PIC_NVMCON0_ADDR] &= (uint8_t)~NVMCON0_GO;

    if (pic_sfr[PIC_NVMADRU_ADDR] != PIC_EEPROM_ADDR_U) {
        return;
    }
    const unsigned offset =
        (unsigned)((pic_sfr[PIC_NVMADRH_ADDR] << 8) | pic_sfr[PIC_NVMADRL_ADDR]);
    if (offset >= PIC_EEPROM_SIZE) {
        return;
    }

    switch (pic_sfr[PIC_NVMCON1_ADDR] & 0x07u) {
        case NVMCMD_READ:
            pic_sfr[PIC_NVMDATL_ADDR] = pic_eeprom[offset];
            break;
        case NVMCMD_WRITE:
            if (!pic_eeprom_write_fails) {
                pic_eeprom[offset] = pic_sfr[PIC_NVMDATL_ADDR];
            }
            pic_eeprom_writes++;
            break;
        default:
            break;
    }
}

/* ── Register watchers ─────────────────────────────────────────────────── */

#define WATCH_SLOTS 8u
static struct {
    unsigned addr;
    PicWatchFn fn;
    void* ctx;
    uint8_t used;
} pic_watches[WATCH_SLOTS];

void pic_watch(unsigned addr, PicWatchFn fn, void* ctx) {
    for (unsigned i = 0; i < WATCH_SLOTS; i++) {
        if (pic_watches[i].used && pic_watches[i].addr == addr) {
            if (fn == 0) {
                pic_watches[i].used = 0;
            } else {
                pic_watches[i].fn = fn;
                pic_watches[i].ctx = ctx;
            }
            return;
        }
    }
    if (fn == 0) {
        return;
    }
    for (unsigned i = 0; i < WATCH_SLOTS; i++) {
        if (!pic_watches[i].used) {
            pic_watches[i].used = 1;
            pic_watches[i].addr = addr;
            pic_watches[i].fn = fn;
            pic_watches[i].ctx = ctx;
            return;
        }
    }
}

/* Guards against a watcher that touches the register it watches. */
static uint8_t pic_in_watch;

static void run_watches(unsigned addr) {
    if (pic_in_watch) {
        return;
    }
    pic_in_watch = 1;
    for (unsigned i = 0; i < WATCH_SLOTS; i++) {
        if (pic_watches[i].used && pic_watches[i].addr == addr) {
            pic_watches[i].fn(addr, pic_watches[i].ctx);
        }
    }
    pic_in_watch = 0;
}

volatile uint8_t* pic_reg(unsigned addr) {
    nvm_tick();
    run_watches(addr);

    if (addr >= PIC_DMA_BANK_LO && addr <= PIC_DMA_BANK_HI) {
        /* DMASELECT holds instance - 1 (§16.12).  Clamp rather than assert:
         * a driver bug that selects a nonexistent channel should show up as a
         * failed expectation in the test, not as a harness crash. */
        unsigned channel = pic_sfr[PIC_DMASELECT_ADDR];
        if (channel >= PIC_DMA_CHANNELS) {
            channel = PIC_DMA_CHANNELS - 1u;
        }
        return &pic_dma[channel][addr - PIC_DMA_BANK_LO];
    }

    if (addr >= PIC_SFR_SIZE) {
        static uint8_t sink;
        sink = 0;
        return &sink;
    }
    return &pic_sfr[addr];
}

/* Side storage for the 24-bit registers.  Keyed by (address, DMA bank), which
 * is at most two addresses across eight banks. */
#define PTRREG_SLOTS 24u
static struct {
    unsigned addr;
    unsigned bank;
    uint8_t used;
    uintptr_t value;
} pic_ptrregs[PTRREG_SLOTS];

volatile uint8_t* pic_ptrreg(unsigned addr) {
    nvm_tick();
    run_watches(addr);

    unsigned bank = 0;
    if (addr >= PIC_DMA_BANK_LO && addr <= PIC_DMA_BANK_HI) {
        bank = pic_sfr[PIC_DMASELECT_ADDR];
        if (bank >= PIC_DMA_CHANNELS) {
            bank = PIC_DMA_CHANNELS - 1u;
        }
    }
    for (unsigned i = 0; i < PTRREG_SLOTS; i++) {
        if (pic_ptrregs[i].used && pic_ptrregs[i].addr == addr && pic_ptrregs[i].bank == bank) {
            return (volatile uint8_t*)&pic_ptrregs[i].value;
        }
    }
    for (unsigned i = 0; i < PTRREG_SLOTS; i++) {
        if (!pic_ptrregs[i].used) {
            pic_ptrregs[i].used = 1;
            pic_ptrregs[i].addr = addr;
            pic_ptrregs[i].bank = bank;
            pic_ptrregs[i].value = 0;
            return (volatile uint8_t*)&pic_ptrregs[i].value;
        }
    }
    static uintptr_t sink;
    sink = 0;
    return (volatile uint8_t*)&sink;
}

void pic_mock_reset(void) {
    memset(pic_sfr, 0, sizeof(pic_sfr));
    memset(pic_dma, 0, sizeof(pic_dma));
    memset(pic_ptrregs, 0, sizeof(pic_ptrregs));
    pic_flash_clear();
    memset(pic_watches, 0, sizeof(pic_watches));
    pic_in_watch = 0;
    pic_eeprom_writes = 0;
    pic_eeprom_write_fails = 0;
    pic_reset_count = 0;
}

void pic_eeprom_erase(void) {
    memset(pic_eeprom, PIC_EEPROM_ERASED, sizeof(pic_eeprom));
}

uint8_t pic_eeprom_get(unsigned offset) {
    return (offset < PIC_EEPROM_SIZE) ? pic_eeprom[offset] : PIC_EEPROM_ERASED;
}

void pic_eeprom_put(unsigned offset, uint8_t value) {
    if (offset < PIC_EEPROM_SIZE) {
        pic_eeprom[offset] = value;
    }
}

/* ── Program flash and inline assembly ─────────────────────────────────── */

/* Sparse table of populated flash bytes.  The firmware only ever table-reads
 * a couple of DIA calibration words, so a short linear array is plenty and
 * avoids modelling a 128 KB address space. */
#define FLASH_SLOTS 32u
static struct {
    uint32_t addr;
    uint8_t value;
    uint8_t used;
} pic_flash[FLASH_SLOTS];

static void pic_flash_clear(void) {
    memset(pic_flash, 0, sizeof(pic_flash));
}

void pic_flash_put_byte(uint32_t addr, uint8_t value) {
    for (unsigned i = 0; i < FLASH_SLOTS; i++) {
        if (pic_flash[i].used && pic_flash[i].addr == addr) {
            pic_flash[i].value = value;
            return;
        }
    }
    for (unsigned i = 0; i < FLASH_SLOTS; i++) {
        if (!pic_flash[i].used) {
            pic_flash[i].used = 1;
            pic_flash[i].addr = addr;
            pic_flash[i].value = value;
            return;
        }
    }
}

void pic_flash_put_word(uint32_t addr, uint16_t value) {
    pic_flash_put_byte(addr, (uint8_t)value);              /* low byte first */
    pic_flash_put_byte(addr + 1u, (uint8_t)(value >> 8));
}

uint8_t pic_flash_get_byte(uint32_t addr) {
    for (unsigned i = 0; i < FLASH_SLOTS; i++) {
        if (pic_flash[i].used && pic_flash[i].addr == addr) {
            return pic_flash[i].value;
        }
    }
    return 0;
}

#define PIC_TBLPTRL_ADDR 0x4F6u
#define PIC_TBLPTRH_ADDR 0x4F7u
#define PIC_TBLPTRU_ADDR 0x4F8u
#define PIC_TABLAT_ADDR  0x4F5u

static uint32_t tblptr_get(void) {
    return ((uint32_t)pic_sfr[PIC_TBLPTRU_ADDR] << 16) | ((uint32_t)pic_sfr[PIC_TBLPTRH_ADDR] << 8) |
           pic_sfr[PIC_TBLPTRL_ADDR];
}

static void tblptr_set(uint32_t v) {
    pic_sfr[PIC_TBLPTRL_ADDR] = (uint8_t)v;
    pic_sfr[PIC_TBLPTRH_ADDR] = (uint8_t)(v >> 8);
    pic_sfr[PIC_TBLPTRU_ADDR] = (uint8_t)(v >> 16);
}

static int insn_is(const char* insn, const char* want) {
    while (*want) {
        if (*insn++ != *want++) {
            return 0;
        }
    }
    return *insn == '\0';
}

void pic_asm(const char* insn) {
    if (insn == 0) {
        return;
    }
    if (insn_is(insn, "RESET")) {
        pic_reset_count++;
        return;
    }
    if (insn_is(insn, "TBLRD*+")) {
        uint32_t p = tblptr_get();
        pic_sfr[PIC_TABLAT_ADDR] = pic_flash_get_byte(p);
        tblptr_set(p + 1u);
        return;
    }
    if (insn_is(insn, "TBLRD")) {
        pic_sfr[PIC_TABLAT_ADDR] = pic_flash_get_byte(tblptr_get());
        return;
    }
    /* NOP and anything else: no observable effect. */
}
