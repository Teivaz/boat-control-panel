/**
 * @file pic18f_q84_utmr.h
 * @brief UTMR (Universal Timer) register definitions for PIC18F27/47/57Q84.
 *
 * The Universal Timer module provides two independent 16-bit timer units
 * (TU16A and TU16B), each combining the capabilities of TMR0, TMR1, and
 * TMR2 into a single flexible peripheral:
 *
 *   - 16-bit timer/counter with 8-bit prescaler
 *   - Capture register (snapshot of running count on command/event)
 *   - Period register with configurable match actions (clear, limit, one-shot)
 *   - Gated operation via external reset/start/stop source (ERS)
 *   - Hardware limit (stop at period value, no overflow)
 *   - Configurable clock polarity and synchronization
 *   - Toggle or pulse output with selectable polarity
 *   - Three independent interrupt sources (period match, zero crossing,
 *     capture event)
 *
 * The two timers can be chained together via the TUCHAIN register to form
 * a 32-bit timer, where TU16A overflow clocks TU16B.
 *
 * Address-aliased registers:
 *   The TMR and CR (Capture Register) for each timer share the same
 *   physical address.  The RDSEL bit in CON1 selects which register is
 *   returned when reading the TMR/CR address pair.  Writing always goes
 *   to the TMR register regardless of RDSEL.
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 25: Universal Timer (UTMR)
 *
 * @note SFR8(addr) and SFR16(addr) macros must be defined before including
 *       this header (typically via pic18f_q84.h).
 * @note All register addresses are absolute.  Multi-byte registers are
 *       little-endian (low byte at base address).
 * @note Reserved bits should be written as 0; their read values are undefined.
 */

#ifndef PIC18F_Q84_UTMR_H
#define PIC18F_Q84_UTMR_H

/* ============================================================================
 * Timer Chain Control Register
 * ========================================================================= */

/**
 * TUCHAIN - Timer Chain Control Register (0x03BA)
 *
 * Controls whether TU16A and TU16B are chained to form a single 32-bit
 * timer.  When chained, TU16A overflow output becomes the clock source
 * for TU16B, and the combined 32-bit count is {TU16B:TU16A}.
 *
 * Bit 7:1  -- Reserved
 * Bit 0    CH16AB  Chain Enable
 *                    1 = TU16A overflow clocks TU16B (32-bit mode)
 *                    0 = TU16A and TU16B operate independently
 */
#define TUCHAIN SFR8(0x03BB)

#define TUCHAIN_CH16AB (1u << 0) /**< Chain TU16A->TU16B (32-bit)    */

/* ============================================================================
 * TU16A - Universal Timer A Registers (base 0x0386)
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * TU16ACON0 - TU16A Control Register 0 (0x0386)
 *
 * Primary control register for timer enable, clock polarity, one-shot
 * mode, clear/limit behavior, clock synchronization, and start condition.
 *
 * Bit 7    ON      Timer Enable
 *                    1 = Timer is running
 *                    0 = Timer is stopped (held in reset)
 * Bit 6    CPOL    Clock Polarity
 *                    1 = Count on falling edge of clock input
 *                    0 = Count on rising edge of clock input
 * Bit 5    OSEN    One-Shot Enable
 *                    1 = Timer stops after one period match event
 *                    0 = Timer runs continuously (free-run or auto-reload)
 * Bit 4    CLR     Clear on Period Match
 *                    1 = Timer count resets to 0 on period match
 *                    0 = Timer free-runs through 0xFFFF
 * Bit 3    LIMIT   Limit Count at Period Value
 *                    1 = Timer stops counting when TMR reaches PR value
 *                    0 = Timer continues counting past period value
 * Bit 2    CSYNC   Clock Synchronization
 *                    1 = Synchronize external clock input to FOSC
 *                    0 = Asynchronous clock input (no sync)
 * Bits 1:0 START[1:0]  Start Condition Select
 *                    00 = Start immediately when ON is set
 *                    01 = Start on next rising edge of clock input
 *                    10 = Start on hardware reset signal assertion
 *                    11 = Start on ERS (External Reset Source) signal
 * ------------------------------------------------------------------------- */
#define TU16ACON0 SFR8(0x0387)

#define TU16ACON0_ON (1u << 7)    /**< Timer enable (1=running)       */
#define TU16ACON0_CPOL (1u << 6)  /**< Clock polarity (1=falling)     */
#define TU16ACON0_OSEN (1u << 5)  /**< One-shot enable                */
#define TU16ACON0_CLR (1u << 4)   /**< Clear timer on period match    */
#define TU16ACON0_LIMIT (1u << 3) /**< Limit count at period value    */
#define TU16ACON0_CSYNC (1u << 2) /**< Sync clock input to FOSC       */

#define TU16ACON0_START_MASK 0x03u /**< START[1:0] bit mask            */
#define TU16ACON0_START_SHIFT 0u   /**< START[1:0] bit position        */

/** Start condition values for TU16ACON0 START[1:0] */
#define TU16ACON0_START_IMMED 0x00u  /**< Start immediately on ON=1      */
#define TU16ACON0_START_CKEDGE 0x01u /**< Start on rising clock edge     */
#define TU16ACON0_START_HWRST 0x02u  /**< Start on hardware reset signal */
#define TU16ACON0_START_ERS 0x03u    /**< Start on ERS signal            */

/* ---------------------------------------------------------------------------
 * TU16ACON1 - TU16A Control Register 1 (0x0387)
 *
 * Controls read-select (TMR vs CR), interrupt enables/flags, and the
 * software capture command.
 *
 * Bit 7    RDSEL   Read Select
 *                    1 = Reading TU16ATMR address returns Capture Register (CR)
 *                    0 = Reading TU16ATMR address returns Timer Register (TMR)
 * Bit 6    PRIE    Period Match Interrupt Enable
 *                    1 = Interrupt on period match event
 *                    0 = Period match interrupt disabled
 * Bit 5    PRIF    Period Match Interrupt Flag
 *                    Set by hardware on period match; clear by software.
 * Bit 4    ZIE     Zero Interrupt Enable
 *                    1 = Interrupt on timer underflow / zero crossing
 *                    0 = Zero crossing interrupt disabled
 * Bit 3    ZIF     Zero Interrupt Flag
 *                    Set by hardware on zero crossing; clear by software.
 * Bit 2    CIE     Capture Interrupt Enable
 *                    1 = Interrupt on capture event
 *                    0 = Capture interrupt disabled
 * Bit 1    CIF     Capture Interrupt Flag
 *                    Set by hardware on capture; clear by software.
 * Bit 0    CAPT    Capture Command
 *                    Write 1 to copy the current TMR value into the CR
 *                    (Capture Register).  Self-clearing.
 * ------------------------------------------------------------------------- */
#define TU16ACON1 SFR8(0x0388)

#define TU16ACON1_RDSEL (1u << 7) /**< Read select (1=CR, 0=TMR)      */
#define TU16ACON1_PRIE (1u << 6)  /**< Period match interrupt enable   */
#define TU16ACON1_PRIF (1u << 5)  /**< Period match interrupt flag     */
#define TU16ACON1_ZIE (1u << 4)   /**< Zero crossing interrupt enable  */
#define TU16ACON1_ZIF (1u << 3)   /**< Zero crossing interrupt flag    */
#define TU16ACON1_CIE (1u << 2)   /**< Capture interrupt enable        */
#define TU16ACON1_CIF (1u << 1)   /**< Capture interrupt flag          */
#define TU16ACON1_CAPT (1u << 0)  /**< Software capture command        */

/* ---------------------------------------------------------------------------
 * TU16AHLT - TU16A HLT (Hardware Limit Timer) Control (0x0388)
 *
 * Configures the ERS (External Reset Source) polarity, hardware
 * reset/stop conditions, and output mode/polarity.
 *
 * Bit 7    EPOL    ERS Polarity
 *                    1 = ERS input is active-high
 *                    0 = ERS input is active-low
 * Bit 6    --      Reserved
 * Bits 5:4 RESET[1:0]  Reset Condition
 *                    00 = Reset disabled
 *                    01 = Reset timer on ERS active edge
 *                    10 = Reset timer on period match
 *                    11 = Reset timer on ERS rising edge
 * Bits 3:2 STOP[1:0]  Stop Condition
 *                    00 = Stop disabled
 *                    01 = Stop timer on ERS active level
 *                    10 = Stop timer on period match
 *                    11 = Stop timer on ERS rising edge
 * Bit 1    OM      Output Mode
 *                    1 = Toggle output on event
 *                    0 = Pulse output on event
 * Bit 0    OPOL    Output Polarity
 *                    1 = Active-high output
 *                    0 = Active-low output
 * ------------------------------------------------------------------------- */
#define TU16AHLT SFR8(0x0389)

#define TU16AHLT_EPOL (1u << 7) /**< ERS polarity (1=active-high)   */

#define TU16AHLT_RESET_MASK 0x30u /**< RESET[1:0] bit mask            */
#define TU16AHLT_RESET_SHIFT 4u   /**< RESET[1:0] bit position        */

/** Reset condition values for TU16AHLT RESET[1:0] */
#define TU16AHLT_RESET_OFF 0x00u     /**< Reset disabled                 */
#define TU16AHLT_RESET_ERS 0x10u     /**< Reset on ERS active edge       */
#define TU16AHLT_RESET_PRMATCH 0x20u /**< Reset on period match          */
#define TU16AHLT_RESET_ERSRISE 0x30u /**< Reset on ERS rising edge       */

#define TU16AHLT_STOP_MASK 0x0Cu /**< STOP[1:0] bit mask             */
#define TU16AHLT_STOP_SHIFT 2u   /**< STOP[1:0] bit position         */

/** Stop condition values for TU16AHLT STOP[1:0] */
#define TU16AHLT_STOP_OFF 0x00u     /**< Stop disabled                  */
#define TU16AHLT_STOP_ERS 0x04u     /**< Stop on ERS active level       */
#define TU16AHLT_STOP_PRMATCH 0x08u /**< Stop on period match           */
#define TU16AHLT_STOP_ERSRISE 0x0Cu /**< Stop on ERS rising edge        */

#define TU16AHLT_OM (1u << 1)   /**< Output mode (1=toggle, 0=pulse)*/
#define TU16AHLT_OPOL (1u << 0) /**< Output polarity (1=active-high)*/

/* ---------------------------------------------------------------------------
 * TU16APS - TU16A Prescaler Register (0x0389)
 *
 * 8-bit prescaler divides the clock source before it reaches the timer
 * counter.  The division ratio is (PS + 1).
 *
 * Bits 7:0  PS[7:0]  Prescaler value
 *                     0x00 = 1:1   (no division)
 *                     0x01 = 1:2
 *                     ...
 *                     0xFF = 1:256
 * ------------------------------------------------------------------------- */
#define TU16APS SFR8(0x038A)

#define TU16APS_PS_MASK 0xFFu /**< PS[7:0] bit mask               */

/* ---------------------------------------------------------------------------
 * TU16ATMR - TU16A Timer Count Register (0x038A-0x038B, 16-bit)
 *
 * Holds the current timer count value.  Can be read or written directly.
 * When RDSEL=0 in TU16ACON1, reading this address returns the TMR value.
 * When RDSEL=1, reading this address returns the Capture Register (CR)
 * value instead (see TU16ACR below).  Writing always targets the TMR
 * register regardless of RDSEL.
 *
 * TU16ATMRH (0x038B):
 *   Bits 7:0  TMR[15:8]  High byte of timer count
 *
 * TU16ATMRL (0x038A):
 *   Bits 7:0  TMR[7:0]   Low byte of timer count
 * ------------------------------------------------------------------------- */
#define TU16ATMRL SFR8(0x038B)
#define TU16ATMRH SFR8(0x038C)
#define TU16ATMR SFR16(0x038B) /**< 16-bit timer count access   */

/* ---------------------------------------------------------------------------
 * TU16ACR - TU16A Capture Register (0x038A-0x038B, 16-bit)
 *
 * Shares the same physical address as TU16ATMR.  The RDSEL bit in
 * TU16ACON1 selects which register is read:
 *   - RDSEL=0: reads return TU16ATMR (timer count)
 *   - RDSEL=1: reads return TU16ACR  (captured snapshot)
 *
 * The capture register latches the timer value when:
 *   - Software writes 1 to the CAPT bit in TU16ACON1
 *   - A hardware capture event occurs (per ERS/HLT configuration)
 *
 * TU16ACRH (0x038B):
 *   Bits 7:0  CR[15:8]  High byte of captured value
 *
 * TU16ACRL (0x038A):
 *   Bits 7:0  CR[7:0]   Low byte of captured value
 *
 * @note These defines alias to the same addresses as TU16ATMR.  Set RDSEL
 *       before reading to select CR content.
 * ------------------------------------------------------------------------- */
#define TU16ACRL SFR8(0x038B)
#define TU16ACRH SFR8(0x038C)
#define TU16ACR SFR16(0x038B) /**< 16-bit capture register     */

/* ---------------------------------------------------------------------------
 * TU16APR - TU16A Period Register (0x038C-0x038D, 16-bit)
 *
 * Sets the period match value for the timer.  When the timer count (TMR)
 * equals the period register (PR), a period match event occurs.  The
 * resulting behavior depends on CON0 bits: CLR (auto-reset), LIMIT
 * (stop at period), and OSEN (one-shot).
 *
 * TU16APRH (0x038D):
 *   Bits 7:0  PR[15:8]  High byte of period value
 *
 * TU16APRL (0x038C):
 *   Bits 7:0  PR[7:0]   Low byte of period value
 * ------------------------------------------------------------------------- */
#define TU16APRL SFR8(0x038D)
#define TU16APRH SFR8(0x038E)
#define TU16APR SFR16(0x038D) /**< 16-bit period register      */

/* ---------------------------------------------------------------------------
 * TU16ACLK - TU16A Clock Source Select Register (0x038E)
 *
 * Selects the clock source that drives the timer counter (after the
 * prescaler).  The available sources are device-specific; consult
 * DS40002213D Table 25-1 for the full clock source mapping.
 *
 * Bits 7:5  -- Reserved
 * Bits 4:0  CLK[4:0]  Clock Source Select
 * ------------------------------------------------------------------------- */
#define TU16ACLK SFR8(0x038F)

#define TU16ACLK_CLK_MASK 0x1Fu /**< CLK[4:0] bit mask             */
#define TU16ACLK_CLK_SHIFT 0u   /**< CLK[4:0] bit position         */

/* ---------------------------------------------------------------------------
 * TU16AERS - TU16A External Reset Source Select Register (0x038F)
 *
 * Selects the external signal used as the ERS (External Reset Source)
 * for start, stop, and reset functions controlled by TU16AHLT and the
 * START bits in TU16ACON0.  Consult DS40002213D Table 25-2 for the
 * full ERS source mapping.
 *
 * Bits 7:6  -- Reserved
 * Bits 5:0  ERS[5:0]  External Reset Source Select
 * ------------------------------------------------------------------------- */
#define TU16AERS SFR8(0x0390)

#define TU16AERS_ERS_MASK 0x3Fu /**< ERS[5:0] bit mask             */
#define TU16AERS_ERS_SHIFT 0u   /**< ERS[5:0] bit position         */

/* ============================================================================
 * TU16B - Universal Timer B Registers (base 0x0392)
 *
 * TU16B is functionally identical to TU16A.  All bit definitions from
 * the TU16A registers apply equally to the corresponding TU16B registers
 * at the addresses shown below.  When chained (TUCHAIN.CH16AB = 1),
 * TU16B is clocked by TU16A overflow output, forming a 32-bit timer
 * with TU16A as the low 16 bits and TU16B as the high 16 bits.
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * TU16BCON0 - TU16B Control Register 0 (0x0392)
 *
 * Identical bit layout to TU16ACON0.  See TU16ACON0 for field descriptions.
 *
 * Bit 7    ON      Timer Enable
 * Bit 6    CPOL    Clock Polarity
 * Bit 5    OSEN    One-Shot Enable
 * Bit 4    CLR     Clear on Period Match
 * Bit 3    LIMIT   Limit Count at Period Value
 * Bit 2    CSYNC   Clock Synchronization
 * Bits 1:0 START[1:0]  Start Condition Select
 * ------------------------------------------------------------------------- */
#define TU16BCON0 SFR8(0x0393)

#define TU16BCON0_ON (1u << 7)    /**< Timer enable (1=running)       */
#define TU16BCON0_CPOL (1u << 6)  /**< Clock polarity (1=falling)     */
#define TU16BCON0_OSEN (1u << 5)  /**< One-shot enable                */
#define TU16BCON0_CLR (1u << 4)   /**< Clear timer on period match    */
#define TU16BCON0_LIMIT (1u << 3) /**< Limit count at period value    */
#define TU16BCON0_CSYNC (1u << 2) /**< Sync clock input to FOSC       */

#define TU16BCON0_START_MASK 0x03u /**< START[1:0] bit mask            */
#define TU16BCON0_START_SHIFT 0u   /**< START[1:0] bit position        */

/** Start condition values for TU16BCON0 START[1:0] */
#define TU16BCON0_START_IMMED 0x00u  /**< Start immediately on ON=1      */
#define TU16BCON0_START_CKEDGE 0x01u /**< Start on rising clock edge     */
#define TU16BCON0_START_HWRST 0x02u  /**< Start on hardware reset signal */
#define TU16BCON0_START_ERS 0x03u    /**< Start on ERS signal            */

/* ---------------------------------------------------------------------------
 * TU16BCON1 - TU16B Control Register 1 (0x0393)
 *
 * Identical bit layout to TU16ACON1.  See TU16ACON1 for field descriptions.
 *
 * Bit 7    RDSEL   Read Select (1=CR, 0=TMR)
 * Bit 6    PRIE    Period Match Interrupt Enable
 * Bit 5    PRIF    Period Match Interrupt Flag
 * Bit 4    ZIE     Zero Interrupt Enable
 * Bit 3    ZIF     Zero Interrupt Flag
 * Bit 2    CIE     Capture Interrupt Enable
 * Bit 1    CIF     Capture Interrupt Flag
 * Bit 0    CAPT    Capture Command
 * ------------------------------------------------------------------------- */
#define TU16BCON1 SFR8(0x0394)

#define TU16BCON1_RDSEL (1u << 7) /**< Read select (1=CR, 0=TMR)      */
#define TU16BCON1_PRIE (1u << 6)  /**< Period match interrupt enable   */
#define TU16BCON1_PRIF (1u << 5)  /**< Period match interrupt flag     */
#define TU16BCON1_ZIE (1u << 4)   /**< Zero crossing interrupt enable  */
#define TU16BCON1_ZIF (1u << 3)   /**< Zero crossing interrupt flag    */
#define TU16BCON1_CIE (1u << 2)   /**< Capture interrupt enable        */
#define TU16BCON1_CIF (1u << 1)   /**< Capture interrupt flag          */
#define TU16BCON1_CAPT (1u << 0)  /**< Software capture command        */

/* ---------------------------------------------------------------------------
 * TU16BHLT - TU16B HLT Control Register (0x0394)
 *
 * Identical bit layout to TU16AHLT.  See TU16AHLT for field descriptions.
 *
 * Bit 7      EPOL          ERS Polarity
 * Bit 6      --            Reserved
 * Bits 5:4   RESET[1:0]    Reset Condition
 * Bits 3:2   STOP[1:0]     Stop Condition
 * Bit 1      OM            Output Mode (1=toggle, 0=pulse)
 * Bit 0      OPOL          Output Polarity (1=active-high)
 * ------------------------------------------------------------------------- */
#define TU16BHLT SFR8(0x0395)

#define TU16BHLT_EPOL (1u << 7) /**< ERS polarity (1=active-high)   */

#define TU16BHLT_RESET_MASK 0x30u /**< RESET[1:0] bit mask            */
#define TU16BHLT_RESET_SHIFT 4u   /**< RESET[1:0] bit position        */

/** Reset condition values for TU16BHLT RESET[1:0] */
#define TU16BHLT_RESET_OFF 0x00u     /**< Reset disabled                 */
#define TU16BHLT_RESET_ERS 0x10u     /**< Reset on ERS active edge       */
#define TU16BHLT_RESET_PRMATCH 0x20u /**< Reset on period match          */
#define TU16BHLT_RESET_ERSRISE 0x30u /**< Reset on ERS rising edge       */

#define TU16BHLT_STOP_MASK 0x0Cu /**< STOP[1:0] bit mask             */
#define TU16BHLT_STOP_SHIFT 2u   /**< STOP[1:0] bit position         */

/** Stop condition values for TU16BHLT STOP[1:0] */
#define TU16BHLT_STOP_OFF 0x00u     /**< Stop disabled                  */
#define TU16BHLT_STOP_ERS 0x04u     /**< Stop on ERS active level       */
#define TU16BHLT_STOP_PRMATCH 0x08u /**< Stop on period match           */
#define TU16BHLT_STOP_ERSRISE 0x0Cu /**< Stop on ERS rising edge        */

#define TU16BHLT_OM (1u << 1)   /**< Output mode (1=toggle, 0=pulse)*/
#define TU16BHLT_OPOL (1u << 0) /**< Output polarity (1=active-high)*/

/* ---------------------------------------------------------------------------
 * TU16BPS - TU16B Prescaler Register (0x0395)
 *
 * Identical to TU16APS.  Division ratio = (PS + 1).
 *
 * Bits 7:0  PS[7:0]  Prescaler value (0x00=1:1, 0xFF=1:256)
 * ------------------------------------------------------------------------- */
#define TU16BPS SFR8(0x0396)

#define TU16BPS_PS_MASK 0xFFu /**< PS[7:0] bit mask               */

/* ---------------------------------------------------------------------------
 * TU16BTMR - TU16B Timer Count Register (0x0396-0x0397, 16-bit)
 *
 * Identical behavior to TU16ATMR.  RDSEL in TU16BCON1 selects whether
 * reads return the timer count (TMR) or capture register (CR).
 *
 * TU16BTMRH (0x0397):  Bits 7:0  TMR[15:8]
 * TU16BTMRL (0x0396):  Bits 7:0  TMR[7:0]
 * ------------------------------------------------------------------------- */
#define TU16BTMRL SFR8(0x0397)
#define TU16BTMRH SFR8(0x0398)
#define TU16BTMR SFR16(0x0397) /**< 16-bit timer count access   */

/* ---------------------------------------------------------------------------
 * TU16BCR - TU16B Capture Register (0x0396-0x0397, 16-bit)
 *
 * Shares physical address with TU16BTMR; selected by TU16BCON1.RDSEL.
 * See TU16ACR for detailed description.
 *
 * TU16BCRH (0x0397):  Bits 7:0  CR[15:8]
 * TU16BCRL (0x0396):  Bits 7:0  CR[7:0]
 *
 * @note Aliases to same addresses as TU16BTMR.
 * ------------------------------------------------------------------------- */
#define TU16BCRL SFR8(0x0397)
#define TU16BCRH SFR8(0x0398)
#define TU16BCR SFR16(0x0397) /**< 16-bit capture register     */

/* ---------------------------------------------------------------------------
 * TU16BPR - TU16B Period Register (0x0398-0x0399, 16-bit)
 *
 * Identical to TU16APR.  See TU16APR for description.
 *
 * TU16BPRH (0x0399):  Bits 7:0  PR[15:8]
 * TU16BPRL (0x0398):  Bits 7:0  PR[7:0]
 * ------------------------------------------------------------------------- */
#define TU16BPRL SFR8(0x0399)
#define TU16BPRH SFR8(0x039A)
#define TU16BPR SFR16(0x0399) /**< 16-bit period register      */

/* ---------------------------------------------------------------------------
 * TU16BCLK - TU16B Clock Source Select Register (0x039A)
 *
 * Identical to TU16ACLK.  See DS40002213D Table 25-1 for source mapping.
 *
 * Bits 7:5  -- Reserved
 * Bits 4:0  CLK[4:0]  Clock Source Select
 * ------------------------------------------------------------------------- */
#define TU16BCLK SFR8(0x039B)

#define TU16BCLK_CLK_MASK 0x1Fu /**< CLK[4:0] bit mask             */
#define TU16BCLK_CLK_SHIFT 0u   /**< CLK[4:0] bit position         */

/* ---------------------------------------------------------------------------
 * TU16BERS - TU16B External Reset Source Select Register (0x039B)
 *
 * Identical to TU16AERS.  See DS40002213D Table 25-2 for source mapping.
 *
 * Bits 7:6  -- Reserved
 * Bits 5:0  ERS[5:0]  External Reset Source Select
 * ------------------------------------------------------------------------- */
#define TU16BERS SFR8(0x039C)

#define TU16BERS_ERS_MASK 0x3Fu /**< ERS[5:0] bit mask             */
#define TU16BERS_ERS_SHIFT 0u   /**< ERS[5:0] bit position         */

#endif /* PIC18F_Q84_UTMR_H */
