/**
 * @file pic18f_q84_vic.h
 * @brief PIC18F27/47/57Q84 Vectored Interrupt Controller (VIC) register
 *        definitions.
 *
 * The PIC18F-Q84 VIC supports two priority levels (high and low) with a
 * fixed three-instruction-cycle latency from interrupt request to first
 * ISR instruction.  When multi-vector mode is enabled (MVECEN = 1 in
 * CONFIG3) the hardware indexes into a programmable Interrupt Vector Table
 * (IVT) stored in program memory, allowing each source to branch directly
 * to its own handler without software polling.
 *
 * Registers covered in this header:
 *
 *   IVT Control:
 *     - IVTLOCK  : IVT Lock (prevents accidental IVT relocation)
 *     - IVTAD    : Computed interrupt vector address (3 bytes, read-only)
 *     - IVTBASE  : IVT base address in program memory (3 bytes)
 *
 *   Interrupt Priority:
 *     - IPR0-IPR15  : Per-source priority assignment (1=high, 0=low)
 *
 *   Interrupt Enable:
 *     - PIE0-PIE15  : Per-source interrupt enable (1=enabled)
 *
 *   Interrupt Flags:
 *     - PIR0-PIR15  : Per-source interrupt flag (1=pending)
 *
 * Each PIR/PIE/IPR register set shares the same bit layout: one bit per
 * interrupt source, eight sources per register.  The PIR flag is set by
 * hardware on the triggering event and cleared by software (or by DMA).
 * The PIE bit enables the corresponding source.  The IPR bit selects
 * high (1) or low (0) priority when IPEN = 1.
 *
 * Note: The global and per-priority interrupt enables (GIE, GIEL, IPEN)
 * are in INTCON0, defined in pic18f_q84_cpu.h.
 *
 * All addresses are absolute SFR addresses.  Multi-byte registers are
 * little-endian (low byte at the base address).
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *
 * @note This file is included automatically by pic18f_q84.h.
 *       It relies on SFR8() and SFR16() macros defined there.
 */

#ifndef PIC18F_Q84_VIC_H
#define PIC18F_Q84_VIC_H

/* ****************************************************************************
 *  Section 1 - IVT Control Registers
 * ****************************************************************************/

/* ============================================================================
 *  IVTLOCK - Interrupt Vector Table Lock Register
 * ============================================================================
 *  Address: 0x0459   (8-bit, read/write)
 *
 *  When IVTLOCKED is set to 1, the IVTAD and IVTBASE registers become
 *  read-only and cannot be modified until the next device reset.  This
 *  prevents accidental relocation of the vector table at run-time.
 *
 *  Writing this register requires the Microchip unlock sequence (see
 *  datasheet section on protected registers):
 *    1. Write 0x55 to a designated register
 *    2. Write 0xAA to the same register
 *    3. Set/clear the IVTLOCKED bit
 *
 *    Bit 7:1 - Reserved (read as 0)
 *    Bit 0   - IVTLOCKED  IVT Lock
 *              1 = IVTAD and IVTBASE registers are locked (read-only)
 *              0 = IVTAD and IVTBASE registers are writable
 * ========================================================================= */

#define IVTLOCK_ADDR 0x0459u
#define IVTLOCK SFR8(IVTLOCK_ADDR)

/* -- Bit positions -------------------------------------------------------- */
#define IVTLOCK_IVTLOCKED_POS 0 /**< IVT Lock bit position            */

/* -- Bit masks ------------------------------------------------------------ */
#define IVTLOCK_IVTLOCKED (1u << IVTLOCK_IVTLOCKED_POS) /**< 0x01 - 1=IVT locked until reset */

/* ============================================================================
 *  IVTAD - Interrupt Vector Table Address (Computed Vector)
 * ============================================================================
 *  Addresses: IVTADL = 0x045A, IVTADH = 0x045B, IVTADU = 0x045C
 *  (3-byte, read-only)
 *
 *  When an interrupt is acknowledged, the hardware computes the vector
 *  address for the current interrupt source and places it here.  The
 *  computed address is:
 *
 *      IVTAD = IVTBASE + (2 * interrupt_vector_number)
 *
 *  Each vector table entry is two bytes (a GOTO instruction).  The
 *  IVTAD register is intended for diagnostic/debug use; the hardware
 *  uses it internally to fetch the ISR entry point.
 *
 *    IVTADL  Bit 7:0  - Computed vector address bits 7:0
 *    IVTADH  Bit 7:0  - Computed vector address bits 15:8
 *    IVTADU  Bit 4:0  - Computed vector address bits 20:16
 *            Bit 7:5  - Reserved (read as 0)
 * ========================================================================= */

#define IVTADL_ADDR 0x045Au
#define IVTADH_ADDR 0x045Bu
#define IVTADU_ADDR 0x045Cu
#define IVTAD_ADDR IVTADL_ADDR /**< 16-bit base address     */

#define IVTADL SFR8(IVTADL_ADDR)
#define IVTADH SFR8(IVTADH_ADDR)
#define IVTADU SFR8(IVTADU_ADDR)
#define IVTAD SFR16(IVTAD_ADDR) /**< 16-bit access (low 16 bits) */

#define IVTADU_MASK 0x1Fu /**< Valid bits [4:0] of IVTADU       */

/* ============================================================================
 *  IVTBASE - Interrupt Vector Table Base Address
 * ============================================================================
 *  Addresses: IVTBASEL = 0x045D, IVTBASEH = 0x045E, IVTBASEU = 0x045F
 *  (3-byte, read/write when IVTLOCKED = 0)
 *
 *  Defines the base address of the interrupt vector table in program
 *  memory.  The table must be aligned to a 256-byte boundary (IVTBASEL
 *  should be 0x00), though the hardware only requires 2-byte alignment.
 *
 *  On Power-on Reset, the default IVTBASE value is 0x000008, placing
 *  the IVT immediately after the reset and high/low priority interrupt
 *  vectors at 0x000000, 0x000008.
 *
 *    IVTBASEL  Bit 7:0  - Base address bits 7:0
 *    IVTBASEH  Bit 7:0  - Base address bits 15:8
 *    IVTBASEU  Bit 4:0  - Base address bits 20:16
 *              Bit 7:5  - Reserved (read as 0)
 * ========================================================================= */

#define IVTBASEL_ADDR 0x045Du
#define IVTBASEH_ADDR 0x045Eu
#define IVTBASEU_ADDR 0x045Fu
#define IVTBASE_ADDR IVTBASEL_ADDR /**< 16-bit base address     */

#define IVTBASEL SFR8(IVTBASEL_ADDR)
#define IVTBASEH SFR8(IVTBASEH_ADDR)
#define IVTBASEU SFR8(IVTBASEU_ADDR)
#define IVTBASE SFR16(IVTBASE_ADDR) /**< 16-bit access (low 16 bits) */

#define IVTBASEU_MASK 0x1Fu /**< Valid bits [4:0] of IVTBASEU     */

/* ****************************************************************************
 *  Section 2 - Interrupt Priority Registers (IPR0-IPR15)
 * ****************************************************************************
 *
 *  Addresses: 0x0360-0x036F  (sixteen 8-bit registers)
 *
 *  Each bit selects the priority level for one interrupt source:
 *    1 = High-priority interrupt (serviced via GIEH / GIE)
 *    0 = Low-priority interrupt  (serviced via GIEL, requires IPEN = 1)
 *
 *  When IPEN = 0 (legacy mode), priority bits are ignored and all
 *  interrupts are high-priority.
 *
 *  The bit positions in each IPR register correspond directly to the
 *  same bit positions in the matching PIE and PIR registers (e.g.,
 *  IPR0 bit 7 controls the priority of the same source as PIE0 bit 7
 *  and PIR0 bit 7).
 * ***************************************************************************/

/* -- IPR Register Addresses ----------------------------------------------- */
#define IPR0_ADDR 0x0360u
#define IPR1_ADDR 0x0361u
#define IPR2_ADDR 0x0362u
#define IPR3_ADDR 0x0363u
#define IPR4_ADDR 0x0364u
#define IPR5_ADDR 0x0365u
#define IPR6_ADDR 0x0366u
#define IPR7_ADDR 0x0367u
#define IPR8_ADDR 0x0368u
#define IPR9_ADDR 0x0369u
#define IPR10_ADDR 0x036Au
#define IPR11_ADDR 0x036Bu
#define IPR12_ADDR 0x036Cu
#define IPR13_ADDR 0x036Du
#define IPR14_ADDR 0x036Eu
#define IPR15_ADDR 0x036Fu

/* -- IPR Register Access -------------------------------------------------- */
#define IPR0 SFR8(IPR0_ADDR)
#define IPR1 SFR8(IPR1_ADDR)
#define IPR2 SFR8(IPR2_ADDR)
#define IPR3 SFR8(IPR3_ADDR)
#define IPR4 SFR8(IPR4_ADDR)
#define IPR5 SFR8(IPR5_ADDR)
#define IPR6 SFR8(IPR6_ADDR)
#define IPR7 SFR8(IPR7_ADDR)
#define IPR8 SFR8(IPR8_ADDR)
#define IPR9 SFR8(IPR9_ADDR)
#define IPR10 SFR8(IPR10_ADDR)
#define IPR11 SFR8(IPR11_ADDR)
#define IPR12 SFR8(IPR12_ADDR)
#define IPR13 SFR8(IPR13_ADDR)
#define IPR14 SFR8(IPR14_ADDR)
#define IPR15 SFR8(IPR15_ADDR)

/* ============================================================================
 *  IPR0 - Interrupt Priority Register 0
 * ============================================================================
 *  Address: 0x0360
 *
 *    Bit 7 - IOCIP    : Interrupt-on-Change priority
 *    Bit 6 - OSFIP    : Oscillator Fail priority
 *    Bit 5 - HLVDIP   : High/Low-Voltage Detect priority
 *    Bit 4 - SWINTIP  : Software Interrupt priority
 *    Bit 3 - CANIP    : CAN module priority
 *    Bit 2 - ADIP     : ADC Conversion Complete priority
 *    Bit 1 - ZCDIP    : Zero-Cross Detection priority
 *    Bit 0 - INT0IP   : External Interrupt 0 priority
 * ========================================================================= */

#define IPR0_IOCIP_POS 7
#define IPR0_OSFIP_POS 6
#define IPR0_HLVDIP_POS 5
#define IPR0_SWINTIP_POS 4
#define IPR0_CANIP_POS 3
#define IPR0_ADIP_POS 2
#define IPR0_ZCDIP_POS 1
#define IPR0_INT0IP_POS 0

#define IPR0_IOCIP (1u << IPR0_IOCIP_POS)     /**< 0x80 */
#define IPR0_OSFIP (1u << IPR0_OSFIP_POS)     /**< 0x40 */
#define IPR0_HLVDIP (1u << IPR0_HLVDIP_POS)   /**< 0x20 */
#define IPR0_SWINTIP (1u << IPR0_SWINTIP_POS) /**< 0x10 */
#define IPR0_CANIP (1u << IPR0_CANIP_POS)     /**< 0x08 */
#define IPR0_ADIP (1u << IPR0_ADIP_POS)       /**< 0x04 */
#define IPR0_ZCDIP (1u << IPR0_ZCDIP_POS)     /**< 0x02 */
#define IPR0_INT0IP (1u << IPR0_INT0IP_POS)   /**< 0x01 */

/* ============================================================================
 *  IPR1 - Interrupt Priority Register 1
 * ============================================================================
 *  Address: 0x0361
 *
 *    Bit 7 - CLC1IP   : CLC1 priority
 *    Bit 6 - ADCH3IP  : ADC Channel 3 Threshold priority
 *    Bit 5 - ADCH2IP  : ADC Channel 2 Threshold priority
 *    Bit 4 - ADCH1IP  : ADC Channel 1 Threshold priority
 *    Bit 3 - TU16AIP  : Universal Timer 16A priority
 *    Bit 2 - SPI1IP   : SPI1 General priority
 *    Bit 1 - SPI1TXIP : SPI1 Transmit priority
 *    Bit 0 - SPI1RXIP : SPI1 Receive priority
 * ========================================================================= */

#define IPR1_CLC1IP_POS 7
#define IPR1_ADCH3IP_POS 6
#define IPR1_ADCH2IP_POS 5
#define IPR1_ADCH1IP_POS 4
#define IPR1_TU16AIP_POS 3
#define IPR1_SPI1IP_POS 2
#define IPR1_SPI1TXIP_POS 1
#define IPR1_SPI1RXIP_POS 0

#define IPR1_CLC1IP (1u << IPR1_CLC1IP_POS)     /**< 0x80 */
#define IPR1_ADCH3IP (1u << IPR1_ADCH3IP_POS)   /**< 0x40 */
#define IPR1_ADCH2IP (1u << IPR1_ADCH2IP_POS)   /**< 0x20 */
#define IPR1_ADCH1IP (1u << IPR1_ADCH1IP_POS)   /**< 0x10 */
#define IPR1_TU16AIP (1u << IPR1_TU16AIP_POS)   /**< 0x08 */
#define IPR1_SPI1IP (1u << IPR1_SPI1IP_POS)     /**< 0x04 */
#define IPR1_SPI1TXIP (1u << IPR1_SPI1TXIP_POS) /**< 0x02 */
#define IPR1_SPI1RXIP (1u << IPR1_SPI1RXIP_POS) /**< 0x01 */

/* ============================================================================
 *  IPR2 - Interrupt Priority Register 2
 * ============================================================================
 *  Address: 0x0362
 *
 *    Bit 7 - CSWIIP   : Clock Switch priority
 *    Bit 6 - SMT1PWAIP: SMT1 Period/Width Acquisition priority
 *    Bit 5 - SMT1PRAIP: SMT1 Pulse/Running Acq. priority
 *    Bit 4 - SMT1IP   : SMT1 Match priority
 *    Bit 3 - U1EIP    : UART1 Error priority
 *    Bit 2 - SPI2IP   : SPI2 General priority
 *    Bit 1 - SPI2TXIP : SPI2 Transmit priority
 *    Bit 0 - SPI2RXIP : SPI2 Receive priority
 * ========================================================================= */

#define IPR2_CSWIIP_POS 7
#define IPR2_SMT1PWAIP_POS 6
#define IPR2_SMT1PRAIP_POS 5
#define IPR2_SMT1IP_POS 4
#define IPR2_U1EIP_POS 3
#define IPR2_SPI2IP_POS 2
#define IPR2_SPI2TXIP_POS 1
#define IPR2_SPI2RXIP_POS 0

#define IPR2_CSWIIP (1u << IPR2_CSWIIP_POS)       /**< 0x80 */
#define IPR2_SMT1PWAIP (1u << IPR2_SMT1PWAIP_POS) /**< 0x40 */
#define IPR2_SMT1PRAIP (1u << IPR2_SMT1PRAIP_POS) /**< 0x20 */
#define IPR2_SMT1IP (1u << IPR2_SMT1IP_POS)       /**< 0x10 */
#define IPR2_U1EIP (1u << IPR2_U1EIP_POS)         /**< 0x08 */
#define IPR2_SPI2IP (1u << IPR2_SPI2IP_POS)       /**< 0x04 */
#define IPR2_SPI2TXIP (1u << IPR2_SPI2TXIP_POS)   /**< 0x02 */
#define IPR2_SPI2RXIP (1u << IPR2_SPI2RXIP_POS)   /**< 0x01 */

/* ============================================================================
 *  IPR3 - Interrupt Priority Register 3
 * ============================================================================
 *  Address: 0x0363
 *
 *    Bit 7 - DMA1AIP   : DMA1 Abort priority
 *    Bit 6 - DMA1ORIP  : DMA1 Overrun priority
 *    Bit 5 - DMA1DCNTIP: DMA1 Destination Count priority
 *    Bit 4 - DMA1SCNTIP: DMA1 Source Count priority
 *    Bit 3 - ADCH4IP   : ADC Channel 4 Threshold priority
 *    Bit 2 - CM1IP     : Comparator 1 priority
 *    Bit 1 - CWG1IP    : CWG1 priority
 *    Bit 0 - I2C1IP    : I2C1 General priority
 * ========================================================================= */

#define IPR3_DMA1AIP_POS 7
#define IPR3_DMA1ORIP_POS 6
#define IPR3_DMA1DCNTIP_POS 5
#define IPR3_DMA1SCNTIP_POS 4
#define IPR3_ADCH4IP_POS 3
#define IPR3_CM1IP_POS 2
#define IPR3_CWG1IP_POS 1
#define IPR3_I2C1IP_POS 0

#define IPR3_DMA1AIP (1u << IPR3_DMA1AIP_POS)       /**< 0x80 */
#define IPR3_DMA1ORIP (1u << IPR3_DMA1ORIP_POS)     /**< 0x40 */
#define IPR3_DMA1DCNTIP (1u << IPR3_DMA1DCNTIP_POS) /**< 0x20 */
#define IPR3_DMA1SCNTIP (1u << IPR3_DMA1SCNTIP_POS) /**< 0x10 */
#define IPR3_ADCH4IP (1u << IPR3_ADCH4IP_POS)       /**< 0x08 */
#define IPR3_CM1IP (1u << IPR3_CM1IP_POS)           /**< 0x04 */
#define IPR3_CWG1IP (1u << IPR3_CWG1IP_POS)         /**< 0x02 */
#define IPR3_I2C1IP (1u << IPR3_I2C1IP_POS)         /**< 0x01 */

/* ============================================================================
 *  IPR4 - Interrupt Priority Register 4
 * ============================================================================
 *  Address: 0x0364
 *
 *    Bit 7 - TMR0IP   : Timer0 priority
 *    Bit 6 - CCP1IP   : CCP1 priority
 *    Bit 5 - TMR1GIP  : Timer1 Gate priority
 *    Bit 4 - TMR1IP   : Timer1 Overflow priority
 *    Bit 3 - TMR2IP   : Timer2 Match priority
 *    Bit 2 - U1IP     : UART1 General priority
 *    Bit 1 - U1TXIP   : UART1 Transmit priority
 *    Bit 0 - U1RXIP   : UART1 Receive priority
 * ========================================================================= */

#define IPR4_TMR0IP_POS 7
#define IPR4_CCP1IP_POS 6
#define IPR4_TMR1GIP_POS 5
#define IPR4_TMR1IP_POS 4
#define IPR4_TMR2IP_POS 3
#define IPR4_U1IP_POS 2
#define IPR4_U1TXIP_POS 1
#define IPR4_U1RXIP_POS 0

#define IPR4_TMR0IP (1u << IPR4_TMR0IP_POS)   /**< 0x80 */
#define IPR4_CCP1IP (1u << IPR4_CCP1IP_POS)   /**< 0x40 */
#define IPR4_TMR1GIP (1u << IPR4_TMR1GIP_POS) /**< 0x20 */
#define IPR4_TMR1IP (1u << IPR4_TMR1IP_POS)   /**< 0x10 */
#define IPR4_TMR2IP (1u << IPR4_TMR2IP_POS)   /**< 0x08 */
#define IPR4_U1IP (1u << IPR4_U1IP_POS)       /**< 0x04 */
#define IPR4_U1TXIP (1u << IPR4_U1TXIP_POS)   /**< 0x02 */
#define IPR4_U1RXIP (1u << IPR4_U1RXIP_POS)   /**< 0x01 */

/* ============================================================================
 *  IPR5 - Interrupt Priority Register 5
 * ============================================================================
 *  Address: 0x0365
 *
 *    Bit 7 - PWM1IP   : PWM1 Period Match priority
 *    Bit 6 - PWM1PIP  : PWM1 Parameter priority
 *    Bit 5 - CANTIP   : CAN Transmit priority
 *    Bit 4 - CANRIP   : CAN Receive priority
 *    Bit 3 - I2C1EIP  : I2C1 Error priority
 *    Bit 2 - I2C1TXIP : I2C1 Transmit priority
 *    Bit 1 - I2C1RXIP : I2C1 Receive priority
 *    Bit 0 - INT1IP   : External Interrupt 1 priority
 * ========================================================================= */

#define IPR5_PWM1IP_POS 7
#define IPR5_PWM1PIP_POS 6
#define IPR5_CANTIP_POS 5
#define IPR5_CANRIP_POS 4
#define IPR5_I2C1EIP_POS 3
#define IPR5_I2C1TXIP_POS 2
#define IPR5_I2C1RXIP_POS 1
#define IPR5_INT1IP_POS 0

#define IPR5_PWM1IP (1u << IPR5_PWM1IP_POS)     /**< 0x80 */
#define IPR5_PWM1PIP (1u << IPR5_PWM1PIP_POS)   /**< 0x40 */
#define IPR5_CANTIP (1u << IPR5_CANTIP_POS)     /**< 0x20 */
#define IPR5_CANRIP (1u << IPR5_CANRIP_POS)     /**< 0x10 */
#define IPR5_I2C1EIP (1u << IPR5_I2C1EIP_POS)   /**< 0x08 */
#define IPR5_I2C1TXIP (1u << IPR5_I2C1TXIP_POS) /**< 0x04 */
#define IPR5_I2C1RXIP (1u << IPR5_I2C1RXIP_POS) /**< 0x02 */
#define IPR5_INT1IP (1u << IPR5_INT1IP_POS)     /**< 0x01 */

/* ============================================================================
 *  IPR6 - Interrupt Priority Register 6
 * ============================================================================
 *  Address: 0x0366
 *
 *    Bit 7 - PWM2IP   : PWM2 Period Match priority
 *    Bit 6 - PWM2PIP  : PWM2 Parameter priority
 *    Bit 5 - TMR3GIP  : Timer3 Gate priority
 *    Bit 4 - TMR3IP   : Timer3 Overflow priority
 *    Bit 3 - TU16BIP  : Universal Timer 16B priority
 *    Bit 2 - U2IP     : UART2 General priority
 *    Bit 1 - U2TXIP   : UART2 Transmit priority
 *    Bit 0 - U2RXIP   : UART2 Receive priority
 * ========================================================================= */

#define IPR6_PWM2IP_POS 7
#define IPR6_PWM2PIP_POS 6
#define IPR6_TMR3GIP_POS 5
#define IPR6_TMR3IP_POS 4
#define IPR6_TU16BIP_POS 3
#define IPR6_U2IP_POS 2
#define IPR6_U2TXIP_POS 1
#define IPR6_U2RXIP_POS 0

#define IPR6_PWM2IP (1u << IPR6_PWM2IP_POS)   /**< 0x80 */
#define IPR6_PWM2PIP (1u << IPR6_PWM2PIP_POS) /**< 0x40 */
#define IPR6_TMR3GIP (1u << IPR6_TMR3GIP_POS) /**< 0x20 */
#define IPR6_TMR3IP (1u << IPR6_TMR3IP_POS)   /**< 0x10 */
#define IPR6_TU16BIP (1u << IPR6_TU16BIP_POS) /**< 0x08 */
#define IPR6_U2IP (1u << IPR6_U2IP_POS)       /**< 0x04 */
#define IPR6_U2TXIP (1u << IPR6_U2TXIP_POS)   /**< 0x02 */
#define IPR6_U2RXIP (1u << IPR6_U2RXIP_POS)   /**< 0x01 */

/* ============================================================================
 *  IPR7 - Interrupt Priority Register 7
 * ============================================================================
 *  Address: 0x0367
 *
 *    Bit 7 - DMA2AIP   : DMA2 Abort priority
 *    Bit 6 - DMA2ORIP  : DMA2 Overrun priority
 *    Bit 5 - DMA2DCNTIP: DMA2 Destination Count priority
 *    Bit 4 - DMA2SCNTIP: DMA2 Source Count priority
 *    Bit 3 - NCO1IP    : NCO1 priority
 *    Bit 2 - U2EIP     : UART2 Error priority
 *    Bit 1 - CLC2IP    : CLC2 priority
 *    Bit 0 - INT2IP    : External Interrupt 2 priority
 * ========================================================================= */

#define IPR7_DMA2AIP_POS 7
#define IPR7_DMA2ORIP_POS 6
#define IPR7_DMA2DCNTIP_POS 5
#define IPR7_DMA2SCNTIP_POS 4
#define IPR7_NCO1IP_POS 3
#define IPR7_U2EIP_POS 2
#define IPR7_CLC2IP_POS 1
#define IPR7_INT2IP_POS 0

#define IPR7_DMA2AIP (1u << IPR7_DMA2AIP_POS)       /**< 0x80 */
#define IPR7_DMA2ORIP (1u << IPR7_DMA2ORIP_POS)     /**< 0x40 */
#define IPR7_DMA2DCNTIP (1u << IPR7_DMA2DCNTIP_POS) /**< 0x20 */
#define IPR7_DMA2SCNTIP (1u << IPR7_DMA2SCNTIP_POS) /**< 0x10 */
#define IPR7_NCO1IP (1u << IPR7_NCO1IP_POS)         /**< 0x08 */
#define IPR7_U2EIP (1u << IPR7_U2EIP_POS)           /**< 0x04 */
#define IPR7_CLC2IP (1u << IPR7_CLC2IP_POS)         /**< 0x02 */
#define IPR7_INT2IP (1u << IPR7_INT2IP_POS)         /**< 0x01 */

/* ============================================================================
 *  IPR8 - Interrupt Priority Register 8
 * ============================================================================
 *  Address: 0x0368
 *
 *    Bit 7 - CLC3IP   : CLC3 priority
 *    Bit 6 - CCP2IP   : CCP2 priority
 *    Bit 5 - TMR5GIP  : Timer5 Gate priority
 *    Bit 4 - TMR5IP   : Timer5 Overflow priority
 *    Bit 3 - TMR4IP   : Timer4 Match priority
 *    Bit 2 - CWG2IP   : CWG2 priority
 *    Bit 1 - CM2IP    : Comparator 2 priority
 *    Bit 0 - NCO2IP   : NCO2 priority
 * ========================================================================= */

#define IPR8_CLC3IP_POS 7
#define IPR8_CCP2IP_POS 6
#define IPR8_TMR5GIP_POS 5
#define IPR8_TMR5IP_POS 4
#define IPR8_TMR4IP_POS 3
#define IPR8_CWG2IP_POS 2
#define IPR8_CM2IP_POS 1
#define IPR8_NCO2IP_POS 0

#define IPR8_CLC3IP (1u << IPR8_CLC3IP_POS)   /**< 0x80 */
#define IPR8_CCP2IP (1u << IPR8_CCP2IP_POS)   /**< 0x40 */
#define IPR8_TMR5GIP (1u << IPR8_TMR5GIP_POS) /**< 0x20 */
#define IPR8_TMR5IP (1u << IPR8_TMR5IP_POS)   /**< 0x10 */
#define IPR8_TMR4IP (1u << IPR8_TMR4IP_POS)   /**< 0x08 */
#define IPR8_CWG2IP (1u << IPR8_CWG2IP_POS)   /**< 0x04 */
#define IPR8_CM2IP (1u << IPR8_CM2IP_POS)     /**< 0x02 */
#define IPR8_NCO2IP (1u << IPR8_NCO2IP_POS)   /**< 0x01 */

/* ============================================================================
 *  IPR9 - Interrupt Priority Register 9
 * ============================================================================
 *  Address: 0x0369
 *
 *    Bit 7 - DMA3AIP   : DMA3 Abort priority
 *    Bit 6 - DMA3ORIP  : DMA3 Overrun priority
 *    Bit 5 - DMA3DCNTIP: DMA3 Destination Count priority
 *    Bit 4 - DMA3SCNTIP: DMA3 Source Count priority
 *    Bit 3 - PWM3IP    : PWM3 Period Match priority
 *    Bit 2 - PWM3PIP   : PWM3 Parameter priority
 *    Bit 1 - CWG3IP    : CWG3 priority
 *    Bit 0 - NCO3IP    : NCO3 priority
 * ========================================================================= */

#define IPR9_DMA3AIP_POS 7
#define IPR9_DMA3ORIP_POS 6
#define IPR9_DMA3DCNTIP_POS 5
#define IPR9_DMA3SCNTIP_POS 4
#define IPR9_PWM3IP_POS 3
#define IPR9_PWM3PIP_POS 2
#define IPR9_CWG3IP_POS 1
#define IPR9_NCO3IP_POS 0

#define IPR9_DMA3AIP (1u << IPR9_DMA3AIP_POS)       /**< 0x80 */
#define IPR9_DMA3ORIP (1u << IPR9_DMA3ORIP_POS)     /**< 0x40 */
#define IPR9_DMA3DCNTIP (1u << IPR9_DMA3DCNTIP_POS) /**< 0x20 */
#define IPR9_DMA3SCNTIP (1u << IPR9_DMA3SCNTIP_POS) /**< 0x10 */
#define IPR9_PWM3IP (1u << IPR9_PWM3IP_POS)         /**< 0x08 */
#define IPR9_PWM3PIP (1u << IPR9_PWM3PIP_POS)       /**< 0x04 */
#define IPR9_CWG3IP (1u << IPR9_CWG3IP_POS)         /**< 0x02 */
#define IPR9_NCO3IP (1u << IPR9_NCO3IP_POS)         /**< 0x01 */

/* ============================================================================
 *  IPR10 - Interrupt Priority Register 10
 * ============================================================================
 *  Address: 0x036A
 *
 *    Bit 7 - CLC4IP    : CLC4 priority
 *    Bit 6 - CCP3IP    : CCP3 priority
 *    Bit 5 - U3EIP     : UART3 Error priority
 *    Bit 4 - U3IP      : UART3 General priority
 *    Bit 3 - U3TXIP    : UART3 Transmit priority
 *    Bit 2 - U3RXIP    : UART3 Receive priority
 *    Bit 1 - TMR6IP    : Timer6 Match priority
 *    Bit 0 - DSMIP     : DSM priority
 * ========================================================================= */

#define IPR10_CLC4IP_POS 7
#define IPR10_CCP3IP_POS 6
#define IPR10_U3EIP_POS 5
#define IPR10_U3IP_POS 4
#define IPR10_U3TXIP_POS 3
#define IPR10_U3RXIP_POS 2
#define IPR10_TMR6IP_POS 1
#define IPR10_DSMIP_POS 0

#define IPR10_CLC4IP (1u << IPR10_CLC4IP_POS) /**< 0x80 */
#define IPR10_CCP3IP (1u << IPR10_CCP3IP_POS) /**< 0x40 */
#define IPR10_U3EIP (1u << IPR10_U3EIP_POS)   /**< 0x20 */
#define IPR10_U3IP (1u << IPR10_U3IP_POS)     /**< 0x10 */
#define IPR10_U3TXIP (1u << IPR10_U3TXIP_POS) /**< 0x08 */
#define IPR10_U3RXIP (1u << IPR10_U3RXIP_POS) /**< 0x04 */
#define IPR10_TMR6IP (1u << IPR10_TMR6IP_POS) /**< 0x02 */
#define IPR10_DSMIP (1u << IPR10_DSMIP_POS)   /**< 0x01 */

/* ============================================================================
 *  IPR11 - Interrupt Priority Register 11
 * ============================================================================
 *  Address: 0x036B
 *
 *    Bit 7 - DMA4AIP   : DMA4 Abort priority
 *    Bit 6 - DMA4ORIP  : DMA4 Overrun priority
 *    Bit 5 - DMA4DCNTIP: DMA4 Destination Count priority
 *    Bit 4 - DMA4SCNTIP: DMA4 Source Count priority
 *    Bit 3 - CLC5IP    : CLC5 priority
 *    Bit 2 - CLC6IP    : CLC6 priority
 *    Bit 1 - U4EIP     : UART4 Error priority
 *    Bit 0 - U4IP      : UART4 General priority
 * ========================================================================= */

#define IPR11_DMA4AIP_POS 7
#define IPR11_DMA4ORIP_POS 6
#define IPR11_DMA4DCNTIP_POS 5
#define IPR11_DMA4SCNTIP_POS 4
#define IPR11_CLC5IP_POS 3
#define IPR11_CLC6IP_POS 2
#define IPR11_U4EIP_POS 1
#define IPR11_U4IP_POS 0

#define IPR11_DMA4AIP (1u << IPR11_DMA4AIP_POS)       /**< 0x80 */
#define IPR11_DMA4ORIP (1u << IPR11_DMA4ORIP_POS)     /**< 0x40 */
#define IPR11_DMA4DCNTIP (1u << IPR11_DMA4DCNTIP_POS) /**< 0x20 */
#define IPR11_DMA4SCNTIP (1u << IPR11_DMA4SCNTIP_POS) /**< 0x10 */
#define IPR11_CLC5IP (1u << IPR11_CLC5IP_POS)         /**< 0x08 */
#define IPR11_CLC6IP (1u << IPR11_CLC6IP_POS)         /**< 0x04 */
#define IPR11_U4EIP (1u << IPR11_U4EIP_POS)           /**< 0x02 */
#define IPR11_U4IP (1u << IPR11_U4IP_POS)             /**< 0x01 */

/* ============================================================================
 *  IPR12 - Interrupt Priority Register 12
 * ============================================================================
 *  Address: 0x036C
 *
 *    Bit 7 - U4TXIP    : UART4 Transmit priority
 *    Bit 6 - U4RXIP    : UART4 Receive priority
 *    Bit 5 - DMA5AIP   : DMA5 Abort priority
 *    Bit 4 - DMA5ORIP  : DMA5 Overrun priority
 *    Bit 3 - DMA5DCNTIP: DMA5 Destination Count priority
 *    Bit 2 - DMA5SCNTIP: DMA5 Source Count priority
 *    Bit 1 - CLC7IP    : CLC7 priority
 *    Bit 0 - CLC8IP    : CLC8 priority
 * ========================================================================= */

#define IPR12_U4TXIP_POS 7
#define IPR12_U4RXIP_POS 6
#define IPR12_DMA5AIP_POS 5
#define IPR12_DMA5ORIP_POS 4
#define IPR12_DMA5DCNTIP_POS 3
#define IPR12_DMA5SCNTIP_POS 2
#define IPR12_CLC7IP_POS 1
#define IPR12_CLC8IP_POS 0

#define IPR12_U4TXIP (1u << IPR12_U4TXIP_POS)         /**< 0x80 */
#define IPR12_U4RXIP (1u << IPR12_U4RXIP_POS)         /**< 0x40 */
#define IPR12_DMA5AIP (1u << IPR12_DMA5AIP_POS)       /**< 0x20 */
#define IPR12_DMA5ORIP (1u << IPR12_DMA5ORIP_POS)     /**< 0x10 */
#define IPR12_DMA5DCNTIP (1u << IPR12_DMA5DCNTIP_POS) /**< 0x08 */
#define IPR12_DMA5SCNTIP (1u << IPR12_DMA5SCNTIP_POS) /**< 0x04 */
#define IPR12_CLC7IP (1u << IPR12_CLC7IP_POS)         /**< 0x02 */
#define IPR12_CLC8IP (1u << IPR12_CLC8IP_POS)         /**< 0x01 */

/* ============================================================================
 *  IPR13 - Interrupt Priority Register 13
 * ============================================================================
 *  Address: 0x036D
 *
 *    Bit 7 - DMA6AIP   : DMA6 Abort priority
 *    Bit 6 - DMA6ORIP  : DMA6 Overrun priority
 *    Bit 5 - DMA6DCNTIP: DMA6 Destination Count priority
 *    Bit 4 - DMA6SCNTIP: DMA6 Source Count priority
 *    Bit 3 - U5EIP     : UART5 Error priority
 *    Bit 2 - U5IP      : UART5 General priority
 *    Bit 1 - U5TXIP    : UART5 Transmit priority
 *    Bit 0 - U5RXIP    : UART5 Receive priority
 * ========================================================================= */

#define IPR13_DMA6AIP_POS 7
#define IPR13_DMA6ORIP_POS 6
#define IPR13_DMA6DCNTIP_POS 5
#define IPR13_DMA6SCNTIP_POS 4
#define IPR13_U5EIP_POS 3
#define IPR13_U5IP_POS 2
#define IPR13_U5TXIP_POS 1
#define IPR13_U5RXIP_POS 0

#define IPR13_DMA6AIP (1u << IPR13_DMA6AIP_POS)       /**< 0x80 */
#define IPR13_DMA6ORIP (1u << IPR13_DMA6ORIP_POS)     /**< 0x40 */
#define IPR13_DMA6DCNTIP (1u << IPR13_DMA6DCNTIP_POS) /**< 0x20 */
#define IPR13_DMA6SCNTIP (1u << IPR13_DMA6SCNTIP_POS) /**< 0x10 */
#define IPR13_U5EIP (1u << IPR13_U5EIP_POS)           /**< 0x08 */
#define IPR13_U5IP (1u << IPR13_U5IP_POS)             /**< 0x04 */
#define IPR13_U5TXIP (1u << IPR13_U5TXIP_POS)         /**< 0x02 */
#define IPR13_U5RXIP (1u << IPR13_U5RXIP_POS)         /**< 0x01 */

/* ============================================================================
 *  IPR14 - Interrupt Priority Register 14
 * ============================================================================
 *  Address: 0x036E
 *
 *    Bit 7 - DMA7AIP   : DMA7 Abort priority
 *    Bit 6 - DMA7ORIP  : DMA7 Overrun priority
 *    Bit 5 - DMA7DCNTIP: DMA7 Destination Count priority
 *    Bit 4 - DMA7SCNTIP: DMA7 Source Count priority
 *    Bit 3 - CRCIP     : CRC priority
 *    Bit 2 - SCANIP    : NVM Scanner priority
 *    Bit 1 - ACTIP     : Active Clock Tuning priority
 *    Bit 0 - NVMIP     : NVM Write Complete priority
 * ========================================================================= */

#define IPR14_DMA7AIP_POS 7
#define IPR14_DMA7ORIP_POS 6
#define IPR14_DMA7DCNTIP_POS 5
#define IPR14_DMA7SCNTIP_POS 4
#define IPR14_CRCIP_POS 3
#define IPR14_SCANIP_POS 2
#define IPR14_ACTIP_POS 1
#define IPR14_NVMIP_POS 0

#define IPR14_DMA7AIP (1u << IPR14_DMA7AIP_POS)       /**< 0x80 */
#define IPR14_DMA7ORIP (1u << IPR14_DMA7ORIP_POS)     /**< 0x40 */
#define IPR14_DMA7DCNTIP (1u << IPR14_DMA7DCNTIP_POS) /**< 0x20 */
#define IPR14_DMA7SCNTIP (1u << IPR14_DMA7SCNTIP_POS) /**< 0x10 */
#define IPR14_CRCIP (1u << IPR14_CRCIP_POS)           /**< 0x08 */
#define IPR14_SCANIP (1u << IPR14_SCANIP_POS)         /**< 0x04 */
#define IPR14_ACTIP (1u << IPR14_ACTIP_POS)           /**< 0x02 */
#define IPR14_NVMIP (1u << IPR14_NVMIP_POS)           /**< 0x01 */

/* ============================================================================
 *  IPR15 - Interrupt Priority Register 15
 * ============================================================================
 *  Address: 0x036F
 *
 *    Bit 7 - DMA8AIP   : DMA8 Abort priority
 *    Bit 6 - DMA8ORIP  : DMA8 Overrun priority
 *    Bit 5 - DMA8DCNTIP: DMA8 Destination Count priority
 *    Bit 4 - DMA8SCNTIP: DMA8 Source Count priority
 *    Bit 3:0 - Reserved
 * ========================================================================= */

#define IPR15_DMA8AIP_POS 7
#define IPR15_DMA8ORIP_POS 6
#define IPR15_DMA8DCNTIP_POS 5
#define IPR15_DMA8SCNTIP_POS 4

#define IPR15_DMA8AIP (1u << IPR15_DMA8AIP_POS)       /**< 0x80 */
#define IPR15_DMA8ORIP (1u << IPR15_DMA8ORIP_POS)     /**< 0x40 */
#define IPR15_DMA8DCNTIP (1u << IPR15_DMA8DCNTIP_POS) /**< 0x20 */
#define IPR15_DMA8SCNTIP (1u << IPR15_DMA8SCNTIP_POS) /**< 0x10 */

/* ****************************************************************************
 *  Section 3 - Peripheral Interrupt Enable Registers (PIE0-PIE15)
 * ****************************************************************************
 *
 *  Addresses: 0x049E-0x04AD  (sixteen 8-bit registers)
 *
 *  Each bit enables the interrupt for one peripheral source:
 *    1 = Interrupt enabled for the corresponding source
 *    0 = Interrupt disabled
 *
 *  The bit positions in PIEx correspond directly to the same sources
 *  as PIRx and IPRx.  The suffix convention is "IE" (Interrupt Enable).
 * ***************************************************************************/

/* -- PIE Register Addresses ----------------------------------------------- */
#define PIE0_ADDR 0x049Eu
#define PIE1_ADDR 0x049Fu
#define PIE2_ADDR 0x04A0u
#define PIE3_ADDR 0x04A1u
#define PIE4_ADDR 0x04A2u
#define PIE5_ADDR 0x04A3u
#define PIE6_ADDR 0x04A4u
#define PIE7_ADDR 0x04A5u
#define PIE8_ADDR 0x04A6u
#define PIE9_ADDR 0x04A7u
#define PIE10_ADDR 0x04A8u
#define PIE11_ADDR 0x04A9u
#define PIE12_ADDR 0x04AAu
#define PIE13_ADDR 0x04ABu
#define PIE14_ADDR 0x04ACu
#define PIE15_ADDR 0x04ADu

/* -- PIE Register Access -------------------------------------------------- */
#define PIE0 SFR8(PIE0_ADDR)
#define PIE1 SFR8(PIE1_ADDR)
#define PIE2 SFR8(PIE2_ADDR)
#define PIE3 SFR8(PIE3_ADDR)
#define PIE4 SFR8(PIE4_ADDR)
#define PIE5 SFR8(PIE5_ADDR)
#define PIE6 SFR8(PIE6_ADDR)
#define PIE7 SFR8(PIE7_ADDR)
#define PIE8 SFR8(PIE8_ADDR)
#define PIE9 SFR8(PIE9_ADDR)
#define PIE10 SFR8(PIE10_ADDR)
#define PIE11 SFR8(PIE11_ADDR)
#define PIE12 SFR8(PIE12_ADDR)
#define PIE13 SFR8(PIE13_ADDR)
#define PIE14 SFR8(PIE14_ADDR)
#define PIE15 SFR8(PIE15_ADDR)

/* ============================================================================
 *  PIE0 / PIR0 Bit Definitions
 * ============================================================================
 *  The PIE and PIR registers share the same bit layout for each register
 *  index.  The suffix changes from "IE" (enable) to "IF" (flag).  Bit
 *  positions are identical for PIE0, PIR0, and IPR0.
 *
 *    Bit 7 - IOC   : Interrupt-on-Change
 *    Bit 6 - OSF   : Oscillator Fail (Fail-Safe Clock Monitor)
 *    Bit 5 - HLVD  : High/Low-Voltage Detect
 *    Bit 4 - SWINT : Software Interrupt
 *    Bit 3 - CAN   : CAN Module
 *    Bit 2 - AD    : ADC Conversion Complete
 *    Bit 1 - ZCD   : Zero-Cross Detection
 *    Bit 0 - INT0  : External Interrupt 0
 * ========================================================================= */

/* -- PIE0 bit masks (Interrupt Enable) ------------------------------------ */
#define PIE0_IOCIE_POS 7
#define PIE0_OSFIE_POS 6
#define PIE0_HLVDIE_POS 5
#define PIE0_SWINTIE_POS 4
#define PIE0_CANIE_POS 3
#define PIE0_ADIE_POS 2
#define PIE0_ZCDIE_POS 1
#define PIE0_INT0IE_POS 0

#define PIE0_IOCIE (1u << PIE0_IOCIE_POS)     /**< 0x80 */
#define PIE0_OSFIE (1u << PIE0_OSFIE_POS)     /**< 0x40 */
#define PIE0_HLVDIE (1u << PIE0_HLVDIE_POS)   /**< 0x20 */
#define PIE0_SWINTIE (1u << PIE0_SWINTIE_POS) /**< 0x10 */
#define PIE0_CANIE (1u << PIE0_CANIE_POS)     /**< 0x08 */
#define PIE0_ADIE (1u << PIE0_ADIE_POS)       /**< 0x04 */
#define PIE0_ZCDIE (1u << PIE0_ZCDIE_POS)     /**< 0x02 */
#define PIE0_INT0IE (1u << PIE0_INT0IE_POS)   /**< 0x01 */

/* ============================================================================
 *  PIE1 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE1_CLC1IE (1u << 7)   /**< CLC1 interrupt enable        */
#define PIE1_ADCH3IE (1u << 6)  /**< ADC Channel 3 Threshold IE   */
#define PIE1_ADCH2IE (1u << 5)  /**< ADC Channel 2 Threshold IE   */
#define PIE1_ADCH1IE (1u << 4)  /**< ADC Channel 1 Threshold IE   */
#define PIE1_TU16AIE (1u << 3)  /**< Universal Timer 16A IE       */
#define PIE1_SPI1IE (1u << 2)   /**< SPI1 General IE              */
#define PIE1_SPI1TXIE (1u << 1) /**< SPI1 Transmit IE             */
#define PIE1_SPI1RXIE (1u << 0) /**< SPI1 Receive IE              */

/* ============================================================================
 *  PIE2 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE2_CSWIIE (1u << 7)    /**< Clock Switch IE              */
#define PIE2_SMT1PWAIE (1u << 6) /**< SMT1 Period/Width Acq. IE    */
#define PIE2_SMT1PRAIE (1u << 5) /**< SMT1 Pulse/Running Acq. IE   */
#define PIE2_SMT1IE (1u << 4)    /**< SMT1 Match IE                */
#define PIE2_U1EIE (1u << 3)     /**< UART1 Error IE               */
#define PIE2_SPI2IE (1u << 2)    /**< SPI2 General IE              */
#define PIE2_SPI2TXIE (1u << 1)  /**< SPI2 Transmit IE             */
#define PIE2_SPI2RXIE (1u << 0)  /**< SPI2 Receive IE              */

/* ============================================================================
 *  PIE3 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE3_DMA1AIE (1u << 7)    /**< DMA1 Abort IE                */
#define PIE3_DMA1ORIE (1u << 6)   /**< DMA1 Overrun IE              */
#define PIE3_DMA1DCNTIE (1u << 5) /**< DMA1 Destination Count IE    */
#define PIE3_DMA1SCNTIE (1u << 4) /**< DMA1 Source Count IE         */
#define PIE3_ADCH4IE (1u << 3)    /**< ADC Channel 4 Threshold IE   */
#define PIE3_CM1IE (1u << 2)      /**< Comparator 1 IE              */
#define PIE3_CWG1IE (1u << 1)     /**< CWG1 IE                      */
#define PIE3_I2C1IE (1u << 0)     /**< I2C1 General IE              */

/* ============================================================================
 *  PIE4 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE4_TMR0IE (1u << 7)  /**< Timer0 IE                    */
#define PIE4_CCP1IE (1u << 6)  /**< CCP1 IE                      */
#define PIE4_TMR1GIE (1u << 5) /**< Timer1 Gate IE               */
#define PIE4_TMR1IE (1u << 4)  /**< Timer1 Overflow IE           */
#define PIE4_TMR2IE (1u << 3)  /**< Timer2 Match IE              */
#define PIE4_U1IE (1u << 2)    /**< UART1 General IE             */
#define PIE4_U1TXIE (1u << 1)  /**< UART1 Transmit IE            */
#define PIE4_U1RXIE (1u << 0)  /**< UART1 Receive IE             */

/* ============================================================================
 *  PIE5 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE5_PWM1IE (1u << 7)   /**< PWM1 Period Match IE         */
#define PIE5_PWM1PIE (1u << 6)  /**< PWM1 Parameter IE            */
#define PIE5_CANTIE (1u << 5)   /**< CAN Transmit IE              */
#define PIE5_CANRIE (1u << 4)   /**< CAN Receive IE               */
#define PIE5_I2C1EIE (1u << 3)  /**< I2C1 Error IE                */
#define PIE5_I2C1TXIE (1u << 2) /**< I2C1 Transmit IE             */
#define PIE5_I2C1RXIE (1u << 1) /**< I2C1 Receive IE              */
#define PIE5_INT1IE (1u << 0)   /**< External Interrupt 1 IE      */

/* ============================================================================
 *  PIE6 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE6_PWM2IE (1u << 7)  /**< PWM2 Period Match IE         */
#define PIE6_PWM2PIE (1u << 6) /**< PWM2 Parameter IE            */
#define PIE6_TMR3GIE (1u << 5) /**< Timer3 Gate IE               */
#define PIE6_TMR3IE (1u << 4)  /**< Timer3 Overflow IE           */
#define PIE6_TU16BIE (1u << 3) /**< Universal Timer 16B IE       */
#define PIE6_U2IE (1u << 2)    /**< UART2 General IE             */
#define PIE6_U2TXIE (1u << 1)  /**< UART2 Transmit IE            */
#define PIE6_U2RXIE (1u << 0)  /**< UART2 Receive IE             */

/* ============================================================================
 *  PIE7 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE7_DMA2AIE (1u << 7)    /**< DMA2 Abort IE                */
#define PIE7_DMA2ORIE (1u << 6)   /**< DMA2 Overrun IE              */
#define PIE7_DMA2DCNTIE (1u << 5) /**< DMA2 Destination Count IE    */
#define PIE7_DMA2SCNTIE (1u << 4) /**< DMA2 Source Count IE         */
#define PIE7_NCO1IE (1u << 3)     /**< NCO1 IE                      */
#define PIE7_U2EIE (1u << 2)      /**< UART2 Error IE               */
#define PIE7_CLC2IE (1u << 1)     /**< CLC2 IE                      */
#define PIE7_INT2IE (1u << 0)     /**< External Interrupt 2 IE      */

/* ============================================================================
 *  PIE8 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE8_CLC3IE (1u << 7)  /**< CLC3 IE                      */
#define PIE8_CCP2IE (1u << 6)  /**< CCP2 IE                      */
#define PIE8_TMR5GIE (1u << 5) /**< Timer5 Gate IE               */
#define PIE8_TMR5IE (1u << 4)  /**< Timer5 Overflow IE           */
#define PIE8_TMR4IE (1u << 3)  /**< Timer4 Match IE              */
#define PIE8_CWG2IE (1u << 2)  /**< CWG2 IE                      */
#define PIE8_CM2IE (1u << 1)   /**< Comparator 2 IE              */
#define PIE8_NCO2IE (1u << 0)  /**< NCO2 IE                      */

/* ============================================================================
 *  PIE9 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE9_DMA3AIE (1u << 7)    /**< DMA3 Abort IE                */
#define PIE9_DMA3ORIE (1u << 6)   /**< DMA3 Overrun IE              */
#define PIE9_DMA3DCNTIE (1u << 5) /**< DMA3 Destination Count IE    */
#define PIE9_DMA3SCNTIE (1u << 4) /**< DMA3 Source Count IE         */
#define PIE9_PWM3IE (1u << 3)     /**< PWM3 Period Match IE         */
#define PIE9_PWM3PIE (1u << 2)    /**< PWM3 Parameter IE            */
#define PIE9_CWG3IE (1u << 1)     /**< CWG3 IE                      */
#define PIE9_NCO3IE (1u << 0)     /**< NCO3 IE                      */

/* ============================================================================
 *  PIE10 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE10_CLC4IE (1u << 7) /**< CLC4 IE                      */
#define PIE10_CCP3IE (1u << 6) /**< CCP3 IE                      */
#define PIE10_U3EIE (1u << 5)  /**< UART3 Error IE               */
#define PIE10_U3IE (1u << 4)   /**< UART3 General IE             */
#define PIE10_U3TXIE (1u << 3) /**< UART3 Transmit IE            */
#define PIE10_U3RXIE (1u << 2) /**< UART3 Receive IE             */
#define PIE10_TMR6IE (1u << 1) /**< Timer6 Match IE              */
#define PIE10_DSMIE (1u << 0)  /**< DSM IE                       */

/* ============================================================================
 *  PIE11 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE11_DMA4AIE (1u << 7)    /**< DMA4 Abort IE                */
#define PIE11_DMA4ORIE (1u << 6)   /**< DMA4 Overrun IE              */
#define PIE11_DMA4DCNTIE (1u << 5) /**< DMA4 Destination Count IE    */
#define PIE11_DMA4SCNTIE (1u << 4) /**< DMA4 Source Count IE         */
#define PIE11_CLC5IE (1u << 3)     /**< CLC5 IE                      */
#define PIE11_CLC6IE (1u << 2)     /**< CLC6 IE                      */
#define PIE11_U4EIE (1u << 1)      /**< UART4 Error IE               */
#define PIE11_U4IE (1u << 0)       /**< UART4 General IE             */

/* ============================================================================
 *  PIE12 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE12_U4TXIE (1u << 7)     /**< UART4 Transmit IE            */
#define PIE12_U4RXIE (1u << 6)     /**< UART4 Receive IE             */
#define PIE12_DMA5AIE (1u << 5)    /**< DMA5 Abort IE                */
#define PIE12_DMA5ORIE (1u << 4)   /**< DMA5 Overrun IE              */
#define PIE12_DMA5DCNTIE (1u << 3) /**< DMA5 Destination Count IE    */
#define PIE12_DMA5SCNTIE (1u << 2) /**< DMA5 Source Count IE         */
#define PIE12_CLC7IE (1u << 1)     /**< CLC7 IE                      */
#define PIE12_CLC8IE (1u << 0)     /**< CLC8 IE                      */

/* ============================================================================
 *  PIE13 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE13_DMA6AIE (1u << 7)    /**< DMA6 Abort IE                */
#define PIE13_DMA6ORIE (1u << 6)   /**< DMA6 Overrun IE              */
#define PIE13_DMA6DCNTIE (1u << 5) /**< DMA6 Destination Count IE    */
#define PIE13_DMA6SCNTIE (1u << 4) /**< DMA6 Source Count IE         */
#define PIE13_U5EIE (1u << 3)      /**< UART5 Error IE               */
#define PIE13_U5IE (1u << 2)       /**< UART5 General IE             */
#define PIE13_U5TXIE (1u << 1)     /**< UART5 Transmit IE            */
#define PIE13_U5RXIE (1u << 0)     /**< UART5 Receive IE             */

/* ============================================================================
 *  PIE14 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE14_DMA7AIE (1u << 7)    /**< DMA7 Abort IE                */
#define PIE14_DMA7ORIE (1u << 6)   /**< DMA7 Overrun IE              */
#define PIE14_DMA7DCNTIE (1u << 5) /**< DMA7 Destination Count IE    */
#define PIE14_DMA7SCNTIE (1u << 4) /**< DMA7 Source Count IE         */
#define PIE14_CRCIE (1u << 3)      /**< CRC IE                       */
#define PIE14_SCANIE (1u << 2)     /**< NVM Scanner IE               */
#define PIE14_ACTIE (1u << 1)      /**< Active Clock Tuning IE       */
#define PIE14_NVMIE (1u << 0)      /**< NVM Write Complete IE        */

/* ============================================================================
 *  PIE15 Bit Definitions (Interrupt Enable)
 * ========================================================================= */
#define PIE15_DMA8AIE (1u << 7)    /**< DMA8 Abort IE                */
#define PIE15_DMA8ORIE (1u << 6)   /**< DMA8 Overrun IE              */
#define PIE15_DMA8DCNTIE (1u << 5) /**< DMA8 Destination Count IE    */
#define PIE15_DMA8SCNTIE (1u << 4) /**< DMA8 Source Count IE         */
/* Bits 3:0 reserved */

/* ****************************************************************************
 *  Section 4 - Peripheral Interrupt Request (Flag) Registers (PIR0-PIR15)
 * ****************************************************************************
 *
 *  Addresses: 0x04AE-0x04BD  (sixteen 8-bit registers)
 *
 *  Each bit indicates a pending interrupt from one peripheral source:
 *    1 = Interrupt condition occurred (flag set by hardware)
 *    0 = No interrupt pending
 *
 *  Most flags must be cleared by software (write 0).  Some flags are
 *  read-only and are cleared automatically by the hardware when the
 *  triggering condition is resolved (e.g., reading a UART data register).
 *
 *  The bit positions in PIRx correspond directly to the same sources
 *  as PIEx and IPRx.  The suffix convention is "IF" (Interrupt Flag).
 * ***************************************************************************/

/* -- PIR Register Addresses ----------------------------------------------- */
#define PIR0_ADDR 0x04AEu
#define PIR1_ADDR 0x04AFu
#define PIR2_ADDR 0x04B0u
#define PIR3_ADDR 0x04B1u
#define PIR4_ADDR 0x04B2u
#define PIR5_ADDR 0x04B3u
#define PIR6_ADDR 0x04B4u
#define PIR7_ADDR 0x04B5u
#define PIR8_ADDR 0x04B6u
#define PIR9_ADDR 0x04B7u
#define PIR10_ADDR 0x04B8u
#define PIR11_ADDR 0x04B9u
#define PIR12_ADDR 0x04BAu
#define PIR13_ADDR 0x04BBu
#define PIR14_ADDR 0x04BCu
#define PIR15_ADDR 0x04BDu

/* -- PIR Register Access -------------------------------------------------- */
#define PIR0 SFR8(PIR0_ADDR)
#define PIR1 SFR8(PIR1_ADDR)
#define PIR2 SFR8(PIR2_ADDR)
#define PIR3 SFR8(PIR3_ADDR)
#define PIR4 SFR8(PIR4_ADDR)
#define PIR5 SFR8(PIR5_ADDR)
#define PIR6 SFR8(PIR6_ADDR)
#define PIR7 SFR8(PIR7_ADDR)
#define PIR8 SFR8(PIR8_ADDR)
#define PIR9 SFR8(PIR9_ADDR)
#define PIR10 SFR8(PIR10_ADDR)
#define PIR11 SFR8(PIR11_ADDR)
#define PIR12 SFR8(PIR12_ADDR)
#define PIR13 SFR8(PIR13_ADDR)
#define PIR14 SFR8(PIR14_ADDR)
#define PIR15 SFR8(PIR15_ADDR)

/* ============================================================================
 *  PIR0 Bit Definitions (Interrupt Flags)
 * ============================================================================
 *  Bit positions are identical to PIE0 / IPR0.
 * ========================================================================= */

#define PIR0_IOCIF_POS 7
#define PIR0_OSFIF_POS 6
#define PIR0_HLVDIF_POS 5
#define PIR0_SWINTIF_POS 4
#define PIR0_CANIF_POS 3
#define PIR0_ADIF_POS 2
#define PIR0_ZCDIF_POS 1
#define PIR0_INT0IF_POS 0

#define PIR0_IOCIF (1u << PIR0_IOCIF_POS)     /**< 0x80 - IOC flag (read-only, cleared by IOC regs)  */
#define PIR0_OSFIF (1u << PIR0_OSFIF_POS)     /**< 0x40 - Oscillator Fail flag                      */
#define PIR0_HLVDIF (1u << PIR0_HLVDIF_POS)   /**< 0x20 - HLVD trip flag                            */
#define PIR0_SWINTIF (1u << PIR0_SWINTIF_POS) /**< 0x10 - Software Interrupt flag                   */
#define PIR0_CANIF (1u << PIR0_CANIF_POS)     /**< 0x08 - CAN module flag                           */
#define PIR0_ADIF (1u << PIR0_ADIF_POS)       /**< 0x04 - ADC Conversion Complete flag              */
#define PIR0_ZCDIF (1u << PIR0_ZCDIF_POS)     /**< 0x02 - Zero-Cross Detection flag                 */
#define PIR0_INT0IF (1u << PIR0_INT0IF_POS)   /**< 0x01 - External Interrupt 0 flag                 */

/* ============================================================================
 *  PIR1 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR1_CLC1IF (1u << 7)   /**< CLC1 interrupt flag          */
#define PIR1_ADCH3IF (1u << 6)  /**< ADC Channel 3 Threshold IF   */
#define PIR1_ADCH2IF (1u << 5)  /**< ADC Channel 2 Threshold IF   */
#define PIR1_ADCH1IF (1u << 4)  /**< ADC Channel 1 Threshold IF   */
#define PIR1_TU16AIF (1u << 3)  /**< Universal Timer 16A IF       */
#define PIR1_SPI1IF (1u << 2)   /**< SPI1 General IF              */
#define PIR1_SPI1TXIF (1u << 1) /**< SPI1 Transmit IF             */
#define PIR1_SPI1RXIF (1u << 0) /**< SPI1 Receive IF              */

/* ============================================================================
 *  PIR2 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR2_CSWIIF (1u << 7)    /**< Clock Switch IF              */
#define PIR2_SMT1PWAIF (1u << 6) /**< SMT1 Period/Width Acq. IF    */
#define PIR2_SMT1PRAIF (1u << 5) /**< SMT1 Pulse/Running Acq. IF   */
#define PIR2_SMT1IF (1u << 4)    /**< SMT1 Match IF                */
#define PIR2_U1EIF (1u << 3)     /**< UART1 Error IF               */
#define PIR2_SPI2IF (1u << 2)    /**< SPI2 General IF              */
#define PIR2_SPI2TXIF (1u << 1)  /**< SPI2 Transmit IF             */
#define PIR2_SPI2RXIF (1u << 0)  /**< SPI2 Receive IF              */

/* ============================================================================
 *  PIR3 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR3_DMA1AIF (1u << 7)    /**< DMA1 Abort IF                */
#define PIR3_DMA1ORIF (1u << 6)   /**< DMA1 Overrun IF              */
#define PIR3_DMA1DCNTIF (1u << 5) /**< DMA1 Destination Count IF    */
#define PIR3_DMA1SCNTIF (1u << 4) /**< DMA1 Source Count IF         */
#define PIR3_ADCH4IF (1u << 3)    /**< ADC Channel 4 Threshold IF   */
#define PIR3_CM1IF (1u << 2)      /**< Comparator 1 IF              */
#define PIR3_CWG1IF (1u << 1)     /**< CWG1 IF                      */
#define PIR3_I2C1IF (1u << 0)     /**< I2C1 General IF              */

/* ============================================================================
 *  PIR4 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR4_TMR0IF (1u << 7)  /**< Timer0 IF                    */
#define PIR4_CCP1IF (1u << 6)  /**< CCP1 IF                      */
#define PIR4_TMR1GIF (1u << 5) /**< Timer1 Gate IF               */
#define PIR4_TMR1IF (1u << 4)  /**< Timer1 Overflow IF           */
#define PIR4_TMR2IF (1u << 3)  /**< Timer2 Match IF              */
#define PIR4_U1IF (1u << 2)    /**< UART1 General IF             */
#define PIR4_U1TXIF (1u << 1)  /**< UART1 Transmit IF            */
#define PIR4_U1RXIF (1u << 0)  /**< UART1 Receive IF             */

/* ============================================================================
 *  PIR5 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR5_PWM1IF (1u << 7)   /**< PWM1 Period Match IF         */
#define PIR5_PWM1PIF (1u << 6)  /**< PWM1 Parameter IF            */
#define PIR5_CANTIF (1u << 5)   /**< CAN Transmit IF              */
#define PIR5_CANRIF (1u << 4)   /**< CAN Receive IF               */
#define PIR5_I2C1EIF (1u << 3)  /**< I2C1 Error IF                */
#define PIR5_I2C1TXIF (1u << 2) /**< I2C1 Transmit IF             */
#define PIR5_I2C1RXIF (1u << 1) /**< I2C1 Receive IF              */
#define PIR5_INT1IF (1u << 0)   /**< External Interrupt 1 IF      */

/* ============================================================================
 *  PIR6 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR6_PWM2IF (1u << 7)  /**< PWM2 Period Match IF         */
#define PIR6_PWM2PIF (1u << 6) /**< PWM2 Parameter IF            */
#define PIR6_TMR3GIF (1u << 5) /**< Timer3 Gate IF               */
#define PIR6_TMR3IF (1u << 4)  /**< Timer3 Overflow IF           */
#define PIR6_TU16BIF (1u << 3) /**< Universal Timer 16B IF       */
#define PIR6_U2IF (1u << 2)    /**< UART2 General IF             */
#define PIR6_U2TXIF (1u << 1)  /**< UART2 Transmit IF            */
#define PIR6_U2RXIF (1u << 0)  /**< UART2 Receive IF             */

/* ============================================================================
 *  PIR7 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR7_DMA2AIF (1u << 7)    /**< DMA2 Abort IF                */
#define PIR7_DMA2ORIF (1u << 6)   /**< DMA2 Overrun IF              */
#define PIR7_DMA2DCNTIF (1u << 5) /**< DMA2 Destination Count IF    */
#define PIR7_DMA2SCNTIF (1u << 4) /**< DMA2 Source Count IF         */
#define PIR7_NCO1IF (1u << 3)     /**< NCO1 IF                      */
#define PIR7_U2EIF (1u << 2)      /**< UART2 Error IF               */
#define PIR7_CLC2IF (1u << 1)     /**< CLC2 IF                      */
#define PIR7_INT2IF (1u << 0)     /**< External Interrupt 2 IF      */

/* ============================================================================
 *  PIR8 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR8_CLC3IF (1u << 7)  /**< CLC3 IF                      */
#define PIR8_CCP2IF (1u << 6)  /**< CCP2 IF                      */
#define PIR8_TMR5GIF (1u << 5) /**< Timer5 Gate IF               */
#define PIR8_TMR5IF (1u << 4)  /**< Timer5 Overflow IF           */
#define PIR8_TMR4IF (1u << 3)  /**< Timer4 Match IF              */
#define PIR8_CWG2IF (1u << 2)  /**< CWG2 IF                      */
#define PIR8_CM2IF (1u << 1)   /**< Comparator 2 IF              */
#define PIR8_NCO2IF (1u << 0)  /**< NCO2 IF                      */

/* ============================================================================
 *  PIR9 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR9_DMA3AIF (1u << 7)    /**< DMA3 Abort IF                */
#define PIR9_DMA3ORIF (1u << 6)   /**< DMA3 Overrun IF              */
#define PIR9_DMA3DCNTIF (1u << 5) /**< DMA3 Destination Count IF    */
#define PIR9_DMA3SCNTIF (1u << 4) /**< DMA3 Source Count IF         */
#define PIR9_PWM3IF (1u << 3)     /**< PWM3 Period Match IF         */
#define PIR9_PWM3PIF (1u << 2)    /**< PWM3 Parameter IF            */
#define PIR9_CWG3IF (1u << 1)     /**< CWG3 IF                      */
#define PIR9_NCO3IF (1u << 0)     /**< NCO3 IF                      */

/* ============================================================================
 *  PIR10 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR10_CLC4IF (1u << 7) /**< CLC4 IF                      */
#define PIR10_CCP3IF (1u << 6) /**< CCP3 IF                      */
#define PIR10_U3EIF (1u << 5)  /**< UART3 Error IF               */
#define PIR10_U3IF (1u << 4)   /**< UART3 General IF             */
#define PIR10_U3TXIF (1u << 3) /**< UART3 Transmit IF            */
#define PIR10_U3RXIF (1u << 2) /**< UART3 Receive IF             */
#define PIR10_TMR6IF (1u << 1) /**< Timer6 Match IF              */
#define PIR10_DSMIF (1u << 0)  /**< DSM IF                       */

/* ============================================================================
 *  PIR11 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR11_DMA4AIF (1u << 7)    /**< DMA4 Abort IF                */
#define PIR11_DMA4ORIF (1u << 6)   /**< DMA4 Overrun IF              */
#define PIR11_DMA4DCNTIF (1u << 5) /**< DMA4 Destination Count IF    */
#define PIR11_DMA4SCNTIF (1u << 4) /**< DMA4 Source Count IF         */
#define PIR11_CLC5IF (1u << 3)     /**< CLC5 IF                      */
#define PIR11_CLC6IF (1u << 2)     /**< CLC6 IF                      */
#define PIR11_U4EIF (1u << 1)      /**< UART4 Error IF               */
#define PIR11_U4IF (1u << 0)       /**< UART4 General IF             */

/* ============================================================================
 *  PIR12 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR12_U4TXIF (1u << 7)     /**< UART4 Transmit IF            */
#define PIR12_U4RXIF (1u << 6)     /**< UART4 Receive IF             */
#define PIR12_DMA5AIF (1u << 5)    /**< DMA5 Abort IF                */
#define PIR12_DMA5ORIF (1u << 4)   /**< DMA5 Overrun IF              */
#define PIR12_DMA5DCNTIF (1u << 3) /**< DMA5 Destination Count IF    */
#define PIR12_DMA5SCNTIF (1u << 2) /**< DMA5 Source Count IF         */
#define PIR12_CLC7IF (1u << 1)     /**< CLC7 IF                      */
#define PIR12_CLC8IF (1u << 0)     /**< CLC8 IF                      */

/* ============================================================================
 *  PIR13 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR13_DMA6AIF (1u << 7)    /**< DMA6 Abort IF                */
#define PIR13_DMA6ORIF (1u << 6)   /**< DMA6 Overrun IF              */
#define PIR13_DMA6DCNTIF (1u << 5) /**< DMA6 Destination Count IF    */
#define PIR13_DMA6SCNTIF (1u << 4) /**< DMA6 Source Count IF         */
#define PIR13_U5EIF (1u << 3)      /**< UART5 Error IF               */
#define PIR13_U5IF (1u << 2)       /**< UART5 General IF             */
#define PIR13_U5TXIF (1u << 1)     /**< UART5 Transmit IF            */
#define PIR13_U5RXIF (1u << 0)     /**< UART5 Receive IF             */

/* ============================================================================
 *  PIR14 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR14_DMA7AIF (1u << 7)    /**< DMA7 Abort IF                */
#define PIR14_DMA7ORIF (1u << 6)   /**< DMA7 Overrun IF              */
#define PIR14_DMA7DCNTIF (1u << 5) /**< DMA7 Destination Count IF    */
#define PIR14_DMA7SCNTIF (1u << 4) /**< DMA7 Source Count IF         */
#define PIR14_CRCIF (1u << 3)      /**< CRC IF                       */
#define PIR14_SCANF (1u << 2)      /**< NVM Scanner IF               */
#define PIR14_ACTIF (1u << 1)      /**< Active Clock Tuning IF       */
#define PIR14_NVMIF (1u << 0)      /**< NVM Write Complete IF        */

/* ============================================================================
 *  PIR15 Bit Definitions (Interrupt Flags)
 * ========================================================================= */
#define PIR15_DMA8AIF (1u << 7)    /**< DMA8 Abort IF                */
#define PIR15_DMA8ORIF (1u << 6)   /**< DMA8 Overrun IF              */
#define PIR15_DMA8DCNTIF (1u << 5) /**< DMA8 Destination Count IF    */
#define PIR15_DMA8SCNTIF (1u << 4) /**< DMA8 Source Count IF         */
/* Bits 3:0 reserved */

/* ****************************************************************************
 *  Section 5 - Software Interrupt
 * ****************************************************************************
 *
 *  The PIC18F-Q84 provides a single software-triggered interrupt via
 *  PIR0.SWINTIF.  Writing 1 to SWINTIF sets the software interrupt flag,
 *  which (if PIE0.SWINTIE is enabled) triggers an interrupt through the
 *  normal VIC path.  The flag must be cleared by software.
 *
 *  No dedicated register is needed; simply write to PIR0:
 *
 *      PIR0 |= PIR0_SWINTIF;   // trigger software interrupt
 *      PIR0 &= ~PIR0_SWINTIF;  // clear software interrupt
 *
 * ***************************************************************************/

#endif /* PIC18F_Q84_VIC_H */
