/**
 * @file pic18f_q84_cmp.h
 * @brief CMP (Comparator) register definitions for PIC18F27/47/57Q84
 *        microcontrollers.
 *
 * The device provides two independent analog comparator modules (CM1, CM2).
 * Each comparator compares the voltage on a selectable positive input
 * against a selectable negative input and produces a digital output
 * reflecting which input has the higher voltage.
 *
 * Key features (per comparator instance):
 *   - Four external positive input channels and four external negative
 *     input channels (pin mapping is package-dependent)
 *   - Internal connections to DAC1 output, FVR, AGND, and VSS as
 *     additional input sources
 *   - Configurable output polarity (inverted or non-inverted)
 *   - Optional hysteresis to reduce output chatter near the trip point
 *   - Optional output synchronization to a timer clock edge
 *   - Independent interrupt-on-rising and interrupt-on-falling output
 *     edge enables for precise event detection
 *   - Output readable in software (CMxCON0.OUT) and optionally routed
 *     to an I/O pin via the PPS output register
 *   - Internal connection to CLC, timer gate, and auto-conversion trigger
 *
 * The combined comparator output register (CMOUT) provides a single-read
 * snapshot of both comparator outputs for simultaneous polling.
 *
 * Typical usage:
 *   1. Select positive input: CMxPCH.PCH[2:0]
 *   2. Select negative input: CMxNCH.NCH[2:0]
 *   3. Configure polarity, hysteresis, sync: CMxCON0, CMxCON1
 *   4. Enable the comparator: CMxCON0.EN = 1
 *   5. Allow settling time (~10 us typical)
 *   6. Read output via CMxCON0.OUT or CMOUT
 *   7. Optionally route output to pin via PPS
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 31: Comparator Module
 *
 * @note SFR8(addr) macro must be defined before including this header
 *       (typically via pic18f_q84.h).
 * @note All register addresses are absolute.
 * @note Reserved bits should be written as 0; their read values are
 *       undefined.
 */

#ifndef PIC18F_Q84_CMP_H
#define PIC18F_Q84_CMP_H

/* ============================================================================
 * CMOUT - Combined Comparator Output Register (0x006F, read-only)
 *
 * Provides a single-byte snapshot of both comparator outputs, allowing
 * software to read both states in one bus cycle.  Each bit reflects the
 * comparator output after polarity inversion (CMxCON0.POL) is applied.
 *
 * Bits 7:2  -- Reserved (read as 0)
 * Bit 1    C2OUT   Comparator 2 Output (read-only)
 *                    1 = CM2 positive input > negative input (after POL)
 *                    0 = CM2 positive input < negative input (after POL)
 * Bit 0    C1OUT   Comparator 1 Output (read-only)
 *                    1 = CM1 positive input > negative input (after POL)
 *                    0 = CM1 positive input < negative input (after POL)
 * ========================================================================= */
#define CMOUT SFR8(0x006F)

#define CMOUT_C2OUT (1u << 1) /**< Comparator 2 output (read-only) */
#define CMOUT_C1OUT (1u << 0) /**< Comparator 1 output (read-only) */

/* ============================================================================
 * CM1 - Comparator Instance 1
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * CM1CON0 - Comparator 1 Control Register 0 (0x0070)
 *
 * Primary control register for comparator 1: enable, output, polarity.
 *
 * Bit 7    EN      Comparator Enable
 *                    1 = Comparator is powered on and active
 *                    0 = Comparator is disabled (output forced low, low power)
 * Bit 6    OUT     Comparator Output (read-only)
 *                    Reflects the comparator output after polarity inversion.
 *                    1 = Positive input voltage > negative input voltage
 *                        (non-inverted) or < (inverted, POL=1)
 *                    0 = Opposite condition
 * Bit 5    POL     Output Polarity
 *                    1 = Output is inverted (active-low)
 *                    0 = Output is non-inverted (active-high, default)
 * Bits 4:0 --      Reserved
 * ------------------------------------------------------------------------- */
#define CM1CON0 SFR8(0x0070)

#define CM1CON0_EN (1u << 7)  /**< Comparator 1 enable             */
#define CM1CON0_OUT (1u << 6) /**< Comparator 1 output (read-only) */
#define CM1CON0_POL (1u << 5) /**< Output polarity (1=inverted)    */

/* ---------------------------------------------------------------------------
 * CM1CON1 - Comparator 1 Control Register 1 (0x0071)
 *
 * Secondary control register for comparator 1: hysteresis, sync, and
 * edge interrupt enables.
 *
 * Bits 7:4 --      Reserved
 * Bit 3    HYS     Hysteresis Enable
 *                    1 = Hysteresis is added to the comparator input.
 *                        Reduces output oscillation (chatter) when the
 *                        two input voltages are nearly equal.  The
 *                        hysteresis voltage is device-specific (~20 mV).
 *                    0 = No hysteresis (maximum sensitivity)
 * Bit 2    SYNC    Output Synchronization Enable
 *                    1 = Comparator output is synchronized to a timer
 *                        clock edge before being presented on the OUT bit
 *                        and interrupt logic.  Prevents metastable output
 *                        from propagating to downstream logic.
 *                    0 = Asynchronous output (no synchronization delay)
 * Bit 1    INTP    Interrupt on Positive (Rising) Edge
 *                    1 = Generate interrupt when comparator output
 *                        transitions from 0 to 1 (rising edge)
 *                    0 = No interrupt on rising edge
 * Bit 0    INTN    Interrupt on Negative (Falling) Edge
 *                    1 = Generate interrupt when comparator output
 *                        transitions from 1 to 0 (falling edge)
 *                    0 = No interrupt on falling edge
 *
 * @note Both INTP and INTN can be set simultaneously to interrupt on
 *       any output transition.
 * ------------------------------------------------------------------------- */
#define CM1CON1 SFR8(0x0071)

#define CM1CON1_HYS (1u << 3)  /**< Hysteresis enable               */
#define CM1CON1_SYNC (1u << 2) /**< Output synchronization enable   */
#define CM1CON1_INTP (1u << 1) /**< Interrupt on rising edge        */
#define CM1CON1_INTN (1u << 0) /**< Interrupt on falling edge       */

/* ---------------------------------------------------------------------------
 * CM1NCH - Comparator 1 Negative Channel Select Register (0x0072)
 *
 * Selects the analog input connected to the comparator negative
 * (inverting) input.
 *
 * Bits 7:3 --      Reserved
 * Bits 2:0 NCH[2:0]  Negative Input Channel Select
 *                       000 = C1IN0- (external pin 0)
 *                       001 = C1IN1- (external pin 1)
 *                       010 = C1IN2- (external pin 2)
 *                       011 = C1IN3- (external pin 3)
 *                       100 = AGND   (analog ground)
 *                       101 = DAC1   (DAC1 output)
 *                       110 = FVR    (Fixed Voltage Reference Buffer 1)
 *                       111 = VSS    (digital ground)
 * ------------------------------------------------------------------------- */
#define CM1NCH SFR8(0x0072)

#define CM1NCH_NCH_MASK 0x07u /**< NCH[2:0] bit mask              */
#define CM1NCH_NCH_SHIFT 0u   /**< NCH[2:0] bit position          */

/** Negative channel codes for CM1NCH / CM2NCH */
#define CMP_NCH_IN0 0x00u  /**< CxIN0- external pin            */
#define CMP_NCH_IN1 0x01u  /**< CxIN1- external pin            */
#define CMP_NCH_IN2 0x02u  /**< CxIN2- external pin            */
#define CMP_NCH_IN3 0x03u  /**< CxIN3- external pin            */
#define CMP_NCH_AGND 0x04u /**< Analog ground                  */
#define CMP_NCH_DAC1 0x05u /**< DAC1 output                    */
#define CMP_NCH_FVR 0x06u  /**< FVR Buffer 1                   */
#define CMP_NCH_VSS 0x07u  /**< VSS (digital ground)           */

/* ---------------------------------------------------------------------------
 * CM1PCH - Comparator 1 Positive Channel Select Register (0x0073)
 *
 * Selects the analog input connected to the comparator positive
 * (non-inverting) input.
 *
 * Bits 7:3 --      Reserved
 * Bits 2:0 PCH[2:0]  Positive Input Channel Select
 *                       000 = C1IN0+ (external pin 0)
 *                       001 = C1IN1+ (external pin 1)
 *                       010 = C1IN2+ (external pin 2)
 *                       011 = C1IN3+ (external pin 3)
 *                       100 = Reserved
 *                       101 = DAC1   (DAC1 output)
 *                       110 = FVR    (Fixed Voltage Reference Buffer 1)
 *                       111 = AGND   (analog ground)
 * ------------------------------------------------------------------------- */
#define CM1PCH SFR8(0x0073)

#define CM1PCH_PCH_MASK 0x07u /**< PCH[2:0] bit mask              */
#define CM1PCH_PCH_SHIFT 0u   /**< PCH[2:0] bit position          */

/** Positive channel codes for CM1PCH / CM2PCH */
#define CMP_PCH_IN0 0x00u  /**< CxIN0+ external pin            */
#define CMP_PCH_IN1 0x01u  /**< CxIN1+ external pin            */
#define CMP_PCH_IN2 0x02u  /**< CxIN2+ external pin            */
#define CMP_PCH_IN3 0x03u  /**< CxIN3+ external pin            */
#define CMP_PCH_DAC1 0x05u /**< DAC1 output                    */
#define CMP_PCH_FVR 0x06u  /**< FVR Buffer 1                   */
#define CMP_PCH_AGND 0x07u /**< Analog ground                  */

/* ============================================================================
 * CM2 - Comparator Instance 2
 *
 * Register layout and bit definitions are identical to CM1.
 * See CM1 register descriptions above for detailed bit-field
 * documentation.
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * CM2CON0 - Comparator 2 Control Register 0 (0x0074)
 * Bit layout identical to CM1CON0.
 * ------------------------------------------------------------------------- */
#define CM2CON0 SFR8(0x0074)

#define CM2CON0_EN (1u << 7)  /**< Comparator 2 enable             */
#define CM2CON0_OUT (1u << 6) /**< Comparator 2 output (read-only) */
#define CM2CON0_POL (1u << 5) /**< Output polarity (1=inverted)    */

/* ---------------------------------------------------------------------------
 * CM2CON1 - Comparator 2 Control Register 1 (0x0075)
 * Bit layout identical to CM1CON1.
 * ------------------------------------------------------------------------- */
#define CM2CON1 SFR8(0x0075)

#define CM2CON1_HYS (1u << 3)  /**< Hysteresis enable               */
#define CM2CON1_SYNC (1u << 2) /**< Output synchronization enable   */
#define CM2CON1_INTP (1u << 1) /**< Interrupt on rising edge        */
#define CM2CON1_INTN (1u << 0) /**< Interrupt on falling edge       */

/* ---------------------------------------------------------------------------
 * CM2NCH - Comparator 2 Negative Channel Select Register (0x0076)
 * Bit layout identical to CM1NCH.  Uses same CMP_NCH_xxx channel codes.
 * ------------------------------------------------------------------------- */
#define CM2NCH SFR8(0x0076)

#define CM2NCH_NCH_MASK 0x07u /**< NCH[2:0] bit mask              */
#define CM2NCH_NCH_SHIFT 0u   /**< NCH[2:0] bit position          */

/* ---------------------------------------------------------------------------
 * CM2PCH - Comparator 2 Positive Channel Select Register (0x0077)
 * Bit layout identical to CM1PCH.  Uses same CMP_PCH_xxx channel codes.
 * ------------------------------------------------------------------------- */
#define CM2PCH SFR8(0x0077)

#define CM2PCH_PCH_MASK 0x07u /**< PCH[2:0] bit mask              */
#define CM2PCH_PCH_SHIFT 0u   /**< PCH[2:0] bit position          */

#endif /* PIC18F_Q84_CMP_H */
