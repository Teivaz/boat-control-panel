/**
 * @file pic18f_q84_adc.h
 * @brief ADC (Analog-to-Digital Converter with Computation) register
 *        definitions for PIC18F27/47/57Q84 microcontrollers.
 *
 * The ADCC module is a 12-bit successive-approximation ADC enhanced with
 * hardware computation and context switching.  Key features:
 *
 *   - 12-bit conversion resolution (right- or left-justified)
 *   - Up to 43 external analog input channels plus 5 internal channels
 *     (temperature indicator, DAC1, FVR, VSS, AVSS)
 *   - Four independent contexts (CTX0-CTX3), each with its own channel
 *     select, acquisition time, precharge, and threshold settings;
 *     hardware can auto-switch between contexts on trigger events
 *   - Automated computation modes:
 *       * Basic (single conversion)
 *       * Accumulate (sum N conversions into 18-bit accumulator)
 *       * Average (right-shift accumulated sum)
 *       * Burst Average (rapid back-to-back accumulate + average)
 *       * Low-Pass Filter (single-pole IIR digital filter)
 *   - Threshold comparison with programmable upper/lower bounds and
 *     configurable interrupt on in-window, out-of-window, above, or below
 *   - Error calculation (difference between result and setpoint or filter)
 *   - Dedicated ADC RC oscillator (ADCRC) for clock-independent operation
 *   - Hardware Capacitive Voltage Divider (CVD) support with precharge
 *     polarity control, guard ring output, and double-sample mode
 *   - Auto-conversion trigger from timers, CCP, comparators, CLC, etc.
 *   - Integrated charge pump for high-impedance source support
 *
 * Typical usage (basic single conversion):
 *   1. Configure clock source: ADCON0.CS=0, ADCLK.CS = divider value
 *      (or ADCON0.CS=1 for dedicated ADCRC oscillator)
 *   2. Select input channel: ADPCH = desired channel code
 *   3. Select voltage reference: ADREF.PREF, ADREF.NREF
 *   4. Set result format: ADCON0.FM (1=right-justified, 0=left)
 *   5. Set acquisition time: ADACQ = minimum acquisition count
 *   6. Enable the ADC: ADCON0.ON = 1
 *   7. Start conversion: ADCON0.GO = 1
 *   8. Wait for GO to clear (poll or use interrupt)
 *   9. Read result from ADRES (ADRESH:ADRESL)
 *
 * Typical usage (accumulate/average with threshold):
 *   1. Steps 1-5 as above
 *   2. Set computation mode: ADCON2.MD = desired mode
 *   3. Set repeat count: ADRPT = N (number of samples)
 *   4. Set right-shift: ADCON2.CRS for averaging divisor
 *   5. Configure thresholds: ADLTH, ADUTH, ADSTPT
 *   6. Configure threshold interrupt mode: ADCON3.TMD
 *   7. Enable, start, and process results as above
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 29: ADC with Computation and Context Switching
 *
 * @note SFR8(addr) and SFR16(addr) macros must be defined before
 *       including this header (typically via pic18f_q84.h).
 * @note All register addresses are absolute.  Multi-byte registers
 *       are little-endian (low byte at base address).
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_ADC_H
#define PIC18F_Q84_ADC_H

/* ============================================================================
 * ADC Charge Pump
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ADCP - ADC Charge Pump Control Register (0x03D7)
 *
 * Controls the integrated charge pump that provides a boosted voltage rail
 * for sampling high-impedance analog sources.  The charge pump should be
 * enabled and allowed to stabilize (CPRDY=1) before starting conversions
 * that require it.
 *
 * Bit 7    CPON    Charge Pump Enable
 *                    1 = Charge pump is enabled
 *                    0 = Charge pump is disabled (default)
 * Bits 6:1 --      Reserved
 * Bit 0    CPRDY   Charge Pump Ready (read-only)
 *                    1 = Charge pump output voltage is stable
 *                    0 = Charge pump is not ready (still ramping up)
 * ------------------------------------------------------------------------- */
#define ADCP SFR8(0x03D7)

#define ADCP_CPON (1u << 7)  /**< Charge pump enable              */
#define ADCP_CPRDY (1u << 0) /**< Charge pump ready (read-only)   */

/* ============================================================================
 * ADC Threshold Registers
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ADLTH - ADC Lower Threshold Register (0x03D8, 16-bit)
 *
 * Lower bound for threshold comparison.  When threshold mode (ADCON3.TMD)
 * is configured for window or boundary checking, the computed error value
 * (ADERR) is compared against this register.
 *
 * ADLTHH (0x03D9):  Bits 7:0  ADLTH[15:8]  High byte
 * ADLTHL (0x03D8):  Bits 7:0  ADLTH[7:0]   Low byte
 * ------------------------------------------------------------------------- */
#define ADLTHL SFR8(0x03D8)
#define ADLTHH SFR8(0x03D9)
#define ADLTH SFR16(0x03D8) /**< 16-bit lower threshold       */

/* ---------------------------------------------------------------------------
 * ADUTH - ADC Upper Threshold Register (0x03DA, 16-bit)
 *
 * Upper bound for threshold comparison.  Used in conjunction with ADLTH
 * for window-mode detection (in-window or out-of-window interrupts).
 *
 * ADUTHH (0x03DB):  Bits 7:0  ADUTH[15:8]  High byte
 * ADUTHL (0x03DA):  Bits 7:0  ADUTH[7:0]   Low byte
 * ------------------------------------------------------------------------- */
#define ADUTHL SFR8(0x03DA)
#define ADUTHH SFR8(0x03DB)
#define ADUTH SFR16(0x03DA) /**< 16-bit upper threshold       */

/* ---------------------------------------------------------------------------
 * ADSTPT - ADC Setpoint Register (0x03DF, 16-bit)
 *
 * Reference setpoint for error calculation.  ADCON3.CALC selects whether
 * the error is computed as (actual - setpoint), (actual - filtered),
 * (filtered - setpoint), or (first derivative of actual).
 *
 * ADSTPTH (0x03E0):  Bits 7:0  ADSTPT[15:8]  High byte
 * ADSTPTL (0x03DF):  Bits 7:0  ADSTPT[7:0]   Low byte
 * ------------------------------------------------------------------------- */
#define ADSTPTL SFR8(0x03DF)
#define ADSTPTH SFR8(0x03E0)
#define ADSTPT SFR16(0x03DF) /**< 16-bit setpoint              */

/* ---------------------------------------------------------------------------
 * ADERR - ADC Error/Setpoint Difference Register (0x03DD, 16-bit)
 *
 * Contains the result of the error calculation selected by ADCON3.CALC.
 * This is a signed value representing the difference between two quantities
 * (e.g. actual result minus setpoint).  The threshold comparison logic
 * in ADCON3.TMD operates on this value.
 *
 * ADERRH (0x03DE):  Bits 7:0  ADERR[15:8]  High byte
 * ADERRL (0x03DD):  Bits 7:0  ADERR[7:0]   Low byte
 * ------------------------------------------------------------------------- */
#define ADERRL SFR8(0x03DD)
#define ADERRH SFR8(0x03DE)
#define ADERR SFR16(0x03DD) /**< 16-bit error value           */

/* ============================================================================
 * ADC Computation Result Registers
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ADFLTR - ADC Filter/Average Result Register (0x03E1, 16-bit)
 *
 * In Average or Burst Average mode, this register contains the averaged
 * result (accumulator right-shifted by CRS bits).  In Low-Pass Filter
 * mode, this register holds the current filter output value.
 *
 * ADFLTRH (0x03E2):  Bits 7:0  ADFLTR[15:8]  High byte
 * ADFLTRL (0x03E1):  Bits 7:0  ADFLTR[7:0]   Low byte
 * ------------------------------------------------------------------------- */
#define ADFLTRL SFR8(0x03E1)
#define ADFLTRH SFR8(0x03E2)
#define ADFLTR SFR16(0x03E1) /**< 16-bit filter/average result */

/* ---------------------------------------------------------------------------
 * ADACC - ADC Accumulator Register (0x03E3-0x03E5, 18-bit)
 *
 * The 18-bit accumulator that sums conversion results in Accumulate,
 * Average, Burst Average, and Low-Pass Filter modes.  The upper byte
 * (ADACCU) only uses bits 1:0 for the two MSBs of the 18-bit value.
 *
 * ADACCU (0x03E5):
 *   Bits 7:2  -- Reserved (read as 0, write as 0)
 *   Bits 1:0  ACC[17:16]  Upper 2 bits of accumulator
 *
 * ADACCH (0x03E4):
 *   Bits 7:0  ACC[15:8]   High byte of accumulator
 *
 * ADACCL (0x03E3):
 *   Bits 7:0  ACC[7:0]    Low byte of accumulator
 * ------------------------------------------------------------------------- */
#define ADACCL SFR8(0x03E3)
#define ADACCH SFR8(0x03E4)
#define ADACCU SFR8(0x03E5)
#define ADACC SFR16(0x03E3) /**< 16-bit access (low 16 bits) */

#define ADACCU_MASK 0x03u /**< ACC[17:16] valid bits in U      */

/* ---------------------------------------------------------------------------
 * ADCNT - ADC Accumulation Count Register (0x03E6)
 *
 * Tracks the number of conversions accumulated so far.  Incremented by
 * hardware after each conversion in Accumulate/Average/Filter modes.
 * Rolls over at 255; software should check for overflow via ADSTAT.AOV.
 *
 * Bits 7:0  CNT[7:0]  Current accumulation count (0-255)
 * ------------------------------------------------------------------------- */
#define ADCNT SFR8(0x03E6)

/* ---------------------------------------------------------------------------
 * ADRPT - ADC Repeat Count Register (0x03E7)
 *
 * Sets the number of conversions to perform per trigger event.  In
 * Accumulate/Average modes, this determines how many samples are summed
 * before the computation interrupt fires.  A value of 0 means 1 conversion
 * (no repeat); a value of N means (N+1) conversions for Burst Average, or
 * N additional conversions for other modes -- see datasheet for exact
 * behavior per mode.
 *
 * Bits 7:0  RPT[7:0]  Repeat count (0-255)
 * ------------------------------------------------------------------------- */
#define ADRPT SFR8(0x03E7)

/* ============================================================================
 * ADC Result Registers
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ADPREV - ADC Previous Result Register (0x03E8, 16-bit)
 *
 * Holds the result of the previous conversion.  Updated automatically
 * by hardware before each new conversion result is written to ADRES.
 * Used internally for first-derivative (CALC=000) error calculation.
 *
 * ADPREVH (0x03E9):  Bits 7:0  ADPREV[15:8]  High byte
 * ADPREVL (0x03E8):  Bits 7:0  ADPREV[7:0]   Low byte
 * ------------------------------------------------------------------------- */
#define ADPREVL SFR8(0x03E8)
#define ADPREVH SFR8(0x03E9)
#define ADPREV SFR16(0x03E8) /**< 16-bit previous result       */

/* ---------------------------------------------------------------------------
 * ADRES - ADC Result Register (0x03EA, 16-bit)
 *
 * Contains the most recent conversion result.  The format depends on
 * ADCON0.FM:
 *   FM=1 (right-justified): bits 11:0 contain the 12-bit result, bits
 *         15:12 are zero (or sign-extended in 2's complement modes).
 *   FM=0 (left-justified):  bits 15:4 contain the 12-bit result, bits
 *         3:0 are zero.
 *
 * This register is updated by hardware at the end of each conversion.
 *
 * ADRESH (0x03EB):  Bits 7:0  ADRES[15:8]  High byte
 * ADRESL (0x03EA):  Bits 7:0  ADRES[7:0]   Low byte
 * ------------------------------------------------------------------------- */
#define ADRESL SFR8(0x03EA)
#define ADRESH SFR8(0x03EB)
#define ADRES SFR16(0x03EA) /**< 16-bit conversion result     */

/* ============================================================================
 * ADC Channel and Timing Configuration
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ADPCH - ADC Positive Channel Select Register (0x03EC)
 *
 * Selects the analog input channel connected to the ADC positive sample-
 * and-hold input.  External channels correspond to device I/O pins;
 * internal channels provide on-chip voltage references.
 *
 * Bits 7:6  -- Reserved
 * Bits 5:0  PCH[5:0]  Positive Channel Select
 *                       0x00-0x2A = ANA0 through ANA42 (external pins,
 *                                   device/package dependent)
 *                       0x3A      = DAC1 output
 *                       0x3B      = Temperature Indicator
 *                       0x3C      = FVR Buffer 1
 *                       0x3D      = VSS (ground reference)
 *                       0x3E-0x3F = Reserved
 *
 * @note Not all external channels are available on all pin counts.
 *       Refer to DS40002213D pin tables for package-specific availability.
 * ------------------------------------------------------------------------- */
#define ADPCH SFR8(0x03EC)

#define ADPCH_PCH_MASK 0x3Fu /**< PCH[5:0] bit mask              */
#define ADPCH_PCH_SHIFT 0u   /**< PCH[5:0] bit position          */

/** Internal channel codes for ADPCH */
#define ADPCH_DAC1 0x3Au /**< DAC1 output                    */
#define ADPCH_TEMP 0x3Bu /**< Temperature indicator          */
#define ADPCH_FVR 0x3Cu  /**< FVR Buffer 1 output            */
#define ADPCH_VSS 0x3Du  /**< VSS (analog ground)            */

/* ---------------------------------------------------------------------------
 * ADACQ - ADC Acquisition Time Register (0x03EE, 16-bit)
 *
 * Sets the minimum acquisition (sampling) time in units of ADC clock
 * periods.  The acquisition phase begins when GO is set and lasts for
 * at least ACQ clock periods before conversion starts.  Longer times
 * are needed for high-impedance sources.
 *
 * ADACQH (0x03EF):
 *   Bits 7:5  -- Reserved
 *   Bits 4:0  ACQ[12:8]  Upper 5 bits of acquisition time
 *
 * ADACQL (0x03EE):
 *   Bits 7:0  ACQ[7:0]   Lower 8 bits of acquisition time
 *
 * Valid range: 0 to 8191 ADC clock periods (13-bit value).
 * ------------------------------------------------------------------------- */
#define ADACQL SFR8(0x03EE)
#define ADACQH SFR8(0x03EF)
#define ADACQ SFR16(0x03EE) /**< 16-bit acquisition time     */

#define ADACQH_MASK 0x1Fu /**< ACQ[12:8] valid bits in H       */
#define ADACQ_MAX 0x1FFFu /**< Maximum acquisition time value  */

/* ---------------------------------------------------------------------------
 * ADCAP - ADC Additional Sample Capacitor Register (0x03F0)
 *
 * Adds internal capacitance to the ADC sample-and-hold circuit to aid
 * in Capacitive Voltage Divider (CVD) measurements.  The additional
 * capacitance improves CVD measurement accuracy for low-capacitance
 * touch sensors.
 *
 * Bits 7:5  -- Reserved
 * Bits 4:0  CAP[4:0]  Additional Capacitor Select
 *                       Each bit adds a specific capacitance step.
 *                       The total additional capacitance is the sum of
 *                       the enabled steps (binary-weighted).
 * ------------------------------------------------------------------------- */
#define ADCAP SFR8(0x03F0)

#define ADCAP_CAP_MASK 0x1Fu /**< CAP[4:0] bit mask              */
#define ADCAP_CAP_SHIFT 0u   /**< CAP[4:0] bit position          */

/* ---------------------------------------------------------------------------
 * ADPRE - ADC Precharge Time Register (0x03F1, 16-bit)
 *
 * Sets the precharge phase duration in ADC clock periods for CVD
 * operation.  During precharge, the internal sample capacitor is
 * connected to VDD or VSS (per ADCON1.PPOL) to establish a known
 * starting voltage before the acquisition phase begins.
 *
 * ADPREH (0x03F2):
 *   Bits 7:5  -- Reserved
 *   Bits 4:0  PRE[12:8]  Upper 5 bits of precharge time
 *
 * ADPREL (0x03F1):
 *   Bits 7:0  PRE[7:0]   Lower 8 bits of precharge time
 *
 * Valid range: 0 to 8191 ADC clock periods (13-bit value).
 * A value of 0 disables the precharge phase.
 * ------------------------------------------------------------------------- */
#define ADPREL SFR8(0x03F1)
#define ADPREH SFR8(0x03F2)
#define ADPRE SFR16(0x03F1) /**< 16-bit precharge time       */

#define ADPREH_MASK 0x1Fu /**< PRE[12:8] valid bits in H       */
#define ADPRE_MAX 0x1FFFu /**< Maximum precharge time value    */

/* ============================================================================
 * ADC Control Registers
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ADCON0 - ADC Control Register 0 (0x03F3)
 *
 * Primary control register: enable, start conversion, select clock
 * source and result format.
 *
 * Bit 7    ON      ADC Enable
 *                    1 = ADC module is powered on and ready for conversion
 *                    0 = ADC is disabled; all analog circuitry is shut down
 *                        to minimize power consumption
 * Bit 6    CONT    Continuous Conversion Enable
 *                    1 = Automatically restart conversion after each
 *                        completion (auto-triggered loop)
 *                    0 = Single-shot mode; one conversion per GO trigger
 * Bit 5    CS      Clock Source Select
 *                    1 = Dedicated ADC RC oscillator (ADCRC, ~600 kHz);
 *                        conversion timing is independent of system clock
 *                    0 = FOSC-derived clock, divided by ADCLK.CS value
 * Bit 4    FM      Result Format
 *                    1 = Right-justified (12-bit result in bits 11:0)
 *                    0 = Left-justified  (12-bit result in bits 15:4)
 * Bit 3    --      Reserved
 * Bit 2    GO      Start Conversion / Busy Status
 *                    Write 1 to start a conversion.  Hardware clears this
 *                    bit when the conversion completes.  Writing 0 during
 *                    a conversion aborts it.
 * Bits 1:0 --      Reserved
 * ------------------------------------------------------------------------- */
#define ADCON0 SFR8(0x03F3)

#define ADCON0_ON (1u << 7)   /**< ADC enable                      */
#define ADCON0_CONT (1u << 6) /**< Continuous conversion enable    */
#define ADCON0_CS (1u << 5)   /**< Clock source (1=ADCRC, 0=FOSC) */
#define ADCON0_FM (1u << 4)   /**< Result format (1=right-just)    */
#define ADCON0_GO (1u << 2)   /**< Start conversion / busy flag    */

/* ---------------------------------------------------------------------------
 * ADCON1 - ADC Control Register 1 (0x03F4)
 *
 * Controls CVD (Capacitive Voltage Divider) operating parameters.
 *
 * Bit 7    PPOL    Precharge Polarity
 *                    1 = Precharge internal sample cap to VDD
 *                    0 = Precharge internal sample cap to VSS
 * Bit 6    --      Reserved
 * Bit 5    GPOL    Guard Ring Output Polarity
 *                    1 = Guard ring output driven high during acquisition
 *                    0 = Guard ring output driven low during acquisition
 * Bit 4    DSEN    Double-Sample Enable
 *                    1 = Double-sample mode: two successive samples are
 *                        taken with opposite precharge polarity; the ADC
 *                        result is the difference.  Improves CVD accuracy
 *                        by canceling common-mode offsets.
 *                    0 = Single-sample mode (normal operation)
 * Bits 3:0 --      Reserved
 * ------------------------------------------------------------------------- */
#define ADCON1 SFR8(0x03F4)

#define ADCON1_PPOL (1u << 7) /**< Precharge polarity (1=VDD)      */
#define ADCON1_GPOL (1u << 5) /**< Guard ring output polarity      */
#define ADCON1_DSEN (1u << 4) /**< Double-sample enable            */

/* ---------------------------------------------------------------------------
 * ADCON2 - ADC Control Register 2 (0x03F5)
 *
 * Configures the computation engine: right-shift amount for averaging
 * and the computation operating mode.
 *
 * Bit 7    PSIS    Precharge Source Input Select
 *                    1 = Positive channel connected during precharge phase
 *                    0 = Positive channel disconnected during precharge
 * Bits 6:4 CRS[2:0]  Computation Right Shift
 *                    Number of bit positions to right-shift the accumulator
 *                    when computing the average/filter result.  The divisor
 *                    is 2^CRS:
 *                      000 = divide by 1   (no shift)
 *                      001 = divide by 2
 *                      010 = divide by 4
 *                      011 = divide by 8
 *                      100 = divide by 16
 *                      101 = divide by 32
 *                      110 = divide by 64
 *                      111 = reserved
 * Bit 3    --      Reserved
 * Bits 2:0 MD[2:0]  Computation Mode Select
 *                      000 = Basic mode (single conversion, no computation)
 *                      001 = Accumulate mode (sum N conversions)
 *                      010 = Average mode (accumulate then right-shift)
 *                      011 = Burst Average (rapid back-to-back average)
 *                      100 = Low-Pass Filter (single-pole IIR filter)
 *                      101-111 = Reserved
 * ------------------------------------------------------------------------- */
#define ADCON2 SFR8(0x03F5)

#define ADCON2_PSIS (1u << 7) /**< Precharge source input select   */

#define ADCON2_CRS_MASK 0x70u /**< CRS[2:0] bit mask              */
#define ADCON2_CRS_SHIFT 4u   /**< CRS[2:0] bit position          */

#define ADCON2_MD_MASK 0x07u /**< MD[2:0] bit mask               */
#define ADCON2_MD_SHIFT 0u   /**< MD[2:0] bit position           */

/** Computation mode values for ADCON2 MD[2:0] */
#define ADCON2_MD_BASIC 0x00u /**< Basic single conversion         */
#define ADCON2_MD_ACC 0x01u   /**< Accumulate mode                 */
#define ADCON2_MD_AVG 0x02u   /**< Average mode                    */
#define ADCON2_MD_BURST 0x03u /**< Burst Average mode              */
#define ADCON2_MD_LPF 0x04u   /**< Low-Pass Filter mode            */

/** Right-shift values for ADCON2 CRS[2:0] (shifted to bit position) */
#define ADCON2_CRS_DIV1 (0x00u << 4)  /**< Divide by 1 (no shift)        */
#define ADCON2_CRS_DIV2 (0x01u << 4)  /**< Divide by 2                   */
#define ADCON2_CRS_DIV4 (0x02u << 4)  /**< Divide by 4                   */
#define ADCON2_CRS_DIV8 (0x03u << 4)  /**< Divide by 8                   */
#define ADCON2_CRS_DIV16 (0x04u << 4) /**< Divide by 16                  */
#define ADCON2_CRS_DIV32 (0x05u << 4) /**< Divide by 32                  */
#define ADCON2_CRS_DIV64 (0x06u << 4) /**< Divide by 64                  */

/* ---------------------------------------------------------------------------
 * ADCON3 - ADC Control Register 3 (0x03F6)
 *
 * Configures the error calculation method, threshold interrupt mode,
 * and stop-on-interrupt behavior.
 *
 * Bit 7    SOI     Stop On Interrupt
 *                    1 = ADC halts (clears GO) when a threshold interrupt
 *                        condition is met; requires software to restart
 *                    0 = ADC continues converting even when threshold fires
 * Bits 6:4 CALC[2:0]  Error Calculation Select
 *                      Determines which values are subtracted to produce
 *                      the error result in ADERR:
 *                        000 = First derivative of actual result
 *                              (ADERR = ADRES - ADPREV)
 *                        001 = Actual result vs. setpoint
 *                              (ADERR = ADRES - ADSTPT)
 *                        010 = Actual result vs. filtered value
 *                              (ADERR = ADRES - ADFLTR)
 *                        011 = Filtered value vs. setpoint
 *                              (ADERR = ADFLTR - ADSTPT)
 *                        100-111 = Reserved
 * Bit 3    --      Reserved
 * Bits 2:0 TMD[2:0]  Threshold Interrupt Mode
 *                      Determines when the ADC threshold interrupt fires,
 *                      based on comparison of ADERR against ADLTH/ADUTH:
 *                        000 = Threshold interrupt disabled (never fires)
 *                        001 = ADERR < ADLTH  (below lower threshold)
 *                        010 = ADERR >= ADLTH (at or above lower threshold)
 *                        011 = ADERR is in window (ADLTH <= ADERR < ADUTH)
 *                        100 = ADERR is out of window
 *                              (ADERR < ADLTH or ADERR >= ADUTH)
 *                        101 = ADERR < ADUTH  (below upper threshold)
 *                        110 = ADERR >= ADUTH (at or above upper threshold)
 *                        111 = Threshold interrupt always fires (every conv)
 * ------------------------------------------------------------------------- */
#define ADCON3 SFR8(0x03F6)

#define ADCON3_SOI (1u << 7) /**< Stop on interrupt               */

#define ADCON3_CALC_MASK 0x70u /**< CALC[2:0] bit mask             */
#define ADCON3_CALC_SHIFT 4u   /**< CALC[2:0] bit position         */

#define ADCON3_TMD_MASK 0x07u /**< TMD[2:0] bit mask              */
#define ADCON3_TMD_SHIFT 0u   /**< TMD[2:0] bit position          */

/** Error calculation values for ADCON3 CALC[2:0] (shifted to bit position) */
#define ADCON3_CALC_DERIV (0x00u << 4)    /**< First derivative (RES-PREV)   */
#define ADCON3_CALC_VS_STPT (0x01u << 4)  /**< Actual vs setpoint (RES-STPT) */
#define ADCON3_CALC_VS_FLTR (0x02u << 4)  /**< Actual vs filter (RES-FLTR)   */
#define ADCON3_CALC_FLT_STPT (0x03u << 4) /**< Filter vs setpoint (FLT-STPT)*/

/** Threshold mode values for ADCON3 TMD[2:0] */
#define ADCON3_TMD_NEVER 0x00u   /**< Threshold interrupt disabled    */
#define ADCON3_TMD_LT_LTH 0x01u  /**< ADERR < ADLTH                  */
#define ADCON3_TMD_GE_LTH 0x02u  /**< ADERR >= ADLTH                 */
#define ADCON3_TMD_IN_WIN 0x03u  /**< In window (LTH <= ERR < UTH)   */
#define ADCON3_TMD_OUT_WIN 0x04u /**< Out of window                  */
#define ADCON3_TMD_LT_UTH 0x05u  /**< ADERR < ADUTH                  */
#define ADCON3_TMD_GE_UTH 0x06u  /**< ADERR >= ADUTH                 */
#define ADCON3_TMD_ALWAYS 0x07u  /**< Always interrupt               */

/* ---------------------------------------------------------------------------
 * ADSTAT - ADC Status Register (0x03F7)
 *
 * Provides real-time status of the ADC conversion and computation stages.
 *
 * Bit 7    --      Reserved
 * Bits 6:4 STAT[2:0]  Conversion Stage Status (read-only)
 *                      Indicates the current stage of the conversion cycle:
 *                        000 = ADC idle / not converting
 *                        001 = Precharge phase active
 *                        010 = Acquisition (sampling) phase active
 *                        011 = Conversion in progress
 *                        1xx = Suspended / waiting
 * Bit 3    MATH    Math Operation in Progress (read-only)
 *                    1 = Computation engine is processing (accumulating,
 *                        filtering, or calculating error)
 *                    0 = Computation engine is idle
 * Bit 2    LTHR    Lower Threshold Exceeded (read-only)
 *                    1 = ADERR has crossed below the lower threshold (ADLTH)
 *                    0 = ADERR is at or above the lower threshold
 * Bit 1    UTHR    Upper Threshold Exceeded (read-only)
 *                    1 = ADERR has crossed at or above the upper threshold
 *                    0 = ADERR is below the upper threshold
 * Bit 0    AOV     Accumulator Overflow (read-only)
 *                    1 = The 18-bit accumulator (ADACC) has overflowed
 *                    0 = No overflow has occurred
 * ------------------------------------------------------------------------- */
#define ADSTAT SFR8(0x03F7)

#define ADSTAT_STAT_MASK 0x70u /**< STAT[2:0] bit mask             */
#define ADSTAT_STAT_SHIFT 4u   /**< STAT[2:0] bit position         */
#define ADSTAT_MATH (1u << 3)  /**< Math operation in progress     */
#define ADSTAT_LTHR (1u << 2)  /**< Lower threshold exceeded       */
#define ADSTAT_UTHR (1u << 1)  /**< Upper threshold exceeded       */
#define ADSTAT_AOV (1u << 0)   /**< Accumulator overflow           */

/* ---------------------------------------------------------------------------
 * ADREF - ADC Reference Voltage Select Register (0x03F8)
 *
 * Selects the positive and negative voltage references used by the ADC
 * for conversion.
 *
 * Bits 7:5 --      Reserved
 * Bit 4    NREF    Negative Reference Select
 *                    1 = External VREF- pin
 *                    0 = VSS (internal ground)
 * Bits 3:2 --      Reserved
 * Bits 1:0 PREF[1:0]  Positive Reference Select
 *                        00 = VDD (supply voltage)
 *                        01 = Reserved
 *                        10 = Reserved
 *                        11 = FVR Buffer 2 output
 * ------------------------------------------------------------------------- */
#define ADREF SFR8(0x03F8)

#define ADREF_NREF (1u << 4) /**< Negative ref (1=ext, 0=VSS)    */

#define ADREF_PREF_MASK 0x03u /**< PREF[1:0] bit mask             */
#define ADREF_PREF_SHIFT 0u   /**< PREF[1:0] bit position         */

/** Positive reference values for ADREF PREF[1:0] */
#define ADREF_PREF_VDD 0x00u /**< VDD as positive reference      */
#define ADREF_PREF_FVR 0x03u /**< FVR Buffer 2 as positive ref   */

/* ---------------------------------------------------------------------------
 * ADACT - ADC Auto-Conversion Trigger Select Register (0x03F9)
 *
 * Selects the hardware event that automatically triggers a conversion
 * when ADCON0.CONT=1 or when using auto-triggered computation modes.
 * When the selected trigger fires, the ADC begins a new conversion
 * without software intervention.
 *
 * Bits 7:6 --      Reserved
 * Bits 5:0 ACT[5:0]  Auto-Conversion Trigger Source
 *                     The specific mapping of ACT values to trigger sources
 *                     (TMR0, TMR1, TMR2, CCP, CMP, CLC, etc.) is device-
 *                     dependent.  Refer to DS40002213D "ADC Auto-Conversion
 *                     Trigger Sources" table for the complete enumeration.
 *                     0x00 typically selects "no auto-trigger / software only".
 * ------------------------------------------------------------------------- */
#define ADACT SFR8(0x03F9)

#define ADACT_ACT_MASK 0x3Fu /**< ACT[5:0] bit mask              */
#define ADACT_ACT_SHIFT 0u   /**< ACT[5:0] bit position          */

/* ---------------------------------------------------------------------------
 * ADCLK - ADC Clock Divider Register (0x03FA)
 *
 * Sets the ADC clock frequency when using FOSC-derived clocking
 * (ADCON0.CS=0).  The ADC clock period is:
 *
 *   TADCLK = 2 * (CS + 1) / FOSC
 *
 * Minimum ADC clock period is specified in the device electrical
 * characteristics.  For 12-bit conversions, the ADC clock must be within
 * the valid range for accurate results.
 *
 * Bits 7:6 --      Reserved
 * Bits 5:0 CS[5:0]  Clock Divider Value
 *                    ADC clock = FOSC / (2 * (CS + 1))
 *                    0x00 = FOSC / 2  (fastest)
 *                    0x3F = FOSC / 128 (slowest FOSC-derived)
 * ------------------------------------------------------------------------- */
#define ADCLK SFR8(0x03FA)

#define ADCLK_CS_MASK 0x3Fu /**< CS[5:0] bit mask               */
#define ADCLK_CS_SHIFT 0u   /**< CS[5:0] bit position           */

/* ============================================================================
 * ADC Context Switching Registers
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ADCTX - ADC Context Select Register (0x03FB)
 *
 * Selects which of the four ADC contexts (CTX0-CTX3) is currently active.
 * Each context maintains independent copies of channel select, acquisition
 * time, precharge, thresholds, and other per-context settings.  Writing
 * to this register switches the ADC to operate with the selected context's
 * configuration.
 *
 * Bits 7:2 --      Reserved
 * Bits 1:0 CTX[1:0]  Context Select
 *                       00 = Context 0
 *                       01 = Context 1
 *                       10 = Context 2
 *                       11 = Context 3
 * ------------------------------------------------------------------------- */
#define ADCTX SFR8(0x03FB)

#define ADCTX_CTX_MASK 0x03u /**< CTX[1:0] bit mask              */
#define ADCTX_CTX_SHIFT 0u   /**< CTX[1:0] bit position          */

#define ADCTX_CTX0 0x00u /**< Select context 0               */
#define ADCTX_CTX1 0x01u /**< Select context 1               */
#define ADCTX_CTX2 0x02u /**< Select context 2               */
#define ADCTX_CTX3 0x03u /**< Select context 3               */

/* ---------------------------------------------------------------------------
 * ADCSEL1 - ADC Context 1 Select Register (0x03FC)
 * ADCSEL2 - ADC Context 2 Select Register (0x03FD)
 * ADCSEL3 - ADC Context 3 Select Register (0x03FE)
 *
 * These registers control the context-switching behavior for contexts
 * 1-3.  Context 0 is the default and does not have a separate select
 * register.  Each register contains enable and interlock bits that
 * determine when and how the ADC switches to that context.
 *
 * ADCSELn bit layout:
 *   Bit 7    CTXSW   Context Switch in Progress (read-only)
 *                      1 = A context switch to this context is underway
 *                      0 = No switch in progress
 *   Bit 6    CHEN    Channel Enable
 *                      1 = This context's channel is enabled and will be
 *                          included in auto-context-switching sequences
 *                      0 = This context is skipped during auto-switching
 *   Bit 5    SSI     Sample/Start Interlock
 *                      1 = Interlock enabled: the trigger for this context
 *                          is synchronized with the sample phase completion
 *                      0 = No interlock
 *   Bits 4:0 --      Reserved
 * ------------------------------------------------------------------------- */
#define ADCSEL1 SFR8(0x03FC)
#define ADCSEL2 SFR8(0x03FD)
#define ADCSEL3 SFR8(0x03FE)

#define ADCSEL_CTXSW (1u << 7) /**< Context switch in progress (ro)*/
#define ADCSEL_CHEN (1u << 6)  /**< Channel enable for context     */
#define ADCSEL_SSI (1u << 5)   /**< Sample/start interlock         */

#endif /* PIC18F_Q84_ADC_H */
