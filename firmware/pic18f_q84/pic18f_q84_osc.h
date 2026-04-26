/*
 * pic18f_q84_osc.h
 *
 * OSC (Oscillator) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * The oscillator module provides the following clock sources:
 *   - HFINTOSC : High-Frequency Internal Oscillator, 1-64 MHz
 *   - LFINTOSC : Low-Frequency Internal Oscillator, 31 kHz
 *   - MFINTOSC : Medium-Frequency Internal, 500 kHz / 31.25 kHz
 *   - SOSC     : Secondary Oscillator, 32.768 kHz crystal
 *   - EXTOSC   : External Oscillator (crystal / resonator / EC)
 *   - 4xPLL    : Phase-Locked Loop (4x frequency multiplier)
 *
 * Features:
 *   - Run-time clock switching with automatic hold until ready
 *   - Active Clock Tuning (ACT) of HFINTOSC via reference clock
 *   - Three independent Fail-Safe Clock Monitors (FSCM):
 *       FOSC (system clock), Primary (EXTOSC), Secondary (SOSC)
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_OSC_H
#define PIC18F_Q84_OSC_H

/* ===================================================================
 * ACTCON - Active Clock Tuning Control Register
 * Address: 0x00D3
 *
 * Controls the Active Clock Tuning (ACT) engine, which automatically
 * adjusts the HFINTOSC frequency by writing to the OSCTUNE register
 * using a stable reference clock (USB SOF or SOSC).
 * ================================================================ */
#define ACTCON SFR8(0x00AC)

/* Bit 7 - ACTEN: Active Clock Tuning Enable
 *   1 = ACT enabled; HFINTOSC is continuously tuned against the
 *       selected reference clock source.
 *   0 = ACT disabled; HFINTOSC tuning controlled only by the
 *       software value in the OSCTUNE register.                   */
#define ACTCON_ACTEN 7

/* Bit 6 - ACTUD: ACT Update Disable
 *   1 = OSCTUNE register updates from the ACT engine are suspended
 *       (tuning value frozen at current setting).
 *   0 = ACT engine is allowed to update OSCTUNE automatically.
 *   Useful for temporarily halting automatic tuning adjustments.  */
#define ACTCON_ACTUD 6

/* Bits 5:4 - Reserved: read as 0, write as 0.                     */

/* Bit 3 - ACTLOCK: ACT Lock Status (read-only)
 *   1 = HFINTOSC frequency is within +/-1% of nominal; the ACT
 *       engine has achieved lock.
 *   0 = HFINTOSC has not yet converged to the target frequency.   */
#define ACTCON_ACTLOCK 3

/* Bit 2 - Reserved: read as 0, write as 0.                        */

/* Bit 1 - ACTORS: ACT Out-of-Range Status (read-only)
 *   1 = The required tuning value exceeds the range of OSCTUNE;
 *       the oscillator cannot be tuned to the target frequency.
 *   0 = Tuning value is within the representable OSCTUNE range.   */
#define ACTCON_ACTORS 1

/* Bit 0 - Reserved: read as 0, write as 0.                        */

/* ===================================================================
 * OSCCON1 - Oscillator Control Register 1
 * Address: 0x00D4
 *
 * Write the desired clock source (NOSC) and divider (NDIV) to
 * request a clock switch.  The hardware transitions to the new
 * configuration once the selected source is stable.  Current
 * active values are readable from OSCCON2.
 * ================================================================ */
#define OSCCON1 SFR8(0x00AD)

/* Bit 7 - Reserved: read as 0, write as 0.                        */

/* Bits 6:4 - NOSC[2:0]: New Oscillator Source Request
 *   Selects the target clock source for a clock switch:
 *   000 = EXTOSC with 4xPLL
 *   001 = Reserved
 *   010 = EXTOSC (external oscillator / EC, no PLL)
 *   011 = Reserved
 *   100 = HFINTOSC (high-frequency internal oscillator)
 *   101 = LFINTOSC (low-frequency internal, 31 kHz)
 *   110 = HFINTOSC with 4xPLL
 *   111 = EXTOSC in LP (low-power crystal) mode                  */
#define OSCCON1_NOSC2 6
#define OSCCON1_NOSC1 5
#define OSCCON1_NOSC0 4

/* NOSC field mask and position                                     */
#define OSCCON1_NOSC_MASK 0x70 /* bits 6:4 */
#define OSCCON1_NOSC_POS 4

/* Named source values for NOSC[2:0] (pre-shifted)                 */
#define NOSC_EXTOSC_PLL (0x00 << 4)   /* EXTOSC + 4xPLL         */
#define NOSC_EXTOSC (0x02 << 4)       /* EXTOSC, no PLL         */
#define NOSC_HFINTOSC (0x04 << 4)     /* HFINTOSC               */
#define NOSC_LFINTOSC (0x05 << 4)     /* LFINTOSC (31 kHz)      */
#define NOSC_HFINTOSC_PLL (0x06 << 4) /* HFINTOSC + 4xPLL       */
#define NOSC_EXTOSC_LP (0x07 << 4)    /* EXTOSC, LP mode        */

/* Bits 3:0 - NDIV[3:0]: New Clock Divider Request
 *   Selects the postscaler divider applied after the clock source:
 *   0000 = 1:1   (no division)
 *   0001 = 1:2
 *   0010 = 1:4
 *   0011 = 1:8
 *   0100 = 1:16
 *   0101 = 1:32
 *   0110 = 1:64
 *   0111 = 1:128
 *   1000 = 1:256
 *   1001 = 1:512
 *   1010-1111 = Reserved                                          */
#define OSCCON1_NDIV3 3
#define OSCCON1_NDIV2 2
#define OSCCON1_NDIV1 1
#define OSCCON1_NDIV0 0

/* NDIV field mask and position                                     */
#define OSCCON1_NDIV_MASK 0x0F /* bits 3:0 */
#define OSCCON1_NDIV_POS 0

/* Named divider values for NDIV[3:0]                               */
#define NDIV_1 0x00   /* 1:1   (no division)            */
#define NDIV_2 0x01   /* 1:2                            */
#define NDIV_4 0x02   /* 1:4                            */
#define NDIV_8 0x03   /* 1:8                            */
#define NDIV_16 0x04  /* 1:16                           */
#define NDIV_32 0x05  /* 1:32                           */
#define NDIV_64 0x06  /* 1:64                           */
#define NDIV_128 0x07 /* 1:128                          */
#define NDIV_256 0x08 /* 1:256                          */
#define NDIV_512 0x09 /* 1:512                          */

/* ===================================================================
 * OSCCON2 - Oscillator Control Register 2 (read-only)
 * Address: 0x00D5
 *
 * Reflects the CURRENT oscillator source and divider.  These fields
 * update automatically after a clock switch completes.  Compare
 * COSC/CDIV with NOSC/NDIV to detect a switch in progress.
 * ================================================================ */
#define OSCCON2 SFR8(0x00AE)

/* Bit 7 - Reserved: read as 0.                                    */

/* Bits 6:4 - COSC[2:0]: Current Oscillator Source (read-only)
 *   Encoding is identical to NOSC[2:0] in OSCCON1.
 *   000 = EXTOSC + 4xPLL
 *   010 = EXTOSC
 *   100 = HFINTOSC
 *   101 = LFINTOSC
 *   110 = HFINTOSC + 4xPLL
 *   111 = EXTOSC LP mode                                         */
#define OSCCON2_COSC2 6
#define OSCCON2_COSC1 5
#define OSCCON2_COSC0 4

/* COSC field mask and position                                     */
#define OSCCON2_COSC_MASK 0x70 /* bits 6:4 */
#define OSCCON2_COSC_POS 4

/* Bits 3:0 - CDIV[3:0]: Current Clock Divider (read-only)
 *   Encoding is identical to NDIV[3:0] in OSCCON1.               */
#define OSCCON2_CDIV3 3
#define OSCCON2_CDIV2 2
#define OSCCON2_CDIV1 1
#define OSCCON2_CDIV0 0

/* CDIV field mask and position                                     */
#define OSCCON2_CDIV_MASK 0x0F /* bits 3:0 */
#define OSCCON2_CDIV_POS 0

/* ===================================================================
 * OSCCON3 - Oscillator Control Register 3
 * Address: 0x00D6
 *
 * Provides clock-switch hold control, SOSC power mode selection,
 * and a ready flag indicating the switch has completed.
 * ================================================================ */
#define OSCCON3 SFR8(0x00AF)

/* Bit 7 - CSWHOLD: Clock Switch Hold
 *   1 = Hold the clock switch; the system continues running on the
 *       old source until software clears CSWHOLD (useful if the
 *       application must prepare before the switch occurs).
 *   0 = Allow the clock switch to proceed as soon as the new
 *       oscillator source is stable.                              */
#define OSCCON3_CSWHOLD 7

/* Bits 6:5 - Reserved: read as 0, write as 0.                     */

/* Bit 4 - SOSCPWR: Secondary Oscillator Power Mode Select
 *   1 = SOSC operates in high-power mode (better drive for
 *       higher-ESR crystals or longer PCB traces).
 *   0 = SOSC operates in low-power mode (lower current, suitable
 *       for low-ESR 32.768 kHz watch crystals).                   */
#define OSCCON3_SOSCPWR 4

/* Bits 3:1 - Reserved: read as 0, write as 0.                     */

/* Bit 0 - ORDY: Oscillator Ready (read-only)
 *   1 = Clock switch is complete; the new oscillator is running
 *       and providing the system clock.
 *   0 = Clock switch is still pending (new source not yet stable
 *       or CSWHOLD is set).                                       */
#define OSCCON3_ORDY 0

/* ===================================================================
 * OSCTUNE - HFINTOSC Frequency Tuning Register
 * Address: 0x00D7
 *
 * Allows fine adjustment of the HFINTOSC frequency.  The value is
 * 6-bit two's complement: positive values increase frequency,
 * negative values decrease it.  The ACT engine writes this register
 * automatically when enabled.
 * ================================================================ */
#define OSCTUNE SFR8(0x00B0)

/* Bits 7:6 - Reserved: read as 0, write as 0.                     */

/* Bits 5:0 - TUN[5:0]: HFINTOSC Tuning Value (6-bit two's complement)
 *   0x00       = Factory-calibrated nominal frequency.
 *   0x01-0x1F  = Increase frequency (positive offset).
 *   0x20-0x3F  = Decrease frequency (negative offset in 2's comp).
 *   Each step adjusts frequency by approximately 0.25% (typical). */
#define OSCTUNE_TUN5 5
#define OSCTUNE_TUN4 4
#define OSCTUNE_TUN3 3
#define OSCTUNE_TUN2 2
#define OSCTUNE_TUN1 1
#define OSCTUNE_TUN0 0

/* TUN field mask and position                                      */
#define OSCTUNE_TUN_MASK 0x3F /* bits 5:0 */
#define OSCTUNE_TUN_POS 0

/* ===================================================================
 * OSCFRQ - HFINTOSC Frequency Selection Register
 * Address: 0x00D8
 *
 * Selects the nominal output frequency of HFINTOSC.  The actual
 * output is this frequency divided by CDIV and optionally
 * multiplied by the 4xPLL.
 * ================================================================ */
#define OSCFRQ SFR8(0x00B1)

/* Bits 7:4 - Reserved: read as 0, write as 0.                     */

/* Bits 3:0 - FRQ[3:0]: HFINTOSC Frequency Select
 *   0000 =  1 MHz
 *   0001 =  2 MHz
 *   0010 =  4 MHz
 *   0011 =  8 MHz  (default after Reset)
 *   0100 = 12 MHz
 *   0101 = 16 MHz
 *   0110 = 32 MHz
 *   0111 = 48 MHz
 *   1000 = 64 MHz
 *   1001-1111 = Reserved                                          */
#define OSCFRQ_FRQ3 3
#define OSCFRQ_FRQ2 2
#define OSCFRQ_FRQ1 1
#define OSCFRQ_FRQ0 0

/* FRQ field mask and position                                      */
#define OSCFRQ_FRQ_MASK 0x0F /* bits 3:0 */
#define OSCFRQ_FRQ_POS 0

/* Named frequency values for FRQ[3:0]                              */
#define FRQ_1MHZ 0x00  /*  1 MHz                        */
#define FRQ_2MHZ 0x01  /*  2 MHz                        */
#define FRQ_4MHZ 0x02  /*  4 MHz                        */
#define FRQ_8MHZ 0x03  /*  8 MHz (POR default)          */
#define FRQ_12MHZ 0x04 /* 12 MHz                        */
#define FRQ_16MHZ 0x05 /* 16 MHz                        */
#define FRQ_32MHZ 0x06 /* 32 MHz                        */
#define FRQ_48MHZ 0x07 /* 48 MHz                        */
#define FRQ_64MHZ 0x08 /* 64 MHz                        */

/* ===================================================================
 * OSCSTAT - Oscillator Status Register (read-only)
 * Address: 0x00D9
 *
 * Each bit indicates whether the corresponding oscillator source
 * is stable and ready for use.  Poll the appropriate bit before
 * switching to that source, or rely on CSWHOLD / ORDY.
 * ================================================================ */
#define OSCSTAT SFR8(0x00B2)

/* Bit 7 - EXTOR: External Oscillator Ready
 *   1 = EXTOSC is running and stable (start-up timer expired).
 *   0 = EXTOSC not ready or not enabled.                          */
#define OSCSTAT_EXTOR 7

/* Bit 6 - HFOR: HFINTOSC Oscillator Ready
 *   1 = HFINTOSC is running and within +/-1% of selected frequency.
 *   0 = HFINTOSC not yet stable.                                  */
#define OSCSTAT_HFOR 6

/* Bit 5 - MFOR: MFINTOSC Oscillator Ready
 *   1 = MFINTOSC (500 kHz / 31.25 kHz) is running and stable.
 *   0 = MFINTOSC not yet stable or not enabled.                   */
#define OSCSTAT_MFOR 5

/* Bit 4 - LFOR: LFINTOSC Oscillator Ready
 *   1 = LFINTOSC (31 kHz) is running and stable.
 *   0 = LFINTOSC not yet stable.                                  */
#define OSCSTAT_LFOR 4

/* Bit 3 - SOR: Secondary Oscillator Ready
 *   1 = SOSC (32.768 kHz) is running and stable.
 *   0 = SOSC not ready or not enabled.                            */
#define OSCSTAT_SOR 3

/* Bit 2 - ADOR: ADC Dedicated RC Oscillator Ready
 *   1 = ADC internal RC oscillator (ADCRC) is running and stable.
 *   0 = ADCRC not ready or not enabled.                           */
#define OSCSTAT_ADOR 2

/* Bit 1 - PLLR: 4xPLL Ready
 *   1 = 4xPLL is locked and providing a stable output.
 *   0 = PLL not locked, not enabled, or input source not ready.   */
#define OSCSTAT_PLLR 1

/* Bit 0 - Unimplemented: read as 0.                               */

/* ===================================================================
 * OSCEN - Oscillator Manual Enable Register
 * Address: 0x00DA
 *
 * Allows software to force-enable individual oscillator sources
 * independently of whether they are selected as the system clock.
 * Useful when a peripheral (Timer1, SOSC-clocked RTCC, etc.)
 * requires a source that is not the active Fosc.
 * ================================================================ */
#define OSCEN SFR8(0x00B3)

/* Bit 7 - EXTOEN: External Oscillator Manual Enable
 *   1 = Force EXTOSC enabled regardless of NOSC/COSC selection.
 *   0 = EXTOSC enabled only when selected as system clock source. */
#define OSCEN_EXTOEN 7

/* Bit 6 - HFOEN: HFINTOSC Manual Enable
 *   1 = Force HFINTOSC enabled regardless of system clock selection.
 *   0 = HFINTOSC enabled automatically when required.            */
#define OSCEN_HFOEN 6

/* Bit 5 - MFOEN: MFINTOSC Manual Enable
 *   1 = Force MFINTOSC enabled.
 *   0 = MFINTOSC enabled only when required by a peripheral.     */
#define OSCEN_MFOEN 5

/* Bit 4 - LFOEN: LFINTOSC Manual Enable
 *   1 = Force LFINTOSC enabled.
 *   0 = LFINTOSC enabled automatically when required.            */
#define OSCEN_LFOEN 4

/* Bit 3 - SOSCEN: Secondary Oscillator Manual Enable
 *   1 = Force SOSC enabled (crystal oscillator on SOSCI/SOSCO).
 *   0 = SOSC enabled only when required by Timer1/RTCC etc.      */
#define OSCEN_SOSCEN 3

/* Bit 2 - ADOEN: ADC RC Oscillator Manual Enable
 *   1 = Force the dedicated ADC RC oscillator (ADCRC) enabled.
 *   0 = ADCRC enabled automatically when ADC requires it.        */
#define OSCEN_ADOEN 2

/* Bit 1 - PLLEN: 4xPLL Manual Enable
 *   1 = Force 4xPLL enabled when EXTOSC is running.
 *   0 = PLL enabled only when selected via NOSC[2:0].            */
#define OSCEN_PLLEN 1

/* Bit 0 - Unimplemented: read as 0.                               */

/* ===================================================================
 * FSCMCON - Fail-Safe Clock Monitor Control Register
 * Address: 0x0458
 *
 * Provides status and fault-injection control for the three
 * independent Fail-Safe Clock Monitors:
 *
 *   FSCMF - FOSC monitor   : watches the active system clock
 *   FSCMP - Primary monitor : watches EXTOSC
 *   FSCMS - Secondary monitor : watches SOSC
 *
 * When a clock failure is detected the corresponding event flag is
 * set.  For the FOSC monitor the system automatically switches to
 * HFINTOSC as a safe fallback.
 *
 * Fault injection bits allow software testing of FSCM behaviour;
 * writing 1 simulates a failure on the target source.
 * ================================================================ */
#define FSCMCON SFR8(0x0458)

/* Bit 7 - FSCMSFI: Secondary FSCM Fault Injection
 *   Write 1 = Inject a simulated SOSC failure event for testing.
 *   Self-clearing (hardware resets to 0 after injection).         */
#define FSCMCON_FSCMSFI 7

/* Bit 6 - FSCMSEV: Secondary FSCM Event Flag (read-only)
 *   1 = SOSC failure detected by the secondary Fail-Safe monitor.
 *   0 = No SOSC failure detected.
 *   Cleared by hardware when SOSC recovers, or by a device Reset. */
#define FSCMCON_FSCMSEV 6

/* Bit 5 - FSCMPFI: Primary FSCM Fault Injection
 *   Write 1 = Inject a simulated EXTOSC failure event for testing.
 *   Self-clearing.                                                */
#define FSCMCON_FSCMPFI 5

/* Bit 4 - FSCMPEV: Primary FSCM Event Flag (read-only)
 *   1 = EXTOSC failure detected by the primary Fail-Safe monitor.
 *   0 = No EXTOSC failure detected.                              */
#define FSCMCON_FSCMPEV 4

/* Bit 3 - FSCMFFI: FOSC FSCM Fault Injection
 *   Write 1 = Inject a simulated system clock failure event.
 *   This triggers the FOSC Fail-Safe response: the hardware
 *   immediately switches the system clock to HFINTOSC.
 *   Self-clearing.                                                */
#define FSCMCON_FSCMFFI 3

/* Bit 2 - FSCMFEV: FOSC FSCM Event Flag (read-only)
 *   1 = System clock (FOSC) failure detected; the hardware has
 *       automatically switched to HFINTOSC as a fallback.
 *   0 = No system clock failure detected.                         */
#define FSCMCON_FSCMFEV 2

/* Bits 1:0 - Reserved: read as 0, write as 0.                     */

#endif /* PIC18F_Q84_OSC_H */
