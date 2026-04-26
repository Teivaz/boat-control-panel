/*
 * pic18f_q84_tmr2.h
 *
 * 8-bit Timer modules with Hardware Limit Timer (TMR2, TMR4, TMR6)
 * registers for PIC18F27/47/57Q84 family.
 *
 * TMR2, TMR4, and TMR6 are three identical 8-bit timer modules with
 * Hardware Limit Timer (HLT) capability.  Each provides:
 *   - 8-bit up-counter with 8-bit period register
 *   - Configurable prescaler (1:1 to 1:128, powers of 2)
 *   - Configurable postscaler (1:1 to 1:16)
 *   - Hardware Limit Timer (HLT) with 32 operating modes controlling
 *     automatic start, stop, and reset behavior based on external or
 *     internal signals
 *   - Configurable clock source via TnCLKCON
 *   - Reset/gate source selection via TnRST
 *   - Clock-synchronized or free-running output
 *
 * These timers are commonly used as PWM time bases (for CCP modules),
 * periodic interrupt generators, and hardware-gated timing applications.
 *
 * Register map per timer instance:
 *   TnTMR    (base+0) - Timer count value
 *   TnPR     (base+1) - Period register (match value)
 *   TnCON    (base+2) - Timer control (enable, prescaler, postscaler)
 *   TnHLT    (base+3) - Hardware Limit Timer mode control
 *   TnCLKCON (base+4) - Clock source select
 *   TnRST    (base+5) - Reset/gate source select
 *
 * Base addresses:
 *   TMR2 = 0x0322    TMR4 = 0x032E    TMR6 = 0x033A
 *
 * Covers:
 *   - T2TMR, T2PR, T2CON, T2HLT, T2CLKCON, T2RST
 *   - T4TMR, T4PR, T4CON, T4HLT, T4CLKCON, T4RST
 *   - T6TMR, T6PR, T6CON, T6HLT, T6CLKCON, T6RST
 *
 * Reference: DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 19 - Timer2/4/6 Module with Hardware Limit Timer
 *
 * Requires: SFR8(addr) macro defined externally (provides volatile
 *           uint8_t access to an absolute SFR address).
 */

#ifndef PIC18F_Q84_TMR2_H
#define PIC18F_Q84_TMR2_H

/* =====================================================================
 *  TMR2 - 8-bit Timer2 with Hardware Limit Timer
 *  Base address: 0x0322
 * ===================================================================== */

/* --------------------------------------------------------------------- */
/*  T2TMR - Timer2 Counter Register                                      */
/*  Address: 0x0322                                                      */
/*                                                                       */
/*  Bits 7:0 - TMR[7:0]: Current timer count value.                     */
/*    Counts up from 0x00.  When the count matches T2PR (period          */
/*    register), the timer resets to 0x00 on the next prescaled clock    */
/*    edge, the postscaler is incremented, and when the postscaler       */
/*    rolls over the TMR2 interrupt flag is set.                         */
/*    Can be read or written at any time.                                */
/* --------------------------------------------------------------------- */
#define T2TMR SFR8(0x0322)

/* --------------------------------------------------------------------- */
/*  T2PR - Timer2 Period Register                                        */
/*  Address: 0x0323                                                      */
/*                                                                       */
/*  Bits 7:0 - PR[7:0]: Period match value.                              */
/*    The timer resets when T2TMR matches this value.  Effective timer    */
/*    period = (T2PR + 1) * prescaler * postscaler * clock period.       */
/*    Default value after reset is 0xFF.                                 */
/* --------------------------------------------------------------------- */
#define T2PR SFR8(0x0323)

/* --------------------------------------------------------------------- */
/*  T2CON - Timer2 Control Register                                      */
/*  Address: 0x0324                                                      */
/*                                                                       */
/*  Bit 7   - ON: Timer2 Enable                                         */
/*      1 = Timer2 is enabled and counting                               */
/*      0 = Timer2 is stopped; TMR2 counter is not cleared               */
/*  Bits 6:4 - CKPS[2:0]: Prescaler Select                              */
/*      Divides the timer clock before incrementing the counter.         */
/*      000 = 1:1   (no prescaling)                                      */
/*      001 = 1:2                                                        */
/*      010 = 1:4                                                        */
/*      011 = 1:8                                                        */
/*      100 = 1:16                                                       */
/*      101 = 1:32                                                       */
/*      110 = 1:64                                                       */
/*      111 = 1:128                                                      */
/*  Bits 3:0 - OUTPS[3:0]: Output Postscaler Select                     */
/*      Divides the period-match output before setting the interrupt     */
/*      flag.  Actual division ratio = OUTPS + 1.                        */
/*      0000 = 1:1  (every period match sets flag)                       */
/*      0001 = 1:2                                                       */
/*      0010 = 1:3                                                       */
/*      0011 = 1:4                                                       */
/*      0100 = 1:5                                                       */
/*      0101 = 1:6                                                       */
/*      0110 = 1:7                                                       */
/*      0111 = 1:8                                                       */
/*      1000 = 1:9                                                       */
/*      1001 = 1:10                                                      */
/*      1010 = 1:11                                                      */
/*      1011 = 1:12                                                      */
/*      1100 = 1:13                                                      */
/*      1101 = 1:14                                                      */
/*      1110 = 1:15                                                      */
/*      1111 = 1:16                                                      */
/* --------------------------------------------------------------------- */
#define T2CON SFR8(0x0324)

/* ON - Timer2 Enable (bit 7) */
#define T2CON_ON (1u << 7)

/* CKPS - Prescaler Select (bits 6:4) */
#define T2CON_CKPS2 (1u << 6) /* CKPS bit 2 */
#define T2CON_CKPS1 (1u << 5) /* CKPS bit 1 */
#define T2CON_CKPS0 (1u << 4) /* CKPS bit 0 */
#define T2CON_CKPS_MASK 0x70u /* Mask for CKPS[2:0] field */
#define T2CON_CKPS_SHIFT 4u   /* Bit position of CKPS[0] */

/* OUTPS - Output Postscaler Select (bits 3:0) */
#define T2CON_OUTPS3 (1u << 3) /* OUTPS bit 3 */
#define T2CON_OUTPS2 (1u << 2) /* OUTPS bit 2 */
#define T2CON_OUTPS1 (1u << 1) /* OUTPS bit 1 */
#define T2CON_OUTPS0 (1u << 0) /* OUTPS bit 0 */
#define T2CON_OUTPS_MASK 0x0Fu /* Mask for OUTPS[3:0] field */

/* --------------------------------------------------------------------- */
/*  T2HLT - Timer2 Hardware Limit Timer Control Register                 */
/*  Address: 0x0325                                                      */
/*                                                                       */
/*  Bit 7   - CSYNC: Output Synchronized to Timer Clock                  */
/*      1 = Timer output is synchronized to the prescaled timer clock.   */
/*          Prevents glitches on the output but adds one clock cycle     */
/*          of latency.                                                  */
/*      0 = Timer output is not synchronized (changes immediately on     */
/*          the system clock).                                           */
/*  Bits 6:5: Unimplemented, read as 0                                  */
/*  Bits 4:0 - MODE[4:0]: Hardware Limit Timer Mode Select               */
/*      Selects the timer counting behavior and how the external         */
/*      reset/gate signal (from TnRST) affects timer operation.          */
/*                                                                       */
/*      --- Software-only modes ---                                      */
/*      00000 = Free-running; software gate only (no HLT input effect)   */
/*      00001 = Free-running; period count with HLT reset (active-high)  */
/*                                                                       */
/*      --- Edge-triggered start modes ---                               */
/*      00001 = Rising edge of TnIN starts counting                      */
/*      00010 = Falling edge of TnIN starts counting                     */
/*      00011 = Any edge of TnIN starts counting                         */
/*                                                                       */
/*      --- Edge-triggered reset modes ---                               */
/*      00100 = Rising edge of TnIN resets timer                         */
/*      00101 = Falling edge of TnIN resets timer                        */
/*      00110 = Any edge of TnIN resets timer                            */
/*                                                                       */
/*      --- Level-gated modes ---                                        */
/*      00111 = Counts while TnIN is high; resets on falling edge        */
/*      01000 = Counts while TnIN is low; resets on rising edge          */
/*                                                                       */
/*      --- Edge-triggered reset with level hold ---                     */
/*      01001 = Rising edge resets; held in reset while TnIN high        */
/*      01010 = Falling edge resets; held in reset while TnIN low        */
/*                                                                       */
/*      --- One-shot modes ---                                           */
/*      01011 = Rising edge starts one-shot (one period, then stops)     */
/*      01100 = Falling edge starts one-shot                             */
/*      01101 = Any edge starts one-shot                                 */
/*                                                                       */
/*      --- Monostable modes ---                                         */
/*      01110 = Rising edge starts, resets on next rising edge           */
/*      01111 = Falling edge starts, resets on next falling edge         */
/*      10000 = Any edge starts, resets on any edge                      */
/*                                                                       */
/*      --- Level-gated count with reset ---                             */
/*      10001 = Counts while high, rising edge resets                    */
/*      10010 = Counts while high, falling edge resets                   */
/*      10011 = Counts while low, rising edge resets                     */
/*      10100 = Counts while low, falling edge resets                    */
/*                                                                       */
/*      Additional modes 10101-11111 provide further combinations        */
/*      of start/stop/reset behavior; see datasheet Table 19-1 for      */
/*      the complete mode truth table.                                   */
/*                                                                       */
/*  Note: The TnIN signal source is selected by the TnRST register.     */
/* --------------------------------------------------------------------- */
#define T2HLT SFR8(0x0325)

/* CSYNC - Clock Synchronization Enable (bit 7) */
#define T2HLT_CSYNC (1u << 7)

/* MODE - HLT Mode Select (bits 4:0) */
#define T2HLT_MODE4 (1u << 4) /* MODE bit 4 */
#define T2HLT_MODE3 (1u << 3) /* MODE bit 3 */
#define T2HLT_MODE2 (1u << 2) /* MODE bit 2 */
#define T2HLT_MODE1 (1u << 1) /* MODE bit 1 */
#define T2HLT_MODE0 (1u << 0) /* MODE bit 0 */
#define T2HLT_MODE_MASK 0x1Fu /* Mask for MODE[4:0] field */

/* --------------------------------------------------------------------- */
/*  T2CLKCON - Timer2 Clock Source Select Register                       */
/*  Address: 0x0326                                                      */
/*                                                                       */
/*  Bits 7:5: Unimplemented, read as 0                                  */
/*  Bits 4:0 - CS[4:0]: Clock Source Select                              */
/*      Selects the input clock for Timer2.  Specific source             */
/*      assignments include:                                             */
/*        00000 = T2CKIPPS pin (external clock via PPS)                  */
/*        00001 = FOSC/4 (instruction clock)                             */
/*        00010 = FOSC (system clock)                                    */
/*        00011 = HFINTOSC                                               */
/*        00100 = LFINTOSC                                               */
/*        00101 = MFINTOSC 500 kHz                                       */
/*        00110 = MFINTOSC 31.25 kHz                                     */
/*        00111 = SOSC                                                   */
/*      See datasheet clock source table for additional selections.      */
/* --------------------------------------------------------------------- */
#define T2CLKCON SFR8(0x0326)

/* CS - Clock Source Select (bits 4:0) */
#define T2CLKCON_CS_MASK 0x1Fu /* Mask for CS[4:0] field */

/* --------------------------------------------------------------------- */
/*  T2RST - Timer2 External Reset (Gate) Source Select Register          */
/*  Address: 0x0327                                                      */
/*                                                                       */
/*  Bits 7:6: Unimplemented, read as 0                                  */
/*  Bits 5:0 - RSEL[5:0]: Reset/Gate Source Select                       */
/*      Selects the external signal routed to the Hardware Limit Timer   */
/*      logic (the "TnIN" input referenced in T2HLT MODE descriptions). */
/*      Source assignments are device-dependent; see the datasheet       */
/*      reset source table.  Common selections include PPS-routed       */
/*      pins, comparator outputs, CLC outputs, ZCD output, and other    */
/*      timer outputs.                                                   */
/* --------------------------------------------------------------------- */
#define T2RST SFR8(0x0327)

/* RSEL - Reset/Gate Source Select (bits 5:0) */
#define T2RST_RSEL_MASK 0x3Fu /* Mask for RSEL[5:0] field */

/* =====================================================================
 *  TMR4 - 8-bit Timer4 with Hardware Limit Timer
 *  Base address: 0x032E
 *
 *  Timer4 is functionally identical to Timer2.
 *  Register layout: T4TMR, T4PR, T4CON, T4HLT, T4CLKCON, T4RST.
 * ===================================================================== */

/* --------------------------------------------------------------------- */
/*  T4TMR - Timer4 Counter Register                                      */
/*  Address: 0x032E                                                      */
/*  Bits 7:0 - TMR[7:0]: Current count value (see T2TMR).               */
/* --------------------------------------------------------------------- */
#define T4TMR SFR8(0x032E)

/* --------------------------------------------------------------------- */
/*  T4PR - Timer4 Period Register                                        */
/*  Address: 0x032F                                                      */
/*  Bits 7:0 - PR[7:0]: Period match value (see T2PR).                  */
/* --------------------------------------------------------------------- */
#define T4PR SFR8(0x032F)

/* --------------------------------------------------------------------- */
/*  T4CON - Timer4 Control Register                                      */
/*  Address: 0x0330                                                      */
/*  Layout identical to T2CON.  See T2CON documentation for bit details. */
/*                                                                       */
/*  Bit 7     - ON:        Timer4 Enable                                */
/*  Bits 6:4  - CKPS[2:0]: Prescaler Select                             */
/*  Bits 3:0  - OUTPS[3:0]:Postscaler Select                            */
/* --------------------------------------------------------------------- */
#define T4CON SFR8(0x0330)

#define T4CON_ON (1u << 7)
#define T4CON_CKPS2 (1u << 6)
#define T4CON_CKPS1 (1u << 5)
#define T4CON_CKPS0 (1u << 4)
#define T4CON_CKPS_MASK 0x70u
#define T4CON_CKPS_SHIFT 4u
#define T4CON_OUTPS3 (1u << 3)
#define T4CON_OUTPS2 (1u << 2)
#define T4CON_OUTPS1 (1u << 1)
#define T4CON_OUTPS0 (1u << 0)
#define T4CON_OUTPS_MASK 0x0Fu

/* --------------------------------------------------------------------- */
/*  T4HLT - Timer4 Hardware Limit Timer Control Register                 */
/*  Address: 0x0331                                                      */
/*  Layout identical to T2HLT.  See T2HLT documentation for bit         */
/*  details and full MODE truth table.                                   */
/*                                                                       */
/*  Bit 7     - CSYNC:     Output Synchronized to Timer Clock            */
/*  Bits 4:0  - MODE[4:0]: HLT Mode Select                              */
/* --------------------------------------------------------------------- */
#define T4HLT SFR8(0x0331)

#define T4HLT_CSYNC (1u << 7)
#define T4HLT_MODE4 (1u << 4)
#define T4HLT_MODE3 (1u << 3)
#define T4HLT_MODE2 (1u << 2)
#define T4HLT_MODE1 (1u << 1)
#define T4HLT_MODE0 (1u << 0)
#define T4HLT_MODE_MASK 0x1Fu

/* --------------------------------------------------------------------- */
/*  T4CLKCON - Timer4 Clock Source Select Register                       */
/*  Address: 0x0332                                                      */
/*  Bits 4:0 - CS[4:0]: Clock Source Select (see T2CLKCON).             */
/* --------------------------------------------------------------------- */
#define T4CLKCON SFR8(0x0332)

#define T4CLKCON_CS_MASK 0x1Fu

/* --------------------------------------------------------------------- */
/*  T4RST - Timer4 External Reset Source Select Register                 */
/*  Address: 0x0333                                                      */
/*  Bits 5:0 - RSEL[5:0]: Reset/Gate Source Select (see T2RST).         */
/* --------------------------------------------------------------------- */
#define T4RST SFR8(0x0333)

#define T4RST_RSEL_MASK 0x3Fu

/* =====================================================================
 *  TMR6 - 8-bit Timer6 with Hardware Limit Timer
 *  Base address: 0x033A
 *
 *  Timer6 is functionally identical to Timer2.
 *  Register layout: T6TMR, T6PR, T6CON, T6HLT, T6CLKCON, T6RST.
 * ===================================================================== */

/* --------------------------------------------------------------------- */
/*  T6TMR - Timer6 Counter Register                                      */
/*  Address: 0x033A                                                      */
/*  Bits 7:0 - TMR[7:0]: Current count value (see T2TMR).               */
/* --------------------------------------------------------------------- */
#define T6TMR SFR8(0x033A)

/* --------------------------------------------------------------------- */
/*  T6PR - Timer6 Period Register                                        */
/*  Address: 0x033B                                                      */
/*  Bits 7:0 - PR[7:0]: Period match value (see T2PR).                  */
/* --------------------------------------------------------------------- */
#define T6PR SFR8(0x033B)

/* --------------------------------------------------------------------- */
/*  T6CON - Timer6 Control Register                                      */
/*  Address: 0x033C                                                      */
/*  Layout identical to T2CON.  See T2CON documentation for bit details. */
/*                                                                       */
/*  Bit 7     - ON:        Timer6 Enable                                */
/*  Bits 6:4  - CKPS[2:0]: Prescaler Select                             */
/*  Bits 3:0  - OUTPS[3:0]:Postscaler Select                            */
/* --------------------------------------------------------------------- */
#define T6CON SFR8(0x033C)

#define T6CON_ON (1u << 7)
#define T6CON_CKPS2 (1u << 6)
#define T6CON_CKPS1 (1u << 5)
#define T6CON_CKPS0 (1u << 4)
#define T6CON_CKPS_MASK 0x70u
#define T6CON_CKPS_SHIFT 4u
#define T6CON_OUTPS3 (1u << 3)
#define T6CON_OUTPS2 (1u << 2)
#define T6CON_OUTPS1 (1u << 1)
#define T6CON_OUTPS0 (1u << 0)
#define T6CON_OUTPS_MASK 0x0Fu

/* --------------------------------------------------------------------- */
/*  T6HLT - Timer6 Hardware Limit Timer Control Register                 */
/*  Address: 0x033D                                                      */
/*  Layout identical to T2HLT.  See T2HLT documentation for bit         */
/*  details and full MODE truth table.                                   */
/*                                                                       */
/*  Bit 7     - CSYNC:     Output Synchronized to Timer Clock            */
/*  Bits 4:0  - MODE[4:0]: HLT Mode Select                              */
/* --------------------------------------------------------------------- */
#define T6HLT SFR8(0x033D)

#define T6HLT_CSYNC (1u << 7)
#define T6HLT_MODE4 (1u << 4)
#define T6HLT_MODE3 (1u << 3)
#define T6HLT_MODE2 (1u << 2)
#define T6HLT_MODE1 (1u << 1)
#define T6HLT_MODE0 (1u << 0)
#define T6HLT_MODE_MASK 0x1Fu

/* --------------------------------------------------------------------- */
/*  T6CLKCON - Timer6 Clock Source Select Register                       */
/*  Address: 0x033E                                                      */
/*  Bits 4:0 - CS[4:0]: Clock Source Select (see T2CLKCON).             */
/* --------------------------------------------------------------------- */
#define T6CLKCON SFR8(0x033E)

#define T6CLKCON_CS_MASK 0x1Fu

/* --------------------------------------------------------------------- */
/*  T6RST - Timer6 External Reset Source Select Register                 */
/*  Address: 0x033F                                                      */
/*  Bits 5:0 - RSEL[5:0]: Reset/Gate Source Select (see T2RST).         */
/* --------------------------------------------------------------------- */
#define T6RST SFR8(0x033F)

#define T6RST_RSEL_MASK 0x3Fu

/* =====================================================================
 *  Common TnCON prescaler field values (apply to T2CON, T4CON, T6CON)
 *  Pre-shifted to bits 6:4 for direct OR into TnCON register.
 * ===================================================================== */
#define TxHLT_CKPS_1 (0x00u << 4)   /* 1:1   (no prescaling) */
#define TxHLT_CKPS_2 (0x01u << 4)   /* 1:2                   */
#define TxHLT_CKPS_4 (0x02u << 4)   /* 1:4                   */
#define TxHLT_CKPS_8 (0x03u << 4)   /* 1:8                   */
#define TxHLT_CKPS_16 (0x04u << 4)  /* 1:16                  */
#define TxHLT_CKPS_32 (0x05u << 4)  /* 1:32                  */
#define TxHLT_CKPS_64 (0x06u << 4)  /* 1:64                  */
#define TxHLT_CKPS_128 (0x07u << 4) /* 1:128                 */

/* =====================================================================
 *  Common TnCON postscaler field values (apply to T2CON, T4CON, T6CON)
 *  Values are in bits 3:0 (no shift needed).
 * ===================================================================== */
#define TxHLT_OUTPS_1 0x00u  /* 1:1  */
#define TxHLT_OUTPS_2 0x01u  /* 1:2  */
#define TxHLT_OUTPS_3 0x02u  /* 1:3  */
#define TxHLT_OUTPS_4 0x03u  /* 1:4  */
#define TxHLT_OUTPS_5 0x04u  /* 1:5  */
#define TxHLT_OUTPS_6 0x05u  /* 1:6  */
#define TxHLT_OUTPS_7 0x06u  /* 1:7  */
#define TxHLT_OUTPS_8 0x07u  /* 1:8  */
#define TxHLT_OUTPS_9 0x08u  /* 1:9  */
#define TxHLT_OUTPS_10 0x09u /* 1:10 */
#define TxHLT_OUTPS_11 0x0Au /* 1:11 */
#define TxHLT_OUTPS_12 0x0Bu /* 1:12 */
#define TxHLT_OUTPS_13 0x0Cu /* 1:13 */
#define TxHLT_OUTPS_14 0x0Du /* 1:14 */
#define TxHLT_OUTPS_15 0x0Eu /* 1:15 */
#define TxHLT_OUTPS_16 0x0Fu /* 1:16 */

/* =====================================================================
 *  Common TnHLT MODE field values (apply to T2HLT, T4HLT, T6HLT)
 *
 *  These define the Hardware Limit Timer operating mode.  The TnIN
 *  signal is selected by the corresponding TnRST register.
 * ===================================================================== */
#define TxMODE_FREE_RUN 0x00u         /* Free-running; software gate only           */
#define TxMODE_RISE_START 0x01u       /* Rising edge of TnIN starts counting        */
#define TxMODE_FALL_START 0x02u       /* Falling edge of TnIN starts counting       */
#define TxMODE_EDGE_START 0x03u       /* Any edge of TnIN starts counting           */
#define TxMODE_RISE_RESET 0x04u       /* Rising edge of TnIN resets timer           */
#define TxMODE_FALL_RESET 0x05u       /* Falling edge of TnIN resets timer          */
#define TxMODE_EDGE_RESET 0x06u       /* Any edge of TnIN resets timer              */
#define TxMODE_HIGH_GATE_FRESET 0x07u /* Counts while high; resets on falling edge  */
#define TxMODE_LOW_GATE_RRESET 0x08u  /* Counts while low; resets on rising edge    */
#define TxMODE_RISE_RESET_HHOLD 0x09u /* Rising edge resets; held while high        */
#define TxMODE_FALL_RESET_LHOLD 0x0Au /* Falling edge resets; held while low        */
#define TxMODE_RISE_ONESHOT 0x0Bu     /* Rising edge starts one-shot                */
#define TxMODE_FALL_ONESHOT 0x0Cu     /* Falling edge starts one-shot               */
#define TxMODE_EDGE_ONESHOT 0x0Du     /* Any edge starts one-shot                   */
#define TxMODE_RISE_MONO 0x0Eu        /* Rising edge starts; next rising resets      */
#define TxMODE_FALL_MONO 0x0Fu        /* Falling edge starts; next falling resets    */
#define TxMODE_EDGE_MONO 0x10u        /* Any edge starts; any edge resets            */
#define TxMODE_HIGH_RISE_RESET 0x11u  /* Counts while high; rising edge resets       */
#define TxMODE_HIGH_FALL_RESET 0x12u  /* Counts while high; falling edge resets      */
#define TxMODE_LOW_RISE_RESET 0x13u   /* Counts while low; rising edge resets        */
#define TxMODE_LOW_FALL_RESET 0x14u   /* Counts while low; falling edge resets       */

#endif /* PIC18F_Q84_TMR2_H */
