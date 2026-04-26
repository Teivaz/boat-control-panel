/**
 * @file pic18f_q84_dsm.h
 * @brief DSM (Data Signal Modulator) register definitions for
 *        PIC18F27/47/57Q84 microcontrollers.
 *
 * The DSM module multiplexes a modulation source signal with two
 * carrier clock signals to produce a modulated output.  When the
 * modulation source is high, the Carrier High (CARH) signal is
 * passed to the output; when the modulation source is low, the
 * Carrier Low (CARL) signal is passed instead.
 *
 * By selecting appropriate carrier and modulation sources, the DSM
 * can implement a variety of modulation schemes:
 *
 *   - Amplitude Modulation (AM/OOK):
 *       Set one carrier to a clock signal and the other to ground.
 *       The output is keyed on/off by the modulation source.
 *
 *   - Frequency Shift Keying (FSK):
 *       Set CARH and CARL to two different clock frequencies.
 *       The modulation source switches between them.
 *
 *   - Phase Shift Keying (PSK):
 *       Set CARH and CARL to the same clock but invert one via
 *       the carrier polarity bits (CHPOL/CLPOL).  The modulation
 *       source selects between 0-degree and 180-degree phases.
 *
 * The module includes glitch-prevention logic: when carrier
 * synchronization is enabled (CHSYNC/CLSYNC), carrier transitions
 * are synchronized to clock edges to prevent output glitches during
 * carrier switching.
 *
 * Typical usage:
 *   1. Select the modulation source via MD1SRC.SEL[3:0]
 *   2. Select the low carrier signal via MD1CARL.CL[4:0]
 *   3. Select the high carrier signal via MD1CARH.CH[4:0]
 *   4. Configure carrier polarity and synchronization in MD1CON1
 *   5. Configure output polarity in MD1CON0
 *   6. Enable the module by setting MD1CON0.EN = 1
 *   7. Route the DSM output pin via the PPS output register
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *
 * @note SFR8(addr) and SFR16(addr) macros must be defined before
 *       including this header (typically via pic18f_q84.h).
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_DSM_H
#define PIC18F_Q84_DSM_H

/* ============================================================================
 * MD1CON0 - DSM Control Register 0
 * Address: 0x006A
 *
 * Controls the enable state, output polarity, and software modulation
 * bit for the Data Signal Modulator.
 *
 * When the module is enabled, the output reflects the selected carrier
 * (CARH or CARL) depending on the modulation source state.  When
 * disabled, the output is held at the inactive level determined by
 * the OPOL bit.
 *
 * Bit 7    EN      DSM Enable
 *                    1 = DSM module is enabled; modulated output is
 *                        generated based on modulation source and
 *                        carrier selections.
 *                    0 = DSM module is disabled; output is driven to
 *                        the inactive polarity level and the module
 *                        draws minimal current.
 * Bit 6    --      Reserved
 * Bit 5    OUT     DSM Output (read-only)
 *                    Reflects the current state of the modulated
 *                    output signal after polarity control (OPOL).
 *                    Useful for monitoring the output in software
 *                    without requiring external pin feedback.
 * Bit 4    OPOL    Output Polarity
 *                    1 = Output signal is inverted
 *                    0 = Output signal is non-inverted
 * Bits 3:1 --      Reserved
 * Bit 0    BIT     Modulation Bit Value
 *                    When the modulation source (MD1SRC) is set to
 *                    software control (SEL = 0000), this bit directly
 *                    selects the carrier:
 *                      1 = CARH (Carrier High) is routed to output
 *                      0 = CARL (Carrier Low) is routed to output
 *                    This bit has no effect when a hardware modulation
 *                    source is selected.
 * ========================================================================= */
#define MD1CON0 SFR8(0x006A)

#define MD1CON0_EN (1u << 7)   /**< DSM enable                     */
#define MD1CON0_OUT (1u << 5)  /**< DSM output state (read-only)   */
#define MD1CON0_OPOL (1u << 4) /**< Output polarity invert         */
#define MD1CON0_BIT (1u << 0)  /**< Software modulation bit value  */

/* ============================================================================
 * MD1CON1 - DSM Control Register 1
 * Address: 0x006B
 *
 * Controls the polarity and synchronization behavior of the two
 * carrier signals (CARH and CARL).
 *
 * The polarity bits (CHPOL, CLPOL) allow inverting each carrier
 * independently, which is essential for PSK modulation (where both
 * carriers derive from the same source but one is inverted).
 *
 * The synchronization bits (CHSYNC, CLSYNC) enable glitch-free
 * carrier switching.  When enabled, carrier transitions are held
 * until the next active clock edge of the respective carrier signal,
 * preventing partial-cycle glitches at the output.  It is strongly
 * recommended to enable synchronization for both carriers in most
 * applications.
 *
 * Bit 7    CHPOL   Carrier High Polarity
 *                    1 = CARH signal is inverted before use
 *                    0 = CARH signal is used as-is
 * Bit 6    CHSYNC  Carrier High Synchronization
 *                    1 = Synchronize CARH to its clock edges for
 *                        glitch-free switching when modulation
 *                        transitions occur
 *                    0 = No synchronization; carrier switches
 *                        immediately (may produce glitches)
 * Bit 5    CLPOL   Carrier Low Polarity
 *                    1 = CARL signal is inverted before use
 *                    0 = CARL signal is used as-is
 * Bit 4    CLSYNC  Carrier Low Synchronization
 *                    1 = Synchronize CARL to its clock edges for
 *                        glitch-free switching
 *                    0 = No synchronization; carrier switches
 *                        immediately (may produce glitches)
 * Bits 3:0 --      Reserved
 * ========================================================================= */
#define MD1CON1 SFR8(0x006B)

#define MD1CON1_CHPOL (1u << 7)  /**< Carrier High polarity invert   */
#define MD1CON1_CHSYNC (1u << 6) /**< Carrier High sync enable       */
#define MD1CON1_CLPOL (1u << 5)  /**< Carrier Low polarity invert    */
#define MD1CON1_CLSYNC (1u << 4) /**< Carrier Low sync enable        */

/* ============================================================================
 * MD1SRC - DSM Modulation Source Select Register
 * Address: 0x006C
 *
 * Selects the signal used as the modulation source.  The modulation
 * source determines which carrier (CARH or CARL) is routed to the
 * output at any given time:
 *   - When modulation source = 1, CARH is selected
 *   - When modulation source = 0, CARL is selected
 *
 * Bits 7:4  -- Reserved (read as 0, write as 0)
 * Bits 3:0  SEL[3:0]  Modulation Source Select
 *
 * Common source selections (refer to DS40002213D "DSM Modulation
 * Source Selection" table for the complete enumeration):
 *   0000 = MD1CON0.BIT (software-controlled modulation)
 *   0001 = UART TX output
 *   0010-1111 = Various peripheral output signals
 * ========================================================================= */
#define MD1SRC SFR8(0x006C)

#define MD1SRC_SEL_MASK 0x0Fu /**< SEL[3:0] bit mask              */
#define MD1SRC_SEL_SHIFT 0u   /**< SEL[3:0] bit position          */

/** Modulation source select values for MD1SRC SEL[3:0] */
#define MD1SRC_SEL_BIT 0x00u     /**< MD1CON0.BIT (software control) */
#define MD1SRC_SEL_UART_TX 0x01u /**< UART TX output                 */

/* ============================================================================
 * MD1CARL - DSM Carrier Low Select Register
 * Address: 0x006D
 *
 * Selects the clock or signal source used as the "low" carrier.  This
 * carrier is routed to the modulated output when the modulation source
 * is in the low (0) state.
 *
 * For OOK/AM modulation, the low carrier is typically set to a DC low
 * level (ground), producing silence when modulation = 0.  For FSK,
 * it is set to the lower of the two shift frequencies.
 *
 * Bits 7:5  -- Reserved (read as 0, write as 0)
 * Bits 4:0  CL[4:0]  Low Carrier Source Select
 *
 * Refer to DS40002213D "DSM Carrier Low Source Selection" table for
 * the complete mapping of CL values to available clock sources.
 * ========================================================================= */
#define MD1CARL SFR8(0x006D)

#define MD1CARL_CL_MASK 0x1Fu /**< CL[4:0] bit mask               */
#define MD1CARL_CL_SHIFT 0u   /**< CL[4:0] bit position           */

/* ============================================================================
 * MD1CARH - DSM Carrier High Select Register
 * Address: 0x006E
 *
 * Selects the clock or signal source used as the "high" carrier.  This
 * carrier is routed to the modulated output when the modulation source
 * is in the high (1) state.
 *
 * For OOK/AM modulation, the high carrier is typically set to the
 * desired RF or baseband clock frequency.  For FSK, it is set to the
 * higher of the two shift frequencies.
 *
 * Bits 7:5  -- Reserved (read as 0, write as 0)
 * Bits 4:0  CH[4:0]  High Carrier Source Select
 *
 * Refer to DS40002213D "DSM Carrier High Source Selection" table for
 * the complete mapping of CH values to available clock sources.
 * ========================================================================= */
#define MD1CARH SFR8(0x006E)

#define MD1CARH_CH_MASK 0x1Fu /**< CH[4:0] bit mask               */
#define MD1CARH_CH_SHIFT 0u   /**< CH[4:0] bit position           */

#endif /* PIC18F_Q84_DSM_H */
