/**
 * @file pic18f_q84_zcd.h
 * @brief ZCD (Zero-Cross Detection) register definitions for
 *        PIC18F27/47/57Q84 microcontrollers.
 *
 * The ZCD module detects when an external AC signal crosses through
 * the zero-volt (ground) level.  This is primarily used for:
 *
 *   - AC mains zero-cross detection for TRIAC/SSR dimming and switching
 *   - Phase-angle control of AC loads
 *   - Synchronizing events to the AC line frequency
 *   - Power factor measurement
 *
 * The module includes an internal current-limiting resistor and a
 * comparator that compares the ZCD pin voltage against an internal
 * reference near ground.  When the external signal transitions through
 * zero, the output (OUT) toggles, and an interrupt can be generated on
 * either or both edges.
 *
 * The ZCD pin has a unique bidirectional current-limited interface:
 *   - When the external voltage is above ground, current flows into the
 *     pin, and OUT reads 1 (or 0 if POL=1).
 *   - When the external voltage is below ground, current flows out of
 *     the pin, and OUT reads 0 (or 1 if POL=1).
 *   - The internal limiting resistor restricts current to a safe level,
 *     allowing direct connection to high-voltage AC through an external
 *     series resistor (typically 100k-1M ohm, power rated appropriately).
 *
 * IMPORTANT: The ZCD pin MUST have an external current-limiting resistor
 * in series with any voltage source above VDD or below VSS.  The internal
 * resistor alone is NOT sufficient for mains-voltage applications.
 *
 * Typical usage (AC zero-cross detection):
 *   1. Connect AC signal to ZCD pin through appropriate series resistor
 *   2. Enable the ZCD sensor: ZCDCON.SEN = 1
 *   3. Configure interrupt edges: ZCDCON.INTP, ZCDCON.INTN
 *   4. Enable the module: ZCDCON.EN = 1
 *   5. Enable the ZCD interrupt in the interrupt controller
 *   6. In ISR, read ZCDCON.OUT to determine signal polarity
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 34: Zero-Cross Detection (ZCD) Module
 *
 * @note SFR8(addr) macro must be defined before including this header
 *       (typically via pic18f_q84.h).
 * @note All register addresses are absolute.
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_ZCD_H
#define PIC18F_Q84_ZCD_H

/* ============================================================================
 * ZCD - Zero-Cross Detection Module (single instance)
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * ZCDCON - ZCD Control Register (0x004C)
 *
 * Bit 7    EN      ZCD Module Enable
 *                    1 = ZCD module is enabled; the output and interrupt
 *                        logic are active.  The ZCD pin is configured for
 *                        analog input automatically.
 *                    0 = ZCD module is disabled; output is forced inactive,
 *                        pin reverts to normal digital I/O control.
 * Bit 6    SEN     Sensor Enable
 *                    1 = ZCD analog sensor on the pin is enabled.  Current
 *                        flows through the internal limiting resistor and
 *                        the comparator is active.
 *                    0 = Sensor disabled; pin current source/sink is off.
 *                        The module can still be used with the comparator
 *                        if an external signal meets input requirements.
 * Bit 5    OUT     ZCD Output (read-only)
 *                    Reflects the zero-cross comparator output after
 *                    polarity inversion (POL):
 *                      POL=0: OUT=1 when pin voltage > ground,
 *                             OUT=0 when pin voltage < ground
 *                      POL=1: OUT=0 when pin voltage > ground,
 *                             OUT=1 when pin voltage < ground
 * Bit 4    POL     Output Polarity
 *                    1 = Output is inverted
 *                    0 = Output is non-inverted (default)
 * Bits 3:2 --      Reserved
 * Bit 1    INTP    Interrupt on Positive (Rising) Edge
 *                    1 = Generate interrupt when OUT transitions from 0 to 1
 *                        (external signal crosses from below to above ground,
 *                        or vice versa depending on POL)
 *                    0 = No interrupt on rising edge of OUT
 * Bit 0    INTN    Interrupt on Negative (Falling) Edge
 *                    1 = Generate interrupt when OUT transitions from 1 to 0
 *                    0 = No interrupt on falling edge of OUT
 *
 * @note Both INTP and INTN can be enabled simultaneously to generate
 *       an interrupt on every zero-crossing (both positive-to-negative
 *       and negative-to-positive transitions of the AC signal).
 * @note The interrupt flag (PIRx.ZCDIF) must be cleared in software
 *       within the ISR.  The interrupt condition is edge-triggered,
 *       not level-triggered.
 * ------------------------------------------------------------------------- */
#define ZCDCON SFR8(0x004C)

#define ZCDCON_EN (1u << 7)   /**< ZCD module enable               */
#define ZCDCON_SEN (1u << 6)  /**< Sensor enable                   */
#define ZCDCON_OUT (1u << 5)  /**< ZCD output (read-only)          */
#define ZCDCON_POL (1u << 4)  /**< Output polarity (1=inverted)    */
#define ZCDCON_INTP (1u << 1) /**< Interrupt on rising edge        */
#define ZCDCON_INTN (1u << 0) /**< Interrupt on falling edge       */

#endif /* PIC18F_Q84_ZCD_H */
