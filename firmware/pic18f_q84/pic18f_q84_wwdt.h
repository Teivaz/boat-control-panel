/*
 * pic18f_q84_wwdt.h
 *
 * Windowed Watchdog Timer (WWDT) registers for PIC18F27/47/57Q84 family.
 *
 * The WWDT provides both standard watchdog timeout protection and an
 * optional window mode.  In windowed operation the WDT period is divided
 * into a "closed" (early) window and an "open" (late) window.  Executing
 * CLRWDT during the closed window causes a window-violation reset;
 * CLRWDT must be executed during the open window to properly clear the
 * timer.  If the timer is never cleared, a standard WDT timeout reset
 * occurs at the end of the period.
 *
 * Covers:
 *   - WDTCON0 : Prescaler select and software enable
 *   - WDTCON1 : Clock source and window size
 *   - WDTPSL  : Prescaler count low byte  (read-only)
 *   - WDTPSH  : Prescaler count high byte (read-only)
 *   - WDTTMR  : WDT timer value and state  (read-only)
 *
 * Reference: DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 15 - Windowed Watchdog Timer (WWDT)
 *
 * Requires: SFR8(addr) macro defined externally (provides volatile
 *           uint8_t access to an absolute SFR address).
 */

#ifndef PIC18F_Q84_WWDT_H
#define PIC18F_Q84_WWDT_H

/* ===================================================================== */
/*  WDTCON0 - Watchdog Timer Control Register 0                          */
/*  Address: 0x0078                                                      */
/*                                                                       */
/*  Bit 7   - SEN: Software WDT Enable                                  */
/*      1 = WDT is enabled by software (effective only when WDTE         */
/*          configuration bits = 01, i.e. "SW controlled")               */
/*      0 = WDT is disabled by software                                  */
/*  Bits 6:5: Unimplemented, read as 0                                  */
/*  Bits 4:0 - PS[4:0]: WDT Prescaler (Period) Select                   */
/*      Selects the nominal WDT timeout period.  Values assume the       */
/*      31 kHz LFINTOSC clock source (~1 ms base period).                */
/*      00000 = 1:1        (~1 ms)                                       */
/*      00001 = 1:2        (~2 ms)                                       */
/*      00010 = 1:4        (~4 ms)                                       */
/*      00011 = 1:8        (~8 ms)                                       */
/*      00100 = 1:16       (~16 ms)                                      */
/*      00101 = 1:32       (~32 ms)                                      */
/*      00110 = 1:64       (~64 ms)                                      */
/*      00111 = 1:128      (~128 ms)                                     */
/*      01000 = 1:256      (~256 ms)                                     */
/*      01001 = 1:512      (~512 ms)                                     */
/*      01010 = 1:1024     (~1 s)                                        */
/*      01011 = 1:2048     (~2 s)                                        */
/*      01100 = 1:4096     (~4 s)                                        */
/*      01101 = 1:8192     (~8 s)                                        */
/*      01110 = 1:16384    (~16 s)                                       */
/*      01111 = 1:32768    (~32 s)                                       */
/*      10000 = 1:65536    (~65 s)                                       */
/*      10001 = 1:131072   (~131 s / ~2 min)                             */
/*      10010 = 1:262144   (~262 s / ~4 min)                             */
/*      10011-11111 = Reserved                                           */
/* ===================================================================== */
#define WDTCON0 SFR8(0x0078)

/* SEN - Software Watchdog Timer Enable (bit 7) */
#define WDTCON0_SEN (1u << 7)

/* PS - WDT Prescaler Select (bits 4:0) */
#define WDTCON0_PS4 (1u << 4) /* PS bit 4 */
#define WDTCON0_PS3 (1u << 3) /* PS bit 3 */
#define WDTCON0_PS2 (1u << 2) /* PS bit 2 */
#define WDTCON0_PS1 (1u << 1) /* PS bit 1 */
#define WDTCON0_PS0 (1u << 0) /* PS bit 0 */
#define WDTCON0_PS_MASK 0x1Fu /* Mask for PS[4:0] field */

/* PS field values - WDT prescaler ratio (nominal timeout @ 31 kHz) */
#define WDTPS_1 0x00u      /* 1:1        ~1 ms   */
#define WDTPS_2 0x01u      /* 1:2        ~2 ms   */
#define WDTPS_4 0x02u      /* 1:4        ~4 ms   */
#define WDTPS_8 0x03u      /* 1:8        ~8 ms   */
#define WDTPS_16 0x04u     /* 1:16       ~16 ms  */
#define WDTPS_32 0x05u     /* 1:32       ~32 ms  */
#define WDTPS_64 0x06u     /* 1:64       ~64 ms  */
#define WDTPS_128 0x07u    /* 1:128      ~128 ms */
#define WDTPS_256 0x08u    /* 1:256      ~256 ms */
#define WDTPS_512 0x09u    /* 1:512      ~512 ms */
#define WDTPS_1024 0x0Au   /* 1:1024     ~1 s    */
#define WDTPS_2048 0x0Bu   /* 1:2048     ~2 s    */
#define WDTPS_4096 0x0Cu   /* 1:4096     ~4 s    */
#define WDTPS_8192 0x0Du   /* 1:8192     ~8 s    */
#define WDTPS_16384 0x0Eu  /* 1:16384    ~16 s   */
#define WDTPS_32768 0x0Fu  /* 1:32768    ~32 s   */
#define WDTPS_65536 0x10u  /* 1:65536    ~65 s   */
#define WDTPS_131072 0x11u /* 1:131072   ~2 min  */
#define WDTPS_262144 0x12u /* 1:262144   ~4 min  */

/* ===================================================================== */
/*  WDTCON1 - Watchdog Timer Control Register 1                          */
/*  Address: 0x0079                                                      */
/*                                                                       */
/*  Bit 7   : Unimplemented, read as 0                                  */
/*  Bits 6:4 - CS[2:0]: WDT Clock Source Select                         */
/*      000 = LFINTOSC 31 kHz (default)                                  */
/*      001 = Reserved                                                   */
/*      010 = MFINTOSC 31.25 kHz                                         */
/*      011 = SOSC 32.768 kHz (secondary oscillator)                     */
/*      100 = Reserved                                                   */
/*      101 = Reserved                                                   */
/*      110 = Reserved                                                   */
/*      111 = Reserved                                                   */
/*  Bit 3   : Unimplemented, read as 0                                  */
/*  Bits 2:0 - WINDOW[2:0]: WDT Window Size Select                     */
/*      Defines the "closed" (early) portion of the WDT period.          */
/*      CLRWDT during the closed window triggers a reset.                */
/*      000 = 100% open window (no window restriction)                   */
/*      001 = 50% open  (first 50% of period is closed)                  */
/*      010 = 37.5% open (first 62.5% closed)                            */
/*      011 = 25% open  (first 75% closed)                               */
/*      100 = 18.75% open (first 81.25% closed)                          */
/*      101 = 12.5% open (first 87.5% closed)                            */
/*      110 = 6.25% open (first 93.75% closed)                           */
/*      111 = Reserved                                                   */
/* ===================================================================== */
#define WDTCON1 SFR8(0x0079)

/* CS - WDT Clock Source Select (bits 6:4) */
#define WDTCON1_CS2 (1u << 6) /* CS bit 2 */
#define WDTCON1_CS1 (1u << 5) /* CS bit 1 */
#define WDTCON1_CS0 (1u << 4) /* CS bit 0 */
#define WDTCON1_CS_MASK 0x70u /* Mask for CS[2:0] field */
#define WDTCON1_CS_SHIFT 4u   /* Bit position of CS[0] */

/* CS field values (pre-shifted to bits 6:4) */
#define WDTCS_LFINTOSC (0x00u << 4) /* LFINTOSC 31 kHz (default) */
#define WDTCS_MFINTOSC (0x02u << 4) /* MFINTOSC 31.25 kHz        */
#define WDTCS_SOSC (0x03u << 4)     /* SOSC 32.768 kHz           */

/* WINDOW - WDT Window Size Select (bits 2:0) */
#define WDTCON1_WINDOW2 (1u << 2) /* WINDOW bit 2 */
#define WDTCON1_WINDOW1 (1u << 1) /* WINDOW bit 1 */
#define WDTCON1_WINDOW0 (1u << 0) /* WINDOW bit 0 */
#define WDTCON1_WINDOW_MASK 0x07u /* Mask for WINDOW[2:0] field */

/* WINDOW field values (percentage of period that is "open") */
#define WDTWIN_100 0x00u   /* 100% open - window mode disabled */
#define WDTWIN_50 0x01u    /* 50% open   */
#define WDTWIN_37_5 0x02u  /* 37.5% open */
#define WDTWIN_25 0x03u    /* 25% open   */
#define WDTWIN_18_75 0x04u /* 18.75% open */
#define WDTWIN_12_5 0x05u  /* 12.5% open  */
#define WDTWIN_6_25 0x06u  /* 6.25% open  */

/* ===================================================================== */
/*  WDTPSL - Watchdog Timer Prescaler Count Low Byte                     */
/*  Address: 0x007A                                                      */
/*                                                                       */
/*  Bits 7:0 - PSCNTL[7:0]: Low byte of the current WDT prescaler       */
/*             count value (read-only).                                  */
/*                                                                       */
/*  Together with WDTPSH and bits in WDTTMR, this provides the full      */
/*  prescaler counter state for diagnostic purposes.                     */
/* ===================================================================== */
#define WDTPSL SFR8(0x007A)

/* No writable bit definitions - entire register is read-only */
#define WDTPSL_PSCNTL_MASK 0xFFu /* PSCNTL[7:0] read-only mask */

/* ===================================================================== */
/*  WDTPSH - Watchdog Timer Prescaler Count High Byte                    */
/*  Address: 0x007B                                                      */
/*                                                                       */
/*  Bits 7:0 - PSCNTH[7:0]: High byte of the current WDT prescaler      */
/*             count value (read-only).                                  */
/*                                                                       */
/*  Combined with WDTPSL this gives prescaler count bits 15:0.  The      */
/*  uppermost prescaler bits (17:16) are in WDTTMR.                      */
/* ===================================================================== */
#define WDTPSH SFR8(0x007B)

/* No writable bit definitions - entire register is read-only */
#define WDTPSH_PSCNTH_MASK 0xFFu /* PSCNTH[7:0] read-only mask */

/* ===================================================================== */
/*  WDTTMR - Watchdog Timer Register                                     */
/*  Address: 0x007C                                                      */
/*                                                                       */
/*  This register provides the upper WDT counter value and the current   */
/*  window state.  All fields are read-only.                             */
/*                                                                       */
/*  Bits 7:3 - TMR[4:0]: Current WDT timer count (upper 5 bits of the   */
/*             WDT counter value).                                       */
/*  Bits 2:1 - PSCNT[17:16]: Prescaler count bits 17 and 16 (extends    */
/*             the 16-bit prescaler count in WDTPSH:WDTPSL).            */
/*  Bit 0    - STATE: WDT Armed / Window State                          */
/*      1 = WDT is armed (window is closed); CLRWDT will properly        */
/*          clear the timer.                                             */
/*      0 = WDT is in the window-open period; CLRWDT during this time    */
/*          will cause a window-violation reset.                         */
/* ===================================================================== */
#define WDTTMR SFR8(0x007C)

/* TMR - WDT Timer Count, read-only (bits 7:3) */
#define WDTTMR_TMR4 (1u << 7) /* TMR bit 4 */
#define WDTTMR_TMR3 (1u << 6) /* TMR bit 3 */
#define WDTTMR_TMR2 (1u << 5) /* TMR bit 2 */
#define WDTTMR_TMR1 (1u << 4) /* TMR bit 1 */
#define WDTTMR_TMR0 (1u << 3) /* TMR bit 0 */
#define WDTTMR_TMR_MASK 0xF8u /* Mask for TMR[4:0] field */
#define WDTTMR_TMR_SHIFT 3u   /* Bit position of TMR[0] */

/* PSCNT - Prescaler count bits 17:16, read-only (bits 2:1) */
#define WDTTMR_PSCNT17 (1u << 2) /* PSCNT bit 17 */
#define WDTTMR_PSCNT16 (1u << 1) /* PSCNT bit 16 */
#define WDTTMR_PSCNT_MASK 0x06u  /* Mask for PSCNT[17:16] field */
#define WDTTMR_PSCNT_SHIFT 1u    /* Bit position of PSCNT[16] */

/* STATE - WDT Window State, read-only (bit 0) */
#define WDTTMR_STATE (1u << 0)
/*  1 = Window closed / WDT armed - CLRWDT clears the timer normally    */
/*  0 = Window open - CLRWDT causes a window-violation reset            */

#endif /* PIC18F_Q84_WWDT_H */
