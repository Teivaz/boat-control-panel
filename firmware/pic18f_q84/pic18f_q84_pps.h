/*
 * pic18f_q84_pps.h
 *
 * PPS (Peripheral Pin Select) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * The PPS module allows remapping of digital peripheral inputs and
 * outputs to different I/O pins at run time:
 *
 *   - Output PPS registers select which peripheral signal drives
 *     a given pin.  Each output register is 7 bits wide; write the
 *     PPS output source code to route that peripheral to the pin.
 *
 *   - Input PPS registers select which physical pin feeds a given
 *     peripheral input.  Each input register is 6 bits wide, encoded
 *     as PORT[2:0]:PIN[2:0], where PORT selects the I/O port and
 *     PIN selects the bit number within that port.
 *
 *   - PPSLOCK provides a locking mechanism to prevent accidental
 *     PPS reconfiguration.  An unlock sequence (0x55, 0xAA, then
 *     set PPSLOCKED) is required.  If the PPS1WAY configuration
 *     bit is set, PPS registers can only be unlocked by a device
 *     reset once locked.
 *
 * Pin availability depends on device package:
 *   - 28-pin : PORTA, PORTB, PORTC only
 *   - 40/44-pin : adds PORTD
 *   - 48-pin : adds PORTD, PORTE (RE0-RE2), PORTF
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) macro defined externally.
 */

#ifndef PIC18F_Q84_PPS_H
#define PIC18F_Q84_PPS_H

/* ===================================================================
 * PPSLOCK - PPS Lock Register
 * Address: 0x0200
 *
 * Controls the write-protection of all PPS registers.  The lock
 * sequence requires three consecutive writes to PPSLOCK:
 *   1. Write 0x55
 *   2. Write 0xAA
 *   3. Set PPSLOCKED bit
 * Interrupts should be disabled during the unlock sequence.
 *
 * Once locked, PPS registers cannot be written.  If the PPS1WAY
 * configuration bit is programmed, the lock can only be released
 * by a device reset.
 * ================================================================ */
#define PPSLOCK SFR8(0x0200)

/* Bit 0 - PPSLOCKED: PPS Lock
 *   1 = PPS registers are locked; writes are ignored.
 *   0 = PPS registers are writable.
 * Bits 7:1 are unimplemented, read as 0.                          */
#define PPSLOCK_PPSLOCKED 0

/* ===================================================================
 *
 *   OUTPUT PPS REGISTERS
 *
 *   Each register selects which peripheral output signal drives
 *   the corresponding I/O pin.  Bits 6:0 hold the PPS output
 *   source code; bits 7 is unimplemented.  Writing 0x00 (LATxy)
 *   restores the default direct port latch output.
 *
 * ================================================================ */

/* -------------------------------------------------------------------
 * PORTA Output PPS Registers (0x0201 - 0x0208)
 * Available on all pin counts (28/40/44/48).
 * ---------------------------------------------------------------- */
#define RA0PPS SFR8(0x0201)
#define RA1PPS SFR8(0x0202)
#define RA2PPS SFR8(0x0203)
#define RA3PPS SFR8(0x0204)
#define RA4PPS SFR8(0x0205)
#define RA5PPS SFR8(0x0206)
#define RA6PPS SFR8(0x0207)
#define RA7PPS SFR8(0x0208)

/* -------------------------------------------------------------------
 * PORTB Output PPS Registers (0x0209 - 0x0210)
 * Available on all pin counts (28/40/44/48).
 * ---------------------------------------------------------------- */
#define RB0PPS SFR8(0x0209)
#define RB1PPS SFR8(0x020A)
#define RB2PPS SFR8(0x020B)
#define RB3PPS SFR8(0x020C)
#define RB4PPS SFR8(0x020D)
#define RB5PPS SFR8(0x020E)
#define RB6PPS SFR8(0x020F)
#define RB7PPS SFR8(0x0210)

/* -------------------------------------------------------------------
 * PORTC Output PPS Registers (0x0211 - 0x0218)
 * Available on all pin counts (28/40/44/48).
 * ---------------------------------------------------------------- */
#define RC0PPS SFR8(0x0211)
#define RC1PPS SFR8(0x0212)
#define RC2PPS SFR8(0x0213)
#define RC3PPS SFR8(0x0214)
#define RC4PPS SFR8(0x0215)
#define RC5PPS SFR8(0x0216)
#define RC6PPS SFR8(0x0217)
#define RC7PPS SFR8(0x0218)

/* -------------------------------------------------------------------
 * PORTD Output PPS Registers (0x0219 - 0x0220)
 * Available on 40/44/48-pin devices only.
 * ---------------------------------------------------------------- */
#define RD0PPS SFR8(0x0219)
#define RD1PPS SFR8(0x021A)
#define RD2PPS SFR8(0x021B)
#define RD3PPS SFR8(0x021C)
#define RD4PPS SFR8(0x021D)
#define RD5PPS SFR8(0x021E)
#define RD6PPS SFR8(0x021F)
#define RD7PPS SFR8(0x0220)

/* -------------------------------------------------------------------
 * PORTE Output PPS Registers (0x0221 - 0x0223)
 * Available on 48-pin devices only.  Only RE0-RE2 exist.
 * ---------------------------------------------------------------- */
#define RE0PPS SFR8(0x0221)
#define RE1PPS SFR8(0x0222)
#define RE2PPS SFR8(0x0223)

/* -------------------------------------------------------------------
 * PORTF Output PPS Registers (0x0228 - 0x022F)
 * Available on 48-pin devices only.
 * ---------------------------------------------------------------- */
#define RF0PPS SFR8(0x0229)
#define RF1PPS SFR8(0x022A)
#define RF2PPS SFR8(0x022B)
#define RF3PPS SFR8(0x022C)
#define RF4PPS SFR8(0x022D)
#define RF5PPS SFR8(0x022E)
#define RF6PPS SFR8(0x022F)
#define RF7PPS SFR8(0x0230)

/* ===================================================================
 *
 *   PPS OUTPUT SOURCE CODES
 *
 *   Write one of these values to an output PPS register (e.g.
 *   RA0PPS, RB3PPS, RC7PPS, etc.) to route the corresponding
 *   peripheral output signal to that pin.
 *
 *   Example: RC6PPS = PPS_OUT_U1TX;   // Route UART1 TX to pin RC6
 *            RB4PPS = PPS_OUT_CLC1;   // Route CLC1 output to RB4
 *            RA0PPS = PPS_OUT_LATxy;  // Restore default latch output
 *
 * ================================================================ */

#define PPS_OUT_LATxy 0x00 /* Default: direct port latch output */

/* Configurable Logic Cell (CLC) outputs */
#define PPS_OUT_CLC1 0x01 /* CLC1 output                      */
#define PPS_OUT_CLC2 0x02 /* CLC2 output                      */
#define PPS_OUT_CLC3 0x03 /* CLC3 output                      */
#define PPS_OUT_CLC4 0x04 /* CLC4 output                      */
#define PPS_OUT_CLC5 0x05 /* CLC5 output                      */
#define PPS_OUT_CLC6 0x06 /* CLC6 output                      */
#define PPS_OUT_CLC7 0x07 /* CLC7 output                      */
#define PPS_OUT_CLC8 0x08 /* CLC8 output                      */

/* Complementary Waveform Generator (CWG) outputs */
#define PPS_OUT_CWG1A 0x09 /* CWG1 output A                    */
#define PPS_OUT_CWG1B 0x0A /* CWG1 output B                    */
#define PPS_OUT_CWG1C 0x0B /* CWG1 output C                    */
#define PPS_OUT_CWG1D 0x0C /* CWG1 output D                    */
#define PPS_OUT_CWG2A 0x0D /* CWG2 output A                    */
#define PPS_OUT_CWG2B 0x0E /* CWG2 output B                    */
#define PPS_OUT_CWG2C 0x0F /* CWG2 output C                    */
#define PPS_OUT_CWG2D 0x10 /* CWG2 output D                    */
#define PPS_OUT_CWG3A 0x11 /* CWG3 output A                    */
#define PPS_OUT_CWG3B 0x12 /* CWG3 output B                    */
#define PPS_OUT_CWG3C 0x13 /* CWG3 output C                    */
#define PPS_OUT_CWG3D 0x14 /* CWG3 output D                    */

/* Capture/Compare/PWM (CCP) outputs */
#define PPS_OUT_CCP1 0x15 /* CCP1 output                      */
#define PPS_OUT_CCP2 0x16 /* CCP2 output                      */
#define PPS_OUT_CCP3 0x17 /* CCP3 output                      */

/* 16-bit PWM module outputs */
#define PPS_OUT_PWM1S1P1 0x18 /* PWM1 slice 1, output 1           */
#define PPS_OUT_PWM1S1P2 0x19 /* PWM1 slice 1, output 2           */
#define PPS_OUT_PWM2S1P1 0x1A /* PWM2 slice 1, output 1           */
#define PPS_OUT_PWM2S1P2 0x1B /* PWM2 slice 1, output 2           */
#define PPS_OUT_PWM3S1P1 0x1C /* PWM3 slice 1, output 1           */
#define PPS_OUT_PWM3S1P2 0x1D /* PWM3 slice 1, output 2           */
#define PPS_OUT_PWM4S1P1 0x1E /* PWM4 slice 1, output 1           */
#define PPS_OUT_PWM4S1P2 0x1F /* PWM4 slice 1, output 2           */

/* Numerically Controlled Oscillator (NCO) outputs */
#define PPS_OUT_NCO1 0x20 /* NCO1 output                      */
#define PPS_OUT_NCO2 0x21 /* NCO2 output                      */
#define PPS_OUT_NCO3 0x22 /* NCO3 output                      */

/* Timer outputs */
#define PPS_OUT_TMR0 0x23  /* Timer0 output                    */
#define PPS_OUT_TMR1G 0x24 /* Timer1 gate output               */
#define PPS_OUT_TMR3G 0x25 /* Timer3 gate output               */
#define PPS_OUT_TMR5G 0x26 /* Timer5 gate output               */

/* SPI module outputs */
#define PPS_OUT_SPI1_SCK 0x27 /* SPI1 clock output                */
#define PPS_OUT_SPI1_SDO 0x28 /* SPI1 data output                 */
#define PPS_OUT_SPI1_SS 0x29  /* SPI1 slave select output         */
#define PPS_OUT_SPI2_SCK 0x2A /* SPI2 clock output                */
#define PPS_OUT_SPI2_SDO 0x2B /* SPI2 data output                 */
#define PPS_OUT_SPI2_SS 0x2C  /* SPI2 slave select output         */

/* I2C module outputs */
#define PPS_OUT_I2C1_SCL 0x2D /* I2C1 clock output                */
#define PPS_OUT_I2C1_SDA 0x2E /* I2C1 data output                 */

/* UART1 outputs */
#define PPS_OUT_U1TX 0x2F  /* UART1 transmit data              */
#define PPS_OUT_U1DTR 0x30 /* UART1 data terminal ready        */
#define PPS_OUT_U1RTS 0x31 /* UART1 request to send            */

/* UART2 outputs */
#define PPS_OUT_U2TX 0x32  /* UART2 transmit data              */
#define PPS_OUT_U2DTR 0x33 /* UART2 data terminal ready        */
#define PPS_OUT_U2RTS 0x34 /* UART2 request to send            */

/* UART3 outputs */
#define PPS_OUT_U3TX 0x35  /* UART3 transmit data              */
#define PPS_OUT_U3DTR 0x36 /* UART3 DTR (or DALI_TX)           */
#define PPS_OUT_U3RTS 0x37 /* UART3 request to send            */

/* UART4 outputs */
#define PPS_OUT_U4TX 0x38  /* UART4 transmit data              */
#define PPS_OUT_U4DTR 0x39 /* UART4 data terminal ready        */
#define PPS_OUT_U4RTS 0x3A /* UART4 request to send            */

/* UART5 outputs */
#define PPS_OUT_U5TX 0x3B  /* UART5 transmit data              */
#define PPS_OUT_U5DTR 0x3C /* UART5 data terminal ready        */
#define PPS_OUT_U5RTS 0x3D /* UART5 request to send            */

/* CAN bus output */
#define PPS_OUT_CANTX0 0x3E /* CAN transmit output              */

/* Comparator outputs */
#define PPS_OUT_C1OUT 0x3F /* Comparator 1 output              */
#define PPS_OUT_C2OUT 0x40 /* Comparator 2 output              */

/* Data Signal Modulator output */
#define PPS_OUT_DSM1 0x41 /* DSM1 modulated output            */

/* Reference clock output */
#define PPS_OUT_CLKR 0x42 /* Reference clock output           */

/* ADC guard ring drive outputs */
#define PPS_OUT_ADGRDA 0x43 /* ADC guard ring drive A           */
#define PPS_OUT_ADGRDB 0x44 /* ADC guard ring drive B           */

/* Universal Timer 16-bit outputs */
#define PPS_OUT_TU16A 0x45 /* Universal Timer 16A output       */
#define PPS_OUT_TU16B 0x46 /* Universal Timer 16B output       */

/* ===================================================================
 *
 *   INPUT PPS PIN ENCODING
 *
 *   Each input PPS register holds a 6-bit value that identifies the
 *   physical pin connected to the peripheral input.  The encoding is:
 *
 *       Bits 5:3 = PORT[2:0]  (port select)
 *       Bits 2:0 = PIN[2:0]   (pin number within port)
 *
 *   Port encoding:
 *       000 (0x00) = PORTA       011 (0x18) = PORTD
 *       001 (0x08) = PORTB       100 (0x20) = PORTE
 *       010 (0x10) = PORTC       101 (0x28) = PORTF
 *
 *   Example: To route pin RC6 to a peripheral input:
 *       U1RXPPS = PPS_PIN(C, 6);    // = 0x10 | 6 = 0x16
 *       U1RXPPS = PPS_PORT_C | 6;   // equivalent
 *
 * ================================================================ */

/* Port base values for input PPS pin encoding (bits 5:3)            */
#define PPS_PORT_A 0x00 /* PORT[2:0] = 000                  */
#define PPS_PORT_B 0x08 /* PORT[2:0] = 001                  */
#define PPS_PORT_C 0x10 /* PORT[2:0] = 010                  */
#define PPS_PORT_D 0x18 /* PORT[2:0] = 011 (40/44/48-pin)  */
#define PPS_PORT_E 0x20 /* PORT[2:0] = 100 (48-pin only)   */
#define PPS_PORT_F 0x28 /* PORT[2:0] = 101 (48-pin only)   */

/* PPS_PIN(port, pin) - Construct an input PPS pin selection value.
 *   port : single letter A-F (unquoted)
 *   pin  : pin number 0-7
 *
 * Example: PPS_PIN(B, 4) expands to (PPS_PORT_B | 4) = 0x0C        */
#define PPS_PIN(port, pin) (PPS_PORT_##port | (pin))

/* ===================================================================
 *
 *   INPUT PPS REGISTERS
 *
 *   Each register selects which physical pin feeds the named
 *   peripheral input.  Write a PPS_PIN(port, pin) value or a
 *   (PPS_PORT_x | n) expression to connect the desired pin.
 *
 *   Example: INT0PPS = PPS_PIN(B, 0);  // External INT0 on pin RB0
 *
 * ================================================================ */

/* -------------------------------------------------------------------
 * CAN Bus Input
 * ---------------------------------------------------------------- */
#define CANRXPPS SFR8(0x023D) /* CAN receive data input   */

/* -------------------------------------------------------------------
 * External Interrupt Inputs
 * ---------------------------------------------------------------- */
#define INT0PPS SFR8(0x023E) /* External interrupt 0 pin */
#define INT1PPS SFR8(0x023F) /* External interrupt 1 pin */
#define INT2PPS SFR8(0x0240) /* External interrupt 2 pin */

/* -------------------------------------------------------------------
 * Timer Clock and Gate Inputs
 * ---------------------------------------------------------------- */
#define T0CKIPPS SFR8(0x0241) /* Timer0 clock input       */
#define T1CKIPPS SFR8(0x0242) /* Timer1 clock input       */
#define T1GPPS SFR8(0x0243)   /* Timer1 gate input        */
#define T3CKIPPS SFR8(0x0244) /* Timer3 clock input       */
#define T3GPPS SFR8(0x0245)   /* Timer3 gate input        */
#define T5CKIPPS SFR8(0x0246) /* Timer5 clock input       */
#define T5GPPS SFR8(0x0247)   /* Timer5 gate input        */
#define T2INPPS SFR8(0x0248)  /* Timer2 input             */
#define T4INPPS SFR8(0x0249)  /* Timer4 input             */
#define T6INPPS SFR8(0x024A)  /* Timer6 input             */

/* 0x024A - 0x024B: Reserved                                         */

/* -------------------------------------------------------------------
 * Universal Timer Inputs
 * ---------------------------------------------------------------- */
#define TUIN0PPS SFR8(0x024D) /* Universal Timer input 0  */
#define TUIN1PPS SFR8(0x024E) /* Universal Timer input 1  */

/* -------------------------------------------------------------------
 * Capture/Compare/PWM (CCP) Inputs
 * ---------------------------------------------------------------- */
#define CCP1PPS SFR8(0x024F) /* CCP1 input               */
#define CCP2PPS SFR8(0x0250) /* CCP2 input               */
#define CCP3PPS SFR8(0x0251) /* CCP3 input               */

/* 0x0251 - 0x0252: Reserved                                         */

/* -------------------------------------------------------------------
 * 16-bit PWM External Reset and Input Pins
 * ---------------------------------------------------------------- */
#define PWM1ERSPPS SFR8(0x0253) /* PWM1 external reset input */
#define PWM2ERSPPS SFR8(0x0254) /* PWM2 external reset input */
#define PWM3ERSPPS SFR8(0x0255) /* PWM3 external reset input */
#define PWM4ERSPPS SFR8(0x0256) /* PWM4 external reset input */
#define PWMIN0PPS SFR8(0x0257)  /* PWM input 0              */
#define PWMIN1PPS SFR8(0x0258)  /* PWM input 1              */

/* -------------------------------------------------------------------
 * Signal Measurement Timer (SMT) Inputs
 * ---------------------------------------------------------------- */
#define SMT1WINPPS SFR8(0x0259) /* SMT1 window input        */
#define SMT1SIGPPS SFR8(0x025A) /* SMT1 signal input        */

/* -------------------------------------------------------------------
 * Complementary Waveform Generator (CWG) Inputs
 * ---------------------------------------------------------------- */
#define CWG1PPS SFR8(0x025B) /* CWG1 input               */
#define CWG2PPS SFR8(0x025C) /* CWG2 input               */
#define CWG3PPS SFR8(0x025D) /* CWG3 input               */

/* -------------------------------------------------------------------
 * Data Signal Modulator (DSM) Inputs
 * ---------------------------------------------------------------- */
#define MD1CARLPPS SFR8(0x025E) /* DSM1 carrier low input   */
#define MD1CARHPPS SFR8(0x025F) /* DSM1 carrier high input  */
#define MD1SRCPPS SFR8(0x0260)  /* DSM1 modulation source   */

/* -------------------------------------------------------------------
 * Configurable Logic Cell (CLC) Inputs
 * ---------------------------------------------------------------- */
#define CLCIN0PPS SFR8(0x0261) /* CLC input 0              */
#define CLCIN1PPS SFR8(0x0262) /* CLC input 1              */
#define CLCIN2PPS SFR8(0x0263) /* CLC input 2              */
#define CLCIN3PPS SFR8(0x0264) /* CLC input 3              */
#define CLCIN4PPS SFR8(0x0265) /* CLC input 4              */
#define CLCIN5PPS SFR8(0x0266) /* CLC input 5              */
#define CLCIN6PPS SFR8(0x0267) /* CLC input 6              */
#define CLCIN7PPS SFR8(0x0268) /* CLC input 7              */

/* -------------------------------------------------------------------
 * ADC Auto-Conversion Trigger Input
 * ---------------------------------------------------------------- */
#define ADACTPPS SFR8(0x0269) /* ADC auto-conversion trigger */

/* -------------------------------------------------------------------
 * SPI Module Inputs
 * ---------------------------------------------------------------- */
#define SPI1SCKPPS SFR8(0x026A) /* SPI1 clock input         */
#define SPI1SDIPPS SFR8(0x026B) /* SPI1 data input          */
#define SPI1SSPPS SFR8(0x026C)  /* SPI1 slave select input  */
#define SPI2SCKPPS SFR8(0x026D) /* SPI2 clock input         */
#define SPI2SDIPPS SFR8(0x026E) /* SPI2 data input          */
#define SPI2SSPPS SFR8(0x026F)  /* SPI2 slave select input  */

/* -------------------------------------------------------------------
 * I2C Module Inputs
 * ---------------------------------------------------------------- */
#define I2C1SDAPPS SFR8(0x0270) /* I2C1 data input          */
#define I2C1SCLPPS SFR8(0x0271) /* I2C1 clock input         */

/* -------------------------------------------------------------------
 * UART Module Inputs
 * ---------------------------------------------------------------- */
#define U1RXPPS SFR8(0x0272)  /* UART1 receive data       */
#define U1CTSPPS SFR8(0x0273) /* UART1 clear to send      */
#define U2RXPPS SFR8(0x0274)  /* UART2 receive data       */
#define U2CTSPPS SFR8(0x0275) /* UART2 clear to send      */
#define U3RXPPS SFR8(0x0276)  /* UART3 receive data       */
#define U3CTSPPS SFR8(0x0277) /* UART3 clear to send      */
#define U4RXPPS SFR8(0x0278)  /* UART4 receive data       */
#define U4CTSPPS SFR8(0x0279) /* UART4 clear to send      */
#define U5RXPPS SFR8(0x027A)  /* UART5 receive data       */
#define U5CTSPPS SFR8(0x027B) /* UART5 clear to send      */

#endif /* PIC18F_Q84_PPS_H */
