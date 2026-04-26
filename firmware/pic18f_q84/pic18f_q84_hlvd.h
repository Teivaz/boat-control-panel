/**
 * @file pic18f_q84_hlvd.h
 * @brief HLVD (High/Low-Voltage Detect) register definitions for
 *        PIC18F27/47/57Q84 microcontrollers.
 *
 * The HLVD module continuously monitors the device supply voltage (VDD)
 * and generates an interrupt when VDD crosses a software-programmable
 * threshold.  This enables:
 *
 *   - Brown-out warning: detect VDD falling below a safe operating level
 *     and save critical data to EEPROM/NVM before a potential reset
 *   - Overvoltage detection: detect VDD rising above a maximum threshold
 *     to protect sensitive external circuitry
 *   - Battery level monitoring: track battery discharge by detecting
 *     multiple programmed voltage levels
 *   - Power sequencing: detect when VDD has reached a target level
 *     during power-up before enabling sensitive peripherals
 *
 * The module operates independently of the CPU clock and remains
 * functional in Sleep mode, making it suitable as a wake-up source for
 * low-power applications.
 *
 * The trip point voltage is selected by the SEL[3:0] field in HLVDCON1.
 * The exact voltage thresholds are device-specific; refer to DS40002213D
 * electrical specifications for the calibrated trip point table.
 *
 * Typical usage (brown-out early warning):
 *   1. Select trip point: HLVDCON1.SEL = desired threshold level
 *   2. Enable low-going interrupt: HLVDCON0.INTL = 1
 *   3. Enable the module: HLVDCON0.EN = 1
 *   4. Wait for module to stabilize: poll HLVDCON0.RDY until it reads 1
 *   5. Enable the HLVD interrupt in the interrupt controller
 *   6. In ISR: VDD has dropped below the trip point; save critical state
 *
 * Typical usage (battery level monitor, multiple thresholds):
 *   1. Set highest threshold, enable INTL, enable module, wait for RDY
 *   2. On interrupt, log "battery at level N"
 *   3. Lower SEL to next threshold, clear interrupt flag, repeat
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 35: High/Low-Voltage Detect (HLVD)
 *
 * @note SFR8(addr) macro must be defined before including this header
 *       (typically via pic18f_q84.h).
 * @note All register addresses are absolute.
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_HLVD_H
#define PIC18F_Q84_HLVD_H

/* ============================================================================
 * HLVD - High/Low-Voltage Detect Module (single instance)
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * HLVDCON0 - HLVD Control Register 0 (0x004A)
 *
 * Primary control: enable, output status, ready flag, and interrupt edge
 * selection.
 *
 * Bit 7    EN      HLVD Enable
 *                    1 = HLVD module is powered on and actively monitoring
 *                        VDD.  The internal voltage divider and comparator
 *                        are running.
 *                    0 = HLVD module is disabled; all analog circuitry is
 *                        shut down to minimize power consumption.
 * Bit 6    --      Reserved
 * Bit 5    OUT     HLVD Output (read-only)
 *                    Reflects the current relationship between VDD and the
 *                    selected trip point:
 *                      1 = VDD is currently above the trip point voltage
 *                      0 = VDD is currently at or below the trip point
 *                    This bit is valid only when RDY = 1.
 * Bit 4    RDY     HLVD Ready (read-only)
 *                    1 = The module has stabilized and the OUT bit and
 *                        interrupt logic are valid.  Stabilization time
 *                        is typically a few hundred microseconds after EN
 *                        is set or after SEL is changed.
 *                    0 = The module is still settling; OUT is not reliable.
 *
 *                    IMPORTANT: Do not act on the OUT bit or rely on
 *                    interrupts until RDY = 1.  Changing the trip point
 *                    (SEL) clears RDY until the new threshold stabilizes.
 *
 * Bit 3    INTH    Interrupt on High-going (Rising) Transition
 *                    1 = Generate interrupt when VDD rises above the trip
 *                        point (OUT transitions from 0 to 1)
 *                    0 = No interrupt on rising VDD crossing
 * Bit 2    INTL    Interrupt on Low-going (Falling) Transition
 *                    1 = Generate interrupt when VDD falls below the trip
 *                        point (OUT transitions from 1 to 0)
 *                    0 = No interrupt on falling VDD crossing
 * Bits 1:0 --      Reserved
 *
 * @note Both INTH and INTL can be enabled simultaneously to interrupt
 *       on any VDD crossing of the trip point in either direction.
 * @note The interrupt flag (PIRx.HLVDIF) must be cleared in software
 *       within the ISR.
 * ------------------------------------------------------------------------- */
#define HLVDCON0 SFR8(0x004A)

#define HLVDCON0_EN (1u << 7)   /**< HLVD module enable              */
#define HLVDCON0_OUT (1u << 5)  /**< HLVD output (read-only)         */
#define HLVDCON0_RDY (1u << 4)  /**< HLVD ready (read-only)          */
#define HLVDCON0_INTH (1u << 3) /**< Interrupt on VDD rising above   */
#define HLVDCON0_INTL (1u << 2) /**< Interrupt on VDD falling below  */

/* ---------------------------------------------------------------------------
 * HLVDCON1 - HLVD Control Register 1 (0x004B)
 *
 * Selects the voltage trip point threshold.  The actual voltage for each
 * SEL value is determined by the internal resistor divider network and
 * is specified in the device electrical characteristics (DS40002213D).
 * Typical trip points range from approximately 1.9 V (SEL=0000) to
 * approximately 4.6 V (SEL=1111), with intermediate steps providing
 * coverage across the entire operating voltage range.
 *
 * Bits 7:4 --      Reserved
 * Bits 3:0 SEL[3:0]  Voltage Trip Point Select
 *                      0000 = Lowest trip point (~1.9 V typical)
 *                      0001 = Next higher trip point
 *                       ...   (monotonically increasing)
 *                      1110 = Second-highest trip point
 *                      1111 = Highest trip point (~4.6 V typical)
 *
 *                    @note Exact voltage values for each SEL code are
 *                    specified in DS40002213D, "HLVD Trip Point" table in
 *                    the Electrical Specifications section.  Values shown
 *                    above are approximate; consult the datasheet for
 *                    min/typ/max specifications at each SEL setting.
 *
 *                    @note Changing SEL while EN=1 temporarily clears the
 *                    RDY bit in HLVDCON0 until the new trip point stabilizes.
 * ------------------------------------------------------------------------- */
#define HLVDCON1 SFR8(0x004B)

#define HLVDCON1_SEL_MASK 0x0Fu /**< SEL[3:0] bit mask              */
#define HLVDCON1_SEL_SHIFT 0u   /**< SEL[3:0] bit position          */

#endif /* PIC18F_Q84_HLVD_H */
