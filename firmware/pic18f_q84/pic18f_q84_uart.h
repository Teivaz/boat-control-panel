/*
 * pic18f_q84_uart.h
 *
 * UART (Universal Asynchronous Receiver Transmitter) with Protocol Support
 * register definitions for PIC18F27/47/57Q84 microcontrollers.
 *
 * The device contains five independent UART modules (U1 through U5),
 * each capable of full-duplex asynchronous serial communication with
 * extensive protocol support.
 *
 * Supported protocols and modes (selected via MODE[3:0] in UxCON0):
 *   - Asynchronous 8-bit UART (standard RS-232 framing)
 *   - Asynchronous 7-bit UART (for ASCII-only applications)
 *   - Asynchronous 9-bit UART (9th bit carried in UxP1)
 *   - LIN host mode (Local Interconnect Network bus master)
 *   - LIN client mode (LIN bus slave with auto-baud on sync field)
 *   - DMX transmit (DMX512 lighting control protocol)
 *   - DMX receive
 *   - DALI Control gear (Digital Addressable Lighting Interface)
 *   - DALI Control device
 *
 * Key features:
 *   - 16-bit baud rate generator with high/low speed select
 *   - Auto-baud detection (waits for 0x55 sync byte)
 *   - Hardware flow control (RTS/CTS or XON/XOFF)
 *   - Configurable stop bits (1, 1.5, or 2)
 *   - TX/RX polarity inversion
 *   - Break character generation and detection
 *   - Protocol-specific parameters (UxP1, UxP2, UxP3 registers)
 *   - DMA-compatible data buffers
 *   - Comprehensive error detection (framing, overflow, checksum, parity)
 *   - Wake-up from Sleep on received data
 *
 * Instance differences:
 *   U1 and U2 include RXCHK and TXCHK checksum registers for hardware
 *   checksum computation in LIN and other protocol modes.  U3, U4, and
 *   U5 do NOT have checksum registers; those addresses are reserved.
 *
 * Register block per instance (using U1 as the canonical example):
 *   UxRXB     - Receive Data Buffer (read-only)
 *   UxRXCHK   - Receive Checksum (U1/U2 only)
 *   UxTXB     - Transmit Data Buffer (write to queue)
 *   UxTXCHK   - Transmit Checksum (U1/U2 only)
 *   UxP1L/H   - Protocol Parameter 1 (16-bit, 9 bits used)
 *   UxP2L/H   - Protocol Parameter 2 (16-bit, 9 bits used)
 *   UxP3L/H   - Protocol Parameter 3 (16-bit, 9 bits used)
 *   UxCON0    - Control Register 0 (baud speed, TX/RX enable, mode)
 *   UxCON1    - Control Register 1 (module on, overflow, break, polarity)
 *   UxCON2    - Control Register 2 (flow control, stop bits, error control)
 *   UxBRGL/H  - Baud Rate Generator (16-bit)
 *   UxFIFO    - FIFO Status
 *   UxUIR     - UART Interrupt Request flags
 *   UxERRIR   - Error Interrupt Request flags
 *   UxERRIE   - Error Interrupt Enable bits
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 30 - UART Module
 *
 * Requires: SFR8(addr) and SFR16(addr) macros defined externally
 *           (provide volatile uint8_t / uint16_t access to absolute
 *           SFR addresses).
 */

#ifndef PIC18F_Q84_UART_H
#define PIC18F_Q84_UART_H

/* =====================================================================
 *  UART1 (U1) Register Addresses
 *  Base: 0x02A0
 *
 *  U1 has the full register set including RXCHK and TXCHK checksum
 *  registers for LIN and protocol mode hardware checksums.
 * =================================================================== */

/* U1RXB - UART1 Receive Data Buffer (0x02A0, read-only)
 *
 * Bits 7:0 - RXB[7:0]: Received byte.
 *   Contains the most recently received data byte (minus start/stop
 *   bits and parity).  Read this register to retrieve data and
 *   advance the receive FIFO.  Writing has no effect.               */
#define U1RXB SFR8(0x02A1)

/* U1RXCHK - UART1 Receive Checksum (0x02A1)
 *
 * Bits 7:0 - RXCHK[7:0]: Auto-calculated receive checksum.
 *   Hardware accumulates a running checksum of received data bytes
 *   in protocol modes (LIN, etc.).  The checksum algorithm depends
 *   on the selected protocol mode and C0EN setting.
 *   Available on U1 and U2 only.                                    */
#define U1RXCHK SFR8(0x02A2)

/* U1TXB - UART1 Transmit Data Buffer (0x02A2, write-only)
 *
 * Bits 7:0 - TXB[7:0]: Transmit byte.
 *   Write to this register to queue a byte for transmission.  The
 *   byte is transferred to the transmit shift register when the
 *   previous byte has finished sending.  Reading returns the last
 *   value written.                                                  */
#define U1TXB SFR8(0x02A3)

/* U1TXCHK - UART1 Transmit Checksum (0x02A3)
 *
 * Bits 7:0 - TXCHK[7:0]: Auto-calculated transmit checksum.
 *   Hardware accumulates a running checksum of transmitted data
 *   bytes in protocol modes.  Can be read to obtain the checksum
 *   for appending to a protocol frame.
 *   Available on U1 and U2 only.                                    */
#define U1TXCHK SFR8(0x02A4)

/* U1P1 - UART1 Protocol Parameter 1 (0x02A4, 16-bit)
 *
 * Bits 8:0 - P1[8:0]: Protocol parameter 1 (9 bits used).
 *   Usage depends on the selected operating mode:
 *     Async 9-bit mode: bit 0 = 9th data bit (TX and RX)
 *     LIN modes: break field length / sync parameters
 *     DMX/DALI: protocol-specific timing/addressing
 *   Bits 15:9 of P1H are reserved (write as 0).                    */
#define U1P1L SFR8(0x02A5)
#define U1P1H SFR8(0x02A6)
#define U1P1 SFR16(0x02A5)

/* U1P2 - UART1 Protocol Parameter 2 (0x02A6, 16-bit)
 *
 * Bits 8:0 - P2[8:0]: Protocol parameter 2 (9 bits used).
 *   Protocol-specific parameter.  Refer to DS40002213D for usage
 *   in each UART mode.
 *   Bits 15:9 of P2H are reserved (write as 0).                    */
#define U1P2L SFR8(0x02A7)
#define U1P2H SFR8(0x02A8)
#define U1P2 SFR16(0x02A7)

/* U1P3 - UART1 Protocol Parameter 3 (0x02A8, 16-bit)
 *
 * Bits 8:0 - P3[8:0]: Protocol parameter 3 (9 bits used).
 *   Protocol-specific parameter.  Refer to DS40002213D for usage
 *   in each UART mode.
 *   Bits 15:9 of P3H are reserved (write as 0).                    */
#define U1P3L SFR8(0x02A9)
#define U1P3H SFR8(0x02AA)
#define U1P3 SFR16(0x02A9)

/* Address 0x02AA is reserved/unimplemented for U1 */

/* U1CON0 - UART1 Control Register 0 (0x02AB)
 * See bit definitions in the shared section below.                  */
#define U1CON0 SFR8(0x02AB)

/* U1CON1 - UART1 Control Register 1 (0x02AC)
 * See bit definitions in the shared section below.                  */
#define U1CON1 SFR8(0x02AC)

/* U1CON2 - UART1 Control Register 2 (0x02AD)
 * See bit definitions in the shared section below.                  */
#define U1CON2 SFR8(0x02AD)

/* U1BRG - UART1 Baud Rate Generator (0x02AE, 16-bit)
 *
 * Bits 15:0 - BRG[15:0]: Baud rate divisor.
 *   Baud Rate = Fosc / (4 * (BRG + 1))   when BRGS = 1 (high speed)
 *   Baud Rate = Fosc / (16 * (BRG + 1))  when BRGS = 0 (low speed)
 *
 *   Example: For 9600 baud at Fosc = 64 MHz, BRGS = 1:
 *     BRG = (64000000 / (4 * 9600)) - 1 = 1666 = 0x0682            */
#define U1BRGL SFR8(0x02AE)
#define U1BRGH SFR8(0x02AF)
#define U1BRG SFR16(0x02AE)

/* U1FIFO - UART1 FIFO Status Register (0x02B0)
 * See bit definitions in the shared section below.                  */
#define U1FIFO SFR8(0x02B0)

/* U1UIR - UART1 Interrupt Request Register (0x02B1)
 * See bit definitions in the shared section below.                  */
#define U1UIR SFR8(0x02B1)

/* U1ERRIR - UART1 Error Interrupt Request Register (0x02B2)
 * See bit definitions in the shared section below.                  */
#define U1ERRIR SFR8(0x02B2)

/* U1ERRIE - UART1 Error Interrupt Enable Register (0x02B3)
 * See bit definitions in the shared section below.                  */
#define U1ERRIE SFR8(0x02B3)

/* =====================================================================
 *  UART2 (U2) Register Addresses
 *  Base: 0x02B4
 *
 *  U2 also has the full register set including RXCHK and TXCHK.
 * =================================================================== */
#define U2RXB SFR8(0x02B4)
#define U2RXCHK SFR8(0x02B5)
#define U2TXB SFR8(0x02B6)
#define U2TXCHK SFR8(0x02B7)

#define U2P1L SFR8(0x02B8)
#define U2P1H SFR8(0x02B9)
#define U2P1 SFR16(0x02B8)
#define U2P2L SFR8(0x02BA)
#define U2P2H SFR8(0x02BB)
#define U2P2 SFR16(0x02BA)
#define U2P3L SFR8(0x02BC)
#define U2P3H SFR8(0x02BD)
#define U2P3 SFR16(0x02BC)

/* Address 0x02BD already used by U2P3H; 0x02BE is CON0 */
#define U2CON0 SFR8(0x02BE)
#define U2CON1 SFR8(0x02BF)
#define U2CON2 SFR8(0x02C0)

#define U2BRGL SFR8(0x02C1)
#define U2BRGH SFR8(0x02C2)
#define U2BRG SFR16(0x02C1)

#define U2FIFO SFR8(0x02C3)
#define U2UIR SFR8(0x02C4)
#define U2ERRIR SFR8(0x02C5)
#define U2ERRIE SFR8(0x02C6)

/* =====================================================================
 *  UART3 (U3) Register Addresses
 *  Base: 0x02C7
 *
 *  U3 does NOT have RXCHK or TXCHK registers.  The addresses between
 *  RXB and TXB (and between TXB and P1) that would correspond to
 *  checksum registers are reserved.
 * =================================================================== */
#define U3RXB SFR8(0x02C7)
/* 0x02C8 reserved (no U3RXCHK) */
#define U3TXB SFR8(0x02C9)
/* 0x02CA reserved (no U3TXCHK) */

#define U3P1L SFR8(0x02CB)
#define U3P1H SFR8(0x02CC)
#define U3P1 SFR16(0x02CB)
#define U3P2L SFR8(0x02CD)
#define U3P2H SFR8(0x02CE)
#define U3P2 SFR16(0x02CD)
#define U3P3L SFR8(0x02CF)
#define U3P3H SFR8(0x02D0)
#define U3P3 SFR16(0x02CF)

#define U3CON0 SFR8(0x02D1)
#define U3CON1 SFR8(0x02D2)
#define U3CON2 SFR8(0x02D3)

#define U3BRGL SFR8(0x02D4)
#define U3BRGH SFR8(0x02D5)
#define U3BRG SFR16(0x02D4)

#define U3FIFO SFR8(0x02D6)
#define U3UIR SFR8(0x02D7)
#define U3ERRIR SFR8(0x02D8)
#define U3ERRIE SFR8(0x02D9)

/* =====================================================================
 *  UART4 (U4) Register Addresses
 *  Base: 0x02DA
 *
 *  U4 does NOT have RXCHK or TXCHK registers.
 * =================================================================== */
#define U4RXB SFR8(0x02DA)
/* 0x02DB reserved (no U4RXCHK) */
#define U4TXB SFR8(0x02DC)
/* 0x02DD reserved (no U4TXCHK) */

#define U4P1L SFR8(0x02DE)
#define U4P1H SFR8(0x02DF)
#define U4P1 SFR16(0x02DE)
#define U4P2L SFR8(0x02E0)
#define U4P2H SFR8(0x02E1)
#define U4P2 SFR16(0x02E0)
#define U4P3L SFR8(0x02E2)
#define U4P3H SFR8(0x02E3)
#define U4P3 SFR16(0x02E2)

#define U4CON0 SFR8(0x02E4)
#define U4CON1 SFR8(0x02E5)
#define U4CON2 SFR8(0x02E6)

#define U4BRGL SFR8(0x02E7)
#define U4BRGH SFR8(0x02E8)
#define U4BRG SFR16(0x02E7)

#define U4FIFO SFR8(0x02E9)
#define U4UIR SFR8(0x02EA)
#define U4ERRIR SFR8(0x02EB)
#define U4ERRIE SFR8(0x02EC)

/* =====================================================================
 *  UART5 (U5) Register Addresses
 *  Base: 0x02ED
 *
 *  U5 does NOT have RXCHK or TXCHK registers.
 * =================================================================== */
#define U5RXB SFR8(0x02ED)
/* 0x02EE reserved (no U5RXCHK) */
#define U5TXB SFR8(0x02EF)
/* 0x02F0 reserved (no U5TXCHK) */

#define U5P1L SFR8(0x02F1)
#define U5P1H SFR8(0x02F2)
#define U5P1 SFR16(0x02F1)
#define U5P2L SFR8(0x02F3)
#define U5P2H SFR8(0x02F4)
#define U5P2 SFR16(0x02F3)
#define U5P3L SFR8(0x02F5)
#define U5P3H SFR8(0x02F6)
#define U5P3 SFR16(0x02F5)

#define U5CON0 SFR8(0x02F7)
#define U5CON1 SFR8(0x02F8)
#define U5CON2 SFR8(0x02F9)

#define U5BRGL SFR8(0x02FA)
#define U5BRGH SFR8(0x02FB)
#define U5BRG SFR16(0x02FA)

#define U5FIFO SFR8(0x02FC)
#define U5UIR SFR8(0x02FD)
#define U5ERRIR SFR8(0x02FE)
#define U5ERRIE SFR8(0x02FF)

/* =====================================================================
 *  Shared Bit Definitions
 *
 *  All five UART instances use identical bit layouts within their
 *  control, status, and interrupt registers.  The bit masks below
 *  apply to any UxCON0, UxCON1, UxCON2, UxFIFO, UxUIR, UxERRIR,
 *  and UxERRIE register.
 *
 *  Usage example:
 *    U1CON0 = UCON0_BRGS | UCON0_TXEN | UCON0_RXEN | UMODE_ASYNC_8BIT;
 *    U1CON1 |= UCON1_ON;
 *    if (U1FIFO & UFIFO_RXBF) { data = U1RXB; }
 * =================================================================== */

/* =====================================================================
 *  UxCON0 - UART Control Register 0
 *
 *  Controls baud rate generator speed, transmitter/receiver enable,
 *  and the operating mode (protocol) selection.
 *
 *  Bit 7    BRGS     Baud Rate Generator Speed select
 *  Bit 6    --       Reserved (read as 0)
 *  Bit 5    TXEN     Transmit Enable
 *  Bit 4    RXEN     Receive Enable
 *  Bits 3:0 MODE[3:0] UART Operating Mode select
 * =================================================================== */

/* Bit 7 - BRGS: Baud Rate Generator Speed
 *   1 = High-speed mode.  BRG divides by 4:
 *       Baud = Fosc / (4 * (UxBRG + 1))
 *   0 = Low-speed mode.  BRG divides by 16:
 *       Baud = Fosc / (16 * (UxBRG + 1))
 *   High-speed mode provides finer baud rate resolution at high
 *   frequencies, while low-speed mode offers better noise immunity.  */
#define UCON0_BRGS (1u << 7)

/* Bit 5 - TXEN: Transmit Enable
 *   1 = Transmitter is enabled.  Writing to UxTXB initiates
 *       transmission of the byte (when shift register is empty).
 *   0 = Transmitter is disabled.  TX pin is released (tri-state)
 *       and any pending transmission is aborted.                     */
#define UCON0_TXEN (1u << 5)

/* Bit 4 - RXEN: Receive Enable
 *   1 = Receiver is enabled.  Incoming serial data on the RX pin
 *       is sampled and assembled into bytes in UxRXB.
 *   0 = Receiver is disabled.  No data reception occurs.            */
#define UCON0_RXEN (1u << 4)

/* Bits 3:0 - MODE[3:0]: UART Operating Mode
 *   Selects the protocol and data framing used by the UART module.
 *   Refer to named constants below for mode values.                  */
#define UCON0_MODE3 (1u << 3)
#define UCON0_MODE2 (1u << 2)
#define UCON0_MODE1 (1u << 1)
#define UCON0_MODE0 (1u << 0)
#define UCON0_MODE_MASK 0x0Fu

/* MODE[3:0] field values (pre-positioned, OR directly into UxCON0)
 *
 *   Asynchronous modes use standard NRZ (Non-Return-to-Zero) framing
 *   with configurable start, data, parity, and stop bits.
 *
 *   Protocol modes (LIN, DMX, DALI) configure the UART for specific
 *   bus protocols with hardware support for framing, timing, and
 *   checksums (on U1/U2).                                            */
#define UMODE_ASYNC_8BIT 0x00u /* Async 8-bit UART (standard)    */
#define UMODE_ASYNC_7BIT 0x01u /* Async 7-bit UART               */
#define UMODE_ASYNC_9BIT 0x02u /* Async 9-bit UART (9th in P1)   */
#define UMODE_LIN_HOST 0x03u   /* LIN host (master) mode         */
#define UMODE_LIN_CLIENT 0x04u /* LIN client (slave) mode        */
#define UMODE_DMX_TX 0x05u     /* DMX512 transmit                */
#define UMODE_DMX_RX 0x06u     /* DMX512 receive                 */
#define UMODE_DALI_CTRL 0x07u  /* DALI Control gear              */
#define UMODE_DALI_DEV 0x08u   /* DALI Control device            */
/* Values 0x09-0x0F are reserved                                      */

/* =====================================================================
 *  UxCON1 - UART Control Register 1
 *
 *  Controls module enable, overflow behaviour, break detection/
 *  generation, and transmit/receive signal polarity.
 *
 *  Bit 7    ON       UART Module On
 *  Bit 6    RUNOVF   Run During Overflow
 *  Bit 5    --       Reserved
 *  Bit 4    RXBIMD   Receive Break Interrupt Mode
 *  Bit 3    BRKOVR   Break Override
 *  Bit 2    SENDB    Send Break Character
 *  Bit 1    TXPOL    Transmit Polarity Invert
 *  Bit 0    RXPOL    Receive Polarity Invert
 * =================================================================== */

/* Bit 7 - ON: UART Module On
 *   1 = UART module is enabled.  I/O pins are controlled by the
 *       UART (via PPS assignments).  Internal clocks and logic are
 *       active.
 *   0 = UART module is disabled.  I/O pins revert to their PORT
 *       functions.  Module draws minimal current.
 *   Note: The module must be ON before TXEN/RXEN have any effect.    */
#define UCON1_ON (1u << 7)

/* Bit 6 - RUNOVF: Run During Overflow
 *   1 = Receiver continues operating even when the receive buffer
 *       is full (overflow condition).  New bytes overwrite the
 *       oldest data in the FIFO.
 *   0 = Receiver stops when the buffer is full.  Incoming data is
 *       lost until the buffer is read.                               */
#define UCON1_RUNOVF (1u << 6)

/* Bit 4 - RXBIMD: Receive Break Interrupt Mode
 *   1 = RXBKIF (Break Interrupt Flag) is set at the START of a
 *       detected Break condition.
 *   0 = RXBKIF is set at the END of a detected Break condition
 *       (after the Break delimiter is received).                     */
#define UCON1_RXBIMD (1u << 4)

/* Bit 3 - BRKOVR: Break Override
 *   1 = TX output is held low continuously (manual Break).  The
 *       transmitter does not send data while BRKOVR = 1.
 *   0 = Normal TX operation.
 *   Used for generating arbitrarily long Break conditions on the
 *   bus (e.g., for LIN bus reset or custom protocols).               */
#define UCON1_BRKOVR (1u << 3)

/* Bit 2 - SENDB: Send Break Character
 *   Write 1 = Queue a Break character for transmission.  The Break
 *             is sent after any currently transmitting byte completes.
 *             Hardware clears this bit automatically after the Break
 *             has been queued into the shift register.
 *   Read:   Current state of the bit (1 = Break pending).
 *   The Break character consists of a start bit followed by 12 bit
 *   periods of low level, then the stop bit(s).                      */
#define UCON1_SENDB (1u << 2)

/* Bit 1 - TXPOL: Transmit Polarity
 *   1 = TX output is inverted (idle-low, active-high).
 *   0 = TX output is normal (idle-high, active-low).
 *   Polarity inversion is useful for direct connection to RS-485
 *   transceivers or other applications requiring inverted signalling. */
#define UCON1_TXPOL (1u << 1)

/* Bit 0 - RXPOL: Receive Polarity
 *   1 = RX input is inverted (idle-low, active-high).
 *   0 = RX input is normal (idle-high, active-low).                  */
#define UCON1_RXPOL (1u << 0)

/* =====================================================================
 *  UxCON2 - UART Control Register 2
 *
 *  Controls transmit write-error status, stop bit mode, hardware
 *  flow control, and additional protocol features.
 *
 *  Bit 7    TXWRE    Transmit Write Error (read-only)
 *  Bit 6    STPMD    Stop Bit Detection Mode
 *  Bits 5:4 FLO[1:0] Flow Control Select
 *  Bit 3    RUNOVF   (or XON, depends on data sheet revision)
 *  Bit 2    ABDIE    Auto-Baud Done Interrupt Enable
 *  Bits 1:0 STP[1:0] Stop Bit Transmit Select
 *
 *  Note: Bit assignments in CON2 vary slightly across data sheet
 *  revisions.  Refer to DS40002213D register details for the
 *  definitive layout.
 * =================================================================== */

/* Bit 7 - TXWRE: Transmit Write Error (read-only)
 *   1 = A write to UxTXB occurred while the transmit buffer was
 *       full.  The written data was lost.  Cleared by reading this
 *       bit (or by disabling the module).
 *   0 = No transmit write error has occurred.                        */
#define UCON2_TXWRE (1u << 7)

/* Bit 6 - STPMD: Stop Bit Detection Mode
 *   1 = Verify stop bit(s) during reception.  A missing stop bit
 *       generates a framing error (FERIF).
 *   0 = Stop bit(s) not verified (more tolerant of timing errors).   */
#define UCON2_STPMD (1u << 6)

/* Bits 5:4 - FLO[1:0]: Flow Control Select
 *   Configures hardware flow control for the UART transmitter and
 *   receiver to prevent data loss at high speeds or with slow
 *   processing.
 *   00 = Flow control off (default)
 *   01 = XON/XOFF software flow control
 *   10 = RTS/CTS hardware flow control
 *   11 = Reserved                                                    */
#define UCON2_FLO1 (1u << 5)
#define UCON2_FLO0 (1u << 4)
#define UCON2_FLO_MASK (0x03u << 4)
#define UCON2_FLO_SHIFT 4u

#define UFLO_OFF (0x00u << 4)      /* No flow control           */
#define UFLO_XON_XOFF (0x01u << 4) /* XON/XOFF software FC     */
#define UFLO_RTS_CTS (0x02u << 4)  /* RTS/CTS hardware FC      */

/* Bit 2 - ABDIE: Auto-Baud Done Interrupt Enable
 *   1 = Enable interrupt when auto-baud detection completes
 *       (ABDIF flag in UxUIR is set).
 *   0 = Auto-baud done interrupt disabled.                           */
#define UCON2_ABDIE (1u << 2)

/* Bits 1:0 - STP[1:0]: Stop Bit Transmit Select
 *   Configures the number of stop bits appended to each transmitted
 *   data frame.
 *   00 = 1 stop bit (standard)
 *   01 = 1.5 stop bits (used in some legacy protocols)
 *   10 = 2 stop bits (improved noise immunity)
 *   11 = Reserved                                                    */
#define UCON2_STP1 (1u << 1)
#define UCON2_STP0 (1u << 0)
#define UCON2_STP_MASK 0x03u

#define USTP_1 0x00u   /* 1 stop bit                */
#define USTP_1_5 0x01u /* 1.5 stop bits             */
#define USTP_2 0x02u   /* 2 stop bits               */

/* =====================================================================
 *  UxFIFO - UART FIFO Status Register
 *
 *  Provides status flags for the transmit and receive data buffers
 *  and receiver idle detection.  All bits are read-only.
 *
 *  Bit 7:6  --       Reserved
 *  Bit 5    TXBE     TX Buffer Empty
 *  Bit 4    TXBF     TX Buffer Full
 *  Bit 3    RXIDL    Receiver Idle
 *  Bit 2    --       Reserved
 *  Bit 1    RXBE     RX Buffer Empty
 *  Bit 0    RXBF     RX Buffer Full
 * =================================================================== */

/* Bit 5 - TXBE: Transmit Buffer Empty (read-only)
 *   1 = Transmit buffer is empty.  UxTXB is ready to accept new
 *       data for transmission.
 *   0 = Transmit buffer contains data waiting to be shifted out.
 *   Typical use: poll TXBE before writing to UxTXB, or use the
 *   TX interrupt to trigger writes.                                  */
#define UFIFO_TXBE (1u << 5)

/* Bit 4 - TXBF: Transmit Buffer Full (read-only)
 *   1 = Transmit buffer is full.  Do NOT write to UxTXB (would
 *       cause a TXWRE error).
 *   0 = Transmit buffer has room for at least one more byte.         */
#define UFIFO_TXBF (1u << 4)

/* Bit 3 - RXIDL: Receiver Idle (read-only)
 *   1 = The receive line is idle (no data being received).
 *   0 = Data reception is in progress (a byte is being assembled
 *       in the receive shift register).                              */
#define UFIFO_RXIDL (1u << 3)

/* Bit 1 - RXBE: Receive Buffer Empty (read-only)
 *   1 = Receive buffer is empty.  No data available in UxRXB.
 *   0 = Receive buffer contains at least one unread byte.            */
#define UFIFO_RXBE (1u << 1)

/* Bit 0 - RXBF: Receive Buffer Full (read-only)
 *   1 = Receive buffer has unread data.  Read UxRXB to retrieve
 *       the byte and clear this flag.
 *   0 = Receive buffer is empty (same meaning as RXBE = 1).
 *   Note: RXBF is the complement of RXBE.  Both are provided for
 *   convenience in different polling idioms.                          */
#define UFIFO_RXBF (1u << 0)

/* =====================================================================
 *  UxUIR - UART Interrupt Request Register
 *
 *  Contains interrupt flags for general UART events (not errors).
 *  Flags must be cleared by software unless otherwise noted.
 *
 *  Bit 7:4  --       Reserved
 *  Bit 3    WUIF     Wake-Up Interrupt Flag
 *  Bit 2    ABDIF    Auto-Baud Detect Done Interrupt Flag
 *  Bit 1    ABDOVF   Auto-Baud Overflow Flag
 *  Bit 0    TXMTIF   Transmit Shift Register Empty Flag
 * =================================================================== */

/* Bit 3 - WUIF: Wake-Up Interrupt Flag
 *   1 = A wake-up event was detected on the RX pin during Sleep
 *       (falling edge or activity detected with WUE = 1).
 *   0 = No wake-up event.  Cleared by software.                     */
#define UUIR_WUIF (1u << 3)

/* Bit 2 - ABDIF: Auto-Baud Detect Done Interrupt Flag
 *   1 = Auto-baud detection has completed successfully.  The BRG
 *       register has been loaded with the measured baud rate value.
 *   0 = Auto-baud not complete.  Cleared by software.
 *   Used after setting ABDEN in UxCON0 to detect when the sync
 *   byte (0x55) has been received and the baud rate calculated.      */
#define UUIR_ABDIF (1u << 2)

/* Bit 1 - ABDOVF: Auto-Baud Overflow Flag
 *   1 = Auto-baud detection overflowed the BRG counter (incoming
 *       baud rate too slow for the selected clock and BRGS setting).
 *   0 = No overflow.  Cleared by software.                           */
#define UUIR_ABDOVF (1u << 1)

/* Bit 0 - TXMTIF: Transmit Shift Register Empty Flag (read-only)
 *   1 = The transmit shift register (TSR) is empty and the last
 *       bit has been clocked out.  The transmission is fully
 *       complete (including stop bits).
 *   0 = The TSR contains data; a transmission is in progress.
 *   This flag is hardware-managed and cannot be cleared by software.
 *   Useful for RS-485 direction control or half-duplex turnaround.   */
#define UUIR_TXMTIF (1u << 0)

/* =====================================================================
 *  UxERRIR - UART Error Interrupt Request Register
 *
 *  Contains interrupt flags for UART error conditions.  Each flag
 *  corresponds to an enable bit in UxERRIE at the same position.
 *
 *  Bit 7    TXMTIF   TX Shift Register Empty (mirrors UIR, or
 *                     distinct error-class TXCIF in some revisions)
 *  Bit 6    PERIF    Parity Error Interrupt Flag
 *  Bit 5    ABDOVF   Auto-Baud Overflow Interrupt Flag
 *  Bit 4    CERIF    Checksum Error Interrupt Flag
 *  Bit 3    FERIF    Framing Error Interrupt Flag
 *  Bit 2    RXBKIF   Receive Break Interrupt Flag
 *  Bit 1    RXFOIF   Receive FIFO Overflow Interrupt Flag
 *  Bit 0    TXCIF    TX Complete Interrupt Flag
 * =================================================================== */

/* Bit 7 - TXMTIF: Transmit Empty Flag
 *   1 = Transmit shift register is empty (read-only).                */
#define UERRIR_TXMTIF (1u << 7)

/* Bit 6 - PERIF: Parity Error Interrupt Flag
 *   1 = A parity error was detected on the most recently received
 *       byte (parity bit did not match expected value).
 *   0 = No parity error.  Cleared by software.                      */
#define UERRIR_PERIF (1u << 6)

/* Bit 5 - ABDOVF: Auto-Baud Overflow Error Flag
 *   1 = Auto-baud timer overflowed (baud rate too slow).
 *   0 = No overflow.  Cleared by software.                           */
#define UERRIR_ABDOVF (1u << 5)

/* Bit 4 - CERIF: Checksum Error Interrupt Flag
 *   1 = A checksum mismatch was detected in a protocol mode frame
 *       (LIN, etc.).  The received checksum did not match the
 *       hardware-calculated value.
 *   0 = No checksum error.  Cleared by software.
 *   Applicable to U1/U2 with RXCHK/TXCHK registers in LIN mode.     */
#define UERRIR_CERIF (1u << 4)

/* Bit 3 - FERIF: Framing Error Interrupt Flag
 *   1 = A framing error was detected.  The expected stop bit was
 *       not received (sampled as 0 instead of 1), indicating a
 *       baud rate mismatch, noise, or line break.
 *   0 = No framing error.  Cleared by software.                     */
#define UERRIR_FERIF (1u << 3)

/* Bit 2 - RXBKIF: Receive Break Interrupt Flag
 *   1 = A Break condition was detected on the receive line (the RX
 *       pin was held low for longer than one character time).
 *       The timing of this flag depends on RXBIMD in UxCON1.
 *   0 = No Break detected.  Cleared by software.                    */
#define UERRIR_RXBKIF (1u << 2)

/* Bit 1 - RXFOIF: Receive FIFO Overflow Interrupt Flag
 *   1 = The receive buffer overflowed.  A new byte arrived while
 *       UxRXB was still full and was not read in time.  The new
 *       byte is lost (unless RUNOVF = 1).
 *   0 = No overflow.  Cleared by software.                           */
#define UERRIR_RXFOIF (1u << 1)

/* Bit 0 - TXCIF: Transmit Complete Interrupt Flag
 *   1 = Transmission of the last queued byte is fully complete
 *       (including stop bits).  Useful for half-duplex turnaround
 *       and RS-485 driver enable de-assertion.
 *   0 = Transmission not complete or no data was sent.
 *       Cleared by software.                                         */
#define UERRIR_TXCIF (1u << 0)

/* =====================================================================
 *  UxERRIE - UART Error Interrupt Enable Register
 *
 *  Each bit enables the corresponding interrupt source in UxERRIR.
 *  When an enable bit is set to 1 and the corresponding flag in
 *  UxERRIR is also set, an interrupt request is generated (routed
 *  through the peripheral interrupt system).
 *
 *  Bit layout matches UxERRIR:
 *  Bit 7    TXMTIE   TX Empty Interrupt Enable
 *  Bit 6    PERIE    Parity Error Interrupt Enable
 *  Bit 5    ABDOVE   Auto-Baud Overflow Interrupt Enable
 *  Bit 4    CERIE    Checksum Error Interrupt Enable
 *  Bit 3    FERIE    Framing Error Interrupt Enable
 *  Bit 2    RXBKIE   Receive Break Interrupt Enable
 *  Bit 1    RXFOIE   Receive FIFO Overflow Interrupt Enable
 *  Bit 0    TXCIE    TX Complete Interrupt Enable
 * =================================================================== */

#define UERRIE_TXMTIE (1u << 7) /* TX shift register empty IE  */
#define UERRIE_PERIE (1u << 6)  /* Parity error IE             */
#define UERRIE_ABDOVE (1u << 5) /* Auto-baud overflow IE       */
#define UERRIE_CERIE (1u << 4)  /* Checksum error IE           */
#define UERRIE_FERIE (1u << 3)  /* Framing error IE            */
#define UERRIE_RXBKIE (1u << 2) /* Receive break IE            */
#define UERRIE_RXFOIE (1u << 1) /* Receive FIFO overflow IE    */
#define UERRIE_TXCIE (1u << 0)  /* TX complete IE              */

/* =====================================================================
 *  Protocol Parameter Register Bit Masks
 *
 *  UxP1, UxP2, UxP3 are all 16-bit registers but only the lower
 *  9 bits (P[8:0]) are implemented.  The high byte contains only
 *  bit 0 (P[8]); bits 7:1 are reserved.
 * =================================================================== */

/* Mask for the 9-bit value when read as a 16-bit register            */
#define UP_VALUE_MASK 0x01FFu

/* High byte: only bit 0 is implemented (P[8])                        */
#define UPH_BIT8 (1u << 0)

/* =====================================================================
 *  Baud Rate Calculation Helpers
 *
 *  The 16-bit BRG register determines the baud rate according to:
 *
 *    BRGS = 1 (high speed): Baud = Fosc / (4  * (BRG + 1))
 *    BRGS = 0 (low speed):  Baud = Fosc / (16 * (BRG + 1))
 *
 *  These macros compute the BRG value from the desired baud rate and
 *  oscillator frequency.  Use the result to load UxBRG (16-bit).
 *
 *  The +div/2 term in the numerator provides rounding to the nearest
 *  integer for improved baud rate accuracy.
 *
 *  Example:
 *    U1BRG = UART_BRG_HIGH(64000000UL, 9600UL);  // = 1666
 *    U1CON0 |= UCON0_BRGS;                        // select /4 mode
 * =================================================================== */
#define UART_BRG_HIGH(fosc, baud) ((uint16_t)(((fosc) + 2UL * (baud)) / (4UL * (baud)) - 1UL))

#define UART_BRG_LOW(fosc, baud) ((uint16_t)(((fosc) + 8UL * (baud)) / (16UL * (baud)) - 1UL))

#endif /* PIC18F_Q84_UART_H */
