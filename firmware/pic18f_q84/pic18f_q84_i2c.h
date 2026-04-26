/*
 * pic18f_q84_i2c.h
 *
 * I2C (Inter-Integrated Circuit) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * The device contains one I2C module (I2C1) supporting both host
 * (master) and client (slave) operating modes with a rich feature
 * set for embedded communication.
 *
 * Features:
 *   - Host and Client operating modes
 *   - 7-bit and 10-bit client addressing
 *   - Multi-address match (up to two client addresses with masking)
 *   - SMBus 2.0 and SMBus 3.0 compliance
 *   - Bus timeout detection with programmable timeout period
 *   - Hardware clock stretching support
 *   - Dedicated Address, Transmit, and Receive buffers
 *   - 16-bit byte count register for automatic transfer length
 *   - General Call address recognition
 *   - Bus collision detection
 *   - DMA source/destination support
 *   - Configurable I2C-specific pin characteristics:
 *       * Slew rate control
 *       * Internal pull-up resistor selection
 *       * Input voltage threshold selection (I2C / SMBus)
 *
 * Standard data rates:
 *   - Standard mode:  100 kHz
 *   - Fast mode:      400 kHz (requires FME bit set)
 *   - Fast mode plus: 1 MHz   (requires appropriate pull-ups and
 *                               threshold settings)
 *
 * Baud rate (host mode):
 *   SCL frequency = F_clocksource / (4 * (BAUD + 1))
 *
 * Typical host-mode usage:
 *   1. Configure I2C pin characteristics (RCxI2C / RBxI2C registers)
 *   2. Select clock source via I2C1CLK.CLK[4:0]
 *   3. Set baud rate via I2C1BAUD
 *   4. Set host mode: I2C1CON0.MODE[2:0] = 100 (host mode)
 *   5. Enable: I2C1CON0.EN = 1
 *   6. Load byte count in I2C1CNT
 *   7. Write address+R/W to I2C1ADB1:I2C1ADB0 to initiate transfer
 *   8. Write/read data via I2C1TXB / I2C1RXB
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_I2C_H
#define PIC18F_Q84_I2C_H

/* *******************************************************************
 *
 *  I2C Pin Configuration Registers
 *
 *  These registers control the electrical characteristics of the
 *  I2C-capable pins.  Each register has the same bit layout and
 *  configures slew rate, pull-up resistance, and input voltage
 *  threshold for one specific pin.
 *
 *  The pin assignments are fixed by the device pinout:
 *    RC4I2C => RC4 (SDA1 default)
 *    RC3I2C => RC3 (SCL1 default)
 *    RB2I2C => RB2 (SDA1 alternate)
 *    RB1I2C => RB1 (SCL1 alternate)
 *
 * ******************************************************************* */

/* ===================================================================
 * RC4I2C - Pin RC4 I2C Configuration Register
 * Address: 0x0288
 *
 * Configures the I2C electrical characteristics of pin RC4, which
 * is the default SDA1 pin.  Must be configured before enabling
 * the I2C module for proper operation at the target bus speed.
 * ================================================================ */
#define RC4I2C SFR8(0x0286)

/* Bits 7:6 - SLEW[1:0]: Slew Rate Control
 *   Selects the output slew rate for the I2C open-drain driver:
 *   00 = Maximum slew rate (fastest edges)
 *   01 = I2C specific slew rate
 *   10 = SMBus 3.0 100 kHz compliant slew rate
 *   11 = SMBus 3.0 1 MHz compliant slew rate
 *
 *   Select the slew rate appropriate for the bus speed and standard
 *   to minimize EMI while meeting rise/fall time requirements.       */
#define RC4I2C_SLEW1 7
#define RC4I2C_SLEW0 6

/* SLEW field mask and position                                       */
#define RC4I2C_SLEW_MASK 0xC0 /* bits 7:6 */
#define RC4I2C_SLEW_POS 6

/* Named slew rate values (pre-shifted)                               */
#define I2C_SLEW_MAX (0x00 << 6)        /* Maximum slew rate        */
#define I2C_SLEW_I2C (0x01 << 6)        /* I2C specific             */
#define I2C_SLEW_SMBUS_100K (0x02 << 6) /* SMBus 3.0 100 kHz       */
#define I2C_SLEW_SMBUS_1M (0x03 << 6)   /* SMBus 3.0 1 MHz         */

/* Bits 5:4 - PU[1:0]: Pull-Up Resistor Selection
 *   Selects the internal pull-up resistor on the pin:
 *   00 = No internal pull-up (external pull-up required)
 *   01 = Standard I2C pull-up resistor enabled
 *   10 = SMBus-compliant pull-up resistor enabled
 *   11 = Reserved (do not use)
 *
 *   Internal pull-ups may be insufficient for higher bus speeds or
 *   longer bus lengths; external pull-ups are recommended.           */
#define RC4I2C_PU1 5
#define RC4I2C_PU0 4

/* PU field mask and position                                         */
#define RC4I2C_PU_MASK 0x30 /* bits 5:4 */
#define RC4I2C_PU_POS 4

/* Named pull-up values (pre-shifted)                                 */
#define I2C_PU_NONE (0x00 << 4)  /* No pull-up               */
#define I2C_PU_STD (0x01 << 4)   /* Standard I2C pull-up     */
#define I2C_PU_SMBUS (0x02 << 4) /* SMBus pull-up            */

/* Bits 3:2 - TH[1:0]: Input Voltage Threshold Selection
 *   Selects the input voltage threshold for the pin's Schmitt
 *   trigger, matching the electrical specification of the bus:
 *   00 = I2C / SMBus 3.0 1 MHz threshold
 *   01 = SMBus 2.0 100 kHz threshold
 *   10 = SMBus 3.0 100 kHz threshold
 *   11 = I2C-specific threshold
 *
 *   Must match the bus standard in use for reliable operation.       */
#define RC4I2C_TH1 3
#define RC4I2C_TH0 2

/* TH field mask and position                                         */
#define RC4I2C_TH_MASK 0x0C /* bits 3:2 */
#define RC4I2C_TH_POS 2

/* Named threshold values (pre-shifted)                               */
#define I2C_TH_I2C_1M (0x00 << 2)      /* I2C / SMBus 3.0 1 MHz   */
#define I2C_TH_SMBUS2_100K (0x01 << 2) /* SMBus 2.0 100 kHz       */
#define I2C_TH_SMBUS3_100K (0x02 << 2) /* SMBus 3.0 100 kHz       */
#define I2C_TH_I2C (0x03 << 2)         /* I2C-specific             */

/* Bits 1:0 - Reserved: read as 0, write as 0.                       */

/* ===================================================================
 * RC3I2C - Pin RC3 I2C Configuration Register
 * Address: 0x0289
 *
 * Configures the I2C electrical characteristics of pin RC3, which
 * is the default SCL1 pin.  Bit layout is identical to RC4I2C.
 * ================================================================ */
#define RC3I2C SFR8(0x0287)

#define RC3I2C_SLEW1 7
#define RC3I2C_SLEW0 6
#define RC3I2C_SLEW_MASK 0xC0
#define RC3I2C_SLEW_POS 6
#define RC3I2C_PU1 5
#define RC3I2C_PU0 4
#define RC3I2C_PU_MASK 0x30
#define RC3I2C_PU_POS 4
#define RC3I2C_TH1 3
#define RC3I2C_TH0 2
#define RC3I2C_TH_MASK 0x0C
#define RC3I2C_TH_POS 2

/* ===================================================================
 * RB2I2C - Pin RB2 I2C Configuration Register
 * Address: 0x028A
 *
 * Configures the I2C electrical characteristics of pin RB2, which
 * is the alternate SDA1 pin.  Bit layout is identical to RC4I2C.
 * ================================================================ */
#define RB2I2C SFR8(0x0288)

#define RB2I2C_SLEW1 7
#define RB2I2C_SLEW0 6
#define RB2I2C_SLEW_MASK 0xC0
#define RB2I2C_SLEW_POS 6
#define RB2I2C_PU1 5
#define RB2I2C_PU0 4
#define RB2I2C_PU_MASK 0x30
#define RB2I2C_PU_POS 4
#define RB2I2C_TH1 3
#define RB2I2C_TH0 2
#define RB2I2C_TH_MASK 0x0C
#define RB2I2C_TH_POS 2

/* ===================================================================
 * RB1I2C - Pin RB1 I2C Configuration Register
 * Address: 0x028B
 *
 * Configures the I2C electrical characteristics of pin RB1, which
 * is the alternate SCL1 pin.  Bit layout is identical to RC4I2C.
 * ================================================================ */
#define RB1I2C SFR8(0x0289)

#define RB1I2C_SLEW1 7
#define RB1I2C_SLEW0 6
#define RB1I2C_SLEW_MASK 0xC0
#define RB1I2C_SLEW_POS 6
#define RB1I2C_PU1 5
#define RB1I2C_PU0 4
#define RB1I2C_PU_MASK 0x30
#define RB1I2C_PU_POS 4
#define RB1I2C_TH1 3
#define RB1I2C_TH0 2
#define RB1I2C_TH_MASK 0x0C
#define RB1I2C_TH_POS 2

/* *******************************************************************
 *
 *  I2C1 - Inter-Integrated Circuit, Instance 1
 *
 * ******************************************************************* */

/* ===================================================================
 * I2C1CNT - I2C1 Byte Count Register (16-bit, little-endian)
 * Address: 0x028C (low byte), 0x028D (high byte)
 *
 * Specifies the number of data bytes in the current transfer
 * (excluding the address byte).  In host mode, the module
 * decrements this count for each byte transferred and
 * automatically issues a Stop condition when the count reaches
 * zero (if configured to do so).  In client mode, the count
 * tracks incoming bytes for software reference.
 *
 * Writing 0xFF (255) enables "streaming" mode where the transfer
 * continues indefinitely until software intervenes.
 * ================================================================ */
#define I2C1CNTL SFR8(0x028C) /* Count bits  7:0         */
#define I2C1CNTH SFR8(0x028D) /* Count bits 15:8         */
#define I2C1CNT SFR16(0x028C) /* 16-bit access (L:H)     */

/* I2C1CNTL - Bits 7:0: Low byte of the byte count.                  */
/* I2C1CNTH - Bits 7:0: High byte of the byte count.                 */

/* ===================================================================
 * I2C1ADB0 - I2C1 Address Buffer 0
 * Address: 0x028E
 *
 * Contains the first byte of the received address when the module
 * is operating in client mode and an address match occurs.  In
 * 7-bit mode, bits 7:1 hold the matched address and bit 0 holds
 * the R/W direction bit.  Read-only; loaded by hardware.
 * ================================================================ */
#define I2C1ADB0 SFR8(0x028E)

/* Bits 7:0 - ADB0[7:0]: Address Buffer byte 0
 *   7-bit mode: bits 7:1 = matched address, bit 0 = R/nW
 *   10-bit mode: contains the first address byte (11110xx + R/nW)   */

/* ===================================================================
 * I2C1ADB1 - I2C1 Address Buffer 1
 * Address: 0x028F
 *
 * Contains the second byte of a 10-bit address match in client
 * mode.  In 7-bit addressing mode, this register is unused.
 * Read-only; loaded by hardware.
 * ================================================================ */
#define I2C1ADB1 SFR8(0x028F)

/* Bits 7:0 - ADB1[7:0]: Address Buffer byte 1
 *   10-bit mode: contains the low 8 bits of the 10-bit address.
 *   7-bit mode: not used.                                           */

/* ===================================================================
 * I2C1ADR0 - I2C1 Client Address Register 0
 * Address: 0x0290
 *
 * Holds the primary client address that the module responds to
 * when operating in client mode.  The address is compared against
 * incoming bus addresses to determine if this device is selected.
 *
 * 7-bit mode:  bits 7:1 = 7-bit address, bit 0 = unused
 * 10-bit mode: bits 7:0 = low byte of the 10-bit address
 * ================================================================ */
#define I2C1ADR0 SFR8(0x0290)

/* Bits 7:0 - ADR0[7:0]: Client Address 0                            */

/* ===================================================================
 * I2C1ADR1 - I2C1 Client Address Register 1
 * Address: 0x0291
 *
 * Secondary address or address mask register.  In 10-bit mode,
 * holds the upper portion of the 10-bit address.  May also serve
 * as an address mask to match a range of addresses.
 * ================================================================ */
#define I2C1ADR1 SFR8(0x0291)

/* Bits 6:0 - ADR1[6:0]: Client Address 1 / Mask
 *   Bit 7 is reserved.                                              */

/* ===================================================================
 * I2C1ADR2 - I2C1 Client Address Register 2
 * Address: 0x0292
 *
 * Second client address (for dual-address client mode).  Allows
 * the device to respond to two different bus addresses.  Format
 * matches I2C1ADR0.
 * ================================================================ */
#define I2C1ADR2 SFR8(0x0292)

/* Bits 7:0 - ADR2[7:0]: Client Address 2                            */

/* ===================================================================
 * I2C1ADR3 - I2C1 Client Address Register 3
 * Address: 0x0293
 *
 * Mask or upper bits for the second client address.  Format
 * matches I2C1ADR1.
 * ================================================================ */
#define I2C1ADR3 SFR8(0x0293)

/* Bits 6:0 - ADR3[6:0]: Client Address 3 / Mask
 *   Bit 7 is reserved.                                              */

/* ===================================================================
 * I2C1CON0 - I2C1 Control Register 0
 * Address: 0x0294
 *
 * Primary control register: module enable, operating mode
 * selection, and bus condition control.
 * ================================================================ */
#define I2C1CON0 SFR8(0x0294)

/* Bit 7 - EN: I2C Module Enable
 *   1 = I2C1 module is enabled.  SDA and SCL pins are controlled
 *       by the I2C module.
 *   0 = I2C1 module is disabled.  Pins revert to general purpose
 *       I/O.  All internal state is reset.                          */
#define I2C1CON0_EN 7

/* Bit 6 - RSEN: Repeated Start Enable
 *   1 = Issue a Repeated Start condition after the current byte
 *       transfer completes (host mode).  Used to change direction
 *       or address without releasing the bus.
 *   0 = Normal operation (no repeated start).
 *   Hardware clears this bit after the Repeated Start is issued.    */
#define I2C1CON0_RSEN 6

/* Bit 5 - S: Start Condition (host mode)
 *   Write 1 = Initiate a Start condition on the bus.
 *   Read  1 = A Start condition has been detected on the bus.
 *   Read  0 = No Start condition pending.
 *   In host mode, software sets this bit to begin a transaction.
 *   Hardware clears it when the Start condition completes.          */
#define I2C1CON0_S 5

/* Bit 4 - CSTR: Clock Stretch
 *   1 = Clock stretching is in effect; SCL is held low by the
 *       module.  In client mode, allows software time to prepare
 *       data before releasing the clock.
 *   0 = Clock is not being stretched.
 *   Software clears this bit to release SCL and resume clocking.    */
#define I2C1CON0_CSTR 4

/* Bit 3 - MDR: Host Data Request
 *   1 = The module is requesting data for a host-mode transfer.
 *       Software should write data to I2C1TXB (for a write) or
 *       read data from I2C1RXB (for a read).
 *   0 = No data request pending.
 *   Set by hardware; cleared when software services the request.    */
#define I2C1CON0_MDR 3

/* Bits 2:0 - MODE[2:0]: Operating Mode Select
 *   Determines the I2C operating mode and address width:
 *   000 = Client mode, 7-bit address
 *   001 = Client mode, 10-bit address
 *   010 = Reserved
 *   011 = Reserved
 *   100 = Host mode (7-bit or 10-bit address per ACNT)
 *   101 = Reserved
 *   110 = Reserved
 *   111 = Reserved
 *
 *   The module must be disabled (EN=0) before changing MODE.        */
#define I2C1CON0_MODE2 2
#define I2C1CON0_MODE1 1
#define I2C1CON0_MODE0 0

/* MODE field mask and position                                       */
#define I2C1CON0_MODE_MASK 0x07 /* bits 2:0 */
#define I2C1CON0_MODE_POS 0

/* Named mode values for MODE[2:0]                                    */
#define I2C_MODE_CLIENT_7 0x00  /* Client, 7-bit addressing        */
#define I2C_MODE_CLIENT_10 0x01 /* Client, 10-bit addressing       */
#define I2C_MODE_HOST 0x04      /* Host mode                       */

/* ===================================================================
 * I2C1CON1 - I2C1 Control Register 1
 * Address: 0x0295
 *
 * Acknowledge control, status, and error flags for byte-level
 * handshaking on the I2C bus.
 * ================================================================ */
#define I2C1CON1 SFR8(0x0295)

/* Bit 7 - ACKCNT: Acknowledge Sequence Count
 *   In host-receive mode:
 *   1 = Send NACK after the next received byte (signals end of
 *       read to the client device).
 *   0 = Send ACK after the next received byte (request more data).
 *   Typically set to 1 before reading the last byte in a transfer.  */
#define I2C1CON1_ACKCNT 7

/* Bit 6 - ACKDT: Acknowledge Data Bit
 *   Controls the value driven on SDA during the acknowledge cycle:
 *   1 = NACK (SDA = 1) will be transmitted.
 *   0 = ACK  (SDA = 0) will be transmitted.
 *   Used to manually control acknowledge responses.                 */
#define I2C1CON1_ACKDT 6

/* Bit 5 - ACKSTAT: Acknowledge Status (read-only)
 *   1 = NACK was received from the addressed device.
 *   0 = ACK was received from the addressed device.
 *   Valid after a byte transmission completes in host mode.         */
#define I2C1CON1_ACKSTAT 5

/* Bit 4 - ACKT: Acknowledge Time Enable
 *   1 = Automatic acknowledge generation is enabled.  The module
 *       automatically sends ACK/NACK based on ACKDT/ACKCNT after
 *       each received byte.
 *   0 = Manual acknowledge mode.  Software must control the
 *       acknowledge sequence.                                       */
#define I2C1CON1_ACKT 4

/* Bit 3 - TXWE: Transmit Write Error
 *   1 = A write to I2C1TXB was attempted while the TX buffer was
 *       not ready (previous data not yet shifted out).
 *   0 = No transmit write error.
 *   Cleared by software.                                            */
#define I2C1CON1_TXWE 3

/* Bit 2 - RXRE: Receive Read Error
 *   1 = A read from I2C1RXB was attempted while the RX buffer
 *       was empty (no data available).
 *   0 = No receive read error.
 *   Cleared by software.                                            */
#define I2C1CON1_RXRE 2

/* Bits 1:0 - Reserved: read as 0, write as 0.                       */

/* ===================================================================
 * I2C1CON2 - I2C1 Control Register 2
 * Address: 0x0296
 *
 * Advanced control: address mode, general call, fast mode, SDA
 * hold time, and buffer management.
 * ================================================================ */
#define I2C1CON2 SFR8(0x0296)

/* Bit 7 - BFRE: Buffer Free / Restart
 *   1 = Clear all internal buffers and reset FIFO pointers.
 *       Self-clearing: hardware resets this bit after the
 *       operation completes.
 *   0 = No action / buffer clear complete.                          */
#define I2C1CON2_BFRE 7

/* Bit 6 - ACNT: Address Count (10-bit mode sequence control)
 *   1 = Automatic 10-bit address sequence.  The module
 *       automatically sends the two-byte 10-bit address on the
 *       bus when a Start condition is initiated.
 *   0 = Manual address management for 10-bit mode.                  */
#define I2C1CON2_ACNT 6

/* Bit 5 - GCEN: General Call Enable
 *   1 = Client mode: the module responds to the General Call
 *       address (0x00) in addition to its programmed address.
 *   0 = General Call address is ignored.                            */
#define I2C1CON2_GCEN 5

/* Bit 4 - FME: Fast Mode Enable
 *   1 = I2C Fast-mode (400 kHz) operation enabled.  Internal
 *       timing is adjusted for the faster clock rate.
 *   0 = Standard-mode (100 kHz) timing.
 *   Ensure the baud rate register and pin configuration are also
 *   set appropriately for the target speed.                         */
#define I2C1CON2_FME 4

/* Bit 3 - ABD: Address Buffer Disable
 *   1 = The address buffer (I2C1ADB0/ADB1) is not loaded on an
 *       address match.  Reduces overhead when the matched address
 *       is not needed by software.
 *   0 = Address buffer is loaded normally on address match.         */
#define I2C1CON2_ABD 3

/* Bits 2:1 - SDAHT[1:0]: SDA Hold Time
 *   Controls the hold time of SDA relative to the falling edge
 *   of SCL, ensuring reliable data capture by the receiver:
 *   00 = Minimum hold time (300 ns typical)
 *   01 = 100 ns hold time
 *   10 = 30 ns hold time
 *   11 = Reserved
 *
 *   Refer to DS40002213D for exact timing values per VDD and
 *   operating mode.                                                  */
#define I2C1CON2_SDAHT1 2
#define I2C1CON2_SDAHT0 1

/* SDAHT field mask and position                                      */
#define I2C1CON2_SDAHT_MASK 0x06 /* bits 2:1 */
#define I2C1CON2_SDAHT_POS 1

/* Bit 0 - Reserved: read as 0, write as 0.                          */

/* ===================================================================
 * I2C1ERR - I2C1 Error Register
 * Address: 0x0297
 *
 * Contains error interrupt flags and their corresponding enable
 * bits for bus-level error conditions.  The upper nibble holds
 * the interrupt flags; the lower nibble holds the enable bits.
 * ================================================================ */
#define I2C1ERR SFR8(0x0297)

/* --- Interrupt flags (upper nibble) --- */

/* Bit 7 - BTOIF: Bus Timeout Interrupt Flag
 *   1 = A bus timeout condition was detected.  SCL or SDA was
 *       held in an unexpected state beyond the timeout period
 *       configured in I2C1BTO.
 *   0 = No bus timeout detected.
 *   Cleared by software writing 0.                                  */
#define I2C1ERR_BTOIF 7

/* Bit 6 - BCLIF: Bus Collision Interrupt Flag
 *   1 = A bus collision was detected.  In host mode, this means
 *       another master drove the bus at the same time (multi-master
 *       arbitration loss).  The current transfer is aborted.
 *   0 = No bus collision detected.
 *   Cleared by software writing 0.                                  */
#define I2C1ERR_BCLIF 6

/* Bit 5 - NACKIF: Not-Acknowledge Interrupt Flag
 *   1 = A NACK was received from the addressed client device
 *       during a host-mode transfer.  Indicates the client did
 *       not acknowledge the address or data byte.
 *   0 = No NACK error.
 *   Cleared by software writing 0.                                  */
#define I2C1ERR_NACKIF 5

/* Bit 4 - Reserved: read as 0, write as 0.                          */

/* --- Interrupt enables (lower nibble) --- */

/* Bit 3 - BTOIE: Bus Timeout Interrupt Enable
 *   1 = Interrupt enabled when BTOIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1ERR_BTOIE 3

/* Bit 2 - BCLIE: Bus Collision Interrupt Enable
 *   1 = Interrupt enabled when BCLIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1ERR_BCLIE 2

/* Bit 1 - NACKIE: Not-Acknowledge Interrupt Enable
 *   1 = Interrupt enabled when NACKIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1ERR_NACKIE 1

/* Bit 0 - Reserved: read as 0, write as 0.                          */

/* ===================================================================
 * I2C1STAT0 - I2C1 Status Register 0
 * Address: 0x0298
 *
 * Read-only status register providing real-time information about
 * the module state, buffer occupancy, and last-byte direction.
 * ================================================================ */
#define I2C1STAT0 SFR8(0x0298)

/* Bit 5 - SMA: SDA/SCL Module Active (read-only)
 *   1 = The I2C bus lines are active (a transfer is in progress
 *       or the bus is busy).
 *   0 = The I2C bus is idle (both SDA and SCL are high).            */
#define I2C1STAT0_SMA 5

/* Bit 4 - MMA: Master Mode Active (read-only)
 *   1 = The module is currently operating as bus host and
 *       owns the bus.
 *   0 = The module is not in active host mode.                      */
#define I2C1STAT0_MMA 4

/* Bit 3 - TXBE: Transmit Buffer Empty (read-only)
 *   1 = The transmit buffer (I2C1TXB) is empty and ready for
 *       new data.
 *   0 = The transmit buffer contains data awaiting transmission.    */
#define I2C1STAT0_TXBE 3

/* Bit 2 - RXBF: Receive Buffer Full (read-only)
 *   1 = The receive buffer (I2C1RXB) contains a byte that has
 *       not yet been read by software.
 *   0 = The receive buffer is empty.                                */
#define I2C1STAT0_RXBF 2

/* Bit 1 - R: Read/Write Information (read-only)
 *   1 = The last address byte on the bus indicated a Read
 *       operation (client transmit).
 *   0 = The last address byte indicated a Write operation
 *       (client receive).                                           */
#define I2C1STAT0_R 1

/* Bit 0 - D: Data/Address (read-only)
 *   1 = The last byte transmitted or received was a data byte.
 *   0 = The last byte was an address byte.
 *   Useful in client mode to distinguish the address phase from
 *   the data phase.                                                 */
#define I2C1STAT0_D 0

/* Bits 7:6 - Reserved: read as 0.                                    */

/* ===================================================================
 * I2C1STAT1 - I2C1 Status Register 1
 * Address: 0x0299
 *
 * Additional status and error indicators for buffer management
 * and clock stretching.
 * ================================================================ */
#define I2C1STAT1 SFR8(0x0299)

/* Bit 7 - TXWE: Transmit Write Error (read-only)
 *   1 = A write to I2C1TXB occurred while the buffer was not
 *       ready.  The written data was lost.
 *   0 = No transmit write error.                                    */
#define I2C1STAT1_TXWE 7

/* Bit 6 - TXU: Transmit Underflow (read-only)
 *   1 = The module attempted to transmit a byte but the TX buffer
 *       was empty.  Undefined data was shifted out.
 *   0 = No transmit underflow.                                      */
#define I2C1STAT1_TXU 6

/* Bit 5 - RXO: Receive Overflow (read-only)
 *   1 = A byte was received but the RX buffer was already full.
 *       The incoming byte was lost.
 *   0 = No receive overflow.                                        */
#define I2C1STAT1_RXO 5

/* Bit 4 - CSD: Clock Stretch Disable
 *   1 = Clock stretching by the client module is disabled.
 *   0 = Clock stretching is allowed (client can hold SCL low).      */
#define I2C1STAT1_CSD 4

/* Bit 3 - CLRBF: Clear Buffers
 *   1 = Clear internal data buffers.  Self-clearing: hardware
 *       resets this bit after the operation completes.
 *   0 = No action / clear complete.                                 */
#define I2C1STAT1_CLRBF 3

/* Bit 2 - Reserved: read as 0, write as 0.                          */

/* Bit 1 - RXRE: Receive Read Error (read-only)
 *   1 = A read from I2C1RXB was attempted while the buffer was
 *       empty.  The returned data is invalid.
 *   0 = No receive read error.                                      */
#define I2C1STAT1_RXRE 1

/* Bit 0 - RXBF: Receive Buffer Full (read-only)
 *   1 = Receive buffer contains valid unread data.
 *   0 = Receive buffer is empty.                                    */
#define I2C1STAT1_RXBF 0

/* ===================================================================
 * I2C1PIR - I2C1 Peripheral Interrupt Request Register
 * Address: 0x029A
 *
 * Contains interrupt flags for I2C1 bus events.  Each flag is set
 * by hardware and must be cleared by software writing 0 to the
 * bit.  These flags are independent of the error flags in I2C1ERR.
 * ================================================================ */
#define I2C1PIR SFR8(0x029A)

/* Bit 7 - CNTIF: Byte Count Interrupt Flag
 *   1 = The I2C1CNT byte counter has reached zero.  All planned
 *       bytes have been transferred.
 *   0 = Byte count has not reached zero.                            */
#define I2C1PIR_CNTIF 7

/* Bit 6 - ACKTIF: Acknowledge Timeout Interrupt Flag
 *   1 = An acknowledge timeout occurred.  The addressed device
 *       did not respond within the expected time.
 *   0 = No acknowledge timeout.                                     */
#define I2C1PIR_ACKTIF 6

/* Bit 5 - WRIF: Write Interrupt Flag
 *   1 = In client mode, a data byte has been received and is
 *       available in I2C1RXB.
 *   0 = No write event pending.                                     */
#define I2C1PIR_WRIF 5

/* Bit 4 - ADRIF: Address Match Interrupt Flag
 *   1 = The module detected an address match on the bus
 *       (client mode was selected).
 *   0 = No address match detected.                                  */
#define I2C1PIR_ADRIF 4

/* Bit 3 - PCIF: Stop Condition Interrupt Flag
 *   1 = A Stop condition was detected on the bus.
 *   0 = No Stop condition detected.                                 */
#define I2C1PIR_PCIF 3

/* Bit 2 - RSCIF: Repeated Start Condition Interrupt Flag
 *   1 = A Repeated Start condition was detected on the bus.
 *   0 = No Repeated Start detected.                                 */
#define I2C1PIR_RSCIF 2

/* Bit 1 - SCIF: Start Condition Interrupt Flag
 *   1 = A Start condition was detected on the bus.
 *   0 = No Start condition detected.                                */
#define I2C1PIR_SCIF 1

/* Bit 0 - Reserved: read as 0, write as 0.                          */

/* ===================================================================
 * I2C1PIE - I2C1 Peripheral Interrupt Enable Register
 * Address: 0x029B
 *
 * Each bit enables the corresponding interrupt source from
 * I2C1PIR.  When an enable bit and its corresponding flag are
 * both set, the I2C1 interrupt request is asserted to the
 * vectored interrupt controller.
 * ================================================================ */
#define I2C1PIE SFR8(0x029B)

/* Bit 7 - CNTIE: Byte Count Interrupt Enable
 *   1 = Interrupt enabled when CNTIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1PIE_CNTIE 7

/* Bit 6 - ACKTIE: Acknowledge Timeout Interrupt Enable
 *   1 = Interrupt enabled when ACKTIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1PIE_ACKTIE 6

/* Bit 5 - WRIE: Write Interrupt Enable
 *   1 = Interrupt enabled when WRIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1PIE_WRIE 5

/* Bit 4 - ADRIE: Address Match Interrupt Enable
 *   1 = Interrupt enabled when ADRIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1PIE_ADRIE 4

/* Bit 3 - PCIE: Stop Condition Interrupt Enable
 *   1 = Interrupt enabled when PCIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1PIE_PCIE 3

/* Bit 2 - RSCIE: Repeated Start Condition Interrupt Enable
 *   1 = Interrupt enabled when RSCIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1PIE_RSCIE 2

/* Bit 1 - SCIE: Start Condition Interrupt Enable
 *   1 = Interrupt enabled when SCIF is set.
 *   0 = Interrupt disabled.                                         */
#define I2C1PIE_SCIE 1

/* Bit 0 - Reserved: read as 0, write as 0.                          */

/* ===================================================================
 * I2C1BTO - I2C1 Bus Timeout Time Register
 * Address: 0x029C
 *
 * Sets the timeout period for the bus timeout detector.  If SCL
 * is held low (by a faulty device or stuck bus) longer than this
 * period, the BTOIF flag in I2C1ERR is set and the module can
 * optionally release the bus.
 *
 * The timeout period is:
 *   T_timeout = TOTIME[5:0] * T_timeout_clock
 *
 * where T_timeout_clock is selected by I2C1BTOC.BTOC[3:0].
 * ================================================================ */
#define I2C1BTO SFR8(0x029C)

/* Bits 5:0 - TOTIME[5:0]: Bus Timeout Time Value
 *   6-bit value that sets the timeout period.  Zero disables the
 *   timeout detector.  Maximum value is 63.
 *   Bits 7:6 are unimplemented and read as 0.                      */
#define I2C1BTO_TOTIME5 5
#define I2C1BTO_TOTIME4 4
#define I2C1BTO_TOTIME3 3
#define I2C1BTO_TOTIME2 2
#define I2C1BTO_TOTIME1 1
#define I2C1BTO_TOTIME0 0

/* TOTIME field mask and position                                     */
#define I2C1BTO_TOTIME_MASK 0x3F /* bits 5:0 */
#define I2C1BTO_TOTIME_POS 0

/* ===================================================================
 * I2C1BAUD - I2C1 Baud Rate Register
 * Address: 0x029D
 *
 * Sets the SCL clock frequency in host mode.
 *
 * SCL frequency = F_clocksource / (4 * (BAUD + 1))
 *
 * where F_clocksource is selected by I2C1CLK.CLK[4:0].
 *
 * Examples (assuming 64 MHz source):
 *   BAUD = 0x9F (159) => SCL = 100 kHz  (Standard mode)
 *   BAUD = 0x27 ( 39) => SCL = 400 kHz  (Fast mode)
 *   BAUD = 0x0F ( 15) => SCL = 1.0 MHz  (Fast mode plus)
 *
 * In client mode this register is not used; the clock is provided
 * externally by the host device.
 * ================================================================ */
#define I2C1BAUD SFR8(0x029D)

/* Bits 7:0 - BAUD[7:0]: Baud Rate Divider
 *   Determines the SCL period.  See formula above.                  */

/* ===================================================================
 * I2C1CLK - I2C1 Clock Source Select Register
 * Address: 0x029E
 *
 * Selects the clock source that feeds the I2C baud rate generator
 * in host mode.  The specific mapping of CLK values to clock
 * sources is device-dependent; refer to the "I2C Clock Source
 * Selection" table in DS40002213D for the complete enumeration.
 * Common sources include FOSC, HFINTOSC, MFINTOSC, and LFINTOSC.
 * ================================================================ */
#define I2C1CLK SFR8(0x029E)

/* Bits 4:0 - CLK[4:0]: Clock Source Select
 *   Selects the input clock for the I2C baud rate generator.
 *   Refer to DS40002213D for the full mapping.                      */
#define I2C1CLK_CLK4 4
#define I2C1CLK_CLK3 3
#define I2C1CLK_CLK2 2
#define I2C1CLK_CLK1 1
#define I2C1CLK_CLK0 0

/* CLK field mask and position                                        */
#define I2C1CLK_CLK_MASK 0x1F /* bits 4:0 */
#define I2C1CLK_CLK_POS 0

/* Bits 7:5 - Reserved: read as 0, write as 0.                       */

/* ===================================================================
 * I2C1BTOC - I2C1 Bus Timeout Clock Source Control Register
 * Address: 0x029F
 *
 * Selects the clock source used by the bus timeout detector.
 * Combined with the timeout value in I2C1BTO, this determines the
 * actual timeout period in seconds.  Refer to DS40002213D for
 * the mapping of BTOC values to specific clock sources.
 * ================================================================ */
#define I2C1BTOC SFR8(0x029F)

/* Bits 3:0 - BTOC[3:0]: Bus Timeout Clock Source Select
 *   Selects the clock that drives the timeout counter.  The
 *   specific mapping is device-dependent; refer to DS40002213D
 *   for the complete enumeration.                                    */
#define I2C1BTOC_BTOC3 3
#define I2C1BTOC_BTOC2 2
#define I2C1BTOC_BTOC1 1
#define I2C1BTOC_BTOC0 0

/* BTOC field mask and position                                       */
#define I2C1BTOC_BTOC_MASK 0x0F /* bits 3:0 */
#define I2C1BTOC_BTOC_POS 0

/* Bits 7:4 - Reserved: read as 0, write as 0.                       */

/* ===================================================================
 * I2C1RXB - I2C1 Receive Buffer Register
 * Address: 0x02A0
 *
 * Read-only.  Returns the last byte received on the I2C bus.
 * In host-receive mode, reading this register also acknowledges
 * receipt and allows the next byte to be clocked in.  Reading
 * when the buffer is empty sets the RXRE error flag.
 * ================================================================ */
#define I2C1RXB SFR8(0x028A)

/* Bits 7:0 - RXB[7:0]: Receive Buffer Data (read-only)              */

/* ===================================================================
 * I2C1TXB - I2C1 Transmit Buffer Register
 * Address: 0x02A1
 *
 * Write-only.  Writing a byte loads it into the transmit buffer
 * for serial transmission.  In host-transmit mode, writing here
 * initiates the byte transfer if the module is ready.  Writing
 * when the buffer is full sets the TXWE error flag.
 * ================================================================ */
#define I2C1TXB SFR8(0x028B)

/* Bits 7:0 - TXB[7:0]: Transmit Buffer Data (write-only)            */

#endif /* PIC18F_Q84_I2C_H */
