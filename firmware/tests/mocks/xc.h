/*
 * xc.h — host-side stand-in for the XC8 compiler's device header.
 *
 * Placed ahead of the real one on the include path by the test Makefile, so
 * firmware sources compile unmodified against a desktop compiler.  It supplies
 * three things:
 *
 *   1. The XC8 language extensions (`__interrupt`, `__asm`, `__nop`,
 *      `__delay_us`, `__bit`, `__uint24`) as no-ops or ordinary types.
 *   2. Bare bit aliases the firmware uses without qualification, notably GIE.
 *   3. Every special function register, via the generated pic_sfr_mock.h.
 *
 * Nothing here tries to be a simulator.  Registers are memory; the only
 * peripheral behaviour modelled is what the firmware *blocks* on, which lives
 * in pic_mock.c.
 */

#ifndef MOCK_XC_H
#define MOCK_XC_H

/* Pulled in up front.  The macros below shadow identifiers the C library's
 * own headers use for symbol aliasing, so the library has to be fully
 * declared before they exist — otherwise a translation unit that includes
 * <xc.h> ahead of <stdlib.h> breaks in the SDK rather than in this file. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Language extensions ───────────────────────────────────────────────── */

/*
 * `void __interrupt(high_priority, irq(I2C1), base(8)) I2C1_ISR(void)`
 * becomes a plain function definition.  `high_priority`, `irq(...)` and
 * `base(...)` are macro *arguments*, so they never have to be defined —
 * consuming them here is what keeps them from leaking into the translation
 * unit as undeclared identifiers.
 */
#define __interrupt(...)

/* Inline assembly.
 *
 * `__asm` is deliberately left alone: the compiler's own spelling, which the
 * platform's headers use for symbol aliasing, and every firmware use of it is
 * `__asm("NOP")` — a mnemonic that happens to assemble on the host too, so it
 * costs nothing to let through.
 *
 * The lowercase form is not a keyword in C99 and is only used by adc.c, for
 * the two TBLRD variants that read the factory FVR calibration out of program
 * flash.  Routing those to pic_asm() is what keeps the ADC scaling testable
 * rather than dividing by an uncalibrated zero. */
#define asm(s) pic_asm(s)

/* XC8's reset builtin. */
#define RESET() pic_asm("RESET")

#define __nop() ((void)0)
#define __delay_us(x) ((void)(x))
#define __delay_ms(x) ((void)(x))

/* XC8's single-bit type.  Used by libcomm.h's INTERRUPT_POP cast. */
typedef unsigned char __bit;

/* XC8's 24-bit integer, and the uint24_t spelling the firmware prefers.
 * Defined as uintptr_t in pic_mock.h so pointer casts stay lossless. */
#include "pic_mock.h"
typedef uint24_t __uint24;

/* ── Registers ─────────────────────────────────────────────────────────── */

#include "pic_sfr_mock.h"

/* ── Bare bit aliases ──────────────────────────────────────────────────── */

/* XC8 exposes a handful of interrupt-control bits as unqualified names.
 * libcomm.h's INTERRUPT_PUSH / INTERRUPT_POP use GIE this way. */
#define GIE  INTCON0bits.GIE
#define GIEH INTCON0bits.GIEH
#define GIEL INTCON0bits.GIEL

#endif /* MOCK_XC_H */
