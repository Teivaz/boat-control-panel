/*
 * pic18f_q84_pwm.h
 *
 * PWM (16-bit Pulse-Width Modulator with Compare) register definitions
 * for PIC18F27/47/57Q84 microcontrollers.
 *
 * The device provides four independent PWM modules (PWM1-PWM4), each
 * containing:
 *
 *   - An integrated 16-bit timer/counter with 8-bit prescaler
 *   - A 16-bit period register (PR) defining the PWM cycle length
 *   - One output slice (Slice 1) with two independent 16-bit compare
 *     parameters (P1 and P2), each driving a separate output pin
 *   - Double-buffered period and duty-cycle registers; new values are
 *     transferred to the active registers on a software-selected load
 *     trigger event (auto-load source)
 *   - Multiple output alignment modes:
 *       Left-aligned   : standard edge-aligned PWM
 *       Right-aligned  : output transitions aligned to period end
 *       Center-aligned : symmetric PWM (up/down counting)
 *       Variable align : programmable left/right alignment
 *       Dual edge      : independent rising and falling edge control
 *   - Configurable external reset source for the period counter
 *   - Push-pull mode for alternating outputs on successive periods
 *   - Period interrupt with programmable postscaler offset
 *
 * Two global registers (PWMLOAD and PWMEN) provide simultaneous
 * control of the load-armed and enable states across all four modules,
 * allowing synchronised multi-channel PWM operation.
 *
 * Register base addresses:
 *   PWM1: 0x0460    PWM2: 0x046F    PWM3: 0x047E    PWM4: 0x048D
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_PWM_H
#define PIC18F_Q84_PWM_H

/* =====================================================================
 *  PWM1 - 16-bit PWM Module 1
 *  Base address: 0x0460
 * ================================================================== */

/* -------------------------------------------------------------------
 * PWM1ERS - PWM1 External Reset Source Select
 * Address: 0x0460
 *
 * Selects the signal source that resets the PWM1 period counter.
 * When the selected event occurs and the external reset is active
 * (respecting ERSPOL), the 16-bit period counter is cleared to zero,
 * restarting the PWM cycle.  This allows external synchronisation of
 * the PWM period to other peripherals or pin events.
 * ---------------------------------------------------------------- */
#define PWM1ERS SFR8(0x0460)

/* Bits 7:5 - Reserved: read as 0, write as 0.                      */

/* Bits 4:0 - ERS[4:0]: External Reset Source Select
 *   Selects which signal can reset the PWM period counter.
 *   Refer to DS40002213D "PWM External Reset Sources" table for
 *   the complete mapping of source codes to signal names.           */
#define PWM1ERS_ERS4 4
#define PWM1ERS_ERS3 3
#define PWM1ERS_ERS2 2
#define PWM1ERS_ERS1 1
#define PWM1ERS_ERS0 0

#define PWM1ERS_ERS_MASK 0x1F /* bits 4:0 */
#define PWM1ERS_ERS_POS 0

/* -------------------------------------------------------------------
 * PWM1CLK - PWM1 Clock Source Select
 * Address: 0x0461
 *
 * Selects the input clock source for the PWM1 period counter.  The
 * selected clock is further divided by the prescaler (PWM1CPRE)
 * before clocking the 16-bit counter.
 * ---------------------------------------------------------------- */
#define PWM1CLK SFR8(0x0461)

/* Bits 7:5 - Reserved: read as 0, write as 0.                      */

/* Bits 4:0 - CLK[4:0]: Clock Source Select
 *   Selects the clock signal for the PWM period counter.
 *   Refer to DS40002213D "PWM Clock Sources" table for the
 *   complete mapping of source codes to signal names.               */
#define PWM1CLK_CLK4 4
#define PWM1CLK_CLK3 3
#define PWM1CLK_CLK2 2
#define PWM1CLK_CLK1 1
#define PWM1CLK_CLK0 0

#define PWM1CLK_CLK_MASK 0x1F /* bits 4:0 */
#define PWM1CLK_CLK_POS 0

/* -------------------------------------------------------------------
 * PWM1LDS - PWM1 Auto-Load Trigger Source Select
 * Address: 0x0462
 *
 * Selects the event that triggers the transfer of double-buffered
 * values (period, duty cycle) from the buffer registers into the
 * active registers.  The load occurs only when the LD (load-armed)
 * bit in PWMnCON is set; after the transfer, LD is automatically
 * cleared.
 * ---------------------------------------------------------------- */
#define PWM1LDS SFR8(0x0462)

/* Bits 7:5 - Reserved: read as 0, write as 0.                      */

/* Bits 4:0 - LDS[4:0]: Auto-Load Trigger Source Select
 *   Selects the event that causes buffered values to be loaded.
 *   Refer to DS40002213D "PWM Auto-Load Sources" table for the
 *   complete mapping of source codes to signal names.               */
#define PWM1LDS_LDS4 4
#define PWM1LDS_LDS3 3
#define PWM1LDS_LDS2 2
#define PWM1LDS_LDS1 1
#define PWM1LDS_LDS0 0

#define PWM1LDS_LDS_MASK 0x1F /* bits 4:0 */
#define PWM1LDS_LDS_POS 0

/* -------------------------------------------------------------------
 * PWM1PR - PWM1 Period Register (16-bit)
 * Address: 0x0463 (low byte), 0x0464 (high byte)
 *
 * Sets the PWM period.  The 16-bit counter counts from 0 up to the
 * value in PWM1PR, then resets to 0 on the next clock tick (period =
 * (PR + 1) * prescaled-clock-period in left/right-aligned modes; in
 * center-aligned mode the counter counts up to PR then back down,
 * giving period = 2 * PR * prescaled-clock-period).
 *
 * This register is double-buffered; writes go to the buffer and are
 * transferred to the active register on the next auto-load event.
 * ---------------------------------------------------------------- */
#define PWM1PR SFR16(0x0463)

/* -------------------------------------------------------------------
 * PWM1CPRE - PWM1 Clock Prescaler
 * Address: 0x0465
 *
 * Divides the selected input clock before it reaches the 16-bit
 * period counter.  The division ratio is (CPRE + 1), giving a range
 * of 1:1 (CPRE = 0x00) through 1:256 (CPRE = 0xFF).
 * ---------------------------------------------------------------- */
#define PWM1CPRE SFR8(0x0465)

/* Bits 7:0 - CPRE[7:0]: Prescaler Divider Value
 *   Clock division ratio = CPRE + 1.
 *   0x00 = 1:1 (no division)
 *   0x01 = 1:2
 *   ...
 *   0xFF = 1:256                                                    */
#define PWM1CPRE_CPRE_MASK 0xFF /* bits 7:0 */
#define PWM1CPRE_CPRE_POS 0

/* -------------------------------------------------------------------
 * PWM1PIPOS - PWM1 Period Interrupt Postscaler Offset
 * Address: 0x0466
 *
 * Specifies how many complete PWM periods elapse before the first
 * period interrupt is generated.  After the first interrupt fires,
 * subsequent period interrupts are generated at the postscaler rate
 * (set elsewhere).  This allows offsetting the interrupt timing
 * relative to the module enable, which is useful for staggering
 * interrupt servicing across multiple PWM channels.
 * ---------------------------------------------------------------- */
#define PWM1PIPOS SFR8(0x0466)

/* Bits 7:0 - PIPOS[7:0]: Postscaler Offset Count
 *   Number of PWM periods before the first period interrupt.
 *   0x00 = Interrupt on the first period completion.
 *   0x01 = Skip one period, interrupt on the second.
 *   ...
 *   0xFF = Skip 255 periods, interrupt on the 256th.                */
#define PWM1PIPOS_PIPOS_MASK 0xFF /* bits 7:0 */
#define PWM1PIPOS_PIPOS_POS 0

/* -------------------------------------------------------------------
 * PWM1GIR - PWM1 General Interrupt Request
 * Address: 0x0467
 *
 * Contains interrupt flags for Slice 1 parameter match events.
 * Each flag is set by hardware when the period counter matches the
 * corresponding parameter value.  Flags must be cleared in software
 * by writing 0 to the bit.
 * ---------------------------------------------------------------- */
#define PWM1GIR SFR8(0x0467)

/* Bits 7:2 - Reserved: read as 0, write as 0.                      */

/* Bit 1 - S1P2: Slice 1 Parameter 2 Interrupt Flag
 *   1 = Period counter matched PWM1S1P2 (P2 compare event occurred).
 *   0 = No P2 match event since last clear.
 *   Must be cleared in software.                                    */
#define PWM1GIR_S1P2 1

/* Bit 0 - S1P1: Slice 1 Parameter 1 Interrupt Flag
 *   1 = Period counter matched PWM1S1P1 (P1 compare event occurred).
 *   0 = No P1 match event since last clear.
 *   Must be cleared in software.                                    */
#define PWM1GIR_S1P1 0

/* -------------------------------------------------------------------
 * PWM1GIE - PWM1 General Interrupt Enable
 * Address: 0x0468
 *
 * Enables or disables the interrupt generation for Slice 1 parameter
 * match events.  When an enable bit is set and the corresponding
 * flag in PWM1GIR is set, an interrupt request is generated.
 * ---------------------------------------------------------------- */
#define PWM1GIE SFR8(0x0468)

/* Bits 7:2 - Reserved: read as 0, write as 0.                      */

/* Bit 1 - S1P2: Slice 1 Parameter 2 Interrupt Enable
 *   1 = Interrupt enabled on P2 match.
 *   0 = P2 match interrupt disabled.                                */
#define PWM1GIE_S1P2 1

/* Bit 0 - S1P1: Slice 1 Parameter 1 Interrupt Enable
 *   1 = Interrupt enabled on P1 match.
 *   0 = P1 match interrupt disabled.                                */
#define PWM1GIE_S1P1 0

/* -------------------------------------------------------------------
 * PWM1CON - PWM1 Control Register
 * Address: 0x0469
 *
 * Master control register for PWM1.  Enables the module, arms the
 * double-buffer load mechanism, selects push-pull operation, controls
 * external reset behaviour, and provides immediate reset capability.
 * ---------------------------------------------------------------- */
#define PWM1CON SFR8(0x0469)

/* Bit 7 - EN: PWM Module Enable
 *   1 = PWM1 is enabled; the period counter runs and outputs are
 *       driven according to the slice configuration.
 *   0 = PWM1 is disabled; counter is held in reset and outputs
 *       are inactive.                                               */
#define PWM1CON_EN 7

/* Bit 6 - LD: Load Armed (read-only status)
 *   1 = New buffered values are pending; they will be transferred
 *       to the active registers on the next auto-load trigger event.
 *   0 = No pending load; active registers hold current values.
 *   This bit is set by software writing new values and cleared
 *   automatically after the load transfer completes.                */
#define PWM1CON_LD 6

/* Bit 5 - Reserved: read as 0, write as 0.                         */

/* Bit 4 - PPEN: Push-Pull Enable
 *   1 = Push-pull mode enabled.  The two slice outputs (P1 and P2)
 *       alternate activity on successive PWM periods, producing a
 *       push-pull drive suitable for transformer-coupled converters.
 *   0 = Normal (non-push-pull) operation; both outputs are active
 *       on every period.                                            */
#define PWM1CON_PPEN 4

/* Bit 3 - Reserved: read as 0, write as 0.                         */

/* Bit 2 - ERSNOW: External Reset Now (write-only trigger)
 *   Writing 1 to this bit forces an immediate reset of the period
 *   counter to zero, regardless of the external reset source
 *   selection.  The bit auto-clears after the reset; reads return 0.
 *   Useful for software-initiated PWM resynchronisation.            */
#define PWM1CON_ERSNOW 2

/* Bit 1 - ERSPOL: External Reset Polarity
 *   1 = External reset is active on the rising (high) edge/level
 *       of the selected reset source.
 *   0 = External reset is active on the falling (low) edge/level
 *       of the selected reset source.                               */
#define PWM1CON_ERSPOL 1

/* Bit 0 - Reserved: read as 0, write as 0.                         */

/* -------------------------------------------------------------------
 * PWM1S1CFG - PWM1 Slice 1 Configuration
 * Address: 0x046A
 *
 * Configures the output polarity and alignment mode for Slice 1.
 * The alignment mode determines how the duty-cycle parameters (P1
 * and P2) relate to the period counter and where output transitions
 * occur within the PWM cycle.
 * ---------------------------------------------------------------- */
#define PWM1S1CFG SFR8(0x046A)

/* Bit 7 - POL2: Parameter 2 Output Polarity
 *   1 = P2 output is active-high (output high during duty portion).
 *   0 = P2 output is active-low (output low during duty portion).   */
#define PWM1S1CFG_POL2 7

/* Bit 6 - POL1: Parameter 1 Output Polarity
 *   1 = P1 output is active-high.
 *   0 = P1 output is active-low.                                   */
#define PWM1S1CFG_POL1 6

/* Bits 5:3 - Reserved: read as 0, write as 0.                      */

/* Bits 2:0 - MODE[2:0]: Output Alignment Mode
 *   000 = Left-aligned   : Standard edge-aligned PWM.  Output
 *           transitions at counter=0 (set) and counter=Px (clear).
 *   001 = Right-aligned  : Output transitions aligned to the end
 *           of the period (counter=PR).
 *   010 = Center-aligned : Symmetric PWM.  Counter counts up to PR
 *           then back down; output toggles on both up and down
 *           matches, producing centred pulses with reduced harmonics.
 *   011 = Variable left/right align : Programmable alignment mixing
 *           left and right edge placement.
 *   100 = Dual edge PWM  : Independent control of both rising and
 *           falling output edges using P1 and P2 respectively.
 *   101-111 = Reserved.                                             */
#define PWM1S1CFG_MODE2 2
#define PWM1S1CFG_MODE1 1
#define PWM1S1CFG_MODE0 0

#define PWM1S1CFG_MODE_MASK 0x07 /* bits 2:0 */
#define PWM1S1CFG_MODE_POS 0

/* Named mode values for PWMnS1CFG MODE[2:0]                        */
#define PWM_MODE_LEFT 0x00      /* Left-aligned (standard PWM)     */
#define PWM_MODE_RIGHT 0x01     /* Right-aligned                   */
#define PWM_MODE_CENTER 0x02    /* Center-aligned (symmetric)      */
#define PWM_MODE_VARIABLE 0x03  /* Variable left/right align       */
#define PWM_MODE_DUAL_EDGE 0x04 /* Dual edge PWM                   */

/* -------------------------------------------------------------------
 * PWM1S1P1 - PWM1 Slice 1 Parameter 1 (16-bit)
 * Address: 0x046B (low byte), 0x046C (high byte)
 *
 * Sets the duty cycle or compare value for output 1 of Slice 1.
 * The exact behaviour depends on the alignment mode:
 *   Left-aligned  : Output 1 is set at counter=0, cleared at P1.
 *   Right-aligned : Output 1 is cleared at counter=0, set at P1.
 *   Center-aligned: Output toggles when counter matches P1 in both
 *                   the up-counting and down-counting directions.
 *   Dual edge     : P1 defines the rising edge position.
 *
 * This register is double-buffered; writes go to the buffer and
 * transfer to the active register on the next auto-load event.
 * ---------------------------------------------------------------- */
#define PWM1S1P1 SFR16(0x046B)

/* -------------------------------------------------------------------
 * PWM1S1P2 - PWM1 Slice 1 Parameter 2 (16-bit)
 * Address: 0x046D (low byte), 0x046E (high byte)
 *
 * Sets the duty cycle or compare value for output 2 of Slice 1.
 * Operates identically to PWM1S1P1 but controls the second output.
 * In dual-edge mode, P2 defines the falling edge position.
 *
 * This register is double-buffered.
 * ---------------------------------------------------------------- */
#define PWM1S1P2 SFR16(0x046D)

/* =====================================================================
 *  PWM2 - 16-bit PWM Module 2
 *  Base address: 0x046F
 *
 *  Functionally identical to PWM1.  Bit definitions within each
 *  register are the same as the corresponding PWM1 register.
 * ================================================================== */

#define PWM2ERS SFR8(0x046F)
#define PWM2CLK SFR8(0x0470)
#define PWM2LDS SFR8(0x0471)
#define PWM2PR SFR16(0x0472)
#define PWM2CPRE SFR8(0x0474)
#define PWM2PIPOS SFR8(0x0475)
#define PWM2GIR SFR8(0x0476)
#define PWM2GIE SFR8(0x0477)
#define PWM2CON SFR8(0x0478)
#define PWM2S1CFG SFR8(0x0479)
#define PWM2S1P1 SFR16(0x047A)
#define PWM2S1P2 SFR16(0x047C)

/* Bit definitions for PWM2 registers - identical positions to PWM1  */

/* PWM2ERS bits                                                      */
#define PWM2ERS_ERS4 4
#define PWM2ERS_ERS3 3
#define PWM2ERS_ERS2 2
#define PWM2ERS_ERS1 1
#define PWM2ERS_ERS0 0
#define PWM2ERS_ERS_MASK 0x1F
#define PWM2ERS_ERS_POS 0

/* PWM2CLK bits                                                      */
#define PWM2CLK_CLK4 4
#define PWM2CLK_CLK3 3
#define PWM2CLK_CLK2 2
#define PWM2CLK_CLK1 1
#define PWM2CLK_CLK0 0
#define PWM2CLK_CLK_MASK 0x1F
#define PWM2CLK_CLK_POS 0

/* PWM2LDS bits                                                      */
#define PWM2LDS_LDS4 4
#define PWM2LDS_LDS3 3
#define PWM2LDS_LDS2 2
#define PWM2LDS_LDS1 1
#define PWM2LDS_LDS0 0
#define PWM2LDS_LDS_MASK 0x1F
#define PWM2LDS_LDS_POS 0

/* PWM2CPRE                                                          */
#define PWM2CPRE_CPRE_MASK 0xFF
#define PWM2CPRE_CPRE_POS 0

/* PWM2PIPOS                                                         */
#define PWM2PIPOS_PIPOS_MASK 0xFF
#define PWM2PIPOS_PIPOS_POS 0

/* PWM2GIR bits                                                      */
#define PWM2GIR_S1P2 1
#define PWM2GIR_S1P1 0

/* PWM2GIE bits                                                      */
#define PWM2GIE_S1P2 1
#define PWM2GIE_S1P1 0

/* PWM2CON bits                                                      */
#define PWM2CON_EN 7
#define PWM2CON_LD 6
#define PWM2CON_PPEN 4
#define PWM2CON_ERSNOW 2
#define PWM2CON_ERSPOL 1

/* PWM2S1CFG bits                                                    */
#define PWM2S1CFG_POL2 7
#define PWM2S1CFG_POL1 6
#define PWM2S1CFG_MODE2 2
#define PWM2S1CFG_MODE1 1
#define PWM2S1CFG_MODE0 0
#define PWM2S1CFG_MODE_MASK 0x07
#define PWM2S1CFG_MODE_POS 0

/* =====================================================================
 *  PWM3 - 16-bit PWM Module 3
 *  Base address: 0x047E
 *
 *  Functionally identical to PWM1.  Bit definitions within each
 *  register are the same as the corresponding PWM1 register.
 * ================================================================== */

#define PWM3ERS SFR8(0x047E)
#define PWM3CLK SFR8(0x047F)
#define PWM3LDS SFR8(0x0480)
#define PWM3PR SFR16(0x0481)
#define PWM3CPRE SFR8(0x0483)
#define PWM3PIPOS SFR8(0x0484)
#define PWM3GIR SFR8(0x0485)
#define PWM3GIE SFR8(0x0486)
#define PWM3CON SFR8(0x0487)
#define PWM3S1CFG SFR8(0x0488)
#define PWM3S1P1 SFR16(0x0489)
#define PWM3S1P2 SFR16(0x048B)

/* Bit definitions for PWM3 registers - identical positions to PWM1  */

/* PWM3ERS bits                                                      */
#define PWM3ERS_ERS4 4
#define PWM3ERS_ERS3 3
#define PWM3ERS_ERS2 2
#define PWM3ERS_ERS1 1
#define PWM3ERS_ERS0 0
#define PWM3ERS_ERS_MASK 0x1F
#define PWM3ERS_ERS_POS 0

/* PWM3CLK bits                                                      */
#define PWM3CLK_CLK4 4
#define PWM3CLK_CLK3 3
#define PWM3CLK_CLK2 2
#define PWM3CLK_CLK1 1
#define PWM3CLK_CLK0 0
#define PWM3CLK_CLK_MASK 0x1F
#define PWM3CLK_CLK_POS 0

/* PWM3LDS bits                                                      */
#define PWM3LDS_LDS4 4
#define PWM3LDS_LDS3 3
#define PWM3LDS_LDS2 2
#define PWM3LDS_LDS1 1
#define PWM3LDS_LDS0 0
#define PWM3LDS_LDS_MASK 0x1F
#define PWM3LDS_LDS_POS 0

/* PWM3CPRE                                                          */
#define PWM3CPRE_CPRE_MASK 0xFF
#define PWM3CPRE_CPRE_POS 0

/* PWM3PIPOS                                                         */
#define PWM3PIPOS_PIPOS_MASK 0xFF
#define PWM3PIPOS_PIPOS_POS 0

/* PWM3GIR bits                                                      */
#define PWM3GIR_S1P2 1
#define PWM3GIR_S1P1 0

/* PWM3GIE bits                                                      */
#define PWM3GIE_S1P2 1
#define PWM3GIE_S1P1 0

/* PWM3CON bits                                                      */
#define PWM3CON_EN 7
#define PWM3CON_LD 6
#define PWM3CON_PPEN 4
#define PWM3CON_ERSNOW 2
#define PWM3CON_ERSPOL 1

/* PWM3S1CFG bits                                                    */
#define PWM3S1CFG_POL2 7
#define PWM3S1CFG_POL1 6
#define PWM3S1CFG_MODE2 2
#define PWM3S1CFG_MODE1 1
#define PWM3S1CFG_MODE0 0
#define PWM3S1CFG_MODE_MASK 0x07
#define PWM3S1CFG_MODE_POS 0

/* =====================================================================
 *  PWM4 - 16-bit PWM Module 4
 *  Base address: 0x048D
 *
 *  Functionally identical to PWM1.  Bit definitions within each
 *  register are the same as the corresponding PWM1 register.
 * ================================================================== */

#define PWM4ERS SFR8(0x048D)
#define PWM4CLK SFR8(0x048E)
#define PWM4LDS SFR8(0x048F)
#define PWM4PR SFR16(0x0490)
#define PWM4CPRE SFR8(0x0492)
#define PWM4PIPOS SFR8(0x0493)
#define PWM4GIR SFR8(0x0494)
#define PWM4GIE SFR8(0x0495)
#define PWM4CON SFR8(0x0496)
#define PWM4S1CFG SFR8(0x0497)
#define PWM4S1P1 SFR16(0x0498)
#define PWM4S1P2 SFR16(0x049A)

/* Bit definitions for PWM4 registers - identical positions to PWM1  */

/* PWM4ERS bits                                                      */
#define PWM4ERS_ERS4 4
#define PWM4ERS_ERS3 3
#define PWM4ERS_ERS2 2
#define PWM4ERS_ERS1 1
#define PWM4ERS_ERS0 0
#define PWM4ERS_ERS_MASK 0x1F
#define PWM4ERS_ERS_POS 0

/* PWM4CLK bits                                                      */
#define PWM4CLK_CLK4 4
#define PWM4CLK_CLK3 3
#define PWM4CLK_CLK2 2
#define PWM4CLK_CLK1 1
#define PWM4CLK_CLK0 0
#define PWM4CLK_CLK_MASK 0x1F
#define PWM4CLK_CLK_POS 0

/* PWM4LDS bits                                                      */
#define PWM4LDS_LDS4 4
#define PWM4LDS_LDS3 3
#define PWM4LDS_LDS2 2
#define PWM4LDS_LDS1 1
#define PWM4LDS_LDS0 0
#define PWM4LDS_LDS_MASK 0x1F
#define PWM4LDS_LDS_POS 0

/* PWM4CPRE                                                          */
#define PWM4CPRE_CPRE_MASK 0xFF
#define PWM4CPRE_CPRE_POS 0

/* PWM4PIPOS                                                         */
#define PWM4PIPOS_PIPOS_MASK 0xFF
#define PWM4PIPOS_PIPOS_POS 0

/* PWM4GIR bits                                                      */
#define PWM4GIR_S1P2 1
#define PWM4GIR_S1P1 0

/* PWM4GIE bits                                                      */
#define PWM4GIE_S1P2 1
#define PWM4GIE_S1P1 0

/* PWM4CON bits                                                      */
#define PWM4CON_EN 7
#define PWM4CON_LD 6
#define PWM4CON_PPEN 4
#define PWM4CON_ERSNOW 2
#define PWM4CON_ERSPOL 1

/* PWM4S1CFG bits                                                    */
#define PWM4S1CFG_POL2 7
#define PWM4S1CFG_POL1 6
#define PWM4S1CFG_MODE2 2
#define PWM4S1CFG_MODE1 1
#define PWM4S1CFG_MODE0 0
#define PWM4S1CFG_MODE_MASK 0x07
#define PWM4S1CFG_MODE_POS 0

/* =====================================================================
 *  Global PWM Control Registers
 *
 *  These registers provide simultaneous control over the load-armed
 *  and enable states of all four PWM modules.  By writing to PWMLOAD
 *  or PWMEN, software can synchronise multiple PWM channels to start,
 *  stop, or load new values at the same time without the latency of
 *  accessing each module's control register individually.
 * ================================================================== */

/* -------------------------------------------------------------------
 * PWMLOAD - PWM Load Control Register
 * Address: 0x049C
 *
 * Each bit arms the double-buffer load for the corresponding PWM
 * module.  Setting a bit is equivalent to setting the LD bit in
 * the individual module's CON register; new buffered values will
 * transfer to the active registers on the next auto-load event.
 * ---------------------------------------------------------------- */
#define PWMLOAD SFR8(0x049C)

/* Bits 7:4 - Reserved: read as 0, write as 0.                      */

/* Bit 3 - MPWM4LD: PWM4 Load Armed
 *   1 = Arm PWM4 buffer load; buffered values will transfer on the
 *       next PWM4 auto-load trigger event.
 *   0 = PWM4 load not armed.                                       */
#define PWMLOAD_MPWM4LD 3

/* Bit 2 - MPWM3LD: PWM3 Load Armed
 *   1 = Arm PWM3 buffer load.
 *   0 = PWM3 load not armed.                                       */
#define PWMLOAD_MPWM3LD 2

/* Bit 1 - MPWM2LD: PWM2 Load Armed
 *   1 = Arm PWM2 buffer load.
 *   0 = PWM2 load not armed.                                       */
#define PWMLOAD_MPWM2LD 1

/* Bit 0 - MPWM1LD: PWM1 Load Armed
 *   1 = Arm PWM1 buffer load.
 *   0 = PWM1 load not armed.                                       */
#define PWMLOAD_MPWM1LD 0

/* -------------------------------------------------------------------
 * PWMEN - PWM Enable Register
 * Address: 0x049D
 *
 * Each bit enables or disables the corresponding PWM module.
 * Setting a bit is equivalent to setting the EN bit in the
 * individual module's CON register.  Writing multiple bits
 * simultaneously allows atomic start/stop of multiple channels.
 * ---------------------------------------------------------------- */
#define PWMEN SFR8(0x049D)

/* Bits 7:4 - Reserved: read as 0, write as 0.                      */

/* Bit 3 - MPWM4EN: PWM4 Enable
 *   1 = PWM4 enabled; period counter runs, outputs active.
 *   0 = PWM4 disabled; counter held in reset, outputs inactive.     */
#define PWMEN_MPWM4EN 3

/* Bit 2 - MPWM3EN: PWM3 Enable
 *   1 = PWM3 enabled.
 *   0 = PWM3 disabled.                                              */
#define PWMEN_MPWM3EN 2

/* Bit 1 - MPWM2EN: PWM2 Enable
 *   1 = PWM2 enabled.
 *   0 = PWM2 disabled.                                              */
#define PWMEN_MPWM2EN 1

/* Bit 0 - MPWM1EN: PWM1 Enable
 *   1 = PWM1 enabled.
 *   0 = PWM1 disabled.                                              */
#define PWMEN_MPWM1EN 0

#endif /* PIC18F_Q84_PWM_H */
