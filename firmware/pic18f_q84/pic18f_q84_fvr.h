/**
 * @file pic18f_q84_fvr.h
 * @brief FVR (Fixed Voltage Reference) register definitions for
 *        PIC18F27/47/57Q84 microcontrollers.
 *
 * The FVR module provides a stable, temperature-compensated voltage
 * reference derived from an internal bandgap circuit.  It is independent
 * of the VDD supply voltage (as long as VDD > the selected FVR output).
 *
 * Two independently-configurable output buffers are available:
 *
 *   Buffer 1 (ADFVR): Routed to the ADC module as a positive reference
 *     or as a measurable internal channel.  Gain settings produce output
 *     voltages of 1.024 V, 2.048 V, or 4.096 V.
 *
 *   Buffer 2 (CDAFVR): Routed to the comparators and the DAC as a
 *     voltage reference.  Same gain settings: 1.024 V, 2.048 V, or 4.096 V.
 *
 * Either buffer can be independently disabled (gain = 00 = off) to save
 * power when not needed.  The FVR enable bit (EN) controls the master
 * bandgap; both buffers require EN=1 to operate.
 *
 * The module also contains the temperature indicator enable and range
 * select bits.  The temperature indicator is a voltage source proportional
 * to die temperature, routed to the ADC as an internal channel.  It is
 * separate from the FVR bandgap and can be used independently (though
 * it shares the FVRCON register).
 *
 * Typical usage (FVR as ADC reference):
 *   1. Select desired ADC buffer gain: FVRCON.ADFVR = 01/10/11
 *   2. Enable the FVR: FVRCON.EN = 1
 *   3. Wait for FVR to stabilize: poll FVRCON.RDY until it reads 1
 *   4. Select FVR as ADC positive reference: ADREF.PREF = 11
 *   5. Perform ADC conversions; results are now relative to the FVR voltage
 *
 * Typical usage (temperature measurement):
 *   1. Enable temperature indicator: FVRCON.TSEN = 1
 *   2. Select temperature range: FVRCON.TSRNG (1=high, 0=low)
 *   3. Select temperature indicator ADC channel: ADPCH = 0x3B
 *   4. Configure ADC with appropriate acquisition time (>25 us recommended)
 *   5. Convert and apply calibration coefficients from DS40002213D
 *      electrical specifications to derive temperature in degrees
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 33: Fixed Voltage Reference (FVR)
 *
 * @note SFR8(addr) macro must be defined before including this header
 *       (typically via pic18f_q84.h).
 * @note All register addresses are absolute.
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 * @note The 4.096 V output (4x gain) is only available when VDD >= 4.75 V.
 *       If VDD < 4.096 V, the FVR output will be clamped near VDD and the
 *       reference will not be accurate at 4x gain.
 */

#ifndef PIC18F_Q84_FVR_H
#define PIC18F_Q84_FVR_H

/* ============================================================================
 * FVR - Fixed Voltage Reference Module
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * FVRCON - FVR Control Register (0x03D6)
 *
 * Controls the FVR bandgap enable, buffer gain settings, and temperature
 * indicator configuration.  All bits except RDY are read/write.
 *
 * Bit 7    EN      FVR Enable (master enable for bandgap circuit)
 *                    1 = FVR bandgap and output buffers are powered on
 *                    0 = FVR is disabled; all outputs are off, minimum power
 * Bit 6    RDY     FVR Ready (read-only)
 *                    1 = FVR output voltage has stabilized and is valid for
 *                        use as a reference.  Typically stabilizes within
 *                        a few hundred microseconds after EN is set.
 *                    0 = FVR is not yet stable (still powering up or disabled)
 *
 *                    IMPORTANT: Do not use the FVR output for conversions
 *                    or comparisons until RDY=1.  Results will be inaccurate
 *                    if the reference has not settled.
 *
 * Bit 5    TSEN    Temperature Sensor Enable
 *                    1 = Temperature indicator voltage source is active and
 *                        available on the ADC internal channel (ADPCH=0x3B).
 *                        The temperature indicator operates independently
 *                        of the FVR bandgap (EN bit).
 *                    0 = Temperature indicator is disabled (saves power)
 * Bit 4    TSRNG   Temperature Sensor Range Select
 *                    1 = High range: higher output voltage with larger
 *                        voltage-per-degree slope (better ADC resolution).
 *                        Output ~0.6-0.9 V typical at room temperature.
 *                    0 = Low range: lower output voltage with smaller slope.
 *                        Output ~0.1-0.4 V typical at room temperature.
 *                        Useful when VDD is very low.
 *
 * Bits 3:2 CDAFVR[1:0]  FVR Buffer 2 Gain Selection
 *                        (output to Comparators and DAC)
 *                          00 = Buffer 2 off (disconnected)
 *                          01 = 1x gain: 1.024 V output
 *                          10 = 2x gain: 2.048 V output
 *                          11 = 4x gain: 4.096 V output (VDD >= 4.75 V)
 *
 * Bits 1:0 ADFVR[1:0]   FVR Buffer 1 Gain Selection
 *                        (output to ADC)
 *                          00 = Buffer 1 off (disconnected)
 *                          01 = 1x gain: 1.024 V output
 *                          10 = 2x gain: 2.048 V output
 *                          11 = 4x gain: 4.096 V output (VDD >= 4.75 V)
 * ------------------------------------------------------------------------- */
#define FVRCON SFR8(0x03D7)

#define FVRCON_EN (1u << 7)    /**< FVR enable (master)             */
#define FVRCON_RDY (1u << 6)   /**< FVR ready (read-only)           */
#define FVRCON_TSEN (1u << 5)  /**< Temperature sensor enable       */
#define FVRCON_TSRNG (1u << 4) /**< Temperature sensor range select */

#define FVRCON_CDAFVR_MASK 0x0Cu /**< CDAFVR[1:0] bit mask           */
#define FVRCON_CDAFVR_SHIFT 2u   /**< CDAFVR[1:0] bit position       */

#define FVRCON_ADFVR_MASK 0x03u /**< ADFVR[1:0] bit mask            */
#define FVRCON_ADFVR_SHIFT 0u   /**< ADFVR[1:0] bit position        */

/** FVR Buffer 2 gain values for FVRCON CDAFVR[1:0] (to Comparators/DAC) */
#define FVRCON_CDAFVR_OFF (0x00u << 2) /**< Buffer 2 off (disconnected)   */
#define FVRCON_CDAFVR_1X (0x01u << 2)  /**< Buffer 2 = 1.024 V            */
#define FVRCON_CDAFVR_2X (0x02u << 2)  /**< Buffer 2 = 2.048 V            */
#define FVRCON_CDAFVR_4X (0x03u << 2)  /**< Buffer 2 = 4.096 V            */

/** FVR Buffer 1 gain values for FVRCON ADFVR[1:0] (to ADC) */
#define FVRCON_ADFVR_OFF 0x00u /**< Buffer 1 off (disconnected)     */
#define FVRCON_ADFVR_1X 0x01u  /**< Buffer 1 = 1.024 V             */
#define FVRCON_ADFVR_2X 0x02u  /**< Buffer 1 = 2.048 V             */
#define FVRCON_ADFVR_4X 0x03u  /**< Buffer 1 = 4.096 V             */

#endif /* PIC18F_Q84_FVR_H */
