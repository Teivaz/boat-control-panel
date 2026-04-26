/*
 * pic18f_q84_nvm.h
 *
 * NVM (Nonvolatile Memory) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * Provides run-time read/write access to:
 *   - Program Flash Memory (PFM) : 128 KB, 16-bit words
 *   - Data EEPROM               : 1024 bytes
 *   - Configuration bits
 *
 * Supported operations:
 *   - Word read / Page read
 *   - Word write / Page write
 *   - Page erase
 *
 * All write and erase operations require the unlock sequence:
 *   NVMLOCK = 0x55;
 *   NVMLOCK = 0xAA;
 * immediately before setting the GO bit in NVMCON0.
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_NVM_H
#define PIC18F_Q84_NVM_H

/* ===================================================================
 * NVMCON0 - NVM Control Register 0
 * Address: 0x0040
 *
 * Controls initiation of NVM operations and reflects busy status.
 * Software sets GO to start an operation; hardware clears it when
 * the operation completes.  Polling GO gives the busy/done status.
 * ================================================================ */
#define NVMCON0 SFR8(0x0040)

/* Bit 7 - GO: NVM Operation Start / Busy Status
 *   Write 1 = Initiate the NVM command selected by NVMCMD[2:0].
 *   Read  1 = Operation in progress (busy).
 *   Read  0 = Operation complete or idle.
 *   Cleared by hardware when the operation finishes.              */
#define NVMCON0_GO 7

/* Bits 6:1 - Reserved: read as 0, write as 0.                     */

/* Bit 0 - Unimplemented: read as 0.                               */

/* ===================================================================
 * NVMCON1 - NVM Control Register 1
 * Address: 0x0041
 *
 * Holds the NVM command selection and the write/erase error flag.
 * ================================================================ */
#define NVMCON1 SFR8(0x0041)

/* Bit 6 - WRERR: Write/Erase Error Flag
 *   1 = A write or erase error occurred (target address was
 *       write-protected or invalid).
 *   0 = No error.
 *   Cleared by starting a new valid NVM operation.                */
#define NVMCON1_WRERR 6

/* Bits 5:3 - Reserved: read as 0, write as 0.                     */

/* Bits 2:0 - NVMCMD[2:0]: NVM Command Select
 *   000 = Read Word         - read one 16-bit word at NVMADR
 *   001 = Read + Post-Inc   - read word, then NVMADR += 2
 *   010 = Read Page         - read an entire page into buffers
 *   011 = Write Word        - write one 16-bit word at NVMADR
 *   100 = Write + Post-Inc  - write word, then NVMADR += 2
 *   101 = Write Page        - write a full page from buffers
 *   110 = Page Erase        - erase the page containing NVMADR
 *   111 = Reserved (NOP)                                          */
#define NVMCON1_NVMCMD2 2
#define NVMCON1_NVMCMD1 1
#define NVMCON1_NVMCMD0 0

/* NVMCMD field mask and position                                   */
#define NVMCON1_NVMCMD_MASK 0x07 /* bits 2:0 */
#define NVMCON1_NVMCMD_POS 0

/* Named command values for NVMCMD[2:0]                             */
#define NVMCMD_READ_WORD 0x00     /* Read single word            */
#define NVMCMD_READ_POSTINC 0x01  /* Read word, post-increment   */
#define NVMCMD_READ_PAGE 0x02     /* Read entire page            */
#define NVMCMD_WRITE_WORD 0x03    /* Write single word           */
#define NVMCMD_WRITE_POSTINC 0x04 /* Write word, post-increment  */
#define NVMCMD_WRITE_PAGE 0x05    /* Write entire page           */
#define NVMCMD_PAGE_ERASE 0x06    /* Erase page at NVMADR        */
#define NVMCMD_NOP 0x07           /* Reserved / no operation     */

/* ===================================================================
 * NVMLOCK - NVM Unlock Register
 * Address: 0x0042
 *
 * Write-only.  The mandatory unlock sequence must be executed
 * immediately before setting NVMCON0.GO for any write or erase:
 *
 *   NVMLOCK = 0x55;
 *   NVMLOCK = 0xAA;
 *   NVMCON0 |= (1 << NVMCON0_GO);
 *
 * If the sequence is interrupted or incorrect the subsequent
 * write/erase operation is silently ignored.
 * ================================================================ */
#define NVMLOCK SFR8(0x0042)

/* Bits 7:0 - NVMLOCK[7:0]: Unlock value (write-only)
 *   Reads return 0x00.  Write 0x55 followed by 0xAA to unlock.   */

/* Unlock sequence magic values                                     */
#define NVMLOCK_KEY1 0x55 /* First key of unlock sequence  */
#define NVMLOCK_KEY2 0xAA /* Second key of unlock sequence */

/* ===================================================================
 * NVMADR - NVM Address Register (3 bytes, little-endian)
 * Address: 0x0043 (NVMADRL) .. 0x0045 (NVMADRU)
 *
 * 22-bit address selects the target location within the unified
 * memory map: Program Flash (128 KB), Data EEPROM, User IDs, and
 * Configuration Words.
 *
 * NVMADRL:NVMADRH can also be accessed as a 16-bit pair.
 * ================================================================ */
#define NVMADRL SFR8(0x0043) /* Address bits  7:0      */
#define NVMADRH SFR8(0x0044) /* Address bits 15:8      */
#define NVMADRU SFR8(0x0045) /* Address bits 21:16     */
#define NVMADR SFR16(0x0043) /* 16-bit access (L:H)    */

/* NVMADRL - Bits 7:0: Low byte of the NVM target address.          */

/* NVMADRH - Bits 7:0: High byte of the NVM target address.         */

/* NVMADRU - Bits 5:0: Upper byte of the NVM target address.
 *   Only bits 5:0 are implemented (22-bit address space).
 *   Bits 7:6 are unimplemented and read as 0.                     */
#define NVMADRU_MASK 0x3F /* bits 5:0 valid */

/* ===================================================================
 * NVMDAT - NVM Data Register (2 bytes, little-endian)
 * Address: 0x0046 (NVMDATL) .. 0x0047 (NVMDATH)
 *
 * Holds the 16-bit data word for read and write operations.
 * PFM words are 16 bits wide; for Data EEPROM only the low byte
 * (NVMDATL) is significant.
 * ================================================================ */
#define NVMDATL SFR8(0x0046) /* Data bits  7:0         */
#define NVMDATH SFR8(0x0047) /* Data bits 15:8         */
#define NVMDAT SFR16(0x0046) /* 16-bit access (L:H)    */

/* NVMDATL - Bits 7:0: Low byte of NVM read/write data.             */

/* NVMDATH - Bits 7:0: High byte of NVM read/write data.
 *   For Data EEPROM accesses this byte is ignored / reads as 0.   */

/* ===================================================================
 * BOOTREG - Boot Block Control Register
 * Address: 0x0038
 *
 * Controls boot partition behaviour.  The boot block code sets
 * BOOTDONE when its initialisation is finished; once set, BPOUT
 * determines external access permissions to the boot block region.
 * ================================================================ */
#define BOOTREG SFR8(0x0038)

/* Bit 1 - BOOTDONE: Boot Block Done Flag
 *   1 = Boot code has completed its initialisation sequence.
 *   0 = Boot sequence not yet complete.
 *   Typically set once by boot-block firmware; cannot be cleared
 *   by software (cleared only by a device Reset).                 */
#define BOOTREG_BOOTDONE 1

/* Bit 0 - BPOUT: Boot Partition Output
 *   Controls access permissions to the boot block after BOOTDONE
 *   has been set.
 *   1 = Boot block accessible by application code.
 *   0 = Boot block locked from application code access.
 *   Only meaningful after BOOTDONE = 1.                           */
#define BOOTREG_BPOUT 0

#endif /* PIC18F_Q84_NVM_H */
