/*
 * pic18f_q84_clkref.h
 *
 * CLKREF (Reference Clock Output) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * The CLKREF module generates a divided reference clock signal that
 * can be routed to an output pin through the Peripheral Pin Select
 * (PPS) system.  This provides a stable, software-configurable
 * clock output for external devices, debugging, or inter-chip
 * synchronization without dedicating a timer resource.
 *
 * Features:
 *   - Selectable clock source (HFINTOSC, FOSC, SOSC, etc.)
 *   - Programmable duty cycle (50%, 25%, 75%)
 *   - Output routed via PPS to any available output pin
 *   - Simple two-register interface: control and clock source
 *
 * Typical usage:
 *   1. Select the desired clock source via CLKRCLK.CLK[4:0]
 *   2. Configure duty cycle via CLKRCON.DC[1:0]
 *   3. Enable the module by setting CLKRCON.EN = 1
 *   4. Route the CLKREF output to a pin using the PPS output register
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_CLKREF_H
#define PIC18F_Q84_CLKREF_H

/* ===================================================================
 * CLKRCON - Reference Clock Control Register
 * Address: 0x0039
 *
 * Controls the enable state and duty cycle of the reference clock
 * output module.  The output frequency equals the selected source
 * frequency (from CLKRCLK); the duty cycle may be adjusted for
 * applications requiring asymmetric high/low times.
 *
 * The module must be disabled (EN = 0) before changing the duty
 * cycle setting to avoid glitches on the output.
 * ================================================================ */
#define CLKRCON SFR8(0x0039)

/* Bit 7 - EN: Reference Clock Enable
 *   1 = Reference clock module is enabled; the divided clock signal
 *       is generated and available at the PPS output.
 *   0 = Reference clock module is disabled; output is held low and
 *       the module draws minimal current.                           */
#define CLKRCON_EN 7

/* Bits 6:5 - Reserved: read as 0, write as 0.                      */

/* Bits 4:3 - DC[1:0]: Duty Cycle Select
 *   Controls the high/low duty cycle of the reference clock output:
 *   00 = 50% duty cycle (equal high and low times)
 *   01 = 25% duty cycle (output low 75% of the period)
 *   10 = 75% duty cycle (output high 75% of the period)
 *   11 = Reserved (do not use)
 *
 *   Note: Duty cycle is measured as the fraction of the period
 *   during which the output is high.                                */
#define CLKRCON_DC1 4
#define CLKRCON_DC0 3

/* DC field mask and position                                        */
#define CLKRCON_DC_MASK 0x18 /* bits 4:3 */
#define CLKRCON_DC_POS 3

/* Named duty-cycle values for DC[1:0] (pre-shifted)                 */
#define CLKRDC_50 (0x00 << 3) /* 50% duty cycle           */
#define CLKRDC_25 (0x01 << 3) /* 25% duty cycle           */
#define CLKRDC_75 (0x02 << 3) /* 75% duty cycle           */

/* Bits 2:0 - Reserved: read as 0, write as 0.                      */

/* ===================================================================
 * CLKRCLK - Reference Clock Source Select Register
 * Address: 0x003A
 *
 * Selects the clock source that feeds the reference clock output
 * module.  The selected source is divided and shaped according to
 * the duty cycle setting in CLKRCON before being routed to the
 * PPS output pin.
 *
 * The specific mapping of CLK values to clock sources is device-
 * dependent; refer to the "CLKREF Clock Source Selection" table in
 * DS40002213D for the complete enumeration.  Common sources include
 * FOSC, HFINTOSC, LFINTOSC, MFINTOSC, SOSC, and EXTOSC.
 * ================================================================ */
#define CLKRCLK SFR8(0x003A)

/* Bits 7:5 - Reserved: read as 0, write as 0.                      */

/* Bits 4:0 - CLK[4:0]: Clock Source Select
 *   Selects the input clock source for the reference clock module.
 *   Refer to DS40002213D Table "CLKREF Clock Source Selection"
 *   for the full mapping of source codes to oscillator signals.     */
#define CLKRCLK_CLK4 4
#define CLKRCLK_CLK3 3
#define CLKRCLK_CLK2 2
#define CLKRCLK_CLK1 1
#define CLKRCLK_CLK0 0

/* CLK field mask and position                                       */
#define CLKRCLK_CLK_MASK 0x1F /* bits 4:0 */
#define CLKRCLK_CLK_POS 0

#endif /* PIC18F_Q84_CLKREF_H */
