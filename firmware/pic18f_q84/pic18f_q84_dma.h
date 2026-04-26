/**
 * @file pic18f_q84_dma.h
 * @brief DMA (Direct Memory Access) register definitions for PIC18F27/47/57Q84.
 *
 * The DMA subsystem provides 8 independent channels capable of hardware data
 * transfers between SFR, GPR (General Purpose RAM), PFM (Program Flash Memory),
 * and EEPROM address spaces without CPU intervention.  Each channel supports:
 *
 *   - Hardware-triggered transfers (via configurable IRQ source)
 *   - Software-triggered transfers (via the DGO bit)
 *   - Hardware abort (via configurable abort IRQ source)
 *   - Configurable source/destination address modes (fixed, increment, decrement)
 *   - Configurable source/destination message sizes (12-bit)
 *   - 22-bit source addressing (access full 4 MB PFM/EEPROM space)
 *   - 16-bit destination addressing (SFR/GPR space)
 *   - Independent source/destination stop-on-count-zero control
 *   - System-bus arbitration with programmable per-channel priority
 *
 * Register banking model:
 *   The 8 DMA channels share a single set of DMAnXXX register addresses
 *   (0x00E8-0x00FF).  The DMASELECT register (0x00E7) determines which
 *   channel's registers are currently mapped to those addresses.  Write the
 *   channel index (0-7 for DMA1-DMA8) to DMASELECT before accessing any
 *   DMAnXXX register.
 *
 * Priority arbitration:
 *   When multiple bus masters (CPU main program, ISR, scanner, DMA channels)
 *   contend for the data bus, the priority registers (DMA1PR-DMA8PR, MAINPR,
 *   ISRPR, SCANPR) determine the winner.  Lower PR[2:0] values indicate
 *   higher priority.  The PRLOCK register can lock priority settings to
 *   prevent accidental modification.
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 17: Direct Memory Access (DMA)
 *
 * @note SFR8(addr) and SFR16(addr) macros must be defined before including
 *       this header (typically via pic18f_q84.h).
 * @note All register addresses are absolute.  Multi-byte registers are
 *       little-endian (low byte at base address).
 * @note Reserved bits should be written as 0; their read values are undefined.
 */

#ifndef PIC18F_Q84_DMA_H
#define PIC18F_Q84_DMA_H

/* ============================================================================
 * DMA Priority and System-Bus Arbitration Registers
 * ========================================================================= */

/**
 * PRLOCK - Priority Lock Register (0x00DB)
 *
 * Once the PRLOCKED bit is set, all DMA priority registers (DMA1PR-DMA8PR,
 * MAINPR, ISRPR, SCANPR) become read-only until the next device reset.
 * This prevents accidental priority changes during operation.
 *
 * Bit 7:1  -- Reserved
 * Bit 0    PRLOCKED  Priority Lock (1 = locked, 0 = unlocked)
 */
#define PRLOCK SFR8(0x00B4)

#define PRLOCK_PRLOCKED (1u << 0) /**< Lock all priority registers    */

/**
 * SCANPR - Scanner Priority Register (0x00DC)
 *
 * Sets the system-bus arbitration priority for the memory scanner module.
 *
 * Bits 7:3  -- Reserved
 * Bits 2:0  PR[2:0]  Scanner priority (0 = highest, 7 = lowest)
 */
#define SCANPR SFR8(0x00B5)

#define SCANPR_PR_MASK 0x07u /**< PR[2:0] bit mask               */
#define SCANPR_PR_SHIFT 0u   /**< PR[2:0] bit position           */

/**
 * DMA1PR through DMA8PR - DMA Channel Priority Registers (0x00DD-0x00E4)
 *
 * Each register sets the system-bus arbitration priority for the
 * corresponding DMA channel.
 *
 * Bits 7:3  -- Reserved
 * Bits 2:0  PR[2:0]  Channel priority (0 = highest, 7 = lowest)
 */
#define DMA1PR SFR8(0x00B6)
#define DMA2PR SFR8(0x00B7)
#define DMA3PR SFR8(0x00B8)
#define DMA4PR SFR8(0x00B9)
#define DMA5PR SFR8(0x00BA)
#define DMA6PR SFR8(0x00BB)
#define DMA7PR SFR8(0x00BC)
#define DMA8PR SFR8(0x00BD)

/** Bit mask and shift common to all DMAcPR registers */
#define DMAxPR_PR_MASK 0x07u /**< PR[2:0] bit mask               */
#define DMAxPR_PR_SHIFT 0u   /**< PR[2:0] bit position           */

/**
 * MAINPR - Main Program Priority Register (0x00E5)
 *
 * Sets the system-bus arbitration priority for the CPU main program
 * (non-interrupt) context.
 *
 * Bits 7:3  -- Reserved
 * Bits 2:0  PR[2:0]  Main program priority (0 = highest, 7 = lowest)
 */
#define MAINPR SFR8(0x00BE)

#define MAINPR_PR_MASK 0x07u /**< PR[2:0] bit mask               */
#define MAINPR_PR_SHIFT 0u   /**< PR[2:0] bit position           */

/**
 * ISRPR - ISR Priority Register (0x00E6)
 *
 * Sets the system-bus arbitration priority for the interrupt service
 * routine context.
 *
 * Bits 7:3  -- Reserved
 * Bits 2:0  PR[2:0]  ISR priority (0 = highest, 7 = lowest)
 */
#define ISRPR SFR8(0x00BF)

#define ISRPR_PR_MASK 0x07u /**< PR[2:0] bit mask               */
#define ISRPR_PR_SHIFT 0u   /**< PR[2:0] bit position           */

/* ============================================================================
 * DMA Channel Select Register
 * ========================================================================= */

/**
 * DMASELECT - DMA Channel Select Register (0x00E7)
 *
 * Selects which DMA channel's registers are mapped to the DMAnXXX addresses
 * (0x00E8-0x00FF).  Write the channel index before accessing any banked
 * DMA register.
 *
 * Bits 7:3  -- Reserved
 * Bits 2:0  SLCT[2:0]  Channel select
 *           000 = DMA1, 001 = DMA2, 010 = DMA3, 011 = DMA4
 *           100 = DMA5, 101 = DMA6, 110 = DMA7, 111 = DMA8
 */
#define DMASELECT SFR8(0x00E8)

#define DMASELECT_SLCT_MASK 0x07u /**< SLCT[2:0] bit mask             */
#define DMASELECT_SLCT_SHIFT 0u   /**< SLCT[2:0] bit position         */

/** Convenience values for DMASELECT_SLCT */
#define DMASELECT_DMA1 0x00u /**< Select DMA channel 1           */
#define DMASELECT_DMA2 0x01u /**< Select DMA channel 2           */
#define DMASELECT_DMA3 0x02u /**< Select DMA channel 3           */
#define DMASELECT_DMA4 0x03u /**< Select DMA channel 4           */
#define DMASELECT_DMA5 0x04u /**< Select DMA channel 5           */
#define DMASELECT_DMA6 0x05u /**< Select DMA channel 6           */
#define DMASELECT_DMA7 0x06u /**< Select DMA channel 7           */
#define DMASELECT_DMA8 0x07u /**< Select DMA channel 8           */

/* ============================================================================
 * Banked DMA Channel Registers (active channel selected by DMASELECT)
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * DMAnBUF - DMA Buffer Register (0x00E8)
 *
 * Holds the data byte currently being transferred by the selected DMA
 * channel.  During a DMA transfer, data read from the source address is
 * latched into BUF and then written to the destination address.
 *
 * Bits 7:0  BUF[7:0]  Transfer data byte
 * ------------------------------------------------------------------------- */
#define DMAnBUF SFR8(0x00E8)

/* ---------------------------------------------------------------------------
 * DMAnDCNT - DMA Destination Count (0x00E9-0x00EA, 12-bit)
 *
 * Current destination byte count.  Loaded from DMAnDSZ at transfer start,
 * then decrements with each byte written to the destination.  When it
 * reaches zero, behavior depends on the DSTP bit in DMAnCON1.
 *
 * DMAnDCNTH (0x00EA):
 *   Bits 7:4  -- Reserved
 *   Bits 3:0  DCNT[11:8]  High nibble of destination count
 *
 * DMAnDCNTL (0x00E9):
 *   Bits 7:0  DCNT[7:0]   Low byte of destination count
 * ------------------------------------------------------------------------- */
#define DMAnDCNTL SFR8(0x00E9)
#define DMAnDCNTH SFR8(0x00EA)
#define DMAnDCNT SFR16(0x00E9) /**< 16-bit access (bits 11:0 valid) */

#define DMAnDCNT_MASK 0x0FFFu /**< 12-bit destination count mask  */

/* ---------------------------------------------------------------------------
 * DMAnDPTR - DMA Destination Pointer (0x00EC-0x00ED, 16-bit)
 *
 * Current destination address.  Loaded from DMAnDSA at transfer start.
 * Auto-adjusts during transfer per the DMODE setting in DMAnCON1.
 *
 * DMAnDPTRH (0x00ED):
 *   Bits 7:0  DPTR[15:8]  High byte of destination pointer
 *
 * DMAnDPTRL (0x00EC):
 *   Bits 7:0  DPTR[7:0]   Low byte of destination pointer
 * ------------------------------------------------------------------------- */
#define DMAnDPTRL SFR8(0x00EC)
#define DMAnDPTRH SFR8(0x00ED)
#define DMAnDPTR SFR16(0x00EC) /**< 16-bit destination pointer   */

/* ---------------------------------------------------------------------------
 * DMAnDSZ - DMA Destination Size (0x00EE-0x00EF, 12-bit)
 *
 * Destination message size: the number of bytes per transfer block on the
 * destination side.  Copied into DMAnDCNT at transfer start.
 *
 * DMAnDSZH (0x00EF):
 *   Bits 7:4  -- Reserved
 *   Bits 3:0  DSZ[11:8]   High nibble of destination size
 *
 * DMAnDSZL (0x00EE):
 *   Bits 7:0  DSZ[7:0]    Low byte of destination size
 * ------------------------------------------------------------------------- */
#define DMAnDSZL SFR8(0x00EE)
#define DMAnDSZH SFR8(0x00EF)
#define DMAnDSZ SFR16(0x00EE) /**< 16-bit access (bits 11:0 valid) */

#define DMAnDSZ_MASK 0x0FFFu /**< 12-bit destination size mask   */

/* ---------------------------------------------------------------------------
 * DMAnDSA - DMA Destination Start Address (0x00F0-0x00F1, 16-bit)
 *
 * Starting address for the destination.  Points into SFR or GPR space
 * (16-bit addressable).  Loaded into DMAnDPTR when a transfer begins.
 *
 * DMAnDSAH (0x00F1):
 *   Bits 7:0  DSA[15:8]   High byte of destination start address
 *
 * DMAnDSAL (0x00F0):
 *   Bits 7:0  DSA[7:0]    Low byte of destination start address
 * ------------------------------------------------------------------------- */
#define DMAnDSAL SFR8(0x00F0)
#define DMAnDSAH SFR8(0x00F1)
#define DMAnDSA SFR16(0x00F0) /**< 16-bit destination start addr */

/* ---------------------------------------------------------------------------
 * DMAnSCNT - DMA Source Count (0x00F2-0x00F3, 12-bit)
 *
 * Current source byte count.  Loaded from DMAnSSZ at transfer start,
 * then decrements with each byte read from the source.  When it reaches
 * zero, behavior depends on the SSTP bit in DMAnCON1.
 *
 * DMAnSCNTH (0x00F3):
 *   Bits 7:4  -- Reserved
 *   Bits 3:0  SCNT[11:8]  High nibble of source count
 *
 * DMAnSCNTL (0x00F2):
 *   Bits 7:0  SCNT[7:0]   Low byte of source count
 * ------------------------------------------------------------------------- */
#define DMAnSCNTL SFR8(0x00F2)
#define DMAnSCNTH SFR8(0x00F3)
#define DMAnSCNT SFR16(0x00F2) /**< 16-bit access (bits 11:0 valid) */

#define DMAnSCNT_MASK 0x0FFFu /**< 12-bit source count mask       */

/* ---------------------------------------------------------------------------
 * DMAnSPTR - DMA Source Pointer (0x00F4-0x00F6, 22-bit)
 *
 * Current source address.  Loaded from DMAnSSA at transfer start.  The
 * 22-bit width allows addressing the full PFM and EEPROM space.
 * Auto-adjusts during transfer per the SMODE setting in DMAnCON0.
 *
 * DMAnSPTRU (0x00F6):
 *   Bits 7:6  -- Reserved
 *   Bits 5:0  SPTR[21:16] Upper 6 bits of source pointer
 *
 * DMAnSPTRH (0x00F5):
 *   Bits 7:0  SPTR[15:8]  High byte of source pointer
 *
 * DMAnSPTRL (0x00F4):
 *   Bits 7:0  SPTR[7:0]   Low byte of source pointer
 * ------------------------------------------------------------------------- */
#define DMAnSPTRL SFR8(0x00F4)
#define DMAnSPTRH SFR8(0x00F5)
#define DMAnSPTRU SFR8(0x00F6)
#define DMAnSPTR SFR16(0x00F4) /**< 16-bit access (low 16 bits)  */

#define DMAnSPTRU_MASK 0x3Fu /**< SPTR[21:16] valid bits in U    */

/* ---------------------------------------------------------------------------
 * DMAnSSZ - DMA Source Size (0x00F7-0x00F8, 12-bit)
 *
 * Source message size: the number of bytes per transfer block on the
 * source side.  Copied into DMAnSCNT at transfer start.
 *
 * DMAnSSZH (0x00F8):
 *   Bits 7:4  -- Reserved
 *   Bits 3:0  SSZ[11:8]   High nibble of source size
 *
 * DMAnSSZL (0x00F7):
 *   Bits 7:0  SSZ[7:0]    Low byte of source size
 * ------------------------------------------------------------------------- */
#define DMAnSSZL SFR8(0x00F7)
#define DMAnSSZH SFR8(0x00F8)
#define DMAnSSZ SFR16(0x00F7) /**< 16-bit access (bits 11:0 valid) */

#define DMAnSSZ_MASK 0x0FFFu /**< 12-bit source size mask        */

/* ---------------------------------------------------------------------------
 * DMAnSSA - DMA Source Start Address (0x00F9-0x00FB, 22-bit)
 *
 * Starting address for the source.  The 22-bit width allows addressing
 * the full PFM (up to 128 KB) and EEPROM space.  For SFR/GPR sources,
 * only the lower 16 bits are relevant (SSA[21:16] = 0).
 * Loaded into DMAnSPTR when a transfer begins.
 *
 * DMAnSSAU (0x00FB):
 *   Bits 7:6  -- Reserved
 *   Bits 5:0  SSA[21:16]  Upper 6 bits of source start address
 *
 * DMAnSSAH (0x00FA):
 *   Bits 7:0  SSA[15:8]   High byte of source start address
 *
 * DMAnSSAL (0x00F9):
 *   Bits 7:0  SSA[7:0]    Low byte of source start address
 * ------------------------------------------------------------------------- */
#define DMAnSSAL SFR8(0x00F9)
#define DMAnSSAH SFR8(0x00FA)
#define DMAnSSAU SFR8(0x00FB)
#define DMAnSSA SFR16(0x00F9) /**< 16-bit access (low 16 bits)  */

#define DMAnSSAU_MASK 0x3Fu /**< SSA[21:16] valid bits in U     */

/* ---------------------------------------------------------------------------
 * DMAnCON0 - DMA Control Register 0 (0x00FC)
 *
 * Controls DMA channel enable, trigger mode, and source address mode.
 *
 * Bit 7    EN       DMA Channel Enable
 *                     1 = Channel enabled
 *                     0 = Channel disabled
 * Bit 6    SIRQEN   Source IRQ Enable
 *                     1 = Transfer triggered by hardware IRQ (see DMAnSIRQ)
 *                     0 = Software trigger only (via DGO bit)
 * Bit 5    DGO      DMA Transfer Start (software trigger)
 *                     Write 1 to initiate a transfer; auto-cleared on
 *                     completion.  Has no effect when SIRQEN = 1 and a
 *                     hardware trigger is pending.
 * Bit 4    --       Reserved
 * Bit 3    AIRQEN   Abort IRQ Enable
 *                     1 = Hardware abort enabled (see DMAnAIRQ)
 *                     0 = No hardware abort source
 * Bit 2    --       Reserved
 * Bits 1:0 SMODE[1:0]  Source Address Mode
 *                     00 = Fixed (address stays at SSA)
 *                     01 = Increment (address increments after each byte)
 *                     10 = Decrement (address decrements after each byte)
 *                     11 = Reserved
 * ------------------------------------------------------------------------- */
#define DMAnCON0 SFR8(0x00FC)

#define DMAnCON0_EN (1u << 7)     /**< DMA channel enable             */
#define DMAnCON0_SIRQEN (1u << 6) /**< Source IRQ (hardware) trigger   */
#define DMAnCON0_DGO (1u << 5)    /**< Software-initiated transfer go  */
#define DMAnCON0_AIRQEN (1u << 3) /**< Abort IRQ enable               */

#define DMAnCON0_SMODE_MASK 0x03u /**< SMODE[1:0] bit mask            */
#define DMAnCON0_SMODE_SHIFT 0u   /**< SMODE[1:0] bit position        */

/** Source address mode values for DMAnCON0 SMODE[1:0] */
#define DMAnCON0_SMODE_FIXED 0x00u /**< Source address fixed at SSA    */
#define DMAnCON0_SMODE_INC 0x01u   /**< Source address increments      */
#define DMAnCON0_SMODE_DEC 0x02u   /**< Source address decrements      */

/* ---------------------------------------------------------------------------
 * DMAnCON1 - DMA Control Register 1 (0x00FD)
 *
 * Controls stop-on-count-zero behavior and destination address mode.
 *
 * Bit 7    --       Reserved
 * Bit 6    DSTP     Destination Stop
 *                     1 = DMA stops when destination count (DCNT) reaches 0
 *                     0 = Destination count wraps (reloads from DSZ)
 * Bit 5    SSTP     Source Stop
 *                     1 = DMA stops when source count (SCNT) reaches 0
 *                     0 = Source count wraps (reloads from SSZ)
 * Bit 4    --       Reserved
 * Bit 3    --       Reserved
 * Bit 2    --       Reserved
 * Bits 1:0 DMODE[1:0]  Destination Address Mode
 *                     00 = Fixed (address stays at DSA)
 *                     01 = Increment (address increments after each byte)
 *                     10 = Decrement (address decrements after each byte)
 *                     11 = Reserved
 * ------------------------------------------------------------------------- */
#define DMAnCON1 SFR8(0x00FD)

#define DMAnCON1_DSTP (1u << 6) /**< Stop when dest count = 0       */
#define DMAnCON1_SSTP (1u << 5) /**< Stop when source count = 0     */

#define DMAnCON1_DMODE_MASK 0x03u /**< DMODE[1:0] bit mask            */
#define DMAnCON1_DMODE_SHIFT 0u   /**< DMODE[1:0] bit position        */

/** Destination address mode values for DMAnCON1 DMODE[1:0] */
#define DMAnCON1_DMODE_FIXED 0x00u /**< Dest address fixed at DSA      */
#define DMAnCON1_DMODE_INC 0x01u   /**< Dest address increments        */
#define DMAnCON1_DMODE_DEC 0x02u   /**< Dest address decrements        */

/* ---------------------------------------------------------------------------
 * DMAnAIRQ - DMA Abort IRQ Source Register (0x00FE)
 *
 * Selects the interrupt source that, when asserted, aborts an in-progress
 * DMA transfer on this channel.  Only effective when AIRQEN = 1 in
 * DMAnCON0.  The IRQ source numbers are defined in the device interrupt
 * source table (see DS40002213D, Table 10-1).
 *
 * Bits 7:0  AIRQ[7:0]  Abort IRQ source number
 * ------------------------------------------------------------------------- */
#define DMAnAIRQ SFR8(0x00FE)

/* ---------------------------------------------------------------------------
 * DMAnSIRQ - DMA Start/Trigger IRQ Source Register (0x00FF)
 *
 * Selects the interrupt source that triggers a DMA transfer on this
 * channel.  Only effective when SIRQEN = 1 in DMAnCON0.  Each trigger
 * event causes one byte (or one complete message, depending on stop
 * configuration) to be transferred.  The IRQ source numbers are defined
 * in the device interrupt source table (see DS40002213D, Table 10-1).
 *
 * Bits 7:0  SIRQ[7:0]  Start/trigger IRQ source number
 * ------------------------------------------------------------------------- */
#define DMAnSIRQ SFR8(0x00FF)

/* ============================================================================
 * Convenience Macros
 * ========================================================================= */

/**
 * @brief Select a DMA channel for banked register access.
 * @param ch  Channel number 1-8
 *
 * After calling this macro, all DMAnXXX register accesses will read/write
 * the registers for the specified channel.
 *
 * Example:
 * @code
 *   DMA_SELECT_CH(3);          // Select DMA channel 3
 *   DMAnSSA  = (uint16_t)src;  // Set source start address (low 16 bits)
 *   DMAnSSAU = 0x00;           // Upper source address bits
 *   DMAnSSZ  = 64;             // 64-byte source message
 *   DMAnDSA  = (uint16_t)dst;  // Destination start address
 *   DMAnDSZ  = 64;             // 64-byte destination message
 *   DMAnCON1 = DMAnCON1_SSTP | DMAnCON1_DSTP | DMAnCON1_DMODE_INC;
 *   DMAnCON0 = DMAnCON0_EN | DMAnCON0_SMODE_INC | DMAnCON0_DGO;
 * @endcode
 */
#define DMA_SELECT_CH(ch) (DMASELECT = (uint8_t)((ch) - 1u))

#endif /* PIC18F_Q84_DMA_H */
