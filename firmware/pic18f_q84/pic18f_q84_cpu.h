/**
 * @file pic18f_q84_cpu.h
 * @brief PIC18F27/47/57Q84 CPU core register definitions.
 *
 * This header defines all registers, bit masks, and bit positions for the
 * PIC18 enhanced CPU core found in the PIC18F-Q84 family.  Included are:
 *
 *   - STATUS register (ALU condition flags)
 *   - Interrupt control registers (INTCON0, INTCON1)
 *   - Bank Select Register (BSR)
 *   - Working register (WREG)
 *   - File Select Registers 0/1/2 and their indirect addressing modes
 *   - Program Counter (PCL, PCLATH, PCLATU)
 *   - Hardware stack pointer and Top-of-Stack (STKPTR, TOS)
 *   - Product register (PROD)
 *   - Table pointer and latch (TBLPTR, TABLAT)
 *   - Power control / reset cause (PCON0, PCON1)
 *   - CPU Doze / Idle control (CPUDOZE)
 *   - Fast-context-save shadow registers
 *
 * All addresses are absolute SFR addresses.  Multi-byte registers are
 * little-endian (low byte at the base address).
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *
 * @note This file is included automatically by pic18f_q84.h.
 *       It relies on SFR8() and SFR16() macros defined there.
 */

#ifndef PIC18F_Q84_CPU_H
#define PIC18F_Q84_CPU_H

/* ============================================================================
 *  STATUS - ALU Status Register
 * ============================================================================
 *  Address: 0x04D7   (8-bit, read/write)
 *
 *  Contains the arithmetic condition flags set by the ALU after most
 *  instructions.  These flags drive conditional branch instructions
 *  (BZ, BNZ, BC, BNC, BN, BNN, BOV, BNOV).
 *
 *    Bit 7:5 - Reserved (read as 0)
 *    Bit 4   - N   Negative flag
 *    Bit 3   - OV  Overflow flag (signed arithmetic)
 *    Bit 2   - Z   Zero flag
 *    Bit 1   - DC  Digit Carry / borrow (BCD half-carry from bit 3 to bit 4)
 *    Bit 0   - C   Carry / borrow (carry out of the MSb)
 * ========================================================================= */

#define STATUS_ADDR 0x04D8u
#define STATUS SFR8(STATUS_ADDR)

/* -- Bit positions -------------------------------------------------------- */
#define STATUS_N_POS 4  /**< Negative flag bit position       */
#define STATUS_OV_POS 3 /**< Overflow flag bit position       */
#define STATUS_Z_POS 2  /**< Zero flag bit position           */
#define STATUS_DC_POS 1 /**< Digit Carry flag bit position    */
#define STATUS_C_POS 0  /**< Carry flag bit position          */

/* -- Bit masks ------------------------------------------------------------ */
#define STATUS_N (1u << STATUS_N_POS)   /**< 0x10 - Result of last ALU op was negative (signed)  */
#define STATUS_OV (1u << STATUS_OV_POS) /**< 0x08 - Signed overflow occurred                     */
#define STATUS_Z (1u << STATUS_Z_POS)   /**< 0x04 - Result was zero                              */
#define STATUS_DC (1u << STATUS_DC_POS) /**< 0x02 - Half-carry (bit 3->4), used for BCD arith    */
#define STATUS_C (1u << STATUS_C_POS)   /**< 0x01 - Carry out from MSb of result                 */

/* ============================================================================
 *  INTCON0 - Interrupt Control Register 0
 * ============================================================================
 *  Address: 0x04D5   (8-bit, read/write)
 *
 *  Controls global interrupt enables, external interrupt edge polarity,
 *  and the interrupt priority enable bit.
 *
 *    Bit 7   - GIE/GIEH   Global Interrupt Enable
 *                          (High-priority enable when IPEN = 1)
 *    Bit 6   - GIEL        Low-priority Global Interrupt Enable
 *                          (effective only when IPEN = 1)
 *    Bit 5:4 - Reserved
 *    Bit 3   - INT2EDG     External Interrupt 2 edge select
 *    Bit 2   - INT1EDG     External Interrupt 1 edge select
 *    Bit 1   - INT0EDG     External Interrupt 0 edge select
 *    Bit 0   - IPEN        Interrupt Priority Enable
 * ========================================================================= */

#define INTCON0_ADDR 0x04D7u
#define INTCON0 SFR8(INTCON0_ADDR)

/* -- Bit positions -------------------------------------------------------- */
#define INTCON0_GIE_POS 7     /**< Global Interrupt Enable (GIEH)   */
#define INTCON0_GIEH_POS 7    /**< Alias: high-priority GIE         */
#define INTCON0_GIEL_POS 6    /**< Low-priority interrupt enable    */
#define INTCON0_INT2EDG_POS 3 /**< INT2 edge select                 */
#define INTCON0_INT1EDG_POS 2 /**< INT1 edge select                 */
#define INTCON0_INT0EDG_POS 1 /**< INT0 edge select                 */
#define INTCON0_IPEN_POS 0    /**< Interrupt priority enable        */

/* -- Bit masks ------------------------------------------------------------ */
#define INTCON0_GIE (1u << INTCON0_GIE_POS)         /**< 0x80 - 1=interrupts enabled globally               */
#define INTCON0_GIEH (1u << INTCON0_GIEH_POS)       /**< 0x80 - Alias for GIE (high-priority when IPEN=1)   */
#define INTCON0_GIEL (1u << INTCON0_GIEL_POS)       /**< 0x40 - 1=low-priority interrupts enabled (IPEN=1)  */
#define INTCON0_INT2EDG (1u << INTCON0_INT2EDG_POS) /**< 0x08 - 1=INT2 triggers on rising edge              */
#define INTCON0_INT1EDG (1u << INTCON0_INT1EDG_POS) /**< 0x04 - 1=INT1 triggers on rising edge              */
#define INTCON0_INT0EDG (1u << INTCON0_INT0EDG_POS) /**< 0x02 - 1=INT0 triggers on rising edge              */
#define INTCON0_IPEN (1u << INTCON0_IPEN_POS)       /**< 0x01 - 1=priority levels enabled, 0=compat mode    */

/* ============================================================================
 *  INTCON1 - Interrupt Control Register 1
 * ============================================================================
 *  Address: 0x04D6   (8-bit, read-only)
 *
 *  Provides the current interrupt status: which priority level is being
 *  serviced.
 *
 *    Bit 7:2 - Reserved
 *    Bit 1:0 - STAT[1:0]  Current interrupt priority level being serviced
 * ========================================================================= */

#define INTCON1_ADDR 0x04D7u
#define INTCON1 SFR8(INTCON1_ADDR)

/* -- Bit positions -------------------------------------------------------- */
#define INTCON1_STAT1_POS 1 /**< Interrupt status bit 1           */
#define INTCON1_STAT0_POS 0 /**< Interrupt status bit 0           */

/* -- Bit masks ------------------------------------------------------------ */
#define INTCON1_STAT_MASK 0x03u                 /**< Both STAT bits                   */
#define INTCON1_STAT1 (1u << INTCON1_STAT1_POS) /**< 0x02        */
#define INTCON1_STAT0 (1u << INTCON1_STAT0_POS) /**< 0x01        */

/* ============================================================================
 *  BSR - Bank Select Register
 * ============================================================================
 *  Address: 0x04E0   (8-bit, read/write)
 *
 *  Selects the active General Purpose Register (GPR) bank for direct
 *  addressing.  The PIC18F-Q84 has 64 banks (0-63).
 *
 *    Bit 7:6 - Reserved (read as 0)
 *    Bit 5:0 - BSR[5:0]  Bank number (0-63)
 * ========================================================================= */

#define BSR_ADDR 0x04E0u
#define BSR SFR8(BSR_ADDR)

#define BSR_MASK 0x3Fu /**< Valid bank select bits [5:0]     */

/* ============================================================================
 *  WREG - Working Register
 * ============================================================================
 *  Address: 0x04E8   (8-bit, read/write)
 *
 *  The accumulator for the PIC18 ALU.  Most ALU instructions use WREG as
 *  an implicit source and/or destination operand.
 *
 *    Bit 7:0 - WREG[7:0]
 * ========================================================================= */

#define WREG_ADDR 0x04E8u
#define WREG SFR8(WREG_ADDR)

/* ============================================================================
 *  FSR0 - File Select Register 0  (16-bit indirect addressing pointer)
 * ============================================================================
 *  Addresses: FSR0L = 0x04E9, FSR0H = 0x04EA
 *
 *  FSR0 forms a 14-bit pointer (high byte bits 5:0 + low byte) used for
 *  indirect data memory access through INDF0, POSTINC0, POSTDEC0,
 *  PREINC0, and PLUSW0 virtual registers.
 *
 *    FSR0L  Bit 7:0 - Low byte of pointer
 *    FSR0H  Bit 5:0 - High byte of pointer (bits 13:8 of address)
 * ========================================================================= */

#define FSR0L_ADDR 0x04E9u
#define FSR0H_ADDR 0x04EAu
#define FSR0_ADDR FSR0L_ADDR /**< 16-bit base address     */

#define FSR0L SFR8(FSR0L_ADDR)
#define FSR0H SFR8(FSR0H_ADDR)
#define FSR0 SFR16(FSR0_ADDR) /**< 16-bit access to FSR0   */

#define FSR0H_MASK 0x3Fu /**< Valid high-byte bits [5:0]       */

/* ============================================================================
 *  FSR1 - File Select Register 1  (16-bit indirect addressing pointer)
 * ============================================================================
 *  Addresses: FSR1L = 0x04E1, FSR1H = 0x04E2
 *
 *  Same function as FSR0 but provides a second independent indirect
 *  addressing pointer.
 *
 *    FSR1L  Bit 7:0 - Low byte of pointer
 *    FSR1H  Bit 5:0 - High byte of pointer
 * ========================================================================= */

#define FSR1L_ADDR 0x04E1u
#define FSR1H_ADDR 0x04E2u
#define FSR1_ADDR FSR1L_ADDR

#define FSR1L SFR8(FSR1L_ADDR)
#define FSR1H SFR8(FSR1H_ADDR)
#define FSR1 SFR16(FSR1_ADDR)

#define FSR1H_MASK 0x3Fu /**< Valid high-byte bits [5:0]       */

/* ============================================================================
 *  FSR2 - File Select Register 2  (16-bit indirect addressing pointer)
 * ============================================================================
 *  Addresses: FSR2L = 0x04D9, FSR2H = 0x04DA
 *
 *  Third independent indirect addressing pointer.  Commonly used as a
 *  software frame/stack pointer by C compilers.
 *
 *    FSR2L  Bit 7:0 - Low byte of pointer
 *    FSR2H  Bit 5:0 - High byte of pointer
 * ========================================================================= */

#define FSR2L_ADDR 0x04D9u
#define FSR2H_ADDR 0x04DAu
#define FSR2_ADDR FSR2L_ADDR

#define FSR2L SFR8(FSR2L_ADDR)
#define FSR2H SFR8(FSR2H_ADDR)
#define FSR2 SFR16(FSR2_ADDR)

#define FSR2H_MASK 0x3Fu /**< Valid high-byte bits [5:0]       */

/* ============================================================================
 *  Indirect Addressing Virtual Registers
 * ============================================================================
 *  Each FSR (0/1/2) has five associated virtual registers that perform
 *  indirect memory access with optional pointer auto-modification:
 *
 *    INDFn    - Read/write using FSRn as-is (no modification)
 *    PLUSWn   - Read/write using FSRn + WREG (signed offset, no modify)
 *    PREINCn  - Increment FSRn first, then read/write through it
 *    POSTDECn - Read/write through FSRn, then decrement it
 *    POSTINCn - Read/write through FSRn, then increment it
 *
 *  These are not physical registers; they are hardware-decoded access
 *  modes.  Writing to or reading from the address triggers the FSR
 *  side-effect.
 * ========================================================================= */

/* -- FSR0 indirect modes -------------------------------------------------- */
#define PLUSW0_ADDR 0x04EBu   /**< FSR0 + WREG offset (no modify)  */
#define PREINC0_ADDR 0x04ECu  /**< ++FSR0, then indirect access     */
#define POSTDEC0_ADDR 0x04EDu /**< Indirect access, then FSR0--     */
#define POSTINC0_ADDR 0x04EEu /**< Indirect access, then FSR0++     */
#define INDF0_ADDR 0x04EFu    /**< Plain indirect via FSR0          */

#define PLUSW0 SFR8(PLUSW0_ADDR)
#define PREINC0 SFR8(PREINC0_ADDR)
#define POSTDEC0 SFR8(POSTDEC0_ADDR)
#define POSTINC0 SFR8(POSTINC0_ADDR)
#define INDF0 SFR8(INDF0_ADDR)

/* -- FSR1 indirect modes -------------------------------------------------- */
#define PLUSW1_ADDR 0x04E3u
#define PREINC1_ADDR 0x04E4u
#define POSTDEC1_ADDR 0x04E5u
#define POSTINC1_ADDR 0x04E6u
#define INDF1_ADDR 0x04E7u

#define PLUSW1 SFR8(PLUSW1_ADDR)
#define PREINC1 SFR8(PREINC1_ADDR)
#define POSTDEC1 SFR8(POSTDEC1_ADDR)
#define POSTINC1 SFR8(POSTINC1_ADDR)
#define INDF1 SFR8(INDF1_ADDR)

/* -- FSR2 indirect modes -------------------------------------------------- */
#define PLUSW2_ADDR 0x04DBu
#define PREINC2_ADDR 0x04DCu
#define POSTDEC2_ADDR 0x04DDu
#define POSTINC2_ADDR 0x04DEu
#define INDF2_ADDR 0x04DFu

#define PLUSW2 SFR8(PLUSW2_ADDR)
#define PREINC2 SFR8(PREINC2_ADDR)
#define POSTDEC2 SFR8(POSTDEC2_ADDR)
#define POSTINC2 SFR8(POSTINC2_ADDR)
#define INDF2 SFR8(INDF2_ADDR)

/* ============================================================================
 *  PCL / PCLATH / PCLATU - Program Counter
 * ============================================================================
 *  Addresses: PCL = 0x04F9, PCLATH = 0x04FA, PCLATU = 0x04FB
 *
 *  The PIC18 program counter is 21 bits wide (addressing up to 2M bytes
 *  of program memory).  PCL is the directly-readable low byte.  PCLATH
 *  and PCLATU are holding registers (latches):
 *
 *    - A write to PCL simultaneously loads PC[15:8] from PCLATH and
 *      PC[20:16] from PCLATU, performing an atomic 21-bit update.
 *    - A read of PCL simultaneously captures PC[15:8] into PCLATH and
 *      PC[20:16] into PCLATU.
 *
 *    PCL     Bit 7:0 - PC bits 7:0 (directly readable/writable)
 *    PCLATH  Bit 7:0 - Write buffer for PC bits 15:8
 *    PCLATU  Bit 4:0 - Write buffer for PC bits 20:16
 * ========================================================================= */

#define PCL_ADDR 0x04F9u
#define PCLATH_ADDR 0x04FAu
#define PCLATU_ADDR 0x04FBu

#define PCL SFR8(PCL_ADDR)
#define PCLATH SFR8(PCLATH_ADDR)
#define PCLATU SFR8(PCLATU_ADDR)

#define PCLATU_MASK 0x1Fu /**< Valid bits [4:0] of PCLATU       */

/* ============================================================================
 *  STKPTR - Hardware Stack Pointer
 * ============================================================================
 *  Address: 0x04FC   (8-bit, read/write)
 *
 *  Points to the current top-of-stack entry in the 128-deep hardware
 *  return address stack.  The stack is used automatically by CALL, RCALL,
 *  and RETURN/RETFIE instructions.  A value of 0 means the stack is
 *  empty; values 1-127 correspond to valid stack entries.
 *
 *    Bit 7   - Reserved
 *    Bit 6:0 - STKPTR[6:0]  Stack pointer value (0-127)
 * ========================================================================= */

#define STKPTR_ADDR 0x04FCu
#define STKPTR SFR8(STKPTR_ADDR)

#define STKPTR_MASK 0x7Fu /**< Valid stack pointer bits [6:0]   */

/* ============================================================================
 *  TOS - Top-of-Stack Registers
 * ============================================================================
 *  Addresses: TOSL = 0x04FD, TOSH = 0x04FE, TOSU = 0x04FF
 *
 *  Provides read/write access to the return address at the top of the
 *  hardware stack (the entry indicated by STKPTR).  This is a 21-bit
 *  program memory address broken into three bytes.
 *
 *    TOSL   Bit 7:0 - TOS bits 7:0  (PC low byte)
 *    TOSH   Bit 7:0 - TOS bits 15:8 (PC high byte)
 *    TOSU   Bit 4:0 - TOS bits 20:16 (PC upper bits)
 * ========================================================================= */

#define TOSL_ADDR 0x04FDu
#define TOSH_ADDR 0x04FEu
#define TOSU_ADDR 0x04FFu

#define TOSL SFR8(TOSL_ADDR)
#define TOSH SFR8(TOSH_ADDR)
#define TOSU SFR8(TOSU_ADDR)

#define TOSU_MASK 0x1Fu /**< Valid bits [4:0] of TOSU         */

/* ============================================================================
 *  PROD - Product Register  (16-bit)
 * ============================================================================
 *  Addresses: PRODL = 0x04F3, PRODH = 0x04F4
 *
 *  Holds the 16-bit result of the hardware MULTIPLY instruction.
 *  PRODL:PRODH = multiplier * multiplicand.
 *
 *    PRODL  Bit 7:0 - Product low byte (result bits 7:0)
 *    PRODH  Bit 7:0 - Product high byte (result bits 15:8)
 * ========================================================================= */

#define PRODL_ADDR 0x04F3u
#define PRODH_ADDR 0x04F4u
#define PROD_ADDR PRODL_ADDR /**< 16-bit base address     */

#define PRODL SFR8(PRODL_ADDR)
#define PRODH SFR8(PRODH_ADDR)
#define PROD SFR16(PROD_ADDR) /**< 16-bit access to PROD   */

/* ============================================================================
 *  TABLAT - Table Latch Register
 * ============================================================================
 *  Address: 0x04F5   (8-bit, read/write)
 *
 *  Holds the data byte transferred during table read (TBLRD) and table
 *  write (TBLWT) operations.  Table reads from program memory place the
 *  result here; table writes send the value from here to the write latch.
 *
 *    Bit 7:0 - Table latch data byte
 * ========================================================================= */

#define TABLAT_ADDR 0x04F5u
#define TABLAT SFR8(TABLAT_ADDR)

/* ============================================================================
 *  TBLPTR - Table Pointer  (21-bit, 3 bytes)
 * ============================================================================
 *  Addresses: TBLPTRL = 0x04F6, TBLPTRH = 0x04F7, TBLPTRU = 0x04F8
 *
 *  Points to the byte address in program memory for table read/write
 *  operations.  The full 22-bit address space allows access up to 4 MB,
 *  though the PIC18F-Q84 uses only 21 bits (2 MB max).
 *
 *    TBLPTRL  Bit 7:0 - Table pointer bits 7:0
 *    TBLPTRH  Bit 7:0 - Table pointer bits 15:8
 *    TBLPTRU  Bit 5   - TBLPTR21 (bit 21 of table pointer)
 *             Bit 4:0 - Table pointer bits 20:16
 * ========================================================================= */

#define TBLPTRL_ADDR 0x04F6u
#define TBLPTRH_ADDR 0x04F7u
#define TBLPTRU_ADDR 0x04F8u

#define TBLPTRL SFR8(TBLPTRL_ADDR)
#define TBLPTRH SFR8(TBLPTRH_ADDR)
#define TBLPTRU SFR8(TBLPTRU_ADDR)

/* -- TBLPTRU bit definitions ---------------------------------------------- */
#define TBLPTRU_TBLPTR21_POS 5                        /**< Bit 21 of table pointer          */
#define TBLPTRU_TBLPTR21 (1u << TBLPTRU_TBLPTR21_POS) /**< 0x20      */
#define TBLPTRU_MASK 0x3Fu                            /**< All valid bits [5:0]             */

/* ============================================================================
 *  PCON0 - Power Control Register 0  (Reset Cause Flags)
 * ============================================================================
 *  Address: 0x04F0   (8-bit, read/write)
 *
 *  Contains active-low flags that indicate the cause of the most recent
 *  device reset.  Each flag reads 0 when that reset source has occurred.
 *  Software must write 1 to clear (re-arm) a flag after reading it.
 *
 *    Bit 7 - STKOVF   Stack Overflow flag
 *                      0 = stack overflow occurred; 1 = no overflow
 *    Bit 6 - STKUNF   Stack Underflow flag
 *                      0 = stack underflow occurred; 1 = no underflow
 *    Bit 5 - Reserved
 *    Bit 4 - WDTWV    WDT Window Violation flag
 *                      0 = WDT cleared outside window; 1 = no violation
 *    Bit 3 - RWDT     Watchdog Timer Reset flag
 *                      0 = WDT timeout reset occurred; 1 = no WDT reset
 *    Bit 2 - RMCLR    MCLR Reset flag
 *                      0 = MCLR reset occurred; 1 = no MCLR reset
 *    Bit 1 - RI       RESET Instruction flag
 *                      0 = RESET instruction was executed; 1 = not executed
 *    Bit 0 - POR      Power-on Reset flag
 *                      0 = POR occurred; 1 = no POR
 * ========================================================================= */

#define PCON0_ADDR 0x04F0u
#define PCON0 SFR8(PCON0_ADDR)

/* -- Bit positions -------------------------------------------------------- */
#define PCON0_STKOVF_POS 7 /**< Stack overflow flag              */
#define PCON0_STKUNF_POS 6 /**< Stack underflow flag             */
#define PCON0_WDTWV_POS 4  /**< WDT window violation flag        */
#define PCON0_RWDT_POS 3   /**< WDT reset flag                   */
#define PCON0_RMCLR_POS 2  /**< MCLR reset flag                  */
#define PCON0_RI_POS 1     /**< RESET instruction flag           */
#define PCON0_POR_POS 0    /**< Power-on reset flag              */

/* -- Bit masks (all flags are active-low: 0 = event occurred) ------------- */
#define PCON0_STKOVF (1u << PCON0_STKOVF_POS) /**< 0x80 - 0=stack overflow occurred     */
#define PCON0_STKUNF (1u << PCON0_STKUNF_POS) /**< 0x40 - 0=stack underflow occurred    */
#define PCON0_WDTWV (1u << PCON0_WDTWV_POS)   /**< 0x10 - 0=WDT cleared outside window  */
#define PCON0_RWDT (1u << PCON0_RWDT_POS)     /**< 0x08 - 0=WDT timeout reset occurred  */
#define PCON0_RMCLR (1u << PCON0_RMCLR_POS)   /**< 0x04 - 0=MCLR reset occurred         */
#define PCON0_RI (1u << PCON0_RI_POS)         /**< 0x02 - 0=RESET instruction executed  */
#define PCON0_POR (1u << PCON0_POR_POS)       /**< 0x01 - 0=power-on reset occurred     */

/* ============================================================================
 *  PCON1 - Power Control Register 1
 * ============================================================================
 *  Address: 0x04F1   (8-bit, read/write)
 *
 *  Additional active-low reset cause flags.
 *
 *    Bit 7:2 - Reserved
 *    Bit 1   - MEMV    Memory Violation Reset flag
 *                       0 = illegal memory access caused reset; 1 = no violation
 *    Bit 0   - RVREG   Voltage Regulator Reset flag (LDO only)
 *                       0 = VREG brown-out reset occurred; 1 = no VREG reset
 * ========================================================================= */

#define PCON1_ADDR 0x04F1u
#define PCON1 SFR8(PCON1_ADDR)

/* -- Bit positions -------------------------------------------------------- */
#define PCON1_MEMV_POS 1  /**< Memory violation flag            */
#define PCON1_RVREG_POS 0 /**< Voltage regulator reset flag     */

/* -- Bit masks ------------------------------------------------------------ */
#define PCON1_MEMV (1u << PCON1_MEMV_POS)   /**< 0x02 - 0=memory violation reset occurred  */
#define PCON1_RVREG (1u << PCON1_RVREG_POS) /**< 0x01 - 0=VREG brown-out occurred (LDO)    */

/* ============================================================================
 *  CPUDOZE - CPU Doze/Idle Control Register
 * ============================================================================
 *  Address: 0x04F2   (8-bit, read/write)
 *
 *  Controls the CPU power-saving modes:
 *
 *    - Idle mode:  CPU halts, peripherals continue running.  Entry via
 *                  SLEEP instruction when IDLEN = 1.
 *    - Doze mode:  CPU runs at a reduced instruction rate while
 *                  peripherals continue at full speed.
 *
 *    Bit 7   - IDLEN  Idle Enable
 *                     1 = SLEEP instruction enters Idle mode
 *                     0 = SLEEP instruction enters full Sleep mode
 *    Bit 6   - DOZEN  Doze Enable
 *                     1 = CPU runs at doze ratio; peripherals at full speed
 *                     0 = CPU and peripherals at same speed
 *    Bit 5   - ROI    Recover on Interrupt
 *                     1 = Exit Doze/Idle on any interrupt
 *    Bit 4   - DOE    Doze on Exit
 *                     1 = Re-enter Doze mode after ISR returns
 *    Bit 3   - Reserved
 *    Bit 2:0 - DOZE[2:0]  Doze ratio (CPU instruction cycle divider)
 *                     000 = 1:2     (CPU runs at half speed)
 *                     001 = 1:4
 *                     010 = 1:8
 *                     011 = 1:16
 *                     100 = 1:32
 *                     101 = 1:64
 *                     110 = 1:128
 *                     111 = 1:256   (CPU runs at 1/256 speed)
 * ========================================================================= */

#define CPUDOZE_ADDR 0x04F2u
#define CPUDOZE SFR8(CPUDOZE_ADDR)

/* -- Bit positions -------------------------------------------------------- */
#define CPUDOZE_IDLEN_POS 7 /**< Idle Enable                      */
#define CPUDOZE_DOZEN_POS 6 /**< Doze Enable                      */
#define CPUDOZE_ROI_POS 5   /**< Recover on Interrupt             */
#define CPUDOZE_DOE_POS 4   /**< Doze on Exit (re-enter after ISR)*/
#define CPUDOZE_DOZE_POS 0  /**< Doze ratio field base position   */

/* -- Bit masks ------------------------------------------------------------ */
#define CPUDOZE_IDLEN (1u << CPUDOZE_IDLEN_POS) /**< 0x80 - 1=Idle mode on SLEEP       */
#define CPUDOZE_DOZEN (1u << CPUDOZE_DOZEN_POS) /**< 0x40 - 1=Doze mode enabled        */
#define CPUDOZE_ROI (1u << CPUDOZE_ROI_POS)     /**< 0x20 - 1=recover from Doze on IRQ */
#define CPUDOZE_DOE (1u << CPUDOZE_DOE_POS)     /**< 0x10 - 1=re-enter Doze after ISR  */
#define CPUDOZE_DOZE_MASK 0x07u                 /**< Doze ratio bits [2:0]            */

/* -- Doze ratio field values ---------------------------------------------- */
#define CPUDOZE_DOZE_1_2 0x00u   /**< CPU at 1/2 speed                 */
#define CPUDOZE_DOZE_1_4 0x01u   /**< CPU at 1/4 speed                 */
#define CPUDOZE_DOZE_1_8 0x02u   /**< CPU at 1/8 speed                 */
#define CPUDOZE_DOZE_1_16 0x03u  /**< CPU at 1/16 speed                */
#define CPUDOZE_DOZE_1_32 0x04u  /**< CPU at 1/32 speed                */
#define CPUDOZE_DOZE_1_64 0x05u  /**< CPU at 1/64 speed                */
#define CPUDOZE_DOZE_1_128 0x06u /**< CPU at 1/128 speed               */
#define CPUDOZE_DOZE_1_256 0x07u /**< CPU at 1/256 speed               */

/* ============================================================================
 *  Fast-Context-Save Shadow Registers
 * ============================================================================
 *  The PIC18 CPU automatically saves STATUS, WREG, and BSR into shadow
 *  registers on high-priority interrupt entry and restores them on
 *  RETFIE FAST.  This provides single-cycle context save/restore without
 *  software overhead.
 *
 *  STATUS_SHAD, WREG_SHAD, BSR_SHAD:
 *      Written by hardware on interrupt entry.
 *      Read by hardware on RETFIE FAST.
 *      Also readable/writable by software.
 *
 *  PCLATH_SHAD, PCLATU_SHAD:
 *      Shadow copies of the PC latch registers.
 *
 *  FSR0_SHAD, FSR1_SHAD, FSR2_SHAD:
 *      Shadow copies of the three file select register pairs.
 *
 *  PROD_SHAD:
 *      Shadow copy of the product register pair.
 * ========================================================================= */

/* -- STATUS shadow -------------------------------------------------------- */
#define STATUS_SHAD_ADDR 0x0377u
#define STATUS_SHAD SFR8(STATUS_SHAD_ADDR) /**< Shadow of STATUS  */

/* -- WREG shadow ---------------------------------------------------------- */
#define WREG_SHAD_ADDR 0x0378u
#define WREG_SHAD SFR8(WREG_SHAD_ADDR) /**< Shadow of WREG    */

/* -- BSR shadow ----------------------------------------------------------- */
#define BSR_SHAD_ADDR 0x0379u
#define BSR_SHAD SFR8(BSR_SHAD_ADDR) /**< Shadow of BSR     */

/* -- PCLATH / PCLATU shadow ----------------------------------------------- */
#define PCLATH_SHAD_ADDR 0x037Bu
#define PCLATU_SHAD_ADDR 0x037Cu

#define PCLATH_SHAD SFR8(PCLATH_SHAD_ADDR) /**< Shadow of PCLATH  */
#define PCLATU_SHAD SFR8(PCLATU_SHAD_ADDR) /**< Shadow of PCLATU  */

/* -- FSR0 shadow ---------------------------------------------------------- */
#define FSR0L_SHAD_ADDR 0x037Du
#define FSR0H_SHAD_ADDR 0x037Eu
#define FSR0_SHAD_ADDR FSR0L_SHAD_ADDR

#define FSR0L_SHAD SFR8(FSR0L_SHAD_ADDR) /**< Shadow of FSR0L   */
#define FSR0H_SHAD SFR8(FSR0H_SHAD_ADDR) /**< Shadow of FSR0H   */
#define FSR0_SHAD SFR16(FSR0_SHAD_ADDR)  /**< 16-bit shadow     */

/* -- FSR1 shadow ---------------------------------------------------------- */
#define FSR1L_SHAD_ADDR 0x037Fu
#define FSR1H_SHAD_ADDR 0x0380u
#define FSR1_SHAD_ADDR FSR1L_SHAD_ADDR

#define FSR1L_SHAD SFR8(FSR1L_SHAD_ADDR) /**< Shadow of FSR1L   */
#define FSR1H_SHAD SFR8(FSR1H_SHAD_ADDR) /**< Shadow of FSR1H   */
#define FSR1_SHAD SFR16(FSR1_SHAD_ADDR)  /**< 16-bit shadow     */

/* -- FSR2 shadow ---------------------------------------------------------- */
#define FSR2L_SHAD_ADDR 0x0381u
#define FSR2H_SHAD_ADDR 0x0382u
#define FSR2_SHAD_ADDR FSR2L_SHAD_ADDR

#define FSR2L_SHAD SFR8(FSR2L_SHAD_ADDR) /**< Shadow of FSR2L   */
#define FSR2H_SHAD SFR8(FSR2H_SHAD_ADDR) /**< Shadow of FSR2H   */
#define FSR2_SHAD SFR16(FSR2_SHAD_ADDR)  /**< 16-bit shadow     */

/* -- PROD shadow ---------------------------------------------------------- */
#define PRODL_SHAD_ADDR 0x0383u
#define PRODH_SHAD_ADDR 0x0384u
#define PROD_SHAD_ADDR PRODL_SHAD_ADDR

#define PRODL_SHAD SFR8(PRODL_SHAD_ADDR) /**< Shadow of PRODL   */
#define PRODH_SHAD SFR8(PRODH_SHAD_ADDR) /**< Shadow of PRODH   */
#define PROD_SHAD SFR16(PROD_SHAD_ADDR)  /**< 16-bit shadow     */

/* ============================================================================
 *  Context-Save Shadow Registers  (Low-Priority Interrupt)
 * ============================================================================
 *  These registers provide a second shadow layer used for low-priority
 *  interrupt context saving when interrupt priority levels are enabled
 *  (IPEN = 1).
 * ========================================================================= */

#define STATUS_CSHAD_ADDR 0x0374u
#define STATUS_CSHAD SFR8(STATUS_CSHAD_ADDR) /**< Context shadow STATUS */

#define WREG_CSHAD_ADDR 0x0375u
#define WREG_CSHAD SFR8(WREG_CSHAD_ADDR) /**< Context shadow WREG   */

#define BSR_CSHAD_ADDR 0x0376u
#define BSR_CSHAD SFR8(BSR_CSHAD_ADDR) /**< Context shadow BSR    */

/* ============================================================================
 *  SHADCON - Shadow Register Control
 * ============================================================================
 *  Address: 0x0377   (8-bit, read/write)
 *
 *  Controls shadow register load behavior.  Shares address space with
 *  STATUS_SHAD; accessible as a distinct register by the hardware.
 *
 *    Bit 7:1 - Reserved
 *    Bit 0   - SHADLO  Shadow Load Override
 *                       1 = Force shadow registers to load from their
 *                           corresponding source registers
 *                       0 = Normal automatic operation
 * ========================================================================= */

#define SHADCON_ADDR 0x0377u
#define SHADCON SFR8(SHADCON_ADDR)

#define SHADCON_SHADLO_POS 0                      /**< Shadow Load Override bit         */
#define SHADCON_SHADLO (1u << SHADCON_SHADLO_POS) /**< 0x01          */

#endif /* PIC18F_Q84_CPU_H */
