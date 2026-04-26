/*
 * pic18f_q84_tmr0.h
 *
 * Timer0 (TMR0) module registers for PIC18F27/47/57Q84 family.
 *
 * Timer0 is a versatile 8-bit or 16-bit timer/counter.  In 8-bit mode
 * it counts from 0x00 up to the match value in TMR0H (period register)
 * and resets, providing a tunable period without software reloading.
 * In 16-bit mode TMR0H:TMR0L form a free-running 16-bit counter that
 * overflows at 0xFFFF.
 *
 * Features:
 *   - 8-bit mode with hardware period match (TMR0H = period register)
 *   - 16-bit free-running mode (TMR0H:TMR0L = full counter)
 *   - Configurable clock source (internal oscillators or external pin)
 *   - 16-value prescaler  (1:1 to 1:32768, powers of 2)
 *   - 16-value postscaler (1:1 to 1:16)
 *   - Asynchronous operation for Sleep-mode counting
 *   - Readable output bit for PPS routing
 *
 * Covers:
 *   - TMR0L   : Timer0 count low byte
 *   - TMR0H   : Timer0 count high byte / 8-bit period register
 *   - T0CON0  : Timer0 control (enable, mode, output, postscaler)
 *   - T0CON1  : Timer0 control (clock source, async, prescaler)
 *
 * Reference: DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 17 - Timer0 Module
 *
 * Requires: SFR8(addr) and SFR16(addr) macros defined externally
 *           (provide volatile uint8_t / uint16_t access to absolute
 *           SFR addresses).
 */

#ifndef PIC18F_Q84_TMR0_H
#define PIC18F_Q84_TMR0_H

/* ===================================================================== */
/*  TMR0L - Timer0 Count Register Low Byte                               */
/*  Address: 0x0317                                                      */
/*                                                                       */
/*  Bits 7:0 - TMR0L[7:0]:                                              */
/*    In 16-bit mode (MD16 = 1):                                         */
/*      Low byte of the 16-bit free-running counter (TMR0H:TMR0L).      */
/*      Reads and writes go through a buffer to ensure atomic 16-bit     */
/*      access when paired with TMR0H.                                   */
/*    In 8-bit mode (MD16 = 0):                                          */
/*      The 8-bit timer/counter value.  Counts up from 0x00 and resets   */
/*      when it matches TMR0H (period register).                         */
/* ===================================================================== */
#define TMR0L SFR8(0x0318)

/* ===================================================================== */
/*  TMR0H - Timer0 Count Register High Byte / Period Register            */
/*  Address: 0x0318                                                      */
/*                                                                       */
/*  Bits 7:0 - TMR0H[7:0]:                                              */
/*    In 16-bit mode (MD16 = 1):                                         */
/*      High byte of the 16-bit free-running counter.  On a read of      */
/*      TMR0L the high byte is latched here; on a write to TMR0L the     */
/*      value previously written to TMR0H is transferred to the actual   */
/*      timer high byte.                                                 */
/*    In 8-bit mode (MD16 = 0):                                          */
/*      Acts as the period/match register.  The timer resets to 0x00     */
/*      on the clock cycle after TMR0L equals TMR0H, generating the      */
/*      TMR0 interrupt flag and advancing the postscaler.                */
/* ===================================================================== */
#define TMR0H SFR8(0x0319)

/* 16-bit combined access to TMR0H:TMR0L (little-endian) */
#define TMR0 SFR16(0x0318)

/* ===================================================================== */
/*  T0CON0 - Timer0 Control Register 0                                   */
/*  Address: 0x0319                                                      */
/*                                                                       */
/*  Bit 7   - EN: Timer0 Enable                                         */
/*      1 = Timer0 is running                                            */
/*      0 = Timer0 is stopped; all counters and prescaler/postscaler     */
/*          are cleared                                                  */
/*  Bit 6   : Unimplemented, read as 0                                  */
/*  Bit 5   - OUT: Timer0 Output (read-only)                            */
/*      Reflects the current state of the timer output signal.  This     */
/*      is the post-postscaler output that can be routed via PPS.        */
/*      Directly readable for software polling.                          */
/*  Bit 4   - MD16: 16-bit Mode Select                                  */
/*      1 = Timer0 operates in 16-bit mode (TMR0H:TMR0L = counter)      */
/*      0 = Timer0 operates in 8-bit mode (TMR0L = counter, TMR0H =     */
/*          period/match register)                                       */
/*  Bits 3:0 - OUTPS[3:0]: Output Postscaler Select                     */
/*      Divides the timer match/overflow output before setting the       */
/*      interrupt flag.                                                  */
/*      0000 = 1:1  (every match/overflow sets flag)                     */
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
/* ===================================================================== */
#define T0CON0 SFR8(0x031A)

/* EN - Timer0 Enable (bit 7) */
#define T0CON0_EN (1u << 7)

/* OUT - Timer0 Output, read-only (bit 5) */
#define T0CON0_OUT (1u << 5)

/* MD16 - 16-bit Mode Select (bit 4) */
#define T0CON0_MD16 (1u << 4)

/* OUTPS - Output Postscaler Select (bits 3:0) */
#define T0CON0_OUTPS3 (1u << 3) /* OUTPS bit 3 */
#define T0CON0_OUTPS2 (1u << 2) /* OUTPS bit 2 */
#define T0CON0_OUTPS1 (1u << 1) /* OUTPS bit 1 */
#define T0CON0_OUTPS0 (1u << 0) /* OUTPS bit 0 */
#define T0CON0_OUTPS_MASK 0x0Fu /* Mask for OUTPS[3:0] field */

/* OUTPS field values (postscaler ratio is value + 1) */
#define T0OUTPS_1_1 0x00u  /* 1:1  */
#define T0OUTPS_1_2 0x01u  /* 1:2  */
#define T0OUTPS_1_3 0x02u  /* 1:3  */
#define T0OUTPS_1_4 0x03u  /* 1:4  */
#define T0OUTPS_1_5 0x04u  /* 1:5  */
#define T0OUTPS_1_6 0x05u  /* 1:6  */
#define T0OUTPS_1_7 0x06u  /* 1:7  */
#define T0OUTPS_1_8 0x07u  /* 1:8  */
#define T0OUTPS_1_9 0x08u  /* 1:9  */
#define T0OUTPS_1_10 0x09u /* 1:10 */
#define T0OUTPS_1_11 0x0Au /* 1:11 */
#define T0OUTPS_1_12 0x0Bu /* 1:12 */
#define T0OUTPS_1_13 0x0Cu /* 1:13 */
#define T0OUTPS_1_14 0x0Du /* 1:14 */
#define T0OUTPS_1_15 0x0Eu /* 1:15 */
#define T0OUTPS_1_16 0x0Fu /* 1:16 */

/* ===================================================================== */
/*  T0CON1 - Timer0 Control Register 1                                   */
/*  Address: 0x031A                                                      */
/*                                                                       */
/*  Bits 7:5 - CS[2:0]: Clock Source Select                              */
/*      Selects the input clock for Timer0.                              */
/*      000 = T0CKIPPS pin (active-high edge on external pin via PPS)    */
/*      001 = T0CKIPPS pin inverted (active-low / falling edge)          */
/*      010 = FOSC/4 (instruction clock, Tcy)                            */
/*      011 = HFINTOSC (high-frequency internal oscillator, pre-divider) */
/*      100 = LFINTOSC (31 kHz low-frequency internal oscillator)        */
/*      101 = MFINTOSC 500 kHz (medium-frequency internal oscillator)    */
/*      110 = MFINTOSC 31.25 kHz (medium-frequency internal oscillator)  */
/*      111 = SOSC (secondary oscillator, 32.768 kHz crystal)            */
/*  Bit 4   - ASYNC: Asynchronous Enable                                 */
/*      1 = Timer0 input is NOT synchronized to the system clock         */
/*          (FOSC).  Required for operation during Sleep.                 */
/*      0 = Timer0 input IS synchronized to FOSC.  Provides glitch       */
/*          filtering but timer halts during Sleep.                      */
/*  Bits 3:0 - CKPS[3:0]: Prescaler Rate Select                         */
/*      Divides the selected clock source before it clocks the timer     */
/*      counter.  Division ratio = 2^CKPS (powers of two).              */
/*      0000 = 1:1      (no prescaling)                                  */
/*      0001 = 1:2                                                       */
/*      0010 = 1:4                                                       */
/*      0011 = 1:8                                                       */
/*      0100 = 1:16                                                      */
/*      0101 = 1:32                                                      */
/*      0110 = 1:64                                                      */
/*      0111 = 1:128                                                     */
/*      1000 = 1:256                                                     */
/*      1001 = 1:512                                                     */
/*      1010 = 1:1024                                                    */
/*      1011 = 1:2048                                                    */
/*      1100 = 1:4096                                                    */
/*      1101 = 1:8192                                                    */
/*      1110 = 1:16384                                                   */
/*      1111 = 1:32768                                                   */
/* ===================================================================== */
#define T0CON1 SFR8(0x031B)

/* CS - Clock Source Select (bits 7:5) */
#define T0CON1_CS2 (1u << 7) /* CS bit 2 */
#define T0CON1_CS1 (1u << 6) /* CS bit 1 */
#define T0CON1_CS0 (1u << 5) /* CS bit 0 */
#define T0CON1_CS_MASK 0xE0u /* Mask for CS[2:0] field */
#define T0CON1_CS_SHIFT 5u   /* Bit position of CS[0] */

/* CS field values (pre-shifted to bits 7:5) */
#define T0CS_T0CKIPPS (0x00u << 5)    /* T0CKIPPS pin (rising edge)     */
#define T0CS_T0CKI_INV (0x01u << 5)   /* T0CKIPPS pin inverted (falling)*/
#define T0CS_FOSC_D4 (0x02u << 5)     /* FOSC/4 (instruction clock)     */
#define T0CS_HFINTOSC (0x03u << 5)    /* HFINTOSC                       */
#define T0CS_LFINTOSC (0x04u << 5)    /* LFINTOSC (31 kHz)              */
#define T0CS_MFINTOSC500 (0x05u << 5) /* MFINTOSC 500 kHz               */
#define T0CS_MFINTOSC31 (0x06u << 5)  /* MFINTOSC 31.25 kHz             */
#define T0CS_SOSC (0x07u << 5)        /* SOSC (32.768 kHz)              */

/* ASYNC - Asynchronous Enable (bit 4) */
#define T0CON1_ASYNC (1u << 4)

/* CKPS - Prescaler Rate Select (bits 3:0) */
#define T0CON1_CKPS3 (1u << 3) /* CKPS bit 3 */
#define T0CON1_CKPS2 (1u << 2) /* CKPS bit 2 */
#define T0CON1_CKPS1 (1u << 1) /* CKPS bit 1 */
#define T0CON1_CKPS0 (1u << 0) /* CKPS bit 0 */
#define T0CON1_CKPS_MASK 0x0Fu /* Mask for CKPS[3:0] field */

/* CKPS field values (prescaler ratio = 2^value) */
#define T0CKPS_1 0x00u     /* 1:1      */
#define T0CKPS_2 0x01u     /* 1:2      */
#define T0CKPS_4 0x02u     /* 1:4      */
#define T0CKPS_8 0x03u     /* 1:8      */
#define T0CKPS_16 0x04u    /* 1:16     */
#define T0CKPS_32 0x05u    /* 1:32     */
#define T0CKPS_64 0x06u    /* 1:64     */
#define T0CKPS_128 0x07u   /* 1:128    */
#define T0CKPS_256 0x08u   /* 1:256    */
#define T0CKPS_512 0x09u   /* 1:512    */
#define T0CKPS_1024 0x0Au  /* 1:1024   */
#define T0CKPS_2048 0x0Bu  /* 1:2048   */
#define T0CKPS_4096 0x0Cu  /* 1:4096   */
#define T0CKPS_8192 0x0Du  /* 1:8192   */
#define T0CKPS_16384 0x0Eu /* 1:16384  */
#define T0CKPS_32768 0x0Fu /* 1:32768  */

#endif /* PIC18F_Q84_TMR0_H */
