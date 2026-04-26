/**
 * @file pic18f_q84_dac.h
 * @brief DAC (Digital-to-Analog Converter) register definitions
 *        for PIC18F27/47/57Q84 microcontrollers.
 *
 * The DAC module provides a single 8-bit voltage output DAC.  The output
 * voltage is a linear function of the data register value:
 *
 *   Vout = (DAC1R / 256) * (VSOURCE+ - VSOURCE-) + VSOURCE-
 *
 * where VSOURCE+ and VSOURCE- are the positive and negative voltage
 * reference sources selected by PSS and NSS, and DAC1R is the 8-bit
 * data value (0x00 to 0xFF).
 *
 * Key features:
 *   - 8-bit resolution (256 output voltage levels)
 *   - Selectable positive reference: VDD, FVR Buffer 2, or external VREF+
 *   - Selectable negative reference: VSS or external VREF-
 *   - Buffered output can be routed to one or two I/O pins (OE1, OE2)
 *   - Internal connection to ADC positive input channel (ADPCH = DAC1)
 *   - Internal connection to comparator positive/negative inputs
 *   - Output can remain internal-only (both OE bits cleared) for use
 *     as a reference to the ADC or comparators without consuming a pin
 *
 * Typical usage:
 *   1. Select voltage references: DAC1CON.PSS, DAC1CON.NSS
 *   2. Optionally enable pin output: DAC1CON.OE1 and/or DAC1CON.OE2
 *   3. Enable the DAC: DAC1CON.EN = 1
 *   4. Write desired output value to DAC1DATL (0x00 = min, 0xFF = max)
 *   5. Output voltage updates immediately upon write to DAC1DATL
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 30: DAC
 *
 * @note SFR8(addr) macro must be defined before including this header
 *       (typically via pic18f_q84.h).
 * @note All register addresses are absolute.
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_DAC_H
#define PIC18F_Q84_DAC_H

/* ============================================================================
 * DAC1 - Digital-to-Analog Converter Instance 1
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * DAC1DATL - DAC1 Data Register (0x007D)
 *
 * The 8-bit output value for the DAC.  Writing this register immediately
 * updates the DAC output voltage (no double-buffering).
 *
 *   0x00 = Output at VSOURCE- (minimum voltage)
 *   0x80 = Output at approximately mid-scale
 *   0xFF = Output at VSOURCE+ * (255/256) (maximum voltage)
 *
 * Bits 7:0  DAC1R[7:0]  DAC output data value
 * ------------------------------------------------------------------------- */
#define DAC1DATL SFR8(0x007D)

/* ---------------------------------------------------------------------------
 * DAC1CON - DAC1 Control Register (0x007F)
 *
 * Controls the DAC enable, output pin routing, and voltage reference
 * source selection.
 *
 * Bit 7    EN      DAC Enable
 *                    1 = DAC is active; output voltage is generated based
 *                        on DAC1DATL value and reference sources.  Internal
 *                        connections to ADC and comparators are available.
 *                    0 = DAC is disabled; output is high-impedance, analog
 *                        circuitry is shut down to minimize power.
 * Bit 6    --      Reserved
 * Bit 5    OE2     Output Enable 2
 *                    1 = DAC output is routed to the secondary output pin
 *                        (pin assignment is device/package dependent)
 *                    0 = Secondary pin not driven by DAC
 * Bit 4    OE1     Output Enable 1
 *                    1 = DAC output is routed to the primary output pin
 *                        (pin assignment is device/package dependent)
 *                    0 = Primary pin not driven by DAC
 * Bit 3    --      Reserved
 * Bit 2    NSS     Negative Source Select
 *                    1 = External VREF- pin as negative reference
 *                    0 = VSS (internal ground) as negative reference
 * Bits 1:0 PSS[1:0]  Positive Source Select
 *                      00 = VDD (supply voltage)
 *                      01 = Reserved
 *                      10 = FVR Buffer 2 output (1.024V / 2.048V / 4.096V
 *                           depending on FVRCON.CDAFVR setting)
 *                      11 = External VREF+ pin
 * ------------------------------------------------------------------------- */
#define DAC1CON SFR8(0x007F)

#define DAC1CON_EN (1u << 7)  /**< DAC enable                      */
#define DAC1CON_OE2 (1u << 5) /**< Output enable, secondary pin    */
#define DAC1CON_OE1 (1u << 4) /**< Output enable, primary pin      */
#define DAC1CON_NSS (1u << 2) /**< Negative source (1=ext VREF-)   */

#define DAC1CON_PSS_MASK 0x03u /**< PSS[1:0] bit mask               */
#define DAC1CON_PSS_SHIFT 0u   /**< PSS[1:0] bit position           */

/** Positive source values for DAC1CON PSS[1:0] */
#define DAC1CON_PSS_VDD 0x00u   /**< VDD as positive source          */
#define DAC1CON_PSS_FVR 0x02u   /**< FVR Buffer 2 as positive source */
#define DAC1CON_PSS_VREFP 0x03u /**< External VREF+ pin             */

/** Output enable convenience masks */
#define DAC1CON_OE_MASK 0x30u /**< OE[1:0] combined mask           */

#endif /* PIC18F_Q84_DAC_H */
