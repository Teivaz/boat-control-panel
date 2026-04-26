/**
 * @file pic18f_q84_cwg.h
 * @brief CWG (Complementary Waveform Generator) register definitions for
 *        PIC18F27/47/57Q84 microcontrollers.
 *
 * The CWG module generates non-overlapping complementary output signals
 * from a single input waveform.  Dead-band delays are independently
 * programmable for the rising and falling edges to prevent shoot-through
 * in half-bridge and full-bridge power converter topologies.
 *
 * There are three identical CWG instances (CWG1, CWG2, CWG3), each
 * with four possible output pins (A, B, C, D) and the following
 * operating modes:
 *
 *   - Half-bridge (MODE = 000):
 *       Outputs A and B are complementary with dead-band.  Outputs C
 *       and D are not used (controlled by steering).  This is the
 *       most common mode for driving half-bridge power stages such as
 *       synchronous buck converters.
 *
 *   - Full-bridge forward (MODE = 001):
 *       Outputs A and D are driven high, outputs B and C are driven
 *       low.  Provides unidirectional current flow through a full
 *       H-bridge load.
 *
 *   - Full-bridge reverse (MODE = 010):
 *       Outputs B and C are driven high, outputs A and D are driven
 *       low.  Reverses current flow through the H-bridge.
 *
 *   - Full-bridge bidirectional (MODE = 011):
 *       All four outputs are active with complementary drive and
 *       dead-band on both pairs (A/B and C/D).  Enables PWM-
 *       controlled bidirectional current through the H-bridge.
 *
 *   - Single output A (MODE = 100):
 *       Only output A is driven from the input source.  The other
 *       outputs are controlled by the steering register.
 *
 * Auto-shutdown:
 *   Each CWG instance can be configured to automatically shut down
 *   its outputs when one or more external or internal fault events
 *   occur (e.g., comparator output, pin state, timer overflow).
 *   The shutdown state for each output pair is independently
 *   configurable to either an inactive drive level or tri-state
 *   (high-impedance).  Auto-restart can be enabled to resume
 *   operation when the shutdown condition clears.
 *
 * Output steering:
 *   The CWGxSTR register allows individual outputs to be overridden
 *   with static levels, useful for commutation patterns in BLDC motor
 *   control or for disabling specific outputs in single-output mode.
 *
 * Typical usage (half-bridge PWM drive):
 *   1. Select the clock source via CWGxCLK.CS
 *   2. Select the input signal (e.g., PWM output) via CWGxISM.ISM[4:0]
 *   3. Set rising dead-band count in CWGxDBR.DBR[5:0]
 *   4. Set falling dead-band count in CWGxDBF.DBF[5:0]
 *   5. Configure mode and enable: CWGxCON0.MODE = 000, CWGxCON0.EN = 1
 *   6. Set CWGxCON0.LD = 1 to load the dead-band values
 *   7. Route CWG output pins via the PPS output registers
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *
 * @note SFR8(addr) macro must be defined before including this header
 *       (typically via pic18f_q84.h).
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_CWG_H
#define PIC18F_Q84_CWG_H

/* ****************************************************************************
 *
 *  CWG1 - Complementary Waveform Generator 1
 *  Registers: 0x03BB - 0x03C3
 *
 * ***************************************************************************/

/* ============================================================================
 * CWG1CLK - CWG1 Clock Source Select Register
 * Address: 0x03BB
 *
 * Selects the clock source used to time the dead-band delay counters.
 * The dead-band delay is measured in periods of this clock, so the
 * clock frequency directly determines dead-band time resolution:
 *   Dead-band time = DBR (or DBF) x T_CWG_CLK
 *
 * A faster clock gives finer dead-band resolution but limits the
 * maximum dead-band delay to 63 clock periods.
 *
 * Bits 7:1  -- Reserved
 * Bit  0    CS  Clock Source Select
 *                1 = HFINTOSC (high-frequency internal oscillator)
 *                0 = FOSC (system instruction clock)
 * ========================================================================= */
#define CWG1CLK SFR8(0x03BC)

#define CWG1CLK_CS (1u << 0) /**< Clock source: 1=HFINTOSC 0=FOSC */

/* ============================================================================
 * CWG1ISM - CWG1 Input Source Select Register
 * Address: 0x03BC
 *
 * Selects the input signal to the CWG.  The input signal is the
 * waveform that will be processed by the complementary waveform
 * generator to produce the dead-band-adjusted output signals.
 *
 * Common input sources include PWM outputs, CLC outputs, and
 * comparator outputs.  Refer to DS40002213D "CWG Input Source
 * Selection" table for the complete mapping of ISM values.
 *
 * Bits 7:5  -- Reserved
 * Bits 4:0  ISM[4:0]  Input Source Select
 * ========================================================================= */
#define CWG1ISM SFR8(0x03BD)

#define CWG1ISM_ISM_MASK 0x1Fu /**< ISM[4:0] bit mask               */
#define CWG1ISM_ISM_SHIFT 0u   /**< ISM[4:0] bit position           */

/* ============================================================================
 * CWG1DBR - CWG1 Dead-Band Rising Edge Count Register
 * Address: 0x03BD
 *
 * Sets the dead-band delay applied to the rising edge of the CWG
 * input signal.  The delay is measured in CWG clock periods (selected
 * by CWG1CLK.CS).
 *
 * The rising dead-band inserts a delay between the turn-off of one
 * output and the turn-on of its complement, preventing simultaneous
 * conduction (shoot-through) during the low-to-high input transition.
 *
 * The new dead-band value takes effect only after CWG1CON0.LD is set
 * to 1.  This double-buffering prevents glitches from mid-cycle
 * dead-band changes.
 *
 * Bits 7:6  -- Reserved
 * Bits 5:0  DBR[5:0]  Rising Dead-Band Count (0-63 clock periods)
 *                       0 = No dead-band delay on rising edge
 *                       63 = Maximum dead-band of 63 clock periods
 * ========================================================================= */
#define CWG1DBR SFR8(0x03BE)

#define CWG1DBR_DBR_MASK 0x3Fu /**< DBR[5:0] bit mask               */
#define CWG1DBR_DBR_SHIFT 0u   /**< DBR[5:0] bit position           */

/* ============================================================================
 * CWG1DBF - CWG1 Dead-Band Falling Edge Count Register
 * Address: 0x03BE
 *
 * Sets the dead-band delay applied to the falling edge of the CWG
 * input signal.  Identical in function to CWG1DBR but applies to the
 * high-to-low transition.
 *
 * Independent rising/falling dead-band values allow compensation for
 * asymmetric turn-on/turn-off delays in the power switching devices.
 *
 * The new dead-band value takes effect only after CWG1CON0.LD is set
 * to 1.
 *
 * Bits 7:6  -- Reserved
 * Bits 5:0  DBF[5:0]  Falling Dead-Band Count (0-63 clock periods)
 *                       0 = No dead-band delay on falling edge
 *                       63 = Maximum dead-band of 63 clock periods
 * ========================================================================= */
#define CWG1DBF SFR8(0x03BF)

#define CWG1DBF_DBF_MASK 0x3Fu /**< DBF[5:0] bit mask               */
#define CWG1DBF_DBF_SHIFT 0u   /**< DBF[5:0] bit position           */

/* ============================================================================
 * CWG1CON0 - CWG1 Control Register 0
 * Address: 0x03BF
 *
 * Primary control register for the CWG module.  Controls the enable
 * state, dead-band load trigger, and operating mode.
 *
 * The LD bit is used to transfer new dead-band values from DBR/DBF
 * into the active counters.  It is auto-cleared by hardware once the
 * transfer completes, ensuring dead-band updates occur at a safe
 * boundary.
 *
 * Bit 7      EN   CWG Enable
 *                   1 = CWG module is enabled and generating outputs
 *                   0 = CWG module is disabled; outputs are inactive
 * Bit 6      LD   Dead-Band Load (auto-clearing)
 *                   1 = Transfer DBR/DBF values to active dead-band
 *                       counters; hardware clears this bit when done
 *                   0 = No pending dead-band update
 * Bits 5:3   --   Reserved
 * Bits 2:0   MODE[2:0]  Output Mode Select
 *                   000 = Half-bridge: A and B are complementary
 *                   001 = Full-bridge forward: A,D high; B,C low
 *                   010 = Full-bridge reverse: B,C high; A,D low
 *                   011 = Full-bridge bidirectional: all four outputs
 *                         active with complementary dead-band
 *                   100 = Single output A only
 *                   101-111 = Reserved
 * ========================================================================= */
#define CWG1CON0 SFR8(0x03C0)

#define CWG1CON0_EN (1u << 7) /**< CWG enable                      */
#define CWG1CON0_LD (1u << 6) /**< Dead-band load (auto-clearing)  */

#define CWG1CON0_MODE_MASK 0x07u /**< MODE[2:0] bit mask              */
#define CWG1CON0_MODE_SHIFT 0u   /**< MODE[2:0] bit position          */

/** CWG1CON0 MODE[2:0] output mode select values */
#define CWG1CON0_MODE_HALF_BRIDGE 0x00u       /**< Half-bridge (A/B compl.)  */
#define CWG1CON0_MODE_FULL_BRIDGE_FWD 0x01u   /**< Full-bridge forward       */
#define CWG1CON0_MODE_FULL_BRIDGE_REV 0x02u   /**< Full-bridge reverse       */
#define CWG1CON0_MODE_FULL_BRIDGE_BIDIR 0x03u /**< Full-bridge bidirectional */
#define CWG1CON0_MODE_SINGLE_A 0x04u          /**< Single output A           */

/* ============================================================================
 * CWG1CON1 - CWG1 Control Register 1
 * Address: 0x03C0
 *
 * Configures the shutdown output states and auto-restart behavior.
 * Also provides read-only access to the current CWG input value.
 *
 * During an auto-shutdown event, outputs B and D are driven to the
 * state selected by LSBD[1:0], and outputs A and C are driven to the
 * state selected by LSAC[1:0].  This allows safe output states to be
 * selected independently for each output pair.
 *
 * Bits 7:6  LSBD[1:0]  Shutdown State for outputs B and D
 *                        00 = Drive inactive level
 *                        01 = Tri-state (high-impedance)
 *                        10 = Reserved
 *                        11 = Reserved
 * Bits 5:4  LSAC[1:0]  Shutdown State for outputs A and C
 *                        00 = Drive inactive level
 *                        01 = Tri-state (high-impedance)
 *                        10 = Reserved
 *                        11 = Reserved
 * Bit  3    REN         Auto-Restart Enable
 *                        1 = CWG automatically restarts when all
 *                            shutdown sources are no longer active
 *                        0 = CWG remains shut down until software
 *                            clears the shutdown condition
 * Bits 2:1  --          Reserved
 * Bit  0    IN          CWG Input Value (read-only)
 *                        Reflects the current logic level of the
 *                        selected input source signal.
 * ========================================================================= */
#define CWG1CON1 SFR8(0x03C1)

#define CWG1CON1_LSBD_MASK (0x03u << 6) /**< LSBD[1:0] bit mask            */
#define CWG1CON1_LSBD_SHIFT 6u          /**< LSBD[1:0] bit position        */
#define CWG1CON1_LSAC_MASK (0x03u << 4) /**< LSAC[1:0] bit mask            */
#define CWG1CON1_LSAC_SHIFT 4u          /**< LSAC[1:0] bit position        */
#define CWG1CON1_REN (1u << 3)          /**< Auto-restart enable            */
#define CWG1CON1_IN (1u << 0)           /**< Input value (read-only)        */

/** CWG1CON1 LSBD[1:0] / LSAC[1:0] shutdown state values */
#define CWG_LSXX_INACTIVE 0x00u /**< Drive inactive level during shutdown  */
#define CWG_LSXX_TRISTATE 0x01u /**< Tri-state (high-Z) during shutdown    */

/* ============================================================================
 * CWG1AS0 - CWG1 Auto-Shutdown Source Register 0
 * Address: 0x03C1
 *
 * Configures the primary auto-shutdown sources.  Each ASxE bit enables
 * a specific peripheral or pin signal as a shutdown trigger.  When any
 * enabled shutdown source becomes active, the CWG immediately forces
 * its outputs to the states configured in CWG1CON1.
 *
 * The SHUTDOWN bit indicates whether the CWG is currently in the
 * shutdown state.  It is set by hardware when a shutdown event occurs
 * and cleared by hardware when auto-restart is enabled and all sources
 * have cleared, or by software writing 0.
 *
 * Bit 7    SHUTDOWN  Auto-Shutdown Status (read-only)
 *                     1 = CWG is currently in shutdown state
 *                     0 = CWG is operating normally
 * Bit 6    REN       Auto-Restart Enable
 *                     1 = Automatic restart when shutdown clears
 *                     0 = Manual restart required (clear SHUTDOWN)
 * Bit 5    AS5E      Auto-Shutdown Source 5 Enable
 * Bit 4    AS4E      Auto-Shutdown Source 4 Enable
 * Bit 3    AS3E      Auto-Shutdown Source 3 Enable
 * Bit 2    AS2E      Auto-Shutdown Source 2 Enable
 * Bit 1    AS1E      Auto-Shutdown Source 1 Enable
 * Bit 0    AS0E      Auto-Shutdown Source 0 Enable
 *
 * Refer to DS40002213D "CWG Auto-Shutdown Sources" table for the
 * mapping of each ASxE bit to specific peripheral signals.
 * ========================================================================= */
#define CWG1AS0 SFR8(0x03C2)

#define CWG1AS0_SHUTDOWN (1u << 7) /**< Shutdown status (read-only)     */
#define CWG1AS0_REN (1u << 6)      /**< Auto-restart enable             */
#define CWG1AS0_AS5E (1u << 5)     /**< Shutdown source 5 enable        */
#define CWG1AS0_AS4E (1u << 4)     /**< Shutdown source 4 enable        */
#define CWG1AS0_AS3E (1u << 3)     /**< Shutdown source 3 enable        */
#define CWG1AS0_AS2E (1u << 2)     /**< Shutdown source 2 enable        */
#define CWG1AS0_AS1E (1u << 1)     /**< Shutdown source 1 enable        */
#define CWG1AS0_AS0E (1u << 0)     /**< Shutdown source 0 enable        */

/* ============================================================================
 * CWG1AS1 - CWG1 Auto-Shutdown Source Register 1
 * Address: 0x03C2
 *
 * Provides additional auto-shutdown source enables beyond those in
 * CWG1AS0.
 *
 * Bit 7    AS7E  Auto-Shutdown Source 7 Enable
 * Bit 6    AS6E  Auto-Shutdown Source 6 Enable
 * Bits 5:0 --    Reserved
 * ========================================================================= */
#define CWG1AS1 SFR8(0x03C3)

#define CWG1AS1_AS7E (1u << 7) /**< Shutdown source 7 enable        */
#define CWG1AS1_AS6E (1u << 6) /**< Shutdown source 6 enable        */

/* ============================================================================
 * CWG1STR - CWG1 Output Steering Register
 * Address: 0x03C3
 *
 * Controls output steering and override functionality.  Each output
 * (A, B, C, D) has two associated bits:
 *
 *   OVRx  - Override enable:  when set, the corresponding output is
 *           driven by the STRx bit value instead of the CWG engine.
 *   STRx  - Steer value:  the static level driven to the output when
 *           the corresponding OVRx bit is set.
 *
 * Output steering is useful for:
 *   - BLDC motor commutation: selectively enabling/disabling bridge
 *     legs to control current flow direction.
 *   - Testing: forcing individual outputs to known states.
 *   - Single-output mode: holding unused outputs at safe levels.
 *
 * When OVRx = 0, the output is controlled normally by the CWG engine.
 * When OVRx = 1, the output is forced to the STRx value regardless of
 * the CWG input and mode.
 *
 * Bit 7    OVRD  Override enable for output D
 * Bit 6    OVRC  Override enable for output C
 * Bit 5    OVRB  Override enable for output B
 * Bit 4    OVRA  Override enable for output A
 * Bit 3    STRD  Steer value for output D (when OVRD = 1)
 * Bit 2    STRC  Steer value for output C (when OVRC = 1)
 * Bit 1    STRB  Steer value for output B (when OVRB = 1)
 * Bit 0    STRA  Steer value for output A (when OVRA = 1)
 * ========================================================================= */
#define CWG1STR SFR8(0x03C4)

#define CWG1STR_OVRD (1u << 7) /**< Output D override enable        */
#define CWG1STR_OVRC (1u << 6) /**< Output C override enable        */
#define CWG1STR_OVRB (1u << 5) /**< Output B override enable        */
#define CWG1STR_OVRA (1u << 4) /**< Output A override enable        */
#define CWG1STR_STRD (1u << 3) /**< Output D steer value            */
#define CWG1STR_STRC (1u << 2) /**< Output C steer value            */
#define CWG1STR_STRB (1u << 1) /**< Output B steer value            */
#define CWG1STR_STRA (1u << 0) /**< Output A steer value            */

/* ****************************************************************************
 *
 *  CWG2 - Complementary Waveform Generator 2
 *  Registers: 0x03C4 - 0x03CC
 *
 *  Functionally identical to CWG1.  All bit definitions, field masks,
 *  and operating modes are the same; only the register addresses and
 *  the output pin assignments (via PPS) differ.
 *
 * ***************************************************************************/

/* ============================================================================
 * CWG2CLK - CWG2 Clock Source Select Register
 * Address: 0x03C4
 *
 * Bit 0  CS  Clock source: 1 = HFINTOSC, 0 = FOSC
 * ========================================================================= */
#define CWG2CLK SFR8(0x03C5)

#define CWG2CLK_CS (1u << 0) /**< Clock source: 1=HFINTOSC 0=FOSC */

/* ============================================================================
 * CWG2ISM - CWG2 Input Source Select Register
 * Address: 0x03C5
 *
 * Bits 4:0  ISM[4:0]  Input source select
 * ========================================================================= */
#define CWG2ISM SFR8(0x03C6)

#define CWG2ISM_ISM_MASK 0x1Fu /**< ISM[4:0] bit mask               */
#define CWG2ISM_ISM_SHIFT 0u   /**< ISM[4:0] bit position           */

/* ============================================================================
 * CWG2DBR - CWG2 Dead-Band Rising Edge Count Register
 * Address: 0x03C6
 *
 * Bits 5:0  DBR[5:0]  Rising dead-band count (0-63 clocks)
 * ========================================================================= */
#define CWG2DBR SFR8(0x03C7)

#define CWG2DBR_DBR_MASK 0x3Fu /**< DBR[5:0] bit mask               */
#define CWG2DBR_DBR_SHIFT 0u   /**< DBR[5:0] bit position           */

/* ============================================================================
 * CWG2DBF - CWG2 Dead-Band Falling Edge Count Register
 * Address: 0x03C7
 *
 * Bits 5:0  DBF[5:0]  Falling dead-band count (0-63 clocks)
 * ========================================================================= */
#define CWG2DBF SFR8(0x03C8)

#define CWG2DBF_DBF_MASK 0x3Fu /**< DBF[5:0] bit mask               */
#define CWG2DBF_DBF_SHIFT 0u   /**< DBF[5:0] bit position           */

/* ============================================================================
 * CWG2CON0 - CWG2 Control Register 0
 * Address: 0x03C8
 *
 * Bit 7      EN          CWG enable
 * Bit 6      LD          Dead-band load (auto-clearing)
 * Bits 2:0   MODE[2:0]   Output mode select (see CWG1CON0)
 * ========================================================================= */
#define CWG2CON0 SFR8(0x03C9)

#define CWG2CON0_EN (1u << 7) /**< CWG enable                      */
#define CWG2CON0_LD (1u << 6) /**< Dead-band load (auto-clearing)  */

#define CWG2CON0_MODE_MASK 0x07u /**< MODE[2:0] bit mask              */
#define CWG2CON0_MODE_SHIFT 0u   /**< MODE[2:0] bit position          */

/** CWG2CON0 MODE[2:0] output mode select values */
#define CWG2CON0_MODE_HALF_BRIDGE 0x00u       /**< Half-bridge (A/B compl.)  */
#define CWG2CON0_MODE_FULL_BRIDGE_FWD 0x01u   /**< Full-bridge forward       */
#define CWG2CON0_MODE_FULL_BRIDGE_REV 0x02u   /**< Full-bridge reverse       */
#define CWG2CON0_MODE_FULL_BRIDGE_BIDIR 0x03u /**< Full-bridge bidirectional */
#define CWG2CON0_MODE_SINGLE_A 0x04u          /**< Single output A           */

/* ============================================================================
 * CWG2CON1 - CWG2 Control Register 1
 * Address: 0x03C9
 *
 * Bits 7:6  LSBD[1:0]  Shutdown state for outputs B/D
 * Bits 5:4  LSAC[1:0]  Shutdown state for outputs A/C
 * Bit  3    REN         Auto-restart enable
 * Bit  0    IN          Input value (read-only)
 * ========================================================================= */
#define CWG2CON1 SFR8(0x03CA)

#define CWG2CON1_LSBD_MASK (0x03u << 6) /**< LSBD[1:0] bit mask            */
#define CWG2CON1_LSBD_SHIFT 6u          /**< LSBD[1:0] bit position        */
#define CWG2CON1_LSAC_MASK (0x03u << 4) /**< LSAC[1:0] bit mask            */
#define CWG2CON1_LSAC_SHIFT 4u          /**< LSAC[1:0] bit position        */
#define CWG2CON1_REN (1u << 3)          /**< Auto-restart enable            */
#define CWG2CON1_IN (1u << 0)           /**< Input value (read-only)        */

/* ============================================================================
 * CWG2AS0 - CWG2 Auto-Shutdown Source Register 0
 * Address: 0x03CA
 *
 * Bit 7    SHUTDOWN  Shutdown status (read-only)
 * Bit 6    REN       Auto-restart enable
 * Bits 5:0 AS5E-AS0E  Shutdown source enables
 * ========================================================================= */
#define CWG2AS0 SFR8(0x03CB)

#define CWG2AS0_SHUTDOWN (1u << 7) /**< Shutdown status (read-only)     */
#define CWG2AS0_REN (1u << 6)      /**< Auto-restart enable             */
#define CWG2AS0_AS5E (1u << 5)     /**< Shutdown source 5 enable        */
#define CWG2AS0_AS4E (1u << 4)     /**< Shutdown source 4 enable        */
#define CWG2AS0_AS3E (1u << 3)     /**< Shutdown source 3 enable        */
#define CWG2AS0_AS2E (1u << 2)     /**< Shutdown source 2 enable        */
#define CWG2AS0_AS1E (1u << 1)     /**< Shutdown source 1 enable        */
#define CWG2AS0_AS0E (1u << 0)     /**< Shutdown source 0 enable        */

/* ============================================================================
 * CWG2AS1 - CWG2 Auto-Shutdown Source Register 1
 * Address: 0x03CB
 *
 * Bit 7  AS7E  Shutdown source 7 enable
 * Bit 6  AS6E  Shutdown source 6 enable
 * ========================================================================= */
#define CWG2AS1 SFR8(0x03CC)

#define CWG2AS1_AS7E (1u << 7) /**< Shutdown source 7 enable        */
#define CWG2AS1_AS6E (1u << 6) /**< Shutdown source 6 enable        */

/* ============================================================================
 * CWG2STR - CWG2 Output Steering Register
 * Address: 0x03CC
 *
 * Bit 7  OVRD   Bit 3  STRD   (output D override / steer value)
 * Bit 6  OVRC   Bit 2  STRC   (output C override / steer value)
 * Bit 5  OVRB   Bit 1  STRB   (output B override / steer value)
 * Bit 4  OVRA   Bit 0  STRA   (output A override / steer value)
 * ========================================================================= */
#define CWG2STR SFR8(0x03CD)

#define CWG2STR_OVRD (1u << 7) /**< Output D override enable        */
#define CWG2STR_OVRC (1u << 6) /**< Output C override enable        */
#define CWG2STR_OVRB (1u << 5) /**< Output B override enable        */
#define CWG2STR_OVRA (1u << 4) /**< Output A override enable        */
#define CWG2STR_STRD (1u << 3) /**< Output D steer value            */
#define CWG2STR_STRC (1u << 2) /**< Output C steer value            */
#define CWG2STR_STRB (1u << 1) /**< Output B steer value            */
#define CWG2STR_STRA (1u << 0) /**< Output A steer value            */

/* ****************************************************************************
 *
 *  CWG3 - Complementary Waveform Generator 3
 *  Registers: 0x03CD - 0x03D5
 *
 *  Functionally identical to CWG1 and CWG2.  All bit definitions,
 *  field masks, and operating modes are the same; only the register
 *  addresses and the output pin assignments (via PPS) differ.
 *
 * ***************************************************************************/

/* ============================================================================
 * CWG3CLK - CWG3 Clock Source Select Register
 * Address: 0x03CD
 *
 * Bit 0  CS  Clock source: 1 = HFINTOSC, 0 = FOSC
 * ========================================================================= */
#define CWG3CLK SFR8(0x03CE)

#define CWG3CLK_CS (1u << 0) /**< Clock source: 1=HFINTOSC 0=FOSC */

/* ============================================================================
 * CWG3ISM - CWG3 Input Source Select Register
 * Address: 0x03CE
 *
 * Bits 4:0  ISM[4:0]  Input source select
 * ========================================================================= */
#define CWG3ISM SFR8(0x03CF)

#define CWG3ISM_ISM_MASK 0x1Fu /**< ISM[4:0] bit mask               */
#define CWG3ISM_ISM_SHIFT 0u   /**< ISM[4:0] bit position           */

/* ============================================================================
 * CWG3DBR - CWG3 Dead-Band Rising Edge Count Register
 * Address: 0x03CF
 *
 * Bits 5:0  DBR[5:0]  Rising dead-band count (0-63 clocks)
 * ========================================================================= */
#define CWG3DBR SFR8(0x03D0)

#define CWG3DBR_DBR_MASK 0x3Fu /**< DBR[5:0] bit mask               */
#define CWG3DBR_DBR_SHIFT 0u   /**< DBR[5:0] bit position           */

/* ============================================================================
 * CWG3DBF - CWG3 Dead-Band Falling Edge Count Register
 * Address: 0x03D0
 *
 * Bits 5:0  DBF[5:0]  Falling dead-band count (0-63 clocks)
 * ========================================================================= */
#define CWG3DBF SFR8(0x03D1)

#define CWG3DBF_DBF_MASK 0x3Fu /**< DBF[5:0] bit mask               */
#define CWG3DBF_DBF_SHIFT 0u   /**< DBF[5:0] bit position           */

/* ============================================================================
 * CWG3CON0 - CWG3 Control Register 0
 * Address: 0x03D1
 *
 * Bit 7      EN          CWG enable
 * Bit 6      LD          Dead-band load (auto-clearing)
 * Bits 2:0   MODE[2:0]   Output mode select (see CWG1CON0)
 * ========================================================================= */
#define CWG3CON0 SFR8(0x03D2)

#define CWG3CON0_EN (1u << 7) /**< CWG enable                      */
#define CWG3CON0_LD (1u << 6) /**< Dead-band load (auto-clearing)  */

#define CWG3CON0_MODE_MASK 0x07u /**< MODE[2:0] bit mask              */
#define CWG3CON0_MODE_SHIFT 0u   /**< MODE[2:0] bit position          */

/** CWG3CON0 MODE[2:0] output mode select values */
#define CWG3CON0_MODE_HALF_BRIDGE 0x00u       /**< Half-bridge (A/B compl.)  */
#define CWG3CON0_MODE_FULL_BRIDGE_FWD 0x01u   /**< Full-bridge forward       */
#define CWG3CON0_MODE_FULL_BRIDGE_REV 0x02u   /**< Full-bridge reverse       */
#define CWG3CON0_MODE_FULL_BRIDGE_BIDIR 0x03u /**< Full-bridge bidirectional */
#define CWG3CON0_MODE_SINGLE_A 0x04u          /**< Single output A           */

/* ============================================================================
 * CWG3CON1 - CWG3 Control Register 1
 * Address: 0x03D2
 *
 * Bits 7:6  LSBD[1:0]  Shutdown state for outputs B/D
 * Bits 5:4  LSAC[1:0]  Shutdown state for outputs A/C
 * Bit  3    REN         Auto-restart enable
 * Bit  0    IN          Input value (read-only)
 * ========================================================================= */
#define CWG3CON1 SFR8(0x03D3)

#define CWG3CON1_LSBD_MASK (0x03u << 6) /**< LSBD[1:0] bit mask            */
#define CWG3CON1_LSBD_SHIFT 6u          /**< LSBD[1:0] bit position        */
#define CWG3CON1_LSAC_MASK (0x03u << 4) /**< LSAC[1:0] bit mask            */
#define CWG3CON1_LSAC_SHIFT 4u          /**< LSAC[1:0] bit position        */
#define CWG3CON1_REN (1u << 3)          /**< Auto-restart enable            */
#define CWG3CON1_IN (1u << 0)           /**< Input value (read-only)        */

/* ============================================================================
 * CWG3AS0 - CWG3 Auto-Shutdown Source Register 0
 * Address: 0x03D3
 *
 * Bit 7    SHUTDOWN  Shutdown status (read-only)
 * Bit 6    REN       Auto-restart enable
 * Bits 5:0 AS5E-AS0E  Shutdown source enables
 * ========================================================================= */
#define CWG3AS0 SFR8(0x03D4)

#define CWG3AS0_SHUTDOWN (1u << 7) /**< Shutdown status (read-only)     */
#define CWG3AS0_REN (1u << 6)      /**< Auto-restart enable             */
#define CWG3AS0_AS5E (1u << 5)     /**< Shutdown source 5 enable        */
#define CWG3AS0_AS4E (1u << 4)     /**< Shutdown source 4 enable        */
#define CWG3AS0_AS3E (1u << 3)     /**< Shutdown source 3 enable        */
#define CWG3AS0_AS2E (1u << 2)     /**< Shutdown source 2 enable        */
#define CWG3AS0_AS1E (1u << 1)     /**< Shutdown source 1 enable        */
#define CWG3AS0_AS0E (1u << 0)     /**< Shutdown source 0 enable        */

/* ============================================================================
 * CWG3AS1 - CWG3 Auto-Shutdown Source Register 1
 * Address: 0x03D4
 *
 * Bit 7  AS7E  Shutdown source 7 enable
 * Bit 6  AS6E  Shutdown source 6 enable
 * ========================================================================= */
#define CWG3AS1 SFR8(0x03D5)

#define CWG3AS1_AS7E (1u << 7) /**< Shutdown source 7 enable        */
#define CWG3AS1_AS6E (1u << 6) /**< Shutdown source 6 enable        */

/* ============================================================================
 * CWG3STR - CWG3 Output Steering Register
 * Address: 0x03D5
 *
 * Bit 7  OVRD   Bit 3  STRD   (output D override / steer value)
 * Bit 6  OVRC   Bit 2  STRC   (output C override / steer value)
 * Bit 5  OVRB   Bit 1  STRB   (output B override / steer value)
 * Bit 4  OVRA   Bit 0  STRA   (output A override / steer value)
 * ========================================================================= */
#define CWG3STR SFR8(0x03D6)

#define CWG3STR_OVRD (1u << 7) /**< Output D override enable        */
#define CWG3STR_OVRC (1u << 6) /**< Output C override enable        */
#define CWG3STR_OVRB (1u << 5) /**< Output B override enable        */
#define CWG3STR_OVRA (1u << 4) /**< Output A override enable        */
#define CWG3STR_STRD (1u << 3) /**< Output D steer value            */
#define CWG3STR_STRC (1u << 2) /**< Output C steer value            */
#define CWG3STR_STRB (1u << 1) /**< Output B steer value            */
#define CWG3STR_STRA (1u << 0) /**< Output A steer value            */

#endif /* PIC18F_Q84_CWG_H */
