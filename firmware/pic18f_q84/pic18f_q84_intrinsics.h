/**
 * @file pic18f_q84_intrinsics.h
 * @brief Compiler intrinsics for PIC18F27/47/57Q84 chip-specific instructions.
 *
 * These inline functions and macros wrap PIC18 assembly instructions that have
 * no equivalent in standard C.  They cover:
 *
 *   - Device control   : RESET, SLEEP, NOP, CLRWDT
 *   - BCD arithmetic   : DAW  (Decimal Adjust WREG)
 *   - Stack control     : PUSH, POP
 *   - Nibble swap       : SWAPF
 *   - Rotate-through-carry : RLCF, RRCF  (multi-byte shift building blocks)
 *   - Barrel rotate     : RLNCF, RRNCF
 *   - Table read/write  : TBLRD / TBLWT  (program-memory access via TBLPTR)
 *   - NVM unlock        : critical unlock sequence helper
 *   - Global interrupt  : enable / disable with save/restore
 *
 * Two toolchain families are supported:
 *   - XC8 (Microchip):  uses __asm("MNEMONIC") or built-in __nop() etc.
 *   - GCC / Clang:      uses __asm volatile("MNEMONIC")
 *
 * Reference: Microchip DS40002213D, Chapter 47 - Instruction Set Summary
 */

#ifndef PIC18F_Q84_INTRINSICS_H
#define PIC18F_Q84_INTRINSICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Toolchain abstraction for inline assembly
 * ========================================================================= */

#if defined(__XC8) || defined(__XC)
/* Microchip XC8 compiler - targeting PIC18 hardware */
#define _PIC18_ASM(instr) __asm(instr)
#define _PIC18_TARGET 1
#elif defined(__GNUC__) && defined(__pic18)
/* GCC targeting PIC18 (pic-none-elf or similar) */
#define _PIC18_ASM(instr) __asm volatile(instr)
#define _PIC18_TARGET 1
#elif defined(__GNUC__) || defined(__clang__)
/* GCC or Clang on a non-PIC18 host (for syntax checking / unit tests) */
#define _PIC18_ASM(instr) /* stripped on non-PIC18 host */
#define _PIC18_TARGET 0
#else
#error "Unsupported compiler - define _PIC18_ASM for your toolchain"
#endif

/* =========================================================================
 * 1. RESET - Software Device Reset
 * =========================================================================
 *
 * Executes the RESET instruction, which forces a complete device reset
 * identical to an external MCLR or POR event.  All registers are restored
 * to their Reset default values, the PC is loaded from the Reset vector,
 * and the stack is cleared.
 *
 * The PCON0.RI flag will be cleared (= 0) after a software reset,
 * allowing firmware to distinguish this from other reset sources.
 *
 * This function never returns.
 *
 * Encoding: 0000 0000 1111 1111
 * Cycles:   1
 * Affects:  All registers (full device reset)
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn))
#endif
static inline void pic18_reset(void) {
    _PIC18_ASM("RESET");
    /* Unreachable - silence compiler warnings */
    for (;;) {}
}

/* =========================================================================
 * 2. SLEEP - Enter Low-Power Standby Mode
 * =========================================================================
 *
 * Puts the device into Sleep mode (or Idle mode if CPUDOZE.IDLEN = 1).
 * In Sleep: CPU halted, most clocks stopped, peripherals with independent
 * clocks may continue.  Device wakes on interrupt, WDT timeout, or Reset.
 *
 * Before executing SLEEP:
 *   - Clear the WDT (CLRWDT) for a full WDT timeout period in Sleep
 *   - Configure wake-up sources (IOC, INT, WDT, etc.)
 *   - Optionally set IDLEN for Idle mode (peripherals keep running)
 *
 * STATUS effects:
 *   TO  = 1 (set on entering Sleep; cleared if WDT wakes the device)
 *   PD  = 0 (cleared on entering Sleep; set on power-up)
 *
 * Encoding: 0000 0000 0000 0011
 * Cycles:   1
 */
static inline void pic18_sleep(void) {
    _PIC18_ASM("SLEEP");
}

/* =========================================================================
 * 3. CLRWDT - Clear Watchdog Timer
 * =========================================================================
 *
 * Resets the Windowed Watchdog Timer (WWDT) prescaler and postscaler.
 * Must be called periodically within the watchdog window to prevent a
 * WDT timeout reset.
 *
 * IMPORTANT: In Windowed WDT mode, calling CLRWDT outside the allowed
 *            window (too early) causes a RESET, not a clear.
 *            Check WDTTMR.STATE to determine if in the window.
 *
 * STATUS effects:
 *   TO = 1 (set)
 *   PD = 1 (set)
 *
 * Encoding: 0000 0000 0000 0100
 * Cycles:   1
 */
static inline void pic18_clrwdt(void) {
    _PIC18_ASM("CLRWDT");
}

/* =========================================================================
 * 4. NOP - No Operation
 * =========================================================================
 *
 * Executes one instruction cycle with no effect.  Useful for:
 *   - Precise timing delays (each NOP = 4 oscillator periods = 62.5 ns @ 64 MHz)
 *   - Pipeline synchronization after SFR writes
 *   - Padding branch targets for alignment
 *
 * Encoding: 0000 0000 0000 0000   (also: 1111 xxxx xxxx xxxx)
 * Cycles:   1
 */
static inline void pic18_nop(void) {
    _PIC18_ASM("NOP");
}

/**
 * Execute a specific number of NOP instructions for short delays.
 * Each NOP = 1 instruction cycle = 4/FOSC seconds.
 * At 64 MHz: 1 NOP = 62.5 ns.
 *
 * @param n  Number of NOPs (evaluated at compile time for small constants)
 */
#define pic18_delay_cycles(n)                                                                                          \
    do {                                                                                                               \
        for (unsigned _i = 0; _i < (n); _i++) _PIC18_ASM("NOP");                                                       \
    } while (0)

/* =========================================================================
 * 5. DAW - Decimal Adjust WREG
 * =========================================================================
 *
 * Adjusts the 8-bit value in WREG after a BCD (Binary-Coded Decimal)
 * addition.  If the lower nibble is > 9 or the DC (Digit Carry) flag
 * is set, 6 is added to the lower nibble.  If the upper nibble is > 9
 * or the C (Carry) flag is set, 6 is added to the upper nibble.
 *
 * Use after ADDWF or ADDLW when operands are valid BCD values (0x00-0x99).
 *
 * Example:
 *   WREG = 0x45, add 0x38 -> WREG = 0x7D (raw binary)
 *   DAW  -> WREG = 0x83 (correct BCD: 45 + 38 = 83)
 *
 * Encoding: 0000 0000 0000 0111
 * Cycles:   1
 * Affects:  C (Carry)
 *
 * @param val  The value to BCD-adjust (must be result of a BCD addition)
 * @return     BCD-corrected result
 */
static inline uint8_t pic18_daw(uint8_t val) {
#if _PIC18_TARGET
    volatile uint8_t tmp = val;
    _PIC18_ASM("MOVF  _tmp, 0, 0"); /* WREG = tmp                      */
    _PIC18_ASM("DAW");              /* Decimal-adjust WREG             */
    _PIC18_ASM("MOVWF _tmp, 0");    /* tmp = adjusted result           */
    return tmp;
#else
    uint8_t r = val;
    if ((r & 0x0Fu) > 9u) {
        r += 0x06u;
    }
    if ((r >> 4) > 9u) {
        r += 0x60u;
    }
    return r;
#endif
}

/* =========================================================================
 * 6. PUSH - Push Top-of-Stack
 * =========================================================================
 *
 * Pushes the current PC+2 value onto the hardware return stack and
 * increments the stack pointer (STKPTR).  Does NOT modify the PC.
 *
 * Useful for:
 *   - Reserving a stack slot before a computed GOTO
 *   - Saving a return address for later RETURN
 *   - Stack depth management
 *
 * Warning: The hardware stack is 128 levels deep.  Overflow sets PCON0.STKOVF
 *          and may cause a reset (if STVREN config bit is set).
 *
 * Encoding: 0000 0000 0000 0101
 * Cycles:   1
 */
static inline void pic18_push(void) {
    _PIC18_ASM("PUSH");
}

/* =========================================================================
 * 7. POP - Pop Top-of-Stack
 * =========================================================================
 *
 * Discards the top entry of the hardware return stack and decrements the
 * stack pointer (STKPTR).  Does NOT modify the PC.
 *
 * Useful for:
 *   - Unwinding a stack frame after PUSH without returning
 *   - Cleaning up after a software exception
 *   - Adjusting stack depth for longjmp-like patterns
 *
 * Warning: Underflow sets PCON0.STKUNF and may cause a reset.
 *
 * Encoding: 0000 0000 0000 0110
 * Cycles:   1
 */
static inline void pic18_pop(void) {
    _PIC18_ASM("POP");
}

/* =========================================================================
 * 8. SWAPF - Swap Nibbles
 * =========================================================================
 *
 * Exchanges the upper and lower nibbles (4-bit halves) of a byte.
 * Equivalent to: ((val >> 4) | (val << 4)) & 0xFF, but executes in a
 * single instruction cycle.
 *
 * Critical property: SWAPF does NOT affect any STATUS flags.  This makes
 * it essential for context save/restore in interrupt handlers where the
 * STATUS register must be saved without altering flags.
 *
 * Encoding: 0011 10da ffff ffff
 * Cycles:   1
 * Affects:  None (no STATUS flags modified)
 *
 * @param val  Byte to swap
 * @return     Byte with nibbles exchanged (0xAB -> 0xBA)
 */
static inline uint8_t pic18_swapf(uint8_t val) {
#if _PIC18_TARGET
    volatile uint8_t tmp = val;
    _PIC18_ASM("SWAPF _tmp, 1, 0"); /* Swap nibbles of tmp in place    */
    return tmp;
#else
    return (uint8_t)((val >> 4) | (val << 4));
#endif
}

/* =========================================================================
 * 9. RLCF / RRCF - Rotate Through Carry
 * =========================================================================
 *
 * These rotate a byte left or right through the Carry flag, giving a
 * 9-bit rotation (8 data bits + C).  Essential building blocks for
 * multi-byte (16-bit, 24-bit, 32-bit) shift operations.
 *
 * RLCF: C <- [b7 b6 b5 b4 b3 b2 b1 b0] <- C   (rotate left through C)
 * RRCF: C -> [b7 b6 b5 b4 b3 b2 b1 b0] -> C   (rotate right through C)
 *
 * Encoding RLCF: 0011 01da ffff ffff
 * Encoding RRCF: 0011 00da ffff ffff
 * Cycles:   1 each
 * Affects:  C, Z, N
 *
 * Note: These operate on a memory-resident byte via the actual PIC18
 *       rotate instructions.  The carry flag is part of the rotation
 *       and is read/written in the hardware STATUS register.
 *       To set carry before calling, use:  STATUSbits.C = 1;
 *       To read carry after calling, use:  if (STATUSbits.C) ...
 */

/** Rotate left through carry: C <- [b7..b0] <- C */
static inline uint8_t pic18_rlcf(uint8_t val) {
#if _PIC18_TARGET
    volatile uint8_t tmp = val;
    _PIC18_ASM("RLCF _tmp, 1, 0"); /* Rotate tmp left through C       */
    return tmp;
#else
    return (uint8_t)(val << 1); /* Approximation without carry     */
#endif
}

/** Rotate right through carry: C -> [b7..b0] -> C */
static inline uint8_t pic18_rrcf(uint8_t val) {
#if _PIC18_TARGET
    volatile uint8_t tmp = val;
    _PIC18_ASM("RRCF _tmp, 1, 0"); /* Rotate tmp right through C      */
    return tmp;
#else
    return (uint8_t)(val >> 1); /* Approximation without carry     */
#endif
}

/* =========================================================================
 * 10. RLNCF / RRNCF - Rotate Without Carry (Barrel Rotate)
 * =========================================================================
 *
 * Rotate a byte left or right by 1 bit position without involving the
 * Carry flag.  The bit shifted out of one end re-enters at the other.
 *
 * RLNCF: [b6 b5 b4 b3 b2 b1 b0 b7]   (b7 wraps to b0)
 * RRNCF: [b0 b7 b6 b5 b4 b3 b2 b1]   (b0 wraps to b7)
 *
 * Encoding RLNCF: 0100 01da ffff ffff
 * Encoding RRNCF: 0100 00da ffff ffff
 * Cycles:   1 each
 * Affects:  Z, N  (Carry is NOT affected)
 *
 * @param val  Byte to rotate
 * @return     Rotated byte
 */
static inline uint8_t pic18_rlncf(uint8_t val) {
#if _PIC18_TARGET
    volatile uint8_t tmp = val;
    _PIC18_ASM("RLNCF _tmp, 1, 0"); /* Rotate tmp left, no carry       */
    return tmp;
#else
    return (uint8_t)((val << 1) | (val >> 7));
#endif
}

static inline uint8_t pic18_rrncf(uint8_t val) {
#if _PIC18_TARGET
    volatile uint8_t tmp = val;
    _PIC18_ASM("RRNCF _tmp, 1, 0"); /* Rotate tmp right, no carry      */
    return tmp;
#else
    return (uint8_t)((val >> 1) | (val << 7));
#endif
}

/* =========================================================================
 * 11. Table Read / Write - Program Memory Access via TBLPTR
 * =========================================================================
 *
 * The TBLRD and TBLWT instructions transfer data between the 8-bit TABLAT
 * register and program memory (PFM) at the address pointed to by the
 * 22-bit TBLPTR register.
 *
 * Four addressing modes each:
 *   *    No change to TBLPTR after access
 *   *+   Post-increment TBLPTR
 *   *-   Post-decrement TBLPTR
 *   +*   Pre-increment TBLPTR
 *
 * TBLPTR[0] selects the byte within a 16-bit PFM word:
 *   0 = least significant byte
 *   1 = most significant byte
 *
 * Encoding TBLRD: 0000 0000 0000 10mm  (mm=0:*, 1:*+, 2:*-, 3:+*)
 * Encoding TBLWT: 0000 0000 0000 11mm
 * Cycles:   2 each
 * Affects:  None (TABLAT loaded/stored, TBLPTR optionally modified)
 */

/* Register definitions used by table operations (from pic18f_q84_cpu.h) */
#ifndef TABLAT_ADDR
#define TABLAT_ADDR 0x04F5
#define TBLPTRL_ADDR 0x04F6
#define TBLPTRH_ADDR 0x04F7
#define TBLPTRU_ADDR 0x04F8
#endif

/** Set the 22-bit TBLPTR to an absolute program memory address */
static inline void pic18_tblptr_set(uint32_t addr) {
    *((volatile uint8_t*)TBLPTRL_ADDR) = (uint8_t)(addr);
    *((volatile uint8_t*)TBLPTRH_ADDR) = (uint8_t)(addr >> 8);
    *((volatile uint8_t*)TBLPTRU_ADDR) = (uint8_t)(addr >> 16);
}

/** Read the current 22-bit TBLPTR value */
static inline uint32_t pic18_tblptr_get(void) {
    uint32_t addr;
    addr = *((volatile uint8_t*)TBLPTRL_ADDR);
    addr |= (uint32_t)(*((volatile uint8_t*)TBLPTRH_ADDR)) << 8;
    addr |= (uint32_t)(*((volatile uint8_t*)TBLPTRU_ADDR)) << 16;
    return addr;
}

/** TBLRD * : Read byte at TBLPTR, pointer unchanged */
static inline uint8_t pic18_tblrd(void) {
    _PIC18_ASM("TBLRD*");
    return *((volatile uint8_t*)TABLAT_ADDR);
}

/** TBLRD *+ : Read byte at TBLPTR, then increment pointer */
static inline uint8_t pic18_tblrd_postinc(void) {
    _PIC18_ASM("TBLRD*+");
    return *((volatile uint8_t*)TABLAT_ADDR);
}

/** TBLRD *- : Read byte at TBLPTR, then decrement pointer */
static inline uint8_t pic18_tblrd_postdec(void) {
    _PIC18_ASM("TBLRD*-");
    return *((volatile uint8_t*)TABLAT_ADDR);
}

/** TBLRD +* : Increment pointer, then read byte at new TBLPTR */
static inline uint8_t pic18_tblrd_preinc(void) {
    _PIC18_ASM("TBLRD+*");
    return *((volatile uint8_t*)TABLAT_ADDR);
}

/** TBLWT * : Write TABLAT to TBLPTR address, pointer unchanged */
static inline void pic18_tblwt(uint8_t data) {
    *((volatile uint8_t*)TABLAT_ADDR) = data;
    _PIC18_ASM("TBLWT*");
}

/** TBLWT *+ : Write TABLAT, then increment pointer */
static inline void pic18_tblwt_postinc(uint8_t data) {
    *((volatile uint8_t*)TABLAT_ADDR) = data;
    _PIC18_ASM("TBLWT*+");
}

/** TBLWT *- : Write TABLAT, then decrement pointer */
static inline void pic18_tblwt_postdec(uint8_t data) {
    *((volatile uint8_t*)TABLAT_ADDR) = data;
    _PIC18_ASM("TBLWT*-");
}

/** TBLWT +* : Increment pointer, then write TABLAT */
static inline void pic18_tblwt_preinc(uint8_t data) {
    *((volatile uint8_t*)TABLAT_ADDR) = data;
    _PIC18_ASM("TBLWT+*");
}

/**
 * Read a block of bytes from program memory into a RAM buffer.
 *
 * Uses TBLRD*+ (post-increment) for efficient sequential reads.
 *
 * @param addr  22-bit program memory start address
 * @param buf   Destination buffer in data RAM
 * @param len   Number of bytes to read
 */
static inline void pic18_flash_read(uint32_t addr, uint8_t* buf, uint16_t len) {
    pic18_tblptr_set(addr);
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = pic18_tblrd_postinc();
    }
}

/* =========================================================================
 * 12. Global Interrupt Enable / Disable Helpers
 * =========================================================================
 *
 * Safe interrupt disable/restore pattern using INTCON0.GIE.
 * The save/restore pair ensures interrupts are only re-enabled if they
 * were enabled before the critical section.
 *
 * Usage:
 *   uint8_t saved = pic18_interrupt_disable();
 *   // ... critical section ...
 *   pic18_interrupt_restore(saved);
 */

/**
 * Disable global interrupts and return the previous GIE state.
 * Uses BCF on INTCON0.GIE for atomic single-instruction disable.
 *
 * @return  Previous INTCON0 value (test bit 7 for prior GIE state)
 */
static inline uint8_t pic18_interrupt_disable(void) {
    /* INTCON0 is at 0x04D6, GIE is bit 7 */
    volatile uint8_t* intcon0 = (volatile uint8_t*)0x04D6;
    uint8_t saved = *intcon0;
    *intcon0 &= ~(1u << 7); /* Clear GIE - compiler emits BCF INTCON0,7 */
    return saved;
}

/**
 * Restore global interrupt state from a previous pic18_interrupt_disable().
 *
 * @param saved  Value returned by pic18_interrupt_disable()
 */
static inline void pic18_interrupt_restore(uint8_t saved) {
    volatile uint8_t* intcon0 = (volatile uint8_t*)0x04D6;
    if (saved & (1u << 7)) {
        *intcon0 |= (1u << 7); /* Re-enable GIE only if it was set before */
    }
}

/** Unconditionally enable global interrupts (set GIE) */
static inline void pic18_interrupt_enable(void) {
    volatile uint8_t* intcon0 = (volatile uint8_t*)0x04D6;
    *intcon0 |= (1u << 7);
}

/* =========================================================================
 * 14. NVM Unlock Sequence
 * =========================================================================
 *
 * The NVM (Flash/EEPROM) write and erase operations require a specific
 * unlock sequence for safety.  This must execute without interruption.
 *
 * Sequence:
 *   1. Disable interrupts
 *   2. Write 0x55 to NVMLOCK
 *   3. Write 0xAA to NVMLOCK
 *   4. Set NVMCON0.GO = 1
 *   5. Wait for GO to clear (operation complete)
 *   6. Restore interrupts
 *
 * This is not an instruction intrinsic per se, but it is a critical
 * chip-specific code sequence that must be executed atomically and
 * cannot be expressed as normal C operations (the compiler must not
 * reorder the writes).
 */

/**
 * Execute the NVM unlock sequence and start the pending operation.
 * Disables interrupts during the sequence and restores them after.
 *
 * Prerequisites:
 *   - NVMADR loaded with target address
 *   - NVMCON1.NVMCMD set to desired operation
 *   - For writes: NVMDAT or buffer RAM loaded with data
 *
 * On return:
 *   - NVMCON0.GO has been set (operation started)
 *   - For blocking operations, waits until GO clears
 *   - Check NVMCON1.WRERR for errors
 */
static inline void pic18_nvm_unlock_and_go(void) {
    volatile uint8_t* nvmlock = (volatile uint8_t*)0x0042;
    volatile uint8_t* nvmcon0 = (volatile uint8_t*)0x0040;

    uint8_t gie_saved = pic18_interrupt_disable();

    /* Unlock sequence - writes must not be reordered */
    *nvmlock = 0x55;
    *nvmlock = 0xAA;
    *nvmcon0 |= (1u << 7); /* Set GO bit to start operation */

    /* Wait for hardware to complete the operation */
    while (*nvmcon0 & (1u << 7));

    pic18_interrupt_restore(gie_saved);
}

/* =========================================================================
 * 15. CALLW - Call Subroutine Using WREG
 * =========================================================================
 *
 * CALLW pushes the return address (PC+2) onto the hardware stack, then
 * loads the PC with {PCLATU:PCLATH:WREG}.  This enables computed calls
 * where the low byte of the target address is passed in WREG at runtime.
 *
 * Primarily used by compilers for jump tables and function pointers.
 * Provided here for completeness; direct use in C is rare.
 *
 * Encoding: 0000 0000 0001 0100
 * Cycles:   2
 * Affects:  None (pushes return address onto stack)
 */
static inline void pic18_callw(void) {
    _PIC18_ASM("CALLW");
}

/* =========================================================================
 * 16. EEPROM (Data Flash Memory) Read / Write API
 * =========================================================================
 *
 * The PIC18F-Q84 contains 1024 bytes of Data EEPROM (called "Data Flash
 * Memory" or DFM in the datasheet).  It is mapped at absolute address
 * 0x380000-0x3803FF in the unified program memory space.
 *
 * Key characteristics:
 *   - Single-byte read and write only (no page operations)
 *   - Write includes an implicit erase (erase-before-write)
 *   - Write requires the NVM unlock sequence (0x55 / 0xAA)
 *   - Write time is controlled by an internal timer (~4 ms typical)
 *   - High endurance: 100,000 erase/write cycles minimum
 *   - Data retained for 40+ years
 *
 * The NVM registers used:
 *   NVMADR  (0x0043, 3 bytes) - target address in unified memory map
 *   NVMDATL (0x0046)          - single-byte data for DFM operations
 *   NVMCON0 (0x0040)          - GO bit starts operation
 *   NVMCON1 (0x0041)          - NVMCMD selects operation, WRERR flag
 *   NVMLOCK (0x0042)          - unlock register
 *
 * Reference: DS40002213D, Section 10.4 - Data Flash Memory (DFM)
 */

/** Base address of EEPROM in the unified 22-bit NVM address space */
#define EEPROM_BASE_ADDR 0x380000UL

/** Size of EEPROM in bytes (all PIC18F-Q84 variants) */
#define EEPROM_SIZE 1024u

/** Maximum valid EEPROM offset (0 to EEPROM_SIZE - 1) */
#define EEPROM_MAX_OFFSET (EEPROM_SIZE - 1u)

/* NVM register addresses (duplicated here to keep this header self-contained
 * when used without pic18f_q84_nvm.h; values are identical) */
#ifndef NVMCON0_GO
#define _EEPROM_NVMCON0 (*(volatile uint8_t*)0x0040)
#define _EEPROM_NVMCON1 (*(volatile uint8_t*)0x0041)
#define _EEPROM_NVMLOCK (*(volatile uint8_t*)0x0042)
#define _EEPROM_NVMADRL (*(volatile uint8_t*)0x0043)
#define _EEPROM_NVMADRH (*(volatile uint8_t*)0x0044)
#define _EEPROM_NVMADRU (*(volatile uint8_t*)0x0045)
#define _EEPROM_NVMDATL (*(volatile uint8_t*)0x0046)
#define _EEPROM_INTCON0 (*(volatile uint8_t*)0x04D6)
#else
#define _EEPROM_NVMCON0 NVMCON0
#define _EEPROM_NVMCON1 NVMCON1
#define _EEPROM_NVMLOCK NVMLOCK
#define _EEPROM_NVMADRL NVMADRL
#define _EEPROM_NVMADRH NVMADRH
#define _EEPROM_NVMADRU NVMADRU
#define _EEPROM_NVMDATL NVMDATL
#define _EEPROM_INTCON0 (*(volatile uint8_t*)0x04D6)
#endif

/**
 * Read a single byte from EEPROM.
 *
 * Sequence (from DS40002213D, Example 10-9):
 *   1. Load NVMADR with DFM address (base + offset)
 *   2. Set NVMCMD = 0x00 (byte read)
 *   3. Set GO = 1
 *   4. Wait for GO to clear (immediate for reads)
 *   5. Read NVMDATL
 *
 * @param offset  Byte offset within EEPROM (0 to 1023)
 * @return        The byte stored at that EEPROM location
 */
static inline uint8_t eeprom_read(uint16_t offset) {
    uint32_t addr = EEPROM_BASE_ADDR + (uint32_t)offset;

    /* Load 22-bit NVM address */
    _EEPROM_NVMADRL = (uint8_t)(addr);
    _EEPROM_NVMADRH = (uint8_t)(addr >> 8);
    _EEPROM_NVMADRU = (uint8_t)(addr >> 16);

    /* Select byte-read command: NVMCMD = 0b000 */
    _EEPROM_NVMCON1 = (_EEPROM_NVMCON1 & 0xF8u) | 0x00u;

    /* Start read operation */
    _EEPROM_NVMCON0 |= (1u << 7); /* GO = 1 */

    /* Wait for completion (typically immediate for reads) */
    while (_EEPROM_NVMCON0 & (1u << 7));

    return _EEPROM_NVMDATL;
}

/**
 * Write a single byte to EEPROM.
 *
 * This function:
 *   - Disables interrupts during the unlock sequence (required)
 *   - Performs the mandatory 0x55/0xAA unlock
 *   - Blocks until the write completes (~4 ms typical)
 *   - Restores the previous interrupt state
 *   - Clears NVMCMD to prevent accidental writes
 *
 * Each byte write includes an implicit erase of that location;
 * no separate erase step is needed.
 *
 * Sequence (from DS40002213D, Example 10-10):
 *   1. Load NVMADR with DFM address
 *   2. Load NVMDATL with data
 *   3. Set NVMCMD = 0x03 (byte write)
 *   4. Disable interrupts
 *   5. Unlock: NVMLOCK = 0x55; NVMLOCK = 0xAA
 *   6. Set GO = 1
 *   7. Wait for GO to clear
 *   8. Restore interrupts
 *   9. Clear NVMCMD
 *
 * @param offset  Byte offset within EEPROM (0 to 1023)
 * @param data    Byte value to write
 * @return        0 on success, 1 if a write error occurred (WRERR set)
 */
static inline uint8_t eeprom_write(uint16_t offset, uint8_t data) {
    uint32_t addr = EEPROM_BASE_ADDR + (uint32_t)offset;

    /* Load 22-bit NVM address */
    _EEPROM_NVMADRL = (uint8_t)(addr);
    _EEPROM_NVMADRH = (uint8_t)(addr >> 8);
    _EEPROM_NVMADRU = (uint8_t)(addr >> 16);

    /* Load data byte */
    _EEPROM_NVMDATL = data;

    /* Select byte-write command: NVMCMD = 0b011 */
    _EEPROM_NVMCON1 = (_EEPROM_NVMCON1 & 0xF8u) | 0x03u;

    /* Save and disable global interrupts (unlock sequence must not
     * be interrupted or the write will silently fail) */
    uint8_t gie_saved = _EEPROM_INTCON0;
    _EEPROM_INTCON0 &= ~(1u << 7); /* GIE = 0 */

    /* === Required NVM unlock sequence === */
    _EEPROM_NVMLOCK = 0x55;
    _EEPROM_NVMLOCK = 0xAA;
    _EEPROM_NVMCON0 |= (1u << 7); /* GO = 1 - start write */

    /* Wait for the internal programming timer to complete the write */
    while (_EEPROM_NVMCON0 & (1u << 7));

    /* Restore interrupt state */
    if (gie_saved & (1u << 7)) {
        _EEPROM_INTCON0 |= (1u << 7);
    }

    /* Check for write error (write-protected or invalid address) */
    uint8_t err = (_EEPROM_NVMCON1 >> 6) & 1u; /* WRERR is bit 6 */

    /* Clear command to prevent accidental writes */
    _EEPROM_NVMCON1 &= 0xF8u; /* NVMCMD = 0b000 */

    return err;
}

/**
 * Read a block of bytes from EEPROM into a RAM buffer.
 *
 * Uses the NVM read-with-post-increment command (NVMCMD = 0x01) for
 * efficient sequential access.  NVMADR auto-increments after each byte.
 *
 * @param offset  Starting byte offset within EEPROM (0 to 1023)
 * @param buf     Destination buffer in data RAM
 * @param len     Number of bytes to read (offset + len must not exceed 1024)
 */
static inline void eeprom_read_block(uint16_t offset, uint8_t* buf, uint16_t len) {
    uint32_t addr = EEPROM_BASE_ADDR + (uint32_t)offset;

    _EEPROM_NVMADRL = (uint8_t)(addr);
    _EEPROM_NVMADRH = (uint8_t)(addr >> 8);
    _EEPROM_NVMADRU = (uint8_t)(addr >> 16);

    /* Select read-with-post-increment: NVMCMD = 0b001 */
    _EEPROM_NVMCON1 = (_EEPROM_NVMCON1 & 0xF8u) | 0x01u;

    for (uint16_t i = 0; i < len; i++) {
        _EEPROM_NVMCON0 |= (1u << 7); /* GO = 1 */
        while (_EEPROM_NVMCON0 & (1u << 7));
        buf[i] = _EEPROM_NVMDATL;
    }

    _EEPROM_NVMCON1 &= 0xF8u; /* Clear NVMCMD */
}

/**
 * Write a block of bytes from a RAM buffer to EEPROM.
 *
 * Uses the NVM write-with-post-increment command (NVMCMD = 0x04) for
 * sequential writes.  Each byte write takes ~4 ms; a full 1024-byte
 * write takes approximately 4 seconds.
 *
 * Interrupts are disabled for the entire block to keep the unlock
 * sequence valid across all bytes.
 *
 * @param offset  Starting byte offset within EEPROM (0 to 1023)
 * @param buf     Source buffer in data RAM
 * @param len     Number of bytes to write (offset + len must not exceed 1024)
 * @return        0 on success, 1 if a write error occurred
 */
static inline uint8_t eeprom_write_block(uint16_t offset, const uint8_t* buf, uint16_t len) {
    uint32_t addr = EEPROM_BASE_ADDR + (uint32_t)offset;

    _EEPROM_NVMADRL = (uint8_t)(addr);
    _EEPROM_NVMADRH = (uint8_t)(addr >> 8);
    _EEPROM_NVMADRU = (uint8_t)(addr >> 16);

    /* Select write-with-post-increment: NVMCMD = 0b100 */
    _EEPROM_NVMCON1 = (_EEPROM_NVMCON1 & 0xF8u) | 0x04u;

    /* Save and disable global interrupts */
    uint8_t gie_saved = _EEPROM_INTCON0;
    _EEPROM_INTCON0 &= ~(1u << 7);

    for (uint16_t i = 0; i < len; i++) {
        _EEPROM_NVMDATL = buf[i];

        /* Unlock and go for each byte */
        _EEPROM_NVMLOCK = 0x55;
        _EEPROM_NVMLOCK = 0xAA;
        _EEPROM_NVMCON0 |= (1u << 7);

        while (_EEPROM_NVMCON0 & (1u << 7));
    }

    /* Restore interrupts */
    if (gie_saved & (1u << 7)) {
        _EEPROM_INTCON0 |= (1u << 7);
    }

    /* Check for errors */
    uint8_t err = (_EEPROM_NVMCON1 >> 6) & 1u;

    /* Clear command */
    _EEPROM_NVMCON1 &= 0xF8u;

    return err;
}

/**
 * Erase a range of EEPROM bytes (set to 0xFF).
 *
 * The DFM does not support page erase; this writes 0xFF to each
 * location individually using write-with-post-increment.
 *
 * @param offset  Starting byte offset (0 to 1023)
 * @param len     Number of bytes to erase
 * @return        0 on success, 1 if a write error occurred
 */
static inline uint8_t eeprom_erase(uint16_t offset, uint16_t len) {
    uint32_t addr = EEPROM_BASE_ADDR + (uint32_t)offset;

    _EEPROM_NVMADRL = (uint8_t)(addr);
    _EEPROM_NVMADRH = (uint8_t)(addr >> 8);
    _EEPROM_NVMADRU = (uint8_t)(addr >> 16);

    _EEPROM_NVMCON1 = (_EEPROM_NVMCON1 & 0xF8u) | 0x04u;

    uint8_t gie_saved = _EEPROM_INTCON0;
    _EEPROM_INTCON0 &= ~(1u << 7);

    _EEPROM_NVMDATL = 0xFF;

    for (uint16_t i = 0; i < len; i++) {
        _EEPROM_NVMLOCK = 0x55;
        _EEPROM_NVMLOCK = 0xAA;
        _EEPROM_NVMCON0 |= (1u << 7);

        while (_EEPROM_NVMCON0 & (1u << 7));
    }

    if (gie_saved & (1u << 7)) {
        _EEPROM_INTCON0 |= (1u << 7);
    }

    uint8_t err = (_EEPROM_NVMCON1 >> 6) & 1u;
    _EEPROM_NVMCON1 &= 0xF8u;

    return err;
}

#ifdef __cplusplus
}
#endif

#endif /* PIC18F_Q84_INTRINSICS_H */
