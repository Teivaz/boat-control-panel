/**
 * @file pic18f_q84_ccp.h
 * @brief CCP (Capture/Compare/PWM) register definitions for PIC18F27/47/57Q84.
 *
 * The PIC18F-Q84 devices include three independent CCP modules (CCP1, CCP2,
 * CCP3), each providing:
 *
 *   - 16-bit Capture mode: latch a timer value on an input pin event
 *     (every falling edge, every rising edge, every 4th rising, every 16th
 *     rising, or every edge)
 *   - 16-bit Compare mode: trigger an action when a timer matches a preset
 *     value (set output, clear output, toggle output, or generate a special
 *     event trigger that can auto-start an ADC conversion or reset a timer)
 *   - 10-bit PWM mode: generate a pulse-width modulated output using the
 *     selected timer as the timebase; duty cycle is set via the CCPRn
 *     register with configurable left/right alignment
 *
 * Each module's timer source is selected by the CCPTMRS0 register, which
 * maps each CCP to one of the available timers (Timer2/4/6, etc.).
 *
 * The capture input source for each module is selected by the CCPnCAP
 * register's CTS field.
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 26: Capture/Compare/PWM (CCP) Module
 *
 * @note SFR8(addr) and SFR16(addr) macros must be defined before including
 *       this header (typically via pic18f_q84.h).
 * @note All register addresses are absolute.  Multi-byte registers are
 *       little-endian (low byte at base address).
 * @note Reserved bits should be written as 0; their read values are undefined.
 */

#ifndef PIC18F_Q84_CCP_H
#define PIC18F_Q84_CCP_H

/* ============================================================================
 * CCP1 - Capture/Compare/PWM Module 1
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * CCPR1 - CCP1 Capture/Compare Register (0x0340-0x0341, 16-bit)
 *
 * In Capture mode:  Latched timer value on capture event (read-only).
 * In Compare mode:  16-bit compare value (read/write); match generates
 *                   the action selected by MODE bits.
 * In PWM mode:      10-bit duty cycle value.
 *                   FMT=1 (right-aligned): duty in bits 9:0 of CCPR1.
 *                   FMT=0 (left-aligned):  duty in bits 15:6 of CCPR1.
 *
 * CCPR1H (0x0341):  Bits 7:0  CCPR1[15:8]
 * CCPR1L (0x0340):  Bits 7:0  CCPR1[7:0]
 * ------------------------------------------------------------------------- */
#define CCPR1L SFR8(0x0340)
#define CCPR1H SFR8(0x0341)
#define CCPR1 SFR16(0x0340) /**< 16-bit capture/compare reg  */

/* ---------------------------------------------------------------------------
 * CCP1CON - CCP1 Control Register (0x0342)
 *
 * Bit 7    EN      Module Enable
 *                    1 = CCP1 module is enabled
 *                    0 = CCP1 module is disabled (reset state)
 * Bit 6    --      Reserved
 * Bit 5    OUT     CCP Output (read-only)
 *                    Reflects the current state of the CCP1 output pin.
 *                    In PWM mode: current PWM output level.
 *                    In Compare mode: state set by last compare match.
 * Bit 4    FMT     Output Format Select
 *                    1 = Right-aligned (10-bit PWM duty in CCPR1[9:0])
 *                    0 = Left-aligned  (10-bit PWM duty in CCPR1[15:6])
 * Bits 3:0 MODE[3:0]  Operating Mode Select
 *                    0000 = Module disabled (Capture/Compare/PWM off)
 *                    0001 = Capture mode: every falling edge
 *                    0010 = Capture mode: every rising edge
 *                    0011 = Capture mode: every 4th rising edge
 *                    0100 = Capture mode: every 16th rising edge
 *                    0101 = Capture mode: every rising AND falling edge
 *                    1000 = Compare mode: set output on match
 *                    1001 = Compare mode: clear output on match
 *                    1010 = Compare mode: toggle output on match
 *                    1011 = Compare mode: trigger special event
 *                    1100 = PWM mode
 *                    Others = Reserved
 * ------------------------------------------------------------------------- */
#define CCP1CON SFR8(0x0342)

#define CCP1CON_EN (1u << 7)  /**< Module enable                  */
#define CCP1CON_OUT (1u << 5) /**< CCP output state (read-only)   */
#define CCP1CON_FMT (1u << 4) /**< Format (1=right, 0=left align) */

#define CCP1CON_MODE_MASK 0x0Fu /**< MODE[3:0] bit mask             */
#define CCP1CON_MODE_SHIFT 0u   /**< MODE[3:0] bit position         */

/** Operating mode values for CCP1CON MODE[3:0] */
#define CCP1CON_MODE_OFF 0x00u      /**< Module disabled                */
#define CCP1CON_MODE_CAP_FALL 0x01u /**< Capture every falling edge     */
#define CCP1CON_MODE_CAP_RISE 0x02u /**< Capture every rising edge      */
#define CCP1CON_MODE_CAP_4TH 0x03u  /**< Capture every 4th rising edge  */
#define CCP1CON_MODE_CAP_16TH 0x04u /**< Capture every 16th rising edge */
#define CCP1CON_MODE_CAP_BOTH 0x05u /**< Capture every edge (rise+fall) */
#define CCP1CON_MODE_CMP_SET 0x08u  /**< Compare: set output on match   */
#define CCP1CON_MODE_CMP_CLR 0x09u  /**< Compare: clear output on match */
#define CCP1CON_MODE_CMP_TOG 0x0Au  /**< Compare: toggle output on match*/
#define CCP1CON_MODE_CMP_TRIG 0x0Bu /**< Compare: special event trigger */
#define CCP1CON_MODE_PWM 0x0Cu      /**< PWM mode                       */

/* ---------------------------------------------------------------------------
 * CCP1CAP - CCP1 Capture Input Source Register (0x0343)
 *
 * Selects the input source for capture events.  The available sources
 * are device-specific; see DS40002213D Table 26-1 for the full mapping.
 *
 * Bits 7:4  -- Reserved
 * Bits 3:0  CTS[3:0]  Capture Timer/Input Source Select
 * ------------------------------------------------------------------------- */
#define CCP1CAP SFR8(0x0343)

#define CCP1CAP_CTS_MASK 0x0Fu /**< CTS[3:0] bit mask              */
#define CCP1CAP_CTS_SHIFT 0u   /**< CTS[3:0] bit position          */

/* ============================================================================
 * CCP2 - Capture/Compare/PWM Module 2
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * CCPR2 - CCP2 Capture/Compare Register (0x0344-0x0345, 16-bit)
 *
 * Identical function to CCPR1.  See CCPR1 for description.
 *
 * CCPR2H (0x0345):  Bits 7:0  CCPR2[15:8]
 * CCPR2L (0x0344):  Bits 7:0  CCPR2[7:0]
 * ------------------------------------------------------------------------- */
#define CCPR2L SFR8(0x0344)
#define CCPR2H SFR8(0x0345)
#define CCPR2 SFR16(0x0344) /**< 16-bit capture/compare reg  */

/* ---------------------------------------------------------------------------
 * CCP2CON - CCP2 Control Register (0x0346)
 *
 * Identical bit layout to CCP1CON.  See CCP1CON for field descriptions.
 *
 * Bit 7    EN      Module Enable
 * Bit 5    OUT     CCP Output (read-only)
 * Bit 4    FMT     Output Format Select
 * Bits 3:0 MODE[3:0]  Operating Mode Select
 * ------------------------------------------------------------------------- */
#define CCP2CON SFR8(0x0346)

#define CCP2CON_EN (1u << 7)  /**< Module enable                  */
#define CCP2CON_OUT (1u << 5) /**< CCP output state (read-only)   */
#define CCP2CON_FMT (1u << 4) /**< Format (1=right, 0=left align) */

#define CCP2CON_MODE_MASK 0x0Fu /**< MODE[3:0] bit mask             */
#define CCP2CON_MODE_SHIFT 0u   /**< MODE[3:0] bit position         */

/** Operating mode values for CCP2CON MODE[3:0] */
#define CCP2CON_MODE_OFF 0x00u      /**< Module disabled                */
#define CCP2CON_MODE_CAP_FALL 0x01u /**< Capture every falling edge     */
#define CCP2CON_MODE_CAP_RISE 0x02u /**< Capture every rising edge      */
#define CCP2CON_MODE_CAP_4TH 0x03u  /**< Capture every 4th rising edge  */
#define CCP2CON_MODE_CAP_16TH 0x04u /**< Capture every 16th rising edge */
#define CCP2CON_MODE_CAP_BOTH 0x05u /**< Capture every edge (rise+fall) */
#define CCP2CON_MODE_CMP_SET 0x08u  /**< Compare: set output on match   */
#define CCP2CON_MODE_CMP_CLR 0x09u  /**< Compare: clear output on match */
#define CCP2CON_MODE_CMP_TOG 0x0Au  /**< Compare: toggle output on match*/
#define CCP2CON_MODE_CMP_TRIG 0x0Bu /**< Compare: special event trigger */
#define CCP2CON_MODE_PWM 0x0Cu      /**< PWM mode                       */

/* ---------------------------------------------------------------------------
 * CCP2CAP - CCP2 Capture Input Source Register (0x0347)
 *
 * Identical to CCP1CAP.  See CCP1CAP for description.
 *
 * Bits 7:4  -- Reserved
 * Bits 3:0  CTS[3:0]  Capture Timer/Input Source Select
 * ------------------------------------------------------------------------- */
#define CCP2CAP SFR8(0x0347)

#define CCP2CAP_CTS_MASK 0x0Fu /**< CTS[3:0] bit mask              */
#define CCP2CAP_CTS_SHIFT 0u   /**< CTS[3:0] bit position          */

/* ============================================================================
 * CCP3 - Capture/Compare/PWM Module 3
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * CCPR3 - CCP3 Capture/Compare Register (0x0348-0x0349, 16-bit)
 *
 * Identical function to CCPR1.  See CCPR1 for description.
 *
 * CCPR3H (0x0349):  Bits 7:0  CCPR3[15:8]
 * CCPR3L (0x0348):  Bits 7:0  CCPR3[7:0]
 * ------------------------------------------------------------------------- */
#define CCPR3L SFR8(0x0348)
#define CCPR3H SFR8(0x0349)
#define CCPR3 SFR16(0x0348) /**< 16-bit capture/compare reg  */

/* ---------------------------------------------------------------------------
 * CCP3CON - CCP3 Control Register (0x034A)
 *
 * Identical bit layout to CCP1CON.  See CCP1CON for field descriptions.
 *
 * Bit 7    EN      Module Enable
 * Bit 5    OUT     CCP Output (read-only)
 * Bit 4    FMT     Output Format Select
 * Bits 3:0 MODE[3:0]  Operating Mode Select
 * ------------------------------------------------------------------------- */
#define CCP3CON SFR8(0x034A)

#define CCP3CON_EN (1u << 7)  /**< Module enable                  */
#define CCP3CON_OUT (1u << 5) /**< CCP output state (read-only)   */
#define CCP3CON_FMT (1u << 4) /**< Format (1=right, 0=left align) */

#define CCP3CON_MODE_MASK 0x0Fu /**< MODE[3:0] bit mask             */
#define CCP3CON_MODE_SHIFT 0u   /**< MODE[3:0] bit position         */

/** Operating mode values for CCP3CON MODE[3:0] */
#define CCP3CON_MODE_OFF 0x00u      /**< Module disabled                */
#define CCP3CON_MODE_CAP_FALL 0x01u /**< Capture every falling edge     */
#define CCP3CON_MODE_CAP_RISE 0x02u /**< Capture every rising edge      */
#define CCP3CON_MODE_CAP_4TH 0x03u  /**< Capture every 4th rising edge  */
#define CCP3CON_MODE_CAP_16TH 0x04u /**< Capture every 16th rising edge */
#define CCP3CON_MODE_CAP_BOTH 0x05u /**< Capture every edge (rise+fall) */
#define CCP3CON_MODE_CMP_SET 0x08u  /**< Compare: set output on match   */
#define CCP3CON_MODE_CMP_CLR 0x09u  /**< Compare: clear output on match */
#define CCP3CON_MODE_CMP_TOG 0x0Au  /**< Compare: toggle output on match*/
#define CCP3CON_MODE_CMP_TRIG 0x0Bu /**< Compare: special event trigger */
#define CCP3CON_MODE_PWM 0x0Cu      /**< PWM mode                       */

/* ---------------------------------------------------------------------------
 * CCP3CAP - CCP3 Capture Input Source Register (0x034B)
 *
 * Identical to CCP1CAP.  See CCP1CAP for description.
 *
 * Bits 7:4  -- Reserved
 * Bits 3:0  CTS[3:0]  Capture Timer/Input Source Select
 * ------------------------------------------------------------------------- */
#define CCP3CAP SFR8(0x034B)

#define CCP3CAP_CTS_MASK 0x0Fu /**< CTS[3:0] bit mask              */
#define CCP3CAP_CTS_SHIFT 0u   /**< CTS[3:0] bit position          */

/* ============================================================================
 * CCP Timer Selection Register
 * ========================================================================= */

/**
 * CCPTMRS0 - CCP Timer Selection Register 0 (0x034C)
 *
 * Selects which timer peripheral provides the timebase for each CCP
 * module.  The timer selection affects all operating modes (capture,
 * compare, and PWM).  Consult DS40002213D Table 26-2 for the mapping
 * of CxTSEL values to specific timer peripherals.
 *
 * Bits 7:6  -- Reserved
 * Bits 5:4  C3TSEL[1:0]  CCP3 Timer Select
 * Bits 3:2  C2TSEL[1:0]  CCP2 Timer Select
 * Bits 1:0  C1TSEL[1:0]  CCP1 Timer Select
 */
#define CCPTMRS0 SFR8(0x034C)

/** CCP3 timer select field */
#define CCPTMRS0_C3TSEL_MASK 0x30u /**< C3TSEL[1:0] bit mask           */
#define CCPTMRS0_C3TSEL_SHIFT 4u   /**< C3TSEL[1:0] bit position       */

/** CCP2 timer select field */
#define CCPTMRS0_C2TSEL_MASK 0x0Cu /**< C2TSEL[1:0] bit mask           */
#define CCPTMRS0_C2TSEL_SHIFT 2u   /**< C2TSEL[1:0] bit position       */

/** CCP1 timer select field */
#define CCPTMRS0_C1TSEL_MASK 0x03u /**< C1TSEL[1:0] bit mask           */
#define CCPTMRS0_C1TSEL_SHIFT 0u   /**< C1TSEL[1:0] bit position       */

#endif /* PIC18F_Q84_CCP_H */
