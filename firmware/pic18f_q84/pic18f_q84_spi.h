/*
 * pic18f_q84_spi.h
 *
 * SPI (Serial Peripheral Interface) Module register definitions for
 * PIC18F27/47/57Q84 microcontrollers.
 *
 * The device contains two independent SPI modules (SPI1 and SPI2),
 * each providing full-duplex synchronous serial communication with
 * configurable clock polarity, phase, and data order.
 *
 * Features:
 *   - Host (master) and Client (slave) operating modes
 *   - Separate 2-byte TX and RX FIFOs per instance
 *   - Configurable transfer width (1-8 bits per byte)
 *   - 11-bit transfer byte counter with automatic stop
 *   - Byte-by-byte mode or total-count mode via BMODE
 *   - Selectable clock polarity (CKP) and phase (CKE)
 *   - MSB-first or LSB-first data order
 *   - Programmable baud rate divider (8-bit)
 *   - Slave Select (SS) output with configurable polarity
 *   - SDI/SDO polarity inversion options
 *   - Fast Start mode (first bit on first clock edge)
 *   - DMA source/destination support for TX and RX buffers
 *   - Dedicated interrupt flags for overflow, underflow,
 *     transfer complete, shift-register empty, and SS edges
 *
 * Clock modes (SPI mode selection):
 *   CKP=0, CKE=1 => Mode 0  (idle low,  sample on rising edge)
 *   CKP=0, CKE=0 => Mode 1  (idle low,  sample on falling edge)
 *   CKP=1, CKE=1 => Mode 2  (idle high, sample on falling edge)
 *   CKP=1, CKE=0 => Mode 3  (idle high, sample on rising edge)
 *
 * Typical host-mode usage:
 *   1. Select clock source via SPIxCLK.CLKSEL[4:0]
 *   2. Set baud rate via SPIxBAUD (SCK = Fsrc / (2*(BAUD+1)))
 *   3. Configure clock polarity/phase in SPIxCON1 (CKP, CKE)
 *   4. Set host mode: SPIxCON0.MST = 1
 *   5. Set transfer mode: SPIxCON0.BMODE = 1 (byte) or 0 (count)
 *   6. Enable: SPIxCON0.EN = 1
 *   7. Write data to SPIxTXB; read received data from SPIxRXB
 *
 * Reference: Microchip DS40002213D
 *            PIC18F27/47/57Q84 Data Sheet
 *
 * Requires SFR8(addr) and SFR16(addr) macros defined externally.
 */

#ifndef PIC18F_Q84_SPI_H
#define PIC18F_Q84_SPI_H

/* *******************************************************************
 *
 *  SPI1 - Serial Peripheral Interface, Instance 1
 *
 * ******************************************************************* */

/* ===================================================================
 * SPI1RXB - SPI1 Receive Buffer Register
 * Address: 0x0080
 *
 * Read-only.  Returns the last byte received by the SPI1 module.
 * Each read pops one byte from the 2-byte receive FIFO.  Reading
 * when the FIFO is empty sets the RX Read Error flag (RXRE) in
 * SPI1STATUS or SPI1CON1.
 * ================================================================ */
#define SPI1RXB SFR8(0x0080)

/* Bits 7:0 - RXB[7:0]: Receive Buffer Data (read-only)
 *   Contains the oldest unread byte from the receive FIFO.         */

/* ===================================================================
 * SPI1TXB - SPI1 Transmit Buffer Register
 * Address: 0x0081
 *
 * Write-only.  Writing a byte pushes it into the 2-byte transmit
 * FIFO for serial transmission.  Writing when the FIFO is full
 * sets the TX Write Error flag (TXWE) in SPI1CON2.
 * ================================================================ */
#define SPI1TXB SFR8(0x0081)

/* Bits 7:0 - TXB[7:0]: Transmit Buffer Data (write-only)
 *   Write a byte here to enqueue it for transmission.              */

/* ===================================================================
 * SPI1TCNT - SPI1 Transfer Byte Counter (16-bit, little-endian)
 * Address: 0x0082 (low byte), 0x0083 (high byte)
 *
 * 11-bit counter that tracks the number of bytes remaining in the
 * current transfer.  In total-count mode (BMODE=0), the module
 * loads this value before a transfer begins and decrements it for
 * each byte exchanged.  When the count reaches zero, the TCZIF
 * interrupt flag is set and the transfer can end automatically.
 *
 * In byte mode (BMODE=1), this counter decrements per byte but
 * does not stop the transfer when it reaches zero.
 * ================================================================ */
#define SPI1TCNTL SFR8(0x0082) /* Count bits  7:0         */
#define SPI1TCNTH SFR8(0x0083) /* Count bits 10:8         */
#define SPI1TCNT SFR16(0x0082) /* 16-bit access (L:H)     */

/* SPI1TCNTL - Bits 7:0: TCNTL[7:0]
 *   Low byte of the 11-bit transfer byte count.                    */

/* SPI1TCNTH - Bits 2:0: TCNTH[2:0]
 *   High bits of the 11-bit transfer byte count (max value 2047).
 *   Bits 7:3 are unimplemented and read as 0.                     */
#define SPI1TCNTH_MASK 0x07 /* bits 2:0 valid */

/* ===================================================================
 * SPI1CON0 - SPI1 Control Register 0
 * Address: 0x0084
 *
 * Primary control register for module enable, operating mode, data
 * order, and transfer count behaviour.
 * ================================================================ */
#define SPI1CON0 SFR8(0x0084)

/* Bit 7 - EN: SPI Module Enable
 *   1 = SPI1 module is enabled.  I/O pins configured per PPS are
 *       controlled by the SPI module.
 *   0 = SPI1 module is disabled.  All pins revert to general
 *       purpose I/O.  Internal state is reset.                     */
#define SPI1CON0_EN 7

/* Bit 6 - LSBF: LSB First
 *   1 = Data is transmitted/received LSB first.
 *   0 = Data is transmitted/received MSB first (default).          */
#define SPI1CON0_LSBF 6

/* Bit 5 - MST: Host/Client Mode Select
 *   1 = Host mode: this device generates the serial clock (SCK).
 *   0 = Client mode: this device receives the serial clock.        */
#define SPI1CON0_MST 5

/* Bit 4 - BMODE: Byte Transfer Count Mode
 *   1 = Byte-by-byte mode: TCNT decrements per byte but does not
 *       terminate the transfer.  Each byte must be individually
 *       managed by software or DMA.
 *   0 = Total count mode: the transfer runs until TCNT reaches
 *       zero, then automatically stops.  TCZIF is set when the
 *       count expires.                                              */
#define SPI1CON0_BMODE 4

/* Bits 3:0 - Reserved: read as 0, write as 0.                      */

/* ===================================================================
 * SPI1CON1 - SPI1 Control Register 1
 * Address: 0x0085
 *
 * Clock configuration, signal polarity, and Slave Select control.
 * These settings determine the SPI clock mode and I/O behaviour.
 * ================================================================ */
#define SPI1CON1 SFR8(0x0085)

/* Bit 7 - SMP: Input Data Sample Phase
 *   1 = Input data sampled at the end of data output time.
 *   0 = Input data sampled at the middle of data output time.
 *   Typically set to 1 for higher clock frequencies to meet
 *   setup time requirements.                                        */
#define SPI1CON1_SMP 7

/* Bit 6 - CKE: Clock Edge Select (transmit timing)
 *   1 = Serial data changes on the transition from active clock
 *       state to idle clock state (active-to-idle edge).
 *   0 = Serial data changes on the transition from idle clock
 *       state to active clock state (idle-to-active edge).
 *   Combined with CKP to select one of four SPI clock modes.       */
#define SPI1CON1_CKE 6

/* Bit 5 - CKP: Clock Polarity Select
 *   1 = Idle state for SCK is high.
 *   0 = Idle state for SCK is low.
 *   Combined with CKE to select one of four SPI clock modes.       */
#define SPI1CON1_CKP 5

/* Bit 4 - FST: Fast Start Enable
 *   1 = The first data bit is transmitted on the first SCK edge
 *       after SS goes active, with no half-clock setup delay.
 *   0 = Standard timing: a half-clock delay occurs before the
 *       first SCK edge.                                             */
#define SPI1CON1_FST 4

/* Bit 3 - SSP: Slave Select Polarity
 *   1 = SS pin is active-high (asserted when high).
 *   0 = SS pin is active-low (asserted when low, standard).        */
#define SPI1CON1_SSP 3

/* Bit 2 - SDIP: SDI (Serial Data In) Polarity
 *   1 = SDI data is inverted (active-low signaling).
 *   0 = SDI data is not inverted (standard positive logic).        */
#define SPI1CON1_SDIP 2

/* Bit 1 - SDOP: SDO (Serial Data Out) Polarity
 *   1 = SDO data is inverted (active-low signaling).
 *   0 = SDO data is not inverted (standard positive logic).        */
#define SPI1CON1_SDOP 1

/* Bit 0 - SSET: Slave Select Output Enable (host mode only)
 *   1 = SS pin is driven by the SPI module in host mode.
 *       SS is asserted during transfers and de-asserted between.
 *   0 = SS pin is not controlled by the SPI module.
 *       Application must manage the SS pin via GPIO if needed.      */
#define SPI1CON1_SSET 0

/* ===================================================================
 * SPI1CON2 - SPI1 Control Register 2
 * Address: 0x0086
 *
 * Status and control for transfer busy state, SS filtering,
 * buffer management, and FIFO readiness.
 * ================================================================ */
#define SPI1CON2 SFR8(0x0086)

/* Bit 7 - BUSY: SPI Busy Status (read-only)
 *   1 = A transfer is currently in progress.
 *   0 = No transfer active; module is idle.                         */
#define SPI1CON2_BUSY 7

/* Bit 6 - TXWE: Transmit Write Error
 *   1 = A write to SPI1TXB was attempted while the TX FIFO was
 *       full.  The written byte was discarded.
 *   0 = No transmit write error has occurred.
 *   Cleared by software writing 0 or by clearing buffers (CLB).    */
#define SPI1CON2_TXWE 6

/* Bit 5 - SSFLT: Slave Select Input Filter Enable
 *   1 = Digital noise filter enabled on the SS input pin.
 *   0 = No filter on SS input (default).
 *   Useful in noisy environments to prevent false SS detection.     */
#define SPI1CON2_SSFLT 5

/* Bit 4 - SSET: Slave Select Output Set
 *   1 = In host mode, the SS output is forced to its active state.
 *   0 = SS output follows normal transfer-based assertion.
 *   Allows software to manually control the SS pin state.           */
#define SPI1CON2_SSET 4

/* Bit 3 - CLB: Clear Buffers
 *   1 = Clear both TX and RX FIFOs.  Self-clearing: hardware
 *       resets this bit to 0 after the buffers are flushed.
 *   0 = No action / buffer clear complete.
 *   Writing 1 also resets the FIFO pointers and status flags.       */
#define SPI1CON2_CLB 3

/* Bit 2 - Reserved: read as 0, write as 0.                          */

/* Bit 1 - TXR: Transmit Ready (read-only)
 *   1 = TX FIFO has space for at least one byte; safe to write
 *       to SPI1TXB.
 *   0 = TX FIFO is full; do not write to SPI1TXB.                  */
#define SPI1CON2_TXR 1

/* Bit 0 - RXR: Receive Ready (read-only)
 *   1 = RX FIFO contains at least one unread byte; a read from
 *       SPI1RXB will return valid data.
 *   0 = RX FIFO is empty.                                          */
#define SPI1CON2_RXR 0

/* ===================================================================
 * SPI1STATUS - SPI1 Status Register
 * Address: 0x0087
 *
 * Read-only FIFO status and error indicators.
 * ================================================================ */
#define SPI1STATUS SFR8(0x0087)

/* Bit 7 - RXBF: Receive Buffer Full (read-only)
 *   1 = The 2-byte RX FIFO is completely full.  Further received
 *       bytes will be lost and will set the RXOIF overflow flag.
 *   0 = RX FIFO has space for at least one more byte.               */
#define SPI1STATUS_RXBF 7

/* Bit 5 - TXBE: Transmit Buffer Empty (read-only)
 *   1 = The 2-byte TX FIFO is completely empty.  If a transfer is
 *       active, a TX underflow (TXUIF) will occur.
 *   0 = TX FIFO contains at least one byte awaiting transmission.   */
#define SPI1STATUS_TXBE 5

/* Bit 1 - RXRE: Receive Read Error (read-only)
 *   1 = A read from SPI1RXB was attempted while the RX FIFO was
 *       empty.  The returned data is invalid.
 *   0 = No receive read error has occurred.                         */
#define SPI1STATUS_RXRE 1

/* Bits 6, 4:2, 0 - Reserved: read as 0.                            */

/* ===================================================================
 * SPI1TWIDTH - SPI1 Transfer Width Register
 * Address: 0x0088
 *
 * Configures the number of bits per byte transfer.  This allows
 * the SPI module to communicate with devices that use non-standard
 * word sizes (e.g., 12-bit DACs, 10-bit ADCs).
 * ================================================================ */
#define SPI1TWIDTH SFR8(0x0088)

/* Bits 2:0 - TWIDTH[2:0]: Transfer Width
 *   Number of bits transferred per byte = TWIDTH + 1 (when nonzero).
 *   000 = 8 bits per byte (default; standard SPI)
 *   001 = 1 bit per byte
 *   010 = 2 bits per byte
 *   011 = 3 bits per byte
 *   100 = 4 bits per byte
 *   101 = 5 bits per byte
 *   110 = 6 bits per byte
 *   111 = 7 bits per byte
 *
 *   Note: When TWIDTH = 0, the module transfers the standard 8 bits.
 *   For nonzero values, TWIDTH directly gives the bit count.        */
#define SPI1TWIDTH_TWIDTH2 2
#define SPI1TWIDTH_TWIDTH1 1
#define SPI1TWIDTH_TWIDTH0 0

/* TWIDTH field mask and position                                     */
#define SPI1TWIDTH_MASK 0x07 /* bits 2:0 */
#define SPI1TWIDTH_POS 0

/* Bits 7:3 - Reserved: read as 0, write as 0.                       */

/* ===================================================================
 * SPI1BAUD - SPI1 Baud Rate Register
 * Address: 0x0089
 *
 * Sets the serial clock (SCK) frequency in host mode.
 *
 * SCK frequency = F_clocksource / (2 * (BAUD + 1))
 *
 * where F_clocksource is selected by SPI1CLK.CLKSEL[4:0].
 *
 * Examples (assuming 64 MHz source):
 *   BAUD = 0x00 => SCK = 32.0 MHz
 *   BAUD = 0x01 => SCK = 16.0 MHz
 *   BAUD = 0x03 => SCK =  8.0 MHz
 *   BAUD = 0x07 => SCK =  4.0 MHz
 *   BAUD = 0x0F => SCK =  2.0 MHz
 *   BAUD = 0x1F => SCK =  1.0 MHz
 *   BAUD = 0xFF => SCK = 125.0 kHz
 *
 * In client mode this register is not used; the clock is provided
 * externally by the host device.
 * ================================================================ */
#define SPI1BAUD SFR8(0x0089)

/* Bits 7:0 - BAUD[7:0]: Baud Rate Divider
 *   Determines the SCK period.  See formula above.                  */

/* ===================================================================
 * SPI1INTF - SPI1 Interrupt Flag Register
 * Address: 0x008A
 *
 * Contains interrupt flags for SPI1 events.  Each flag is set by
 * hardware when the corresponding event occurs.  Flags must be
 * cleared by software writing 0 to the bit.
 * ================================================================ */
#define SPI1INTF SFR8(0x008A)

/* Bit 7 - SRMTIF: Shift Register Empty Interrupt Flag
 *   1 = The shift register is empty; all bits of the last byte
 *       have been clocked out.  Indicates the transfer is fully
 *       complete at the wire level (not just FIFO level).
 *   0 = Shift register has data or is idle.                         */
#define SPI1INTF_SRMTIF 7

/* Bit 6 - TCZIF: Transfer Count Zero Interrupt Flag
 *   1 = The SPI1TCNT counter has decremented to zero.
 *       In total-count mode (BMODE=0), this signals the planned
 *       number of bytes have been exchanged.
 *   0 = Transfer count has not reached zero.                        */
#define SPI1INTF_TCZIF 6

/* Bit 5 - SOSIF: Start of Slave Select Interrupt Flag
 *   1 = The SS pin transitioned to its active state.
 *       In client mode, indicates the host has selected this device.
 *   0 = No SS active edge detected.                                 */
#define SPI1INTF_SOSIF 5

/* Bit 4 - EOSIF: End of Slave Select Interrupt Flag
 *   1 = The SS pin transitioned to its inactive state.
 *       In client mode, indicates the host has de-selected this
 *       device and the transfer is complete.
 *   0 = No SS inactive edge detected.                               */
#define SPI1INTF_EOSIF 4

/* Bits 3:2 - Reserved: read as 0, write as 0.                       */

/* Bit 1 - RXOIF: Receive Overflow Interrupt Flag
 *   1 = A byte was received while the RX FIFO was already full.
 *       The incoming byte was lost.
 *   0 = No receive overflow has occurred.                           */
#define SPI1INTF_RXOIF 1

/* Bit 0 - TXUIF: Transmit Underflow Interrupt Flag
 *   1 = A byte transmission was attempted while the TX FIFO was
 *       empty.  Stale/undefined data was shifted out.
 *   0 = No transmit underflow has occurred.                         */
#define SPI1INTF_TXUIF 0

/* ===================================================================
 * SPI1INTE - SPI1 Interrupt Enable Register
 * Address: 0x008B
 *
 * Each bit enables the corresponding interrupt source from SPI1INTF.
 * When an enable bit and its corresponding flag are both set, the
 * SPI1 interrupt request is asserted to the interrupt controller.
 * ================================================================ */
#define SPI1INTE SFR8(0x008B)

/* Bit 7 - SRMTIE: Shift Register Empty Interrupt Enable
 *   1 = Interrupt enabled when SRMTIF is set.
 *   0 = Interrupt disabled.                                         */
#define SPI1INTE_SRMTIE 7

/* Bit 6 - TCZIE: Transfer Count Zero Interrupt Enable
 *   1 = Interrupt enabled when TCZIF is set.
 *   0 = Interrupt disabled.                                         */
#define SPI1INTE_TCZIE 6

/* Bit 5 - SOSIE: Start of Slave Select Interrupt Enable
 *   1 = Interrupt enabled when SOSIF is set.
 *   0 = Interrupt disabled.                                         */
#define SPI1INTE_SOSIE 5

/* Bit 4 - EOSIE: End of Slave Select Interrupt Enable
 *   1 = Interrupt enabled when EOSIF is set.
 *   0 = Interrupt disabled.                                         */
#define SPI1INTE_EOSIE 4

/* Bits 3:2 - Reserved: read as 0, write as 0.                       */

/* Bit 1 - RXOIE: Receive Overflow Interrupt Enable
 *   1 = Interrupt enabled when RXOIF is set.
 *   0 = Interrupt disabled.                                         */
#define SPI1INTE_RXOIE 1

/* Bit 0 - TXUIE: Transmit Underflow Interrupt Enable
 *   1 = Interrupt enabled when TXUIF is set.
 *   0 = Interrupt disabled.                                         */
#define SPI1INTE_TXUIE 0

/* ===================================================================
 * SPI1CLK - SPI1 Clock Source Select Register
 * Address: 0x008C
 *
 * Selects the clock source that feeds the baud rate generator in
 * host mode.  The specific mapping of CLKSEL values to clock
 * sources is device-dependent; refer to the "SPI Clock Source
 * Selection" table in DS40002213D for the complete enumeration.
 * Common sources include FOSC, HFINTOSC, MFINTOSC, and LFINTOSC.
 * ================================================================ */
#define SPI1CLK SFR8(0x008C)

/* Bits 4:0 - CLKSEL[4:0]: Clock Source Select
 *   Selects the input clock for the SPI baud rate generator.
 *   Refer to DS40002213D for the full mapping.                      */
#define SPI1CLK_CLKSEL4 4
#define SPI1CLK_CLKSEL3 3
#define SPI1CLK_CLKSEL2 2
#define SPI1CLK_CLKSEL1 1
#define SPI1CLK_CLKSEL0 0

/* CLKSEL field mask and position                                     */
#define SPI1CLK_CLKSEL_MASK 0x1F /* bits 4:0 */
#define SPI1CLK_CLKSEL_POS 0

/* Bits 7:5 - Reserved: read as 0, write as 0.                       */

/* *******************************************************************
 *
 *  SPI2 - Serial Peripheral Interface, Instance 2
 *
 *  Register layout is identical to SPI1.  Bit definitions, masks,
 *  and field positions are the same; only the base addresses differ.
 *  Refer to the SPI1 register descriptions above for full details.
 *
 * ******************************************************************* */

/* ===================================================================
 * SPI2RXB - SPI2 Receive Buffer Register
 * Address: 0x008D
 * ================================================================ */
#define SPI2RXB SFR8(0x008D)

/* ===================================================================
 * SPI2TXB - SPI2 Transmit Buffer Register
 * Address: 0x008E
 * ================================================================ */
#define SPI2TXB SFR8(0x008E)

/* ===================================================================
 * SPI2TCNT - SPI2 Transfer Byte Counter (16-bit, little-endian)
 * Address: 0x008F (low byte), 0x0090 (high byte)
 * ================================================================ */
#define SPI2TCNTL SFR8(0x008F) /* Count bits  7:0         */
#define SPI2TCNTH SFR8(0x0090) /* Count bits 10:8         */
#define SPI2TCNT SFR16(0x008F) /* 16-bit access (L:H)     */

#define SPI2TCNTH_MASK 0x07 /* bits 2:0 valid */

/* ===================================================================
 * SPI2CON0 - SPI2 Control Register 0
 * Address: 0x0091
 * ================================================================ */
#define SPI2CON0 SFR8(0x0091)

#define SPI2CON0_EN 7
#define SPI2CON0_LSBF 6
#define SPI2CON0_MST 5
#define SPI2CON0_BMODE 4

/* ===================================================================
 * SPI2CON1 - SPI2 Control Register 1
 * Address: 0x0092
 * ================================================================ */
#define SPI2CON1 SFR8(0x0092)

#define SPI2CON1_SMP 7
#define SPI2CON1_CKE 6
#define SPI2CON1_CKP 5
#define SPI2CON1_FST 4
#define SPI2CON1_SSP 3
#define SPI2CON1_SDIP 2
#define SPI2CON1_SDOP 1
#define SPI2CON1_SSET 0

/* ===================================================================
 * SPI2CON2 - SPI2 Control Register 2
 * Address: 0x0093
 * ================================================================ */
#define SPI2CON2 SFR8(0x0093)

#define SPI2CON2_BUSY 7
#define SPI2CON2_TXWE 6
#define SPI2CON2_SSFLT 5
#define SPI2CON2_SSET 4
#define SPI2CON2_CLB 3
#define SPI2CON2_TXR 1
#define SPI2CON2_RXR 0

/* ===================================================================
 * SPI2STATUS - SPI2 Status Register
 * Address: 0x0094
 * ================================================================ */
#define SPI2STATUS SFR8(0x0094)

#define SPI2STATUS_RXBF 7
#define SPI2STATUS_TXBE 5
#define SPI2STATUS_RXRE 1

/* ===================================================================
 * SPI2TWIDTH - SPI2 Transfer Width Register
 * Address: 0x0095
 * ================================================================ */
#define SPI2TWIDTH SFR8(0x0095)

#define SPI2TWIDTH_TWIDTH2 2
#define SPI2TWIDTH_TWIDTH1 1
#define SPI2TWIDTH_TWIDTH0 0

#define SPI2TWIDTH_MASK 0x07 /* bits 2:0 */
#define SPI2TWIDTH_POS 0

/* ===================================================================
 * SPI2BAUD - SPI2 Baud Rate Register
 * Address: 0x0096
 * ================================================================ */
#define SPI2BAUD SFR8(0x0096)

/* ===================================================================
 * SPI2INTF - SPI2 Interrupt Flag Register
 * Address: 0x0097
 * ================================================================ */
#define SPI2INTF SFR8(0x0097)

#define SPI2INTF_SRMTIF 7
#define SPI2INTF_TCZIF 6
#define SPI2INTF_SOSIF 5
#define SPI2INTF_EOSIF 4
#define SPI2INTF_RXOIF 1
#define SPI2INTF_TXUIF 0

/* ===================================================================
 * SPI2INTE - SPI2 Interrupt Enable Register
 * Address: 0x0098
 * ================================================================ */
#define SPI2INTE SFR8(0x0098)

#define SPI2INTE_SRMTIE 7
#define SPI2INTE_TCZIE 6
#define SPI2INTE_SOSIE 5
#define SPI2INTE_EOSIE 4
#define SPI2INTE_RXOIE 1
#define SPI2INTE_TXUIE 0

/* ===================================================================
 * SPI2CLK - SPI2 Clock Source Select Register
 * Address: 0x0099
 * ================================================================ */
#define SPI2CLK SFR8(0x0099)

#define SPI2CLK_CLKSEL4 4
#define SPI2CLK_CLKSEL3 3
#define SPI2CLK_CLKSEL2 2
#define SPI2CLK_CLKSEL1 1
#define SPI2CLK_CLKSEL0 0

#define SPI2CLK_CLKSEL_MASK 0x1F /* bits 4:0 */
#define SPI2CLK_CLKSEL_POS 0

#endif /* PIC18F_Q84_SPI_H */
