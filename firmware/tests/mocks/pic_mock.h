/*
 * pic_mock.h — host-side stand-in for the PIC18F27Q84's special function
 * registers, plus the small amount of peripheral behaviour the firmware
 * blocks on.
 *
 * Every register in the generated pic_sfr_mock.h resolves to
 * `*(volatile T*)pic_reg(addr)`, so all SFR traffic funnels through one
 * accessor.  That gives us three things a plain array could not:
 *
 *   1. Aliasing.  `LATA` and `LATAbits` share a byte, exactly as on silicon.
 *   2. DMA banking.  `DMASELECT` really does switch which channel's
 *      registers you are looking at, which the I2C driver depends on.
 *   3. Peripherals that spin.  `while (NVMCON0bits.GO);` terminates because
 *      the accessor runs the NVM state machine on the way through.
 *
 * Tests read and write registers with the same names the firmware uses —
 * include <xc.h> (the mock one) and assign.  pic_mock_reset() puts
 * everything back to a known state between tests.
 */

#ifndef PIC_MOCK_H
#define PIC_MOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* XC8 spells the 24-bit pointer type __uint24 and the firmware casts real
 * pointers to it (DMA source addresses).  uintptr_t keeps that cast lossless
 * and warning-free on a 64-bit host; the stored value is never dereferenced
 * by the mock. */
typedef uintptr_t uint24_t;

/* ── Address map constants ─────────────────────────────────────────────── */

#define PIC_SFR_SIZE 0x1000u /* highest register in the device header is 0xC5F */

#define PIC_NVMCON0_ADDR 0x040u
#define PIC_NVMCON1_ADDR 0x041u
#define PIC_NVMLOCK_ADDR 0x042u
#define PIC_NVMADRL_ADDR 0x043u
#define PIC_NVMADRH_ADDR 0x044u
#define PIC_NVMADRU_ADDR 0x045u
#define PIC_NVMDATL_ADDR 0x046u

#define PIC_DMASELECT_ADDR 0x0E8u
#define PIC_DMA_BANK_LO    0x0E9u /* DMAnBUF  */
#define PIC_DMA_BANK_HI    0x0FFu /* DMAnSIRQ */
/* +8 of slack: DMAnSSA / DMAnDSA are pointer-width registers here (see
 * uint24_t above), so a write to the register nearest the top of the window
 * touches a few bytes past it.  On the device those registers are 3 bytes and
 * the window is exact. */
#define PIC_DMA_BANK_SPAN  (PIC_DMA_BANK_HI - PIC_DMA_BANK_LO + 1u + 8u)
#define PIC_DMA_CHANNELS   8u

/* Data EEPROM: 1 KB at NVM address 0x380000.  NVMADRU selects the region. */
#define PIC_EEPROM_ADDR_U 0x38u
#define PIC_EEPROM_SIZE   0x400u
#define PIC_EEPROM_ERASED 0xFFu

/* ── Backing store ─────────────────────────────────────────────────────── */

extern uint8_t pic_sfr[PIC_SFR_SIZE];
extern uint8_t pic_dma[PIC_DMA_CHANNELS][PIC_DMA_BANK_SPAN];
extern uint8_t pic_eeprom[PIC_EEPROM_SIZE];

/* Number of EEPROM byte-program operations the NVM model has performed.
 * config.c skips a write whose cell already holds the value, so this is how
 * a test proves the write-coalescing actually coalesced. */
extern unsigned pic_eeprom_writes;

/* Set non-zero to make the NVM model refuse programs (they read back as the
 * old value), simulating a worn or write-protected cell. */
extern int pic_eeprom_write_fails;

/* Runs the NVM state machine, then returns a pointer to `addr`'s storage,
 * routed to the DMA bank selected by DMASELECT where applicable.
 *
 * Not inline: it is the single choke point every register access goes
 * through, and keeping it out of line means a breakpoint here catches all
 * of them. */
volatile uint8_t* pic_reg(unsigned addr);

/*
 * Storage for the device's 24-bit registers (DMAnSSA, NVMADR).
 *
 * uint24_t is pointer-width here so the firmware's `(uint24_t)buffer` casts
 * stay lossless, which means a write is eight bytes wide where the device
 * writes three.  In the register array that would trample the neighbours —
 * DMAnSSA sits four bytes below DMAnCON0 — so these registers live in a
 * side slot instead, banked by DMASELECT like the rest of the DMA window.
 *
 * The upside is that a DMA source address written by the firmware can be read
 * straight back and dereferenced, which is how the WS2812 tests inspect an
 * encoded frame that is otherwise file-static.
 */
volatile uint8_t* pic_ptrreg(unsigned addr);

/* ── Test control ──────────────────────────────────────────────────────── */

/* Zero every register, DMA bank and counter.  EEPROM contents are *kept* —
 * it is non-volatile storage, and tests that want a virgin device call
 * pic_eeprom_erase() as well. */
void pic_mock_reset(void);

/* Fill the EEPROM with 0xFF, the erased pattern a never-programmed device
 * reads back.  config_init() treats that as "no magic header" and seeds
 * defaults. */
void pic_eeprom_erase(void);

/* Direct EEPROM access that bypasses the NVM state machine, for arranging a
 * starting state or asserting on the result. */
uint8_t pic_eeprom_get(unsigned offset);
void pic_eeprom_put(unsigned offset, uint8_t value);

/* ── Program flash and inline assembly ─────────────────────────────────── */

/*
 * The firmware reaches program flash through the table-read instruction
 * (`asm("TBLRD*+")` in adc.c, reading the chip's factory FVR calibration from
 * the DIA region).  Rather than stub that out — which would leave the ADC
 * scaling untestable — the mock's `asm` macro routes the instruction string
 * here and interprets the handful of mnemonics the firmware actually emits:
 *
 *   NOP        nothing
 *   RESET      bumps pic_reset_count
 *   TBLRD      TABLAT <- flash[TBLPTR]
 *   TBLRD*+    TABLAT <- flash[TBLPTR], then TBLPTR++
 *
 * Anything else is ignored.
 */
void pic_asm(const char* insn);

/* Backing store for table reads.  Sparse: only addresses a test writes are
 * non-zero.  Reads outside the populated range return 0. */
void pic_flash_put_word(uint32_t addr, uint16_t value);
void pic_flash_put_byte(uint32_t addr, uint8_t value);
uint8_t pic_flash_get_byte(uint32_t addr);

/* Incremented every time the firmware executes a RESET instruction — how a
 * test asserts that a reset command actually reboots the device. */
extern unsigned pic_reset_count;

/* ── Register watchers ─────────────────────────────────────────────────── */

/*
 * Called on every access to `addr`, before the firmware reads or writes it.
 *
 * Two things become testable through this that plain memory cannot express:
 *
 *   Bit-banged outputs.  relay_out.c clocks a 16-bit word into a pair of
 *   74HC595s by toggling LATA; a watcher on LATA sees each access and can
 *   reconstruct the shifted word by looking for rising clock edges.  Because
 *   the watcher runs *before* the pending write lands, what it observes on one
 *   call is the result of the previous one — which is exactly the edge it
 *   needs.
 *
 *   Inputs that depend on outputs.  relay_mon.c drives three select lines and
 *   then reads two mux commons; a watcher on PORTA can compute what the mux
 *   would present for the currently selected address and place it in the
 *   register just in time for the read.
 *
 * Pass NULL to remove.  Watchers are cleared by pic_mock_reset().
 */
typedef void (*PicWatchFn)(unsigned addr, void* ctx);
void pic_watch(unsigned addr, PicWatchFn fn, void* ctx);

/* ── Interrupts ────────────────────────────────────────────────────────── */

/* The firmware's INTERRUPT_PUSH / INTERRUPT_POP save, clear and restore GIE,
 * which is a real bit in the mocked INTCON0 — so nesting behaves as it does on
 * the device and a test can assert GIE was left the way it was found.
 *
 * ISRs are ordinary functions here: `__interrupt(...)` expands to nothing, so
 * a test raises an interrupt by setting the flag bit the handler checks and
 * calling the handler directly.  There is no dispatcher to fool. */

#ifdef __cplusplus
}
#endif

#endif /* PIC_MOCK_H */
