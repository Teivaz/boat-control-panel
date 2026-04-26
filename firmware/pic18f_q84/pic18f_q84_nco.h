/**
 * @file pic18f_q84_nco.h
 * @brief NCO (Numerically Controlled Oscillator) register definitions
 *        for PIC18F27/47/57Q84 microcontrollers.
 *
 * The NCO module provides true linear frequency control using a 20-bit
 * phase accumulator and a 20-bit increment value.  On each input clock
 * edge the increment value (INC) is added to the accumulator (ACC).
 * Each time the accumulator overflows (bit 19 transitions from 0 to 1),
 * the output toggles or a pulse is generated, depending on the mode.
 *
 * Output frequency calculation:
 *
 *   Fout = (Fclock * INC) / 2^20
 *
 * where Fclock is the selected input clock and INC is the 20-bit
 * increment value (1 to 1,048,575).  The relationship is perfectly
 * linear: doubling INC exactly doubles Fout.
 *
 * This device provides three independent NCO instances (NCO1, NCO2,
 * NCO3) with identical register layouts at consecutive addresses.
 * Each instance can be independently enabled and configured.
 *
 * Operating modes:
 *   - Fixed Duty Cycle (FDC) mode (PFM = 0):
 *       Output toggles on each accumulator overflow, producing a
 *       50% duty-cycle square wave at frequency Fout.
 *   - Pulse Frequency Mode (PFM = 1):
 *       Output generates a single pulse of configurable width
 *       (PWS[2:0]) on each accumulator overflow.  The pulse rate
 *       equals Fout; the duty cycle varies with frequency.
 *
 * Typical usage:
 *   1. Select the input clock source via NCOnCLK.CKS[4:0]
 *   2. Write the desired increment value to NCOnINCU:NCOnINCH:NCOnINCL
 *   3. Optionally clear the accumulator (NCOnACC = 0)
 *   4. Configure mode (FDC/PFM), polarity, and pulse width in NCOnCON
 *   5. Enable the module by setting NCOnCON.EN = 1
 *   6. Route the NCO output pin via the PPS output register
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *
 * @note SFR8(addr) and SFR16(addr) macros must be defined before
 *       including this header (typically via pic18f_q84.h).
 * @note All register addresses are absolute.  Multi-byte registers
 *       are little-endian (low byte at base address).
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_NCO_H
#define PIC18F_Q84_NCO_H

/* ============================================================================
 * NCO1 - Numerically Controlled Oscillator Instance 1
 * Base address: 0x0440
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * NCO1ACC - NCO1 Accumulator (0x0440-0x0442, 20-bit)
 *
 * The 20-bit phase accumulator.  On each input clock edge the increment
 * value (NCO1INC) is added to this register.  When the MSb (bit 19)
 * rolls over, the output toggles (FDC mode) or a pulse is generated
 * (PFM mode).
 *
 * The accumulator can be read or written at any time.  Writing a value
 * allows phase-presetting the output.  Reading provides the current
 * phase of the oscillator.
 *
 * NCO1ACCU (0x0442):
 *   Bits 7:4  -- Reserved (read as 0, write as 0)
 *   Bits 3:0  ACC[19:16]  Upper nibble of accumulator
 *
 * NCO1ACCH (0x0441):
 *   Bits 7:0  ACC[15:8]   High byte of accumulator
 *
 * NCO1ACCL (0x0440):
 *   Bits 7:0  ACC[7:0]    Low byte of accumulator
 * ------------------------------------------------------------------------- */
#define NCO1ACCL SFR8(0x0440)
#define NCO1ACCH SFR8(0x0441)
#define NCO1ACCU SFR8(0x0442)
#define NCO1ACC SFR16(0x0440) /**< 16-bit access (low 16 bits)  */

#define NCO1ACCU_MASK 0x0Fu /**< ACC[19:16] valid bits in U     */

/* ---------------------------------------------------------------------------
 * NCO1INC - NCO1 Increment Value (0x0443-0x0445, 20-bit)
 *
 * The 20-bit value added to the accumulator on every input clock edge.
 * Larger values produce higher output frequencies.  The relationship
 * is linear: Fout = (Fclock * INC) / 2^20.
 *
 * An increment of 0 effectively halts output transitions (accumulator
 * never overflows).  An increment of 1 produces the minimum output
 * frequency: Fclock / 2^20.
 *
 * NCO1INCU (0x0445):
 *   Bits 7:4  -- Reserved (read as 0, write as 0)
 *   Bits 3:0  INC[19:16]  Upper nibble of increment
 *
 * NCO1INCH (0x0444):
 *   Bits 7:0  INC[15:8]   High byte of increment
 *
 * NCO1INCL (0x0443):
 *   Bits 7:0  INC[7:0]    Low byte of increment
 * ------------------------------------------------------------------------- */
#define NCO1INCL SFR8(0x0443)
#define NCO1INCH SFR8(0x0444)
#define NCO1INCU SFR8(0x0445)
#define NCO1INC SFR16(0x0443) /**< 16-bit access (low 16 bits)  */

#define NCO1INCU_MASK 0x0Fu /**< INC[19:16] valid bits in U     */

/* ---------------------------------------------------------------------------
 * NCO1CON - NCO1 Control Register (0x0446)
 *
 * Controls the enable state, output polarity, operating mode, and
 * pulse width of NCO1.
 *
 * Bit 7    EN      NCO Enable
 *                    1 = NCO module is enabled; accumulator clocks and
 *                        output generation are active.
 *                    0 = NCO module is disabled; output is held inactive,
 *                        accumulator stops.
 * Bit 6    --      Reserved
 * Bit 5    OUT     NCO Output (read-only)
 *                    Reflects the current state of the NCO output signal
 *                    before polarity inversion (POL).  Useful for reading
 *                    the output state in software without external pin
 *                    feedback.
 * Bit 4    POL     Output Polarity
 *                    1 = NCO output is inverted (active-low)
 *                    0 = NCO output is non-inverted (active-high)
 * Bit 3    PFM     Pulse Frequency Mode
 *                    1 = Pulse mode: a single pulse of width defined by
 *                        PWS[2:0] is generated on each accumulator overflow.
 *                    0 = Fixed Duty Cycle mode: output toggles on each
 *                        overflow, producing a 50% duty-cycle square wave.
 * Bits 2:0 PWS[2:0]  Output Pulse Width Select (PFM mode only)
 *                    Defines the pulse duration as a number of NCO input
 *                    clock periods:
 *                      000 = 1 clock period
 *                      001 = 2 clock periods
 *                      010 = 4 clock periods
 *                      011 = 8 clock periods
 *                      100 = 16 clock periods
 *                      101 = 32 clock periods
 *                      110 = 64 clock periods
 *                      111 = 128 clock periods
 *                    Ignored in FDC mode (PFM = 0).
 * ------------------------------------------------------------------------- */
#define NCO1CON SFR8(0x0446)

#define NCO1CON_EN (1u << 7)  /**< NCO enable                     */
#define NCO1CON_OUT (1u << 5) /**< NCO output state (read-only)   */
#define NCO1CON_POL (1u << 4) /**< Output polarity invert         */
#define NCO1CON_PFM (1u << 3) /**< Pulse Frequency Mode enable    */

#define NCO1CON_PWS_MASK 0x07u /**< PWS[2:0] bit mask              */
#define NCO1CON_PWS_SHIFT 0u   /**< PWS[2:0] bit position          */

/** Pulse width values for NCOnCON PWS[2:0] (PFM mode only) */
#define NCO_PWS_1 0x00u   /**< 1 input clock period           */
#define NCO_PWS_2 0x01u   /**< 2 input clock periods          */
#define NCO_PWS_4 0x02u   /**< 4 input clock periods          */
#define NCO_PWS_8 0x03u   /**< 8 input clock periods          */
#define NCO_PWS_16 0x04u  /**< 16 input clock periods         */
#define NCO_PWS_32 0x05u  /**< 32 input clock periods         */
#define NCO_PWS_64 0x06u  /**< 64 input clock periods         */
#define NCO_PWS_128 0x07u /**< 128 input clock periods        */

/* ---------------------------------------------------------------------------
 * NCO1CLK - NCO1 Input Clock Source Select Register (0x0447)
 *
 * Selects the clock source that drives the NCO1 accumulator.  The
 * specific mapping of CKS values to clock sources is device-dependent;
 * refer to DS40002213D, "NCO Clock Source Selection" table for the
 * complete enumeration.
 *
 * Bits 7:5  -- Reserved (read as 0, write as 0)
 * Bits 4:0  CKS[4:0]  Clock Source Select
 * ------------------------------------------------------------------------- */
#define NCO1CLK SFR8(0x0447)

#define NCO1CLK_CKS_MASK 0x1Fu /**< CKS[4:0] bit mask              */
#define NCO1CLK_CKS_SHIFT 0u   /**< CKS[4:0] bit position          */

/* ============================================================================
 * NCO2 - Numerically Controlled Oscillator Instance 2
 * Base address: 0x0448
 *
 * Register layout and bit definitions are identical to NCO1.
 * See NCO1 register descriptions above for detailed bit-field
 * documentation.
 * ========================================================================= */

/* NCO2ACC - NCO2 Accumulator (0x0448-0x044A, 20-bit) */
#define NCO2ACCL SFR8(0x0448)
#define NCO2ACCH SFR8(0x0449)
#define NCO2ACCU SFR8(0x044A)
#define NCO2ACC SFR16(0x0448) /**< 16-bit access (low 16 bits)  */

#define NCO2ACCU_MASK 0x0Fu /**< ACC[19:16] valid bits in U     */

/* NCO2INC - NCO2 Increment Value (0x044B-0x044D, 20-bit) */
#define NCO2INCL SFR8(0x044B)
#define NCO2INCH SFR8(0x044C)
#define NCO2INCU SFR8(0x044D)
#define NCO2INC SFR16(0x044B) /**< 16-bit access (low 16 bits)  */

#define NCO2INCU_MASK 0x0Fu /**< INC[19:16] valid bits in U     */

/* NCO2CON - NCO2 Control Register (0x044E)
 * Bit layout identical to NCO1CON. */
#define NCO2CON SFR8(0x044E)

#define NCO2CON_EN (1u << 7)   /**< NCO enable                     */
#define NCO2CON_OUT (1u << 5)  /**< NCO output state (read-only)   */
#define NCO2CON_POL (1u << 4)  /**< Output polarity invert         */
#define NCO2CON_PFM (1u << 3)  /**< Pulse Frequency Mode enable    */
#define NCO2CON_PWS_MASK 0x07u /**< PWS[2:0] bit mask              */
#define NCO2CON_PWS_SHIFT 0u   /**< PWS[2:0] bit position          */

/* NCO2CLK - NCO2 Input Clock Source Select (0x044F) */
#define NCO2CLK SFR8(0x044F)

#define NCO2CLK_CKS_MASK 0x1Fu /**< CKS[4:0] bit mask              */
#define NCO2CLK_CKS_SHIFT 0u   /**< CKS[4:0] bit position          */

/* ============================================================================
 * NCO3 - Numerically Controlled Oscillator Instance 3
 * Base address: 0x0450
 *
 * Register layout and bit definitions are identical to NCO1.
 * See NCO1 register descriptions above for detailed bit-field
 * documentation.
 * ========================================================================= */

/* NCO3ACC - NCO3 Accumulator (0x0450-0x0452, 20-bit) */
#define NCO3ACCL SFR8(0x0450)
#define NCO3ACCH SFR8(0x0451)
#define NCO3ACCU SFR8(0x0452)
#define NCO3ACC SFR16(0x0450) /**< 16-bit access (low 16 bits)  */

#define NCO3ACCU_MASK 0x0Fu /**< ACC[19:16] valid bits in U     */

/* NCO3INC - NCO3 Increment Value (0x0453-0x0455, 20-bit) */
#define NCO3INCL SFR8(0x0453)
#define NCO3INCH SFR8(0x0454)
#define NCO3INCU SFR8(0x0455)
#define NCO3INC SFR16(0x0453) /**< 16-bit access (low 16 bits)  */

#define NCO3INCU_MASK 0x0Fu /**< INC[19:16] valid bits in U     */

/* NCO3CON - NCO3 Control Register (0x0456)
 * Bit layout identical to NCO1CON. */
#define NCO3CON SFR8(0x0456)

#define NCO3CON_EN (1u << 7)   /**< NCO enable                     */
#define NCO3CON_OUT (1u << 5)  /**< NCO output state (read-only)   */
#define NCO3CON_POL (1u << 4)  /**< Output polarity invert         */
#define NCO3CON_PFM (1u << 3)  /**< Pulse Frequency Mode enable    */
#define NCO3CON_PWS_MASK 0x07u /**< PWS[2:0] bit mask              */
#define NCO3CON_PWS_SHIFT 0u   /**< PWS[2:0] bit position          */

/* NCO3CLK - NCO3 Input Clock Source Select (0x0457) */
#define NCO3CLK SFR8(0x0457)

#define NCO3CLK_CKS_MASK 0x1Fu /**< CKS[4:0] bit mask              */
#define NCO3CLK_CKS_SHIFT 0u   /**< CKS[4:0] bit position          */

#endif /* PIC18F_Q84_NCO_H */
