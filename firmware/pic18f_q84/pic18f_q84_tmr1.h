/*
 * pic18f_q84_tmr1.h
 *
 * 16-bit Timer modules with Gate Control (TMR1, TMR3, TMR5) registers
 * for PIC18F27/47/57Q84 family.
 *
 * TMR1, TMR3, and TMR5 are three identical 16-bit timer/counter modules.
 * Each provides:
 *   - 16-bit counter (TMRnH:TMRnL) with optional buffered read/write
 *   - Configurable prescaler (1:1, 1:2, 1:4, 1:8)
 *   - Selectable clock source via PPS-routable TnCLK register
 *   - Gate control (TnGCON/TnGATE) enabling conditional counting based
 *     on an external or internal signal, with polarity, toggle, and
 *     single-pulse modes
 *   - Synchronous or asynchronous external clock operation
 *
 * These timers are commonly used for period measurement, event counting,
 * time-base generation, and gated timing applications.
 *
 * Register map per timer instance:
 *   TMRnL   (base+0) - Timer count low byte
 *   TMRnH   (base+1) - Timer count high byte
 *   TnCON   (base+2) - Timer control
 *   TnGCON  (base+3) - Gate control
 *   TnGATE  (base+4) - Gate source select
 *   TnCLK   (base+5) - Clock source select
 *
 * Base addresses:
 *   TMR1 = 0x031C    TMR3 = 0x0328    TMR5 = 0x0334
 *
 * Covers:
 *   - TMR1L/H, T1CON, T1GCON, T1GATE, T1CLK
 *   - TMR3L/H, T3CON, T3GCON, T3GATE, T3CLK
 *   - TMR5L/H, T5CON, T5GCON, T5GATE, T5CLK
 *
 * Reference: DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 18 - Timer1/3/5 Module with Gate Control
 *
 * Requires: SFR8(addr) and SFR16(addr) macros defined externally
 *           (provide volatile uint8_t / uint16_t access to absolute
 *           SFR addresses).
 */

#ifndef PIC18F_Q84_TMR1_H
#define PIC18F_Q84_TMR1_H

/* =====================================================================
 *  TMR1 - 16-bit Timer1 with Gate Control
 *  Base address: 0x031C
 * ===================================================================== */

/* --------------------------------------------------------------------- */
/*  TMR1L - Timer1 Count Register Low Byte                               */
/*  Address: 0x031C                                                      */
/*                                                                       */
/*  Bits 7:0 - TMR1L[7:0]: Low byte of the 16-bit Timer1 counter.       */
/*    When RD16 = 1 (T1CON), reading TMR1L returns the low byte and      */
/*    simultaneously latches TMR1H into a buffer for a coherent 16-bit   */
/*    read.  Writing TMR1L transfers the previously written TMR1H        */
/*    buffer value into the actual high byte.                            */
/*    When RD16 = 0, TMR1L and TMR1H are accessed independently.        */
/* --------------------------------------------------------------------- */
#define TMR1L SFR8(0x031C)

/* --------------------------------------------------------------------- */
/*  TMR1H - Timer1 Count Register High Byte                              */
/*  Address: 0x031D                                                      */
/*                                                                       */
/*  Bits 7:0 - TMR1H[7:0]: High byte of the 16-bit Timer1 counter.      */
/*    When RD16 = 1, reads return the buffered value latched on the      */
/*    last TMR1L read; writes go to a buffer that is transferred to      */
/*    the actual high byte on the next TMR1L write.                      */
/* --------------------------------------------------------------------- */
#define TMR1H SFR8(0x031D)

/* 16-bit combined access to TMR1H:TMR1L (little-endian) */
#define TMR1 SFR16(0x031C)

/* --------------------------------------------------------------------- */
/*  T1CON - Timer1 Control Register                                      */
/*  Address: 0x031E                                                      */
/*                                                                       */
/*  Bit 7   - ON: Timer1 Enable                                         */
/*      1 = Timer1 is enabled and counting                               */
/*      0 = Timer1 is stopped; counter holds its value                   */
/*  Bit 6   : Unimplemented, read as 0                                  */
/*  Bit 5   - CKPS1: Prescaler Select bit 1                             */
/*  Bit 4   - CKPS0: Prescaler Select bit 0                             */
/*      CKPS[1:0] prescaler ratio:                                       */
/*      00 = 1:1 (no prescaling)                                         */
/*      01 = 1:2                                                         */
/*      10 = 1:4                                                         */
/*      11 = 1:8                                                         */
/*  Bit 3   : Unimplemented, read as 0                                  */
/*  Bit 2   - SYNC: External Clock Synchronization Control               */
/*      1 = Synchronize external clock input to system clock (FOSC).     */
/*          Required for reliable reading of the timer in synchronous    */
/*          mode.                                                        */
/*      0 = Do not synchronize external clock input.  Required when      */
/*          the timer must continue running during Sleep mode.           */
/*  Bit 1   - RD16: 16-bit Read/Write Mode Enable                       */
/*      1 = Enable 16-bit read/write of TMR1 in one operation.          */
/*          Reading TMR1L latches TMR1H into a buffer; writing TMR1L    */
/*          transfers the buffer to TMR1H simultaneously.               */
/*      0 = TMR1L and TMR1H are read/written as independent 8-bit       */
/*          registers (risk of incoherent reads during rollover).       */
/*  Bit 0   : Unimplemented, read as 0                                  */
/* --------------------------------------------------------------------- */
#define T1CON SFR8(0x031E)

/* ON - Timer1 Enable (bit 7) */
#define T1CON_ON (1u << 7)

/* CKPS - Prescaler Select (bits 5:4) */
#define T1CON_CKPS1 (1u << 5) /* CKPS bit 1 */
#define T1CON_CKPS0 (1u << 4) /* CKPS bit 0 */
#define T1CON_CKPS_MASK 0x30u /* Mask for CKPS[1:0] field */
#define T1CON_CKPS_SHIFT 4u   /* Bit position of CKPS[0] */

/* SYNC - External Clock Synchronization (bit 2) */
#define T1CON_SYNC (1u << 2)

/* RD16 - 16-bit Read/Write Mode Enable (bit 1) */
#define T1CON_RD16 (1u << 1)

/* --------------------------------------------------------------------- */
/*  T1GCON - Timer1 Gate Control Register                                */
/*  Address: 0x031F                                                      */
/*                                                                       */
/*  Bit 7   - GE: Timer Gate Enable                                     */
/*      1 = Timer1 counts only when the gate input is active             */
/*          (after polarity selection).                                   */
/*      0 = Timer1 counts regardless of gate input.                     */
/*  Bit 6   - GPOL: Gate Polarity                                       */
/*      1 = Timer counts when gate signal is high (active-high)          */
/*      0 = Timer counts when gate signal is low  (active-low)           */
/*  Bit 5   - GTM: Gate Toggle Mode                                     */
/*      1 = Gate flip-flop toggles on each rising edge of the gate       */
/*          input, creating a toggle-based gating signal.                */
/*      0 = Gate flip-flop follows the gate input signal directly.       */
/*  Bit 4   - GSPM: Gate Single-Pulse Mode                              */
/*      1 = Gate allows exactly one complete timer count cycle, then     */
/*          stops (self-clearing gate).  Useful for single-shot          */
/*          period measurement.                                          */
/*      0 = Gate operates normally (continuous gating).                  */
/*  Bit 3   - GGO/DONE: Gate Single-Pulse Trigger / Status              */
/*      Write 1 = Arm the single-pulse gate (begin waiting for gate      */
/*                active edge).                                          */
/*      Read  1 = Single-pulse acquisition is in progress.              */
/*      Read  0 = Single-pulse acquisition is complete (or not armed).   */
/*      Cleared by hardware when the single pulse is complete.           */
/*  Bit 2   - GVAL: Gate Current Value (read-only)                      */
/*      Reflects the current state of the gate input after polarity      */
/*      and toggle processing.  1 = gate is active (timer counting);     */
/*      0 = gate is inactive (timer paused, if GE = 1).                 */
/*  Bits 1:0: Unimplemented, read as 0                                  */
/* --------------------------------------------------------------------- */
#define T1GCON SFR8(0x031F)

/* GE - Gate Enable (bit 7) */
#define T1GCON_GE (1u << 7)

/* GPOL - Gate Polarity (bit 6) */
#define T1GCON_GPOL (1u << 6)

/* GTM - Gate Toggle Mode (bit 5) */
#define T1GCON_GTM (1u << 5)

/* GSPM - Gate Single-Pulse Mode (bit 4) */
#define T1GCON_GSPM (1u << 4)

/* GGO_DONE - Gate Single-Pulse Trigger/Status (bit 3) */
#define T1GCON_GGO_DONE (1u << 3)

/* GVAL - Gate Value, read-only (bit 2) */
#define T1GCON_GVAL (1u << 2)

/* --------------------------------------------------------------------- */
/*  T1GATE - Timer1 Gate Source Select Register                          */
/*  Address: 0x0320                                                      */
/*                                                                       */
/*  Bits 7:6: Unimplemented, read as 0                                  */
/*  Bits 5:0 - GSS[5:0]: Gate Source Select                              */
/*      Selects the signal that controls the Timer1 gate input.          */
/*      The source is routed through GPOL polarity control before        */
/*      reaching the gate logic.  Specific source assignments are        */
/*      device- and pin-dependent; see the datasheet gate source table.  */
/* --------------------------------------------------------------------- */
#define T1GATE SFR8(0x0320)

/* GSS - Gate Source Select (bits 5:0) */
#define T1GATE_GSS_MASK 0x3Fu /* Mask for GSS[5:0] field */

/* --------------------------------------------------------------------- */
/*  T1CLK - Timer1 Clock Source Select Register                          */
/*  Address: 0x0321                                                      */
/*                                                                       */
/*  Bits 7:5: Unimplemented, read as 0                                  */
/*  Bits 4:0 - CS[4:0]: Clock Source Select                              */
/*      Selects the input clock for Timer1.  Specific source             */
/*      assignments are device-dependent; see the datasheet clock        */
/*      source table.  Common selections include:                        */
/*        00000 = T1CKIPPS pin (external clock via PPS)                  */
/*        00001 = FOSC/4 (instruction clock)                             */
/*        00010 = FOSC (system clock)                                    */
/*        00011 = HFINTOSC                                               */
/*        00100 = LFINTOSC                                               */
/*        00101 = MFINTOSC 500 kHz                                       */
/*        00110 = MFINTOSC 31.25 kHz                                     */
/*        00111 = SOSC                                                   */
/* --------------------------------------------------------------------- */
#define T1CLK SFR8(0x0321)

/* CS - Clock Source Select (bits 4:0) */
#define T1CLK_CS_MASK 0x1Fu /* Mask for CS[4:0] field */

/* =====================================================================
 *  TMR3 - 16-bit Timer3 with Gate Control
 *  Base address: 0x0328
 *
 *  Timer3 is functionally identical to Timer1.
 *  Register layout: TMR3L, TMR3H, T3CON, T3GCON, T3GATE, T3CLK.
 * ===================================================================== */

/* --------------------------------------------------------------------- */
/*  TMR3L - Timer3 Count Register Low Byte                               */
/*  Address: 0x0328                                                      */
/*  Bits 7:0 - TMR3L[7:0]: Low byte of 16-bit Timer3 counter.           */
/*    Buffered 16-bit access behavior identical to TMR1L (see above).    */
/* --------------------------------------------------------------------- */
#define TMR3L SFR8(0x0328)

/* --------------------------------------------------------------------- */
/*  TMR3H - Timer3 Count Register High Byte                              */
/*  Address: 0x0329                                                      */
/*  Bits 7:0 - TMR3H[7:0]: High byte of 16-bit Timer3 counter.          */
/*    Buffered 16-bit access behavior identical to TMR1H (see above).    */
/* --------------------------------------------------------------------- */
#define TMR3H SFR8(0x0329)

/* 16-bit combined access to TMR3H:TMR3L (little-endian) */
#define TMR3 SFR16(0x0328)

/* --------------------------------------------------------------------- */
/*  T3CON - Timer3 Control Register                                      */
/*  Address: 0x032A                                                      */
/*  Layout identical to T1CON.  See T1CON documentation for bit details. */
/*                                                                       */
/*  Bit 7   - ON:    Timer3 Enable                                      */
/*  Bit 5   - CKPS1: Prescaler bit 1                                    */
/*  Bit 4   - CKPS0: Prescaler bit 0                                    */
/*  Bit 2   - SYNC:  External Clock Synchronization                     */
/*  Bit 1   - RD16:  16-bit Read/Write Enable                           */
/* --------------------------------------------------------------------- */
#define T3CON SFR8(0x032A)

#define T3CON_ON (1u << 7)
#define T3CON_CKPS1 (1u << 5)
#define T3CON_CKPS0 (1u << 4)
#define T3CON_CKPS_MASK 0x30u
#define T3CON_CKPS_SHIFT 4u
#define T3CON_SYNC (1u << 2)
#define T3CON_RD16 (1u << 1)

/* --------------------------------------------------------------------- */
/*  T3GCON - Timer3 Gate Control Register                                */
/*  Address: 0x032B                                                      */
/*  Layout identical to T1GCON.  See T1GCON documentation for bit        */
/*  details.                                                             */
/*                                                                       */
/*  Bit 7 - GE:       Gate Enable                                       */
/*  Bit 6 - GPOL:     Gate Polarity                                     */
/*  Bit 5 - GTM:      Gate Toggle Mode                                  */
/*  Bit 4 - GSPM:     Gate Single-Pulse Mode                            */
/*  Bit 3 - GGO/DONE: Gate Single-Pulse Trigger/Status                  */
/*  Bit 2 - GVAL:     Gate Value (read-only)                            */
/* --------------------------------------------------------------------- */
#define T3GCON SFR8(0x032B)

#define T3GCON_GE (1u << 7)
#define T3GCON_GPOL (1u << 6)
#define T3GCON_GTM (1u << 5)
#define T3GCON_GSPM (1u << 4)
#define T3GCON_GGO_DONE (1u << 3)
#define T3GCON_GVAL (1u << 2)

/* --------------------------------------------------------------------- */
/*  T3GATE - Timer3 Gate Source Select Register                          */
/*  Address: 0x032C                                                      */
/*  Bits 5:0 - GSS[5:0]: Gate Source Select (see T1GATE).               */
/* --------------------------------------------------------------------- */
#define T3GATE SFR8(0x032C)

#define T3GATE_GSS_MASK 0x3Fu

/* --------------------------------------------------------------------- */
/*  T3CLK - Timer3 Clock Source Select Register                          */
/*  Address: 0x032D                                                      */
/*  Bits 4:0 - CS[4:0]: Clock Source Select (see T1CLK).                */
/* --------------------------------------------------------------------- */
#define T3CLK SFR8(0x032D)

#define T3CLK_CS_MASK 0x1Fu

/* =====================================================================
 *  TMR5 - 16-bit Timer5 with Gate Control
 *  Base address: 0x0334
 *
 *  Timer5 is functionally identical to Timer1.
 *  Register layout: TMR5L, TMR5H, T5CON, T5GCON, T5GATE, T5CLK.
 * ===================================================================== */

/* --------------------------------------------------------------------- */
/*  TMR5L - Timer5 Count Register Low Byte                               */
/*  Address: 0x0334                                                      */
/*  Bits 7:0 - TMR5L[7:0]: Low byte of 16-bit Timer5 counter.           */
/*    Buffered 16-bit access behavior identical to TMR1L (see above).    */
/* --------------------------------------------------------------------- */
#define TMR5L SFR8(0x0334)

/* --------------------------------------------------------------------- */
/*  TMR5H - Timer5 Count Register High Byte                              */
/*  Address: 0x0335                                                      */
/*  Bits 7:0 - TMR5H[7:0]: High byte of 16-bit Timer5 counter.          */
/*    Buffered 16-bit access behavior identical to TMR1H (see above).    */
/* --------------------------------------------------------------------- */
#define TMR5H SFR8(0x0335)

/* 16-bit combined access to TMR5H:TMR5L (little-endian) */
#define TMR5 SFR16(0x0334)

/* --------------------------------------------------------------------- */
/*  T5CON - Timer5 Control Register                                      */
/*  Address: 0x0336                                                      */
/*  Layout identical to T1CON.  See T1CON documentation for bit details. */
/*                                                                       */
/*  Bit 7   - ON:    Timer5 Enable                                      */
/*  Bit 5   - CKPS1: Prescaler bit 1                                    */
/*  Bit 4   - CKPS0: Prescaler bit 0                                    */
/*  Bit 2   - SYNC:  External Clock Synchronization                     */
/*  Bit 1   - RD16:  16-bit Read/Write Enable                           */
/* --------------------------------------------------------------------- */
#define T5CON SFR8(0x0336)

#define T5CON_ON (1u << 7)
#define T5CON_CKPS1 (1u << 5)
#define T5CON_CKPS0 (1u << 4)
#define T5CON_CKPS_MASK 0x30u
#define T5CON_CKPS_SHIFT 4u
#define T5CON_SYNC (1u << 2)
#define T5CON_RD16 (1u << 1)

/* --------------------------------------------------------------------- */
/*  T5GCON - Timer5 Gate Control Register                                */
/*  Address: 0x0337                                                      */
/*  Layout identical to T1GCON.  See T1GCON documentation for bit        */
/*  details.                                                             */
/*                                                                       */
/*  Bit 7 - GE:       Gate Enable                                       */
/*  Bit 6 - GPOL:     Gate Polarity                                     */
/*  Bit 5 - GTM:      Gate Toggle Mode                                  */
/*  Bit 4 - GSPM:     Gate Single-Pulse Mode                            */
/*  Bit 3 - GGO/DONE: Gate Single-Pulse Trigger/Status                  */
/*  Bit 2 - GVAL:     Gate Value (read-only)                            */
/* --------------------------------------------------------------------- */
#define T5GCON SFR8(0x0337)

#define T5GCON_GE (1u << 7)
#define T5GCON_GPOL (1u << 6)
#define T5GCON_GTM (1u << 5)
#define T5GCON_GSPM (1u << 4)
#define T5GCON_GGO_DONE (1u << 3)
#define T5GCON_GVAL (1u << 2)

/* --------------------------------------------------------------------- */
/*  T5GATE - Timer5 Gate Source Select Register                          */
/*  Address: 0x0338                                                      */
/*  Bits 5:0 - GSS[5:0]: Gate Source Select (see T1GATE).               */
/* --------------------------------------------------------------------- */
#define T5GATE SFR8(0x0338)

#define T5GATE_GSS_MASK 0x3Fu

/* --------------------------------------------------------------------- */
/*  T5CLK - Timer5 Clock Source Select Register                          */
/*  Address: 0x0339                                                      */
/*  Bits 4:0 - CS[4:0]: Clock Source Select (see T1CLK).                */
/* --------------------------------------------------------------------- */
#define T5CLK SFR8(0x0339)

#define T5CLK_CS_MASK 0x1Fu

/* =====================================================================
 *  Common TnCON prescaler field values (apply to T1CON, T3CON, T5CON)
 *  Pre-shifted to bits 5:4 for direct OR into TnCON register.
 * ===================================================================== */
#define TxCKPS_1_1 (0x00u << 4) /* 1:1 (no prescaling) */
#define TxCKPS_1_2 (0x01u << 4) /* 1:2                 */
#define TxCKPS_1_4 (0x02u << 4) /* 1:4                 */
#define TxCKPS_1_8 (0x03u << 4) /* 1:8                 */

#endif /* PIC18F_Q84_TMR1_H */
