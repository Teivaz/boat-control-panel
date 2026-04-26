/**
 * @file pic18f_q84_crc.h
 * @brief CRC (Cyclic Redundancy Check) and Memory Scanner register
 *        definitions for PIC18F27/47/57Q84.
 *
 * The CRC module provides hardware-accelerated CRC calculation with a
 * programmable polynomial up to 32 bits wide.  Key features:
 *
 *   - Configurable polynomial length (1 to 32 bits via PLEN)
 *   - Configurable data input width (1 to 32 bits via DLEN)
 *   - Selectable shift direction (MSb-first or LSb-first)
 *   - Accumulate mode for continuous multi-word CRC computation
 *   - 32-bit shift register, XOR polynomial register, and output register
 *
 * The integrated Memory Scanner can automatically feed sequential words
 * from Program Flash Memory (PFM) into the CRC engine, enabling CRC
 * calculation over arbitrary flash regions without CPU intervention.
 *
 * Address-aliased registers:
 *   CRCOUT, CRCSHIFT, and CRCXOR share the same address range
 *   (0x0353-0x0356).  The SETUP bits in CRCCON0 (via the EN bit and
 *   initialization sequence) determine which register is actually
 *   accessed:
 *     - Normal operation (EN=1, after init): reads return CRCOUT (result)
 *     - Setup mode: reads/writes access CRCSHIFT or CRCXOR depending
 *       on the initialization step (seed vs polynomial loading)
 *   Refer to DS40002213D Section 23 for the complete setup procedure.
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 23: CRC with Memory Scanner
 *
 * @note SFR8(addr), SFR16(addr), and SFR32(addr) macros must be defined
 *       before including this header (typically via pic18f_q84.h).
 * @note All register addresses are absolute.  Multi-byte registers are
 *       little-endian (low byte at base address).
 * @note Reserved bits should be written as 0; their read values are undefined.
 */

#ifndef PIC18F_Q84_CRC_H
#define PIC18F_Q84_CRC_H

/* ============================================================================
 * CRC Data Input Register (32-bit)
 * ========================================================================= */

/**
 * CRCDATA - CRC Data Input Register (0x034F-0x0352, 32-bit)
 *
 * Data to be processed by the CRC engine is written here.  The actual
 * number of bits consumed per write is determined by DLEN in CRCCON2
 * (data width = DLEN + 1 bits).  The CRC engine begins shifting when
 * CRCDATA is written and the module is enabled.
 *
 * CRCDATAT (0x0352):  Bits 7:0  DATA[31:24]  (top byte)
 * CRCDATAU (0x0351):  Bits 7:0  DATA[23:16]  (upper byte)
 * CRCDATAH (0x0350):  Bits 7:0  DATA[15:8]   (high byte)
 * CRCDATAL (0x034F):  Bits 7:0  DATA[7:0]    (low byte)
 */
#define CRCDATAL SFR8(0x034F)
#define CRCDATAH SFR8(0x0350)
#define CRCDATAU SFR8(0x0351)
#define CRCDATAT SFR8(0x0352)
#define CRCDATA SFR32(0x034F) /**< 32-bit data input access    */

/* ============================================================================
 * CRC Output / Shift / XOR Registers (32-bit, address-aliased)
 *
 * The following three register sets share the physical address range
 * 0x0353-0x0356.  Which register is accessed depends on the module state
 * controlled by CRCCON0:
 *
 *   - CRCOUT:   CRC result, readable during normal operation (EN=1,
 *               SETUP complete, CRC not busy).
 *   - CRCSHIFT: CRC shift register (seed value), loaded during setup.
 *   - CRCXOR:   CRC XOR polynomial, loaded during setup.
 *
 * The defines below provide named access to each logical register.
 * The programmer is responsible for ensuring the correct module state
 * before accessing these addresses.
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * CRCOUT - CRC Output/Result Register (0x0353-0x0356, 32-bit, read mode)
 *
 * Contains the CRC computation result after all data has been shifted
 * through the engine (BUSY=0).  The valid result width is (PLEN+1) bits,
 * starting from bit 0.
 *
 * CRCOUTT (0x0356):  Bits 7:0  OUT[31:24]
 * CRCOUTU (0x0355):  Bits 7:0  OUT[23:16]
 * CRCOUTH (0x0354):  Bits 7:0  OUT[15:8]
 * CRCOUTL (0x0353):  Bits 7:0  OUT[7:0]
 * ------------------------------------------------------------------------- */
#define CRCOUTL SFR8(0x0353)
#define CRCOUTH SFR8(0x0354)
#define CRCOUTU SFR8(0x0355)
#define CRCOUTT SFR8(0x0356)
#define CRCOUT SFR32(0x0353) /**< 32-bit CRC result access    */

/* ---------------------------------------------------------------------------
 * CRCSHIFT - CRC Shift Register (0x0353-0x0356, 32-bit, setup mode)
 *
 * During setup, this register is loaded with the initial CRC seed value
 * (often all-ones or all-zeros depending on the CRC algorithm).  During
 * operation, it holds the intermediate shift register state.
 *
 * Shares physical address with CRCOUT and CRCXOR.  Access during the
 * seed-loading phase of the setup sequence.
 *
 * CRCSHIFTT (0x0356):  Bits 7:0  SHIFT[31:24]
 * CRCSHIFTU (0x0355):  Bits 7:0  SHIFT[23:16]
 * CRCSHIFTH (0x0354):  Bits 7:0  SHIFT[15:8]
 * CRCSHIFTL (0x0353):  Bits 7:0  SHIFT[7:0]
 *
 * @note Aliases to same addresses as CRCOUT and CRCXOR.
 * ------------------------------------------------------------------------- */
#define CRCSHIFTL SFR8(0x0353)
#define CRCSHIFTH SFR8(0x0354)
#define CRCSHIFTU SFR8(0x0355)
#define CRCSHIFTT SFR8(0x0356)
#define CRCSHIFT SFR32(0x0353) /**< 32-bit shift register access */

/* ---------------------------------------------------------------------------
 * CRCXOR - CRC XOR Polynomial Register (0x0353-0x0356, 32-bit, setup mode)
 *
 * During setup, this register is loaded with the generator polynomial.
 * Bit 0 of the polynomial is implicitly 1 and should NOT be included
 * in the CRCXOR value (the hardware handles it).  The polynomial width
 * is (PLEN+1) bits.
 *
 * Example: For CRC-16-CCITT (polynomial 0x1021), with PLEN=15:
 *          Write 0x1021 to CRCXOR during the polynomial-load phase.
 *
 * Shares physical address with CRCOUT and CRCSHIFT.  Access during the
 * polynomial-loading phase of the setup sequence.
 *
 * CRCXORT (0x0356):  Bits 7:0  XOR[31:24]
 * CRCXORU (0x0355):  Bits 7:0  XOR[23:16]
 * CRCXORH (0x0354):  Bits 7:0  XOR[15:8]
 * CRCXORL (0x0353):  Bits 7:0  XOR[7:0]
 *
 * @note Aliases to same addresses as CRCOUT and CRCSHIFT.
 * ------------------------------------------------------------------------- */
#define CRCXORL SFR8(0x0353)
#define CRCXORH SFR8(0x0354)
#define CRCXORU SFR8(0x0355)
#define CRCXORT SFR8(0x0356)
#define CRCXOR SFR32(0x0353) /**< 32-bit XOR polynomial access */

/* ============================================================================
 * CRC Control Registers
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * CRCCON0 - CRC Control Register 0 (0x0357)
 *
 * Bit 7    EN      CRC Module Enable
 *                    1 = CRC module enabled
 *                    0 = CRC module disabled (reset state, low power)
 * Bit 6    ACCM    Accumulate Mode
 *                    1 = Data is shifted through CRC engine continuously;
 *                        each new CRCDATA write feeds the next block of
 *                        data without resetting the shift register.
 *                    0 = Data is loaded and shifted; the shift register
 *                        is loaded from CRCDATA before shifting begins.
 * Bit 5    --      Reserved
 * Bit 4    SHIFTM  Shift Direction
 *                    1 = Shift left (MSb first / big-endian bit order)
 *                    0 = Shift right (LSb first / little-endian bit order)
 * Bit 3    --      Reserved
 * Bit 2    --      Reserved
 * Bit 1    FULL    Data Register Full (read-only)
 *                    1 = CRCDATA contains unprocessed data (do not write
 *                        new data until FULL clears)
 *                    0 = CRCDATA is empty; ready for new data
 * Bit 0    BUSY    CRC Busy (read-only)
 *                    1 = CRC engine is currently shifting/calculating
 *                    0 = CRC engine is idle; result is valid in CRCOUT
 * ------------------------------------------------------------------------- */
#define CRCCON0 SFR8(0x0357)

#define CRCCON0_EN (1u << 7)     /**< CRC module enable               */
#define CRCCON0_ACCM (1u << 6)   /**< Accumulate mode                 */
#define CRCCON0_SHIFTM (1u << 4) /**< Shift direction (1=MSb first)   */
#define CRCCON0_FULL (1u << 1)   /**< Data register full (read-only)  */
#define CRCCON0_BUSY (1u << 0)   /**< CRC busy flag (read-only)       */

/* ---------------------------------------------------------------------------
 * CRCCON1 - CRC Control Register 1 (0x0358)
 *
 * Specifies the polynomial length (width) for the CRC calculation.
 * The actual polynomial width is (PLEN + 1) bits.
 *
 * Bits 7:5  -- Reserved
 * Bits 4:0  PLEN[4:0]  Polynomial Length minus 1
 *                       0x00 =  1-bit polynomial
 *                       0x07 =  8-bit polynomial (e.g. CRC-8)
 *                       0x0F = 16-bit polynomial (e.g. CRC-16)
 *                       0x1F = 32-bit polynomial (e.g. CRC-32)
 * ------------------------------------------------------------------------- */
#define CRCCON1 SFR8(0x0358)

#define CRCCON1_PLEN_MASK 0x1Fu /**< PLEN[4:0] bit mask              */
#define CRCCON1_PLEN_SHIFT 0u   /**< PLEN[4:0] bit position          */

/** Common polynomial length presets (value to write to PLEN field) */
#define CRCCON1_PLEN_8BIT 0x07u  /**< 8-bit polynomial  (PLEN=7)      */
#define CRCCON1_PLEN_16BIT 0x0Fu /**< 16-bit polynomial (PLEN=15)     */
#define CRCCON1_PLEN_32BIT 0x1Fu /**< 32-bit polynomial (PLEN=31)     */

/* ---------------------------------------------------------------------------
 * CRCCON2 - CRC Control Register 2 (0x0359)
 *
 * Specifies the data input width.  The actual data width is (DLEN + 1)
 * bits.  Only the least-significant (DLEN+1) bits of CRCDATA are used
 * per shift cycle.
 *
 * Bits 7:5  -- Reserved
 * Bits 4:0  DLEN[4:0]  Data Length minus 1
 *                       0x00 =  1-bit data input
 *                       0x07 =  8-bit data input
 *                       0x0F = 16-bit data input
 *                       0x1F = 32-bit data input
 * ------------------------------------------------------------------------- */
#define CRCCON2 SFR8(0x0359)

#define CRCCON2_DLEN_MASK 0x1Fu /**< DLEN[4:0] bit mask              */
#define CRCCON2_DLEN_SHIFT 0u   /**< DLEN[4:0] bit position          */

/** Common data length presets (value to write to DLEN field) */
#define CRCCON2_DLEN_8BIT 0x07u  /**< 8-bit data  (DLEN=7)            */
#define CRCCON2_DLEN_16BIT 0x0Fu /**< 16-bit data (DLEN=15)           */
#define CRCCON2_DLEN_32BIT 0x1Fu /**< 32-bit data (DLEN=31)           */

/* ============================================================================
 * Memory Scanner Registers
 *
 * The Memory Scanner automatically reads sequential words from Program
 * Flash Memory (PFM) and feeds them into the CRC engine.  This allows
 * CRC computation over flash regions (e.g. for firmware integrity checks)
 * without CPU involvement beyond setup and initiation.
 *
 * Typical usage:
 *   1. Configure CRC module (polynomial, seed, data width)
 *   2. Set scanner address range (SCANADRL/H/U to SCANADRL/H/U + count)
 *   3. Enable scanner (SCANCON0.EN = 1)
 *   4. Start scan (SCANCON0.GO = 1)
 *   5. Wait for SCANCON0.BUSY to clear
 *   6. Read result from CRCOUT
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * SCANCON0 - Scanner Control Register 0 (0x0360)
 *
 * Bit 7    EN      Scanner Enable
 *                    1 = Scanner module enabled
 *                    0 = Scanner module disabled
 * Bit 6    --      Reserved
 * Bit 5    BUSY    Scanner Busy (read-only)
 *                    1 = Scan is in progress
 *                    0 = Scanner is idle (scan complete or not started)
 * Bit 4    --      Reserved
 * Bit 3    MREG    (Implementation-specific; not publicly documented)
 * Bit 2    --      Reserved
 * Bit 1    GO      Scanner Go
 *                    Write 1 to start scanning the configured address
 *                    range.  Cleared by hardware when scan completes.
 * Bit 0    BURSTMD Burst Mode
 *                    1 = Scan entire range without releasing the system
 *                        bus (fastest, but blocks CPU and DMA)
 *                    0 = Scan one word per system clock arbitration cycle
 *                        (slower, but allows interleaved CPU/DMA access)
 * ------------------------------------------------------------------------- */
#define SCANCON0 SFR8(0x0360)

#define SCANCON0_EN (1u << 7)      /**< Scanner enable                  */
#define SCANCON0_BUSY (1u << 5)    /**< Scanner busy (read-only)        */
#define SCANCON0_MREG (1u << 3)    /**< Implementation-specific flag    */
#define SCANCON0_GO (1u << 1)      /**< Start scan (write 1 to begin)   */
#define SCANCON0_BURSTMD (1u << 0) /**< Burst mode (1=continuous scan)  */

/* ---------------------------------------------------------------------------
 * SCANTRIG - Scanner Trigger Source Register (0x0361)
 *
 * When the scanner is not in burst mode (BURSTMD=0), each scan step
 * (one word read from PFM) can be paced by a hardware trigger event.
 * This register selects the trigger source.  When BURSTMD=1, the
 * trigger source is ignored (scanner runs continuously).
 *
 * Bits 7:5  -- Reserved
 * Bits 4:0  TSEL[4:0]  Trigger Source Select
 *                       Selects which hardware event triggers each scan
 *                       step.  See DS40002213D Table 23-1 for the
 *                       complete trigger source mapping.
 * ------------------------------------------------------------------------- */
#define SCANTRIG SFR8(0x0361)

#define SCANTRIG_TSEL_MASK 0x1Fu /**< TSEL[4:0] bit mask              */
#define SCANTRIG_TSEL_SHIFT 0u   /**< TSEL[4:0] bit position          */

#endif /* PIC18F_Q84_CRC_H */
