/**
 * @file pic18f_q84.h
 * @brief Master include header for PIC18F27/47/57Q84 microcontroller family.
 *
 * This file includes all subsystem register definition headers for the
 * PIC18F-Q84 family (28/40/44/48-pin, Low-Power, High-Performance MCUs
 * with XLP Technology).
 *
 * Devices covered:
 *   - PIC18F27Q84  (128KB Flash, 12800B SRAM, 1024B EEPROM, 25 I/O, 28-pin)
 *   - PIC18F47Q84  (128KB Flash, 12800B SRAM, 1024B EEPROM, 36 I/O, 40/44-pin)
 *   - PIC18F57Q84  (128KB Flash, 12800B SRAM, 1024B EEPROM, 44 I/O, 48-pin)
 *
 * Operating voltage:  1.8V to 5.5V
 * Max clock speed:    64 MHz (62.5 ns instruction cycle)
 * Architecture:       C-compiler optimized 8-bit RISC (PIC18)
 *
 * Reference: Microchip DS40002213D
 *
 * @note All register addresses are absolute SFR addresses.
 *       Multi-byte registers are little-endian (low byte at base address).
 *       Reserved bits should be written as 0 and read values ignored.
 */

#ifndef PIC18F_Q84_H
#define PIC18F_Q84_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Register access macros
 * -------------------------------------------------------------------------*/

/** Access an 8-bit Special Function Register by absolute address */
#define SFR8(addr) (*(volatile uint8_t*)(addr))
/** Access a 16-bit Special Function Register pair (little-endian) */
#define SFR16(addr) (*(volatile uint16_t*)(addr))
/** Access a 32-bit Special Function Register group (little-endian) */
#define SFR32(addr) (*(volatile uint32_t*)(addr))

/* ---------------------------------------------------------------------------
 * Subsystem headers
 * -------------------------------------------------------------------------*/

#include "pic18f_q84_adc.h"        /* ADC - 12-bit ADC w/ Computation & Ctx    */
#include "pic18f_q84_can.h"        /* CAN FD - CAN Flexible Data-Rate          */
#include "pic18f_q84_ccp.h"        /* CCP - Capture/Compare/PWM (x3)           */
#include "pic18f_q84_clc.h"        /* CLC - Configurable Logic Cell (x8)       */
#include "pic18f_q84_clkref.h"     /* CLKREF - Reference Clock Output          */
#include "pic18f_q84_cmp.h"        /* CMP - Comparator (x2)                    */
#include "pic18f_q84_config.h"     /* Device Configuration Words               */
#include "pic18f_q84_cpu.h"        /* PIC18 CPU core registers                 */
#include "pic18f_q84_crc.h"        /* CRC - Cyclic Redundancy Check w/ Scanner */
#include "pic18f_q84_cwg.h"        /* CWG - Complementary Waveform Gen (x3)    */
#include "pic18f_q84_dac.h"        /* DAC - 8-bit Digital-to-Analog Converter  */
#include "pic18f_q84_dma.h"        /* DMA - Direct Memory Access (8 channels)  */
#include "pic18f_q84_dsm.h"        /* DSM - Data Signal Modulator              */
#include "pic18f_q84_fvr.h"        /* FVR - Fixed Voltage Reference            */
#include "pic18f_q84_hlvd.h"       /* HLVD - High/Low-Voltage Detect           */
#include "pic18f_q84_i2c.h"        /* I2C - Inter-Integrated Circuit           */
#include "pic18f_q84_intrinsics.h" /* Chip-specific instruction intrinsics     */
#include "pic18f_q84_ioc.h"        /* IOC - Interrupt-on-Change                */
#include "pic18f_q84_nco.h"        /* NCO - Numerically Controlled Osc (x3)    */
#include "pic18f_q84_nvm.h"        /* NVM - Nonvolatile Memory (Flash/EEPROM)  */
#include "pic18f_q84_osc.h"        /* OSC - Oscillator with Fail-Safe Monitor  */
#include "pic18f_q84_pmd.h"        /* PMD - Peripheral Module Disable          */
#include "pic18f_q84_port.h"       /* I/O Ports (PORTA-PORTF, LAT, TRIS, etc.) */
#include "pic18f_q84_pps.h"        /* PPS - Peripheral Pin Select              */
#include "pic18f_q84_pwm.h"        /* PWM - 16-bit Pulse-Width Modulator (x4)  */
#include "pic18f_q84_reset.h"      /* Resets, BOR, VREG                        */
#include "pic18f_q84_smt.h"        /* SMT - Signal Measurement Timer (24-bit)  */
#include "pic18f_q84_spi.h"        /* SPI - Serial Peripheral Interface (x2)   */
#include "pic18f_q84_tmr0.h"       /* TMR0 - Timer0 (16-bit)                   */
#include "pic18f_q84_tmr1.h"       /* TMR1/3/5 - 16-bit Timers with Gate       */
#include "pic18f_q84_tmr2.h"       /* TMR2/4/6 - 8-bit Timers with HLT        */
#include "pic18f_q84_uart.h"       /* UART - UART with Protocol Support (x5)   */
#include "pic18f_q84_utmr.h"       /* UTMR - Universal Timer (TU16A/TU16B)     */
#include "pic18f_q84_vic.h"        /* VIC - Vectored Interrupt Controller      */
#include "pic18f_q84_wwdt.h"       /* WWDT - Windowed Watchdog Timer           */
#include "pic18f_q84_zcd.h"        /* ZCD - Zero-Cross Detection               */

#define set_bit(b, i) b |= (1 << (i))
#define clear_bit(b, i) b &= (~(1 << (i)))

#ifdef __cplusplus
}
#endif

#endif /* PIC18F_Q84_H */
