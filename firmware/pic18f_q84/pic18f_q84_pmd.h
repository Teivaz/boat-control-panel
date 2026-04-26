/*
 * pic18f_q84_pmd.h
 *
 * Peripheral Module Disable (PMD) registers for PIC18F27/47/57Q84 family.
 *
 * The PMD registers allow individual peripheral clock gates to be
 * disabled, reducing power consumption.  Setting a bit to 1 disables
 * the clock to the corresponding peripheral module; the module cannot
 * be used until the bit is cleared.  All PMD bits default to 0 (all
 * peripherals enabled) after any reset.
 *
 * Covers:
 *   - PMD0 : System, IOC, Clock Ref, Scanner, CRC, HLVD, FVR
 *   - PMD1 : SMT1, Timer0-6
 *   - PMD2 : ACT, CAN FD, Universal Timers 16A/16B
 *   - PMD3 : ZCD, Comparators, ADC, DAC1
 *   - PMD4 : NCO1-3, CWG1-3, DSM1
 *   - PMD5 : CCP1-3, PWM1-4
 *   - PMD6 : I2C1, SPI1-2, UART1-5
 *   - PMD7 : CLC1-8
 *   - PMD8 : DMA1-8
 *
 * Reference: DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 8 - Peripheral Module Disable (PMD)
 *
 * Requires: SFR8(addr) macro defined externally (provides volatile
 *           uint8_t access to an absolute SFR address).
 */

#ifndef PIC18F_Q84_PMD_H
#define PIC18F_Q84_PMD_H

/* ===================================================================== */
/*  PMD0 - Peripheral Module Disable 0                                   */
/*  Address: 0x0060                                                      */
/*                                                                       */
/*  Bit 7 - FVRMD:  Fixed Voltage Reference Module Disable               */
/*      1 = FVR module clock disabled, module cannot be used             */
/*      0 = FVR module clock enabled                                     */
/*  Bit 6 - HLVDMD: High/Low-Voltage Detect Module Disable              */
/*      1 = HLVD module clock disabled                                   */
/*      0 = HLVD module clock enabled                                    */
/*  Bit 5 - CRCMD:  CRC Module Disable                                  */
/*      1 = CRC module clock disabled                                    */
/*      0 = CRC module clock enabled                                     */
/*  Bit 4 - SCANMD: Memory Scanner Module Disable                       */
/*      1 = Scanner module clock disabled                                */
/*      0 = Scanner module clock enabled                                 */
/*  Bit 3 - CLKRMD: Reference Clock Output Module Disable               */
/*      1 = CLKR module clock disabled                                   */
/*      0 = CLKR module clock enabled                                    */
/*  Bit 2 - IOCMD:  Interrupt-on-Change Module Disable                  */
/*      1 = IOC module clock disabled                                    */
/*      0 = IOC module clock enabled                                     */
/*  Bit 1 - SYSCMD: System Clock Module Disable                         */
/*      1 = System clock network disabled (CAUTION: halts the CPU)       */
/*      0 = System clock network enabled                                 */
/*  Bit 0 - Reserved, read as 0                                         */
/* ===================================================================== */
#define PMD0 SFR8(0x0060)

#define PMD0_FVRMD (1u << 7)  /* FVR Module Disable               */
#define PMD0_HLVDMD (1u << 6) /* HLVD Module Disable              */
#define PMD0_CRCMD (1u << 5)  /* CRC Module Disable               */
#define PMD0_SCANMD (1u << 4) /* Memory Scanner Module Disable    */
#define PMD0_CLKRMD (1u << 3) /* Reference Clock Module Disable   */
#define PMD0_IOCMD (1u << 2)  /* IOC Module Disable               */
#define PMD0_SYSCMD (1u << 1) /* System Clock Module Disable      */
/* Bit 0: Reserved */

/* ===================================================================== */
/*  PMD1 - Peripheral Module Disable 1                                   */
/*  Address: 0x0061                                                      */
/*                                                                       */
/*  Bit 7 - TMR6MD: Timer6 Module Disable                               */
/*      1 = Timer6 clock disabled    0 = Timer6 clock enabled            */
/*  Bit 6 - TMR5MD: Timer5 Module Disable                               */
/*      1 = Timer5 clock disabled    0 = Timer5 clock enabled            */
/*  Bit 5 - TMR4MD: Timer4 Module Disable                               */
/*      1 = Timer4 clock disabled    0 = Timer4 clock enabled            */
/*  Bit 4 - TMR3MD: Timer3 Module Disable                               */
/*      1 = Timer3 clock disabled    0 = Timer3 clock enabled            */
/*  Bit 3 - TMR2MD: Timer2 Module Disable                               */
/*      1 = Timer2 clock disabled    0 = Timer2 clock enabled            */
/*  Bit 2 - TMR1MD: Timer1 Module Disable                               */
/*      1 = Timer1 clock disabled    0 = Timer1 clock enabled            */
/*  Bit 1 - TMR0MD: Timer0 Module Disable                               */
/*      1 = Timer0 clock disabled    0 = Timer0 clock enabled            */
/*  Bit 0 - SMT1MD: Signal Measurement Timer 1 Module Disable           */
/*      1 = SMT1 clock disabled      0 = SMT1 clock enabled             */
/* ===================================================================== */
#define PMD1 SFR8(0x0061)

#define PMD1_TMR6MD (1u << 7) /* Timer6 Module Disable            */
#define PMD1_TMR5MD (1u << 6) /* Timer5 Module Disable            */
#define PMD1_TMR4MD (1u << 5) /* Timer4 Module Disable            */
#define PMD1_TMR3MD (1u << 4) /* Timer3 Module Disable            */
#define PMD1_TMR2MD (1u << 3) /* Timer2 Module Disable            */
#define PMD1_TMR1MD (1u << 2) /* Timer1 Module Disable            */
#define PMD1_TMR0MD (1u << 1) /* Timer0 Module Disable            */
#define PMD1_SMT1MD (1u << 0) /* SMT1 Module Disable              */

/* ===================================================================== */
/*  PMD2 - Peripheral Module Disable 2                                   */
/*  Address: 0x0062                                                      */
/*                                                                       */
/*  Bit 7 - Reserved, read as 0                                         */
/*  Bit 6 - Reserved, read as 0                                         */
/*  Bit 5 - TU16BMD: Universal Timer 16B Module Disable                 */
/*      1 = TU16B clock disabled     0 = TU16B clock enabled            */
/*  Bit 4 - TU16AMD: Universal Timer 16A Module Disable                 */
/*      1 = TU16A clock disabled     0 = TU16A clock enabled            */
/*  Bit 3 - CANMD:   CAN FD Module Disable                              */
/*      1 = CAN FD clock disabled    0 = CAN FD clock enabled           */
/*  Bit 2 - ACTMD:   Active Clock Tuning Module Disable                 */
/*      1 = ACT clock disabled       0 = ACT clock enabled              */
/*  Bit 1 - Reserved, read as 0                                         */
/*  Bit 0 - Reserved, read as 0                                         */
/* ===================================================================== */
#define PMD2 SFR8(0x0062)

/* Bit 7: Reserved */
/* Bit 6: Reserved */
#define PMD2_TU16BMD (1u << 5) /* Universal Timer 16B Module Disable */
#define PMD2_TU16AMD (1u << 4) /* Universal Timer 16A Module Disable */
#define PMD2_CANMD (1u << 3)   /* CAN FD Module Disable              */
#define PMD2_ACTMD (1u << 2)   /* Active Clock Tuning Module Disable */
/* Bit 1: Reserved */
/* Bit 0: Reserved */

/* ===================================================================== */
/*  PMD3 - Peripheral Module Disable 3                                   */
/*  Address: 0x0063                                                      */
/*                                                                       */
/*  Bit 7 - DAC1MD: DAC1 Module Disable                                 */
/*      1 = DAC1 clock disabled      0 = DAC1 clock enabled             */
/*  Bit 6 - ADCMD:  ADC Module Disable                                  */
/*      1 = ADC clock disabled       0 = ADC clock enabled              */
/*  Bit 5 - Reserved, read as 0                                         */
/*  Bit 4 - Reserved, read as 0                                         */
/*  Bit 3 - C2MD:   Comparator 2 Module Disable                         */
/*      1 = CMP2 clock disabled      0 = CMP2 clock enabled             */
/*  Bit 2 - C1MD:   Comparator 1 Module Disable                         */
/*      1 = CMP1 clock disabled      0 = CMP1 clock enabled             */
/*  Bit 1 - ZCDMD:  Zero-Cross Detect Module Disable                    */
/*      1 = ZCD clock disabled       0 = ZCD clock enabled              */
/*  Bit 0 - Reserved, read as 0                                         */
/* ===================================================================== */
#define PMD3 SFR8(0x0063)

#define PMD3_DAC1MD (1u << 7) /* DAC1 Module Disable              */
#define PMD3_ADCMD (1u << 6)  /* ADC Module Disable               */
/* Bit 5: Reserved */
/* Bit 4: Reserved */
#define PMD3_C2MD (1u << 3)  /* Comparator 2 Module Disable      */
#define PMD3_C1MD (1u << 2)  /* Comparator 1 Module Disable      */
#define PMD3_ZCDMD (1u << 1) /* ZCD Module Disable               */
/* Bit 0: Reserved */

/* ===================================================================== */
/*  PMD4 - Peripheral Module Disable 4                                   */
/*  Address: 0x0064                                                      */
/*                                                                       */
/*  Bit 7 - DSM1MD: Data Signal Modulator 1 Module Disable              */
/*      1 = DSM1 clock disabled      0 = DSM1 clock enabled             */
/*  Bit 6 - Reserved, read as 0                                         */
/*  Bit 5 - CWG3MD: Complementary Waveform Generator 3 Module Disable   */
/*      1 = CWG3 clock disabled      0 = CWG3 clock enabled             */
/*  Bit 4 - CWG2MD: Complementary Waveform Generator 2 Module Disable   */
/*      1 = CWG2 clock disabled      0 = CWG2 clock enabled             */
/*  Bit 3 - CWG1MD: Complementary Waveform Generator 1 Module Disable   */
/*      1 = CWG1 clock disabled      0 = CWG1 clock enabled             */
/*  Bit 2 - NCO3MD: NCO3 Module Disable                                 */
/*      1 = NCO3 clock disabled      0 = NCO3 clock enabled             */
/*  Bit 1 - NCO2MD: NCO2 Module Disable                                 */
/*      1 = NCO2 clock disabled      0 = NCO2 clock enabled             */
/*  Bit 0 - NCO1MD: NCO1 Module Disable                                 */
/*      1 = NCO1 clock disabled      0 = NCO1 clock enabled             */
/* ===================================================================== */
#define PMD4 SFR8(0x0064)

#define PMD4_DSM1MD (1u << 7) /* DSM1 Module Disable              */
/* Bit 6: Reserved */
#define PMD4_CWG3MD (1u << 5) /* CWG3 Module Disable              */
#define PMD4_CWG2MD (1u << 4) /* CWG2 Module Disable              */
#define PMD4_CWG1MD (1u << 3) /* CWG1 Module Disable              */
#define PMD4_NCO3MD (1u << 2) /* NCO3 Module Disable              */
#define PMD4_NCO2MD (1u << 1) /* NCO2 Module Disable              */
#define PMD4_NCO1MD (1u << 0) /* NCO1 Module Disable              */

/* ===================================================================== */
/*  PMD5 - Peripheral Module Disable 5                                   */
/*  Address: 0x0065                                                      */
/*                                                                       */
/*  Bit 7 - PWM4MD: PWM4 Module Disable                                 */
/*      1 = PWM4 clock disabled      0 = PWM4 clock enabled             */
/*  Bit 6 - PWM3MD: PWM3 Module Disable                                 */
/*      1 = PWM3 clock disabled      0 = PWM3 clock enabled             */
/*  Bit 5 - PWM2MD: PWM2 Module Disable                                 */
/*      1 = PWM2 clock disabled      0 = PWM2 clock enabled             */
/*  Bit 4 - PWM1MD: PWM1 Module Disable                                 */
/*      1 = PWM1 clock disabled      0 = PWM1 clock enabled             */
/*  Bit 3 - CCP3MD: CCP3 Module Disable                                 */
/*      1 = CCP3 clock disabled      0 = CCP3 clock enabled             */
/*  Bit 2 - CCP2MD: CCP2 Module Disable                                 */
/*      1 = CCP2 clock disabled      0 = CCP2 clock enabled             */
/*  Bit 1 - CCP1MD: CCP1 Module Disable                                 */
/*      1 = CCP1 clock disabled      0 = CCP1 clock enabled             */
/*  Bit 0 - Reserved, read as 0                                         */
/* ===================================================================== */
#define PMD5 SFR8(0x0065)

#define PMD5_PWM4MD (1u << 7) /* PWM4 Module Disable              */
#define PMD5_PWM3MD (1u << 6) /* PWM3 Module Disable              */
#define PMD5_PWM2MD (1u << 5) /* PWM2 Module Disable              */
#define PMD5_PWM1MD (1u << 4) /* PWM1 Module Disable              */
#define PMD5_CCP3MD (1u << 3) /* CCP3 Module Disable              */
#define PMD5_CCP2MD (1u << 2) /* CCP2 Module Disable              */
#define PMD5_CCP1MD (1u << 1) /* CCP1 Module Disable              */
/* Bit 0: Reserved */

/* ===================================================================== */
/*  PMD6 - Peripheral Module Disable 6                                   */
/*  Address: 0x0066                                                      */
/*                                                                       */
/*  Bit 7 - U5MD:   UART5 Module Disable                                */
/*      1 = UART5 clock disabled     0 = UART5 clock enabled            */
/*  Bit 6 - U4MD:   UART4 Module Disable                                */
/*      1 = UART4 clock disabled     0 = UART4 clock enabled            */
/*  Bit 5 - U3MD:   UART3 Module Disable                                */
/*      1 = UART3 clock disabled     0 = UART3 clock enabled            */
/*  Bit 4 - U2MD:   UART2 Module Disable                                */
/*      1 = UART2 clock disabled     0 = UART2 clock enabled            */
/*  Bit 3 - U1MD:   UART1 Module Disable                                */
/*      1 = UART1 clock disabled     0 = UART1 clock enabled            */
/*  Bit 2 - SPI2MD: SPI2 Module Disable                                 */
/*      1 = SPI2 clock disabled      0 = SPI2 clock enabled             */
/*  Bit 1 - SPI1MD: SPI1 Module Disable                                 */
/*      1 = SPI1 clock disabled      0 = SPI1 clock enabled             */
/*  Bit 0 - I2C1MD: I2C1 Module Disable                                 */
/*      1 = I2C1 clock disabled      0 = I2C1 clock enabled             */
/* ===================================================================== */
#define PMD6 SFR8(0x0066)

#define PMD6_U5MD (1u << 7)   /* UART5 Module Disable             */
#define PMD6_U4MD (1u << 6)   /* UART4 Module Disable             */
#define PMD6_U3MD (1u << 5)   /* UART3 Module Disable             */
#define PMD6_U2MD (1u << 4)   /* UART2 Module Disable             */
#define PMD6_U1MD (1u << 3)   /* UART1 Module Disable             */
#define PMD6_SPI2MD (1u << 2) /* SPI2 Module Disable              */
#define PMD6_SPI1MD (1u << 1) /* SPI1 Module Disable              */
#define PMD6_I2C1MD (1u << 0) /* I2C1 Module Disable              */

/* ===================================================================== */
/*  PMD7 - Peripheral Module Disable 7                                   */
/*  Address: 0x0067                                                      */
/*                                                                       */
/*  Bit 7 - CLC8MD: CLC8 Module Disable                                 */
/*      1 = CLC8 clock disabled      0 = CLC8 clock enabled             */
/*  Bit 6 - CLC7MD: CLC7 Module Disable                                 */
/*      1 = CLC7 clock disabled      0 = CLC7 clock enabled             */
/*  Bit 5 - CLC6MD: CLC6 Module Disable                                 */
/*      1 = CLC6 clock disabled      0 = CLC6 clock enabled             */
/*  Bit 4 - CLC5MD: CLC5 Module Disable                                 */
/*      1 = CLC5 clock disabled      0 = CLC5 clock enabled             */
/*  Bit 3 - CLC4MD: CLC4 Module Disable                                 */
/*      1 = CLC4 clock disabled      0 = CLC4 clock enabled             */
/*  Bit 2 - CLC3MD: CLC3 Module Disable                                 */
/*      1 = CLC3 clock disabled      0 = CLC3 clock enabled             */
/*  Bit 1 - CLC2MD: CLC2 Module Disable                                 */
/*      1 = CLC2 clock disabled      0 = CLC2 clock enabled             */
/*  Bit 0 - CLC1MD: CLC1 Module Disable                                 */
/*      1 = CLC1 clock disabled      0 = CLC1 clock enabled             */
/* ===================================================================== */
#define PMD7 SFR8(0x0067)

#define PMD7_CLC8MD (1u << 7) /* CLC8 Module Disable              */
#define PMD7_CLC7MD (1u << 6) /* CLC7 Module Disable              */
#define PMD7_CLC6MD (1u << 5) /* CLC6 Module Disable              */
#define PMD7_CLC5MD (1u << 4) /* CLC5 Module Disable              */
#define PMD7_CLC4MD (1u << 3) /* CLC4 Module Disable              */
#define PMD7_CLC3MD (1u << 2) /* CLC3 Module Disable              */
#define PMD7_CLC2MD (1u << 1) /* CLC2 Module Disable              */
#define PMD7_CLC1MD (1u << 0) /* CLC1 Module Disable              */

/* ===================================================================== */
/*  PMD8 - Peripheral Module Disable 8                                   */
/*  Address: 0x0068                                                      */
/*                                                                       */
/*  Bit 7 - DMA8MD: DMA Channel 8 Module Disable                        */
/*      1 = DMA8 clock disabled      0 = DMA8 clock enabled             */
/*  Bit 6 - DMA7MD: DMA Channel 7 Module Disable                        */
/*      1 = DMA7 clock disabled      0 = DMA7 clock enabled             */
/*  Bit 5 - DMA6MD: DMA Channel 6 Module Disable                        */
/*      1 = DMA6 clock disabled      0 = DMA6 clock enabled             */
/*  Bit 4 - DMA5MD: DMA Channel 5 Module Disable                        */
/*      1 = DMA5 clock disabled      0 = DMA5 clock enabled             */
/*  Bit 3 - DMA4MD: DMA Channel 4 Module Disable                        */
/*      1 = DMA4 clock disabled      0 = DMA4 clock enabled             */
/*  Bit 2 - DMA3MD: DMA Channel 3 Module Disable                        */
/*      1 = DMA3 clock disabled      0 = DMA3 clock enabled             */
/*  Bit 1 - DMA2MD: DMA Channel 2 Module Disable                        */
/*      1 = DMA2 clock disabled      0 = DMA2 clock enabled             */
/*  Bit 0 - DMA1MD: DMA Channel 1 Module Disable                        */
/*      1 = DMA1 clock disabled      0 = DMA1 clock enabled             */
/* ===================================================================== */
#define PMD8 SFR8(0x0068)

#define PMD8_DMA8MD (1u << 7) /* DMA8 Module Disable              */
#define PMD8_DMA7MD (1u << 6) /* DMA7 Module Disable              */
#define PMD8_DMA6MD (1u << 5) /* DMA6 Module Disable              */
#define PMD8_DMA5MD (1u << 4) /* DMA5 Module Disable              */
#define PMD8_DMA4MD (1u << 3) /* DMA4 Module Disable              */
#define PMD8_DMA3MD (1u << 2) /* DMA3 Module Disable              */
#define PMD8_DMA2MD (1u << 1) /* DMA2 Module Disable              */
#define PMD8_DMA1MD (1u << 0) /* DMA1 Module Disable              */

#endif /* PIC18F_Q84_PMD_H */
