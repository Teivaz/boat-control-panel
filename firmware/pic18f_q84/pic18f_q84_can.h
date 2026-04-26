/*
 * pic18f_q84_can.h
 *
 * CAN FD (Controller Area Network with Flexible Data-Rate) register
 * definitions for PIC18F27/47/57Q84 microcontrollers.
 *
 * The CAN FD module implements ISO 11898-1:2015 CAN FD and CAN 2.0B
 * protocols with the following features:
 *
 *   - CAN FD and CAN 2.0B operating modes
 *   - Bit rates up to 8 Mbit/s (data phase) and 1 Mbit/s (nominal)
 *   - Payload sizes up to 64 bytes per frame (CAN FD)
 *   - 1 dedicated TX FIFO queue (TXQ) with priority-based scheduling
 *   - 3 programmable TX/RX FIFOs (FIFO1-FIFO3)
 *   - 1 TX Event FIFO (TEF) for transmit-complete timestamps
 *   - 12 acceptance filters with individual masks
 *   - Transmitter delay compensation (TDC) for high data rates
 *   - Transmit bandwidth sharing (TXBWS) for fair bus access
 *   - ISO or non-ISO CRC selection
 *   - 32-bit time base counter with configurable prescaler
 *   - DeviceNet filter support (DNCNT)
 *   - Protocol exception event control
 *   - Listen-only, loopback, and restricted operation modes
 *
 * Register architecture:
 *   All CAN control/status registers (prefixed C1) are 32 bits wide,
 *   mapped at 4-byte-aligned addresses starting at 0x0100.  Each
 *   32-bit register can be accessed as a single 32-bit word via SFR32
 *   or as four individual bytes via SFR8 with the L/H/U/T suffixes:
 *     L = byte 0 (bits  7:0),  offset +0
 *     H = byte 1 (bits 15:8),  offset +1
 *     U = byte 2 (bits 23:16), offset +2
 *     T = byte 3 (bits 31:24), offset +3
 *
 * Message RAM:
 *   TX/RX message objects reside in a dedicated CAN message RAM area
 *   (separate from these SFR registers).  The FIFO user-address
 *   registers (C1TXQUA, C1FIFOUAn) point into that RAM.  Refer to
 *   DS40002213D Section 29 for message object format details.
 *
 * Typical CAN FD initialisation sequence:
 *   1. Request Configuration mode: C1CON REQOP = 0b100
 *   2. Wait for OPMOD == 0b100 (Configuration mode confirmed)
 *   3. Configure bit timing: C1NBTCFG (nominal), C1DBTCFG (data)
 *   4. Configure TDC: C1TDC (offset, mode, SID11 enable)
 *   5. Configure FIFOs: C1TXQCON, C1FIFOCON1-3 (sizes, directions)
 *   6. Configure TEF: C1TEFCON (if store-in-TEF enabled)
 *   7. Set FIFO base address: C1FIFOBA
 *   8. Configure filters/masks: C1FLTOBJn, C1MASKn, C1FLTCONn
 *   9. Enable interrupts: C1INT
 *  10. Request Normal CAN FD mode: C1CON REQOP = 0b000
 *  11. Wait for OPMOD == 0b000
 *
 * Reference: Microchip DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 29: CAN FD Module
 *
 * Requires: SFR8(addr), SFR16(addr), and SFR32(addr) macros defined
 *           externally (typically via pic18f_q84.h).
 */

#ifndef PIC18F_Q84_CAN_H
#define PIC18F_Q84_CAN_H

/* =====================================================================
 *
 *  CAN FD Operation Mode Constants
 *
 *  Used with the REQOP field (to request a mode transition) and the
 *  OPMOD field (to read the current operating mode) in C1CON.
 *
 * =================================================================== */

#define CAN_MODE_NORMAL_FD 0x00u     /* Normal CAN FD mode             */
#define CAN_MODE_SLEEP 0x01u         /* Sleep / low-power mode         */
#define CAN_MODE_INT_LOOPBACK 0x02u  /* Internal Loopback (no bus)     */
#define CAN_MODE_LISTEN_ONLY 0x03u   /* Listen Only (no TX, no ACK)    */
#define CAN_MODE_CONFIGURATION 0x04u /* Configuration (registers R/W)  */
#define CAN_MODE_EXT_LOOPBACK 0x05u  /* External Loopback (TX->RX)    */
#define CAN_MODE_NORMAL_2_0 0x06u    /* Normal CAN 2.0B mode           */
#define CAN_MODE_RESTRICTED 0x07u    /* Restricted Operation mode      */

/* =====================================================================
 *
 *  CAN FD Payload Size Constants
 *
 *  Used with the PLSIZE field in C1TXQCON and C1FIFOCONn to select
 *  the data payload size per message object in the FIFO.
 *
 * =================================================================== */

#define CAN_PLSIZE_8 0x00u  /*  8 bytes (CAN 2.0B compatible) */
#define CAN_PLSIZE_12 0x01u /* 12 bytes                       */
#define CAN_PLSIZE_16 0x02u /* 16 bytes                       */
#define CAN_PLSIZE_20 0x03u /* 20 bytes                       */
#define CAN_PLSIZE_24 0x04u /* 24 bytes                       */
#define CAN_PLSIZE_32 0x05u /* 32 bytes                       */
#define CAN_PLSIZE_48 0x06u /* 48 bytes                       */
#define CAN_PLSIZE_64 0x07u /* 64 bytes (maximum CAN FD)      */

/* =====================================================================
 *
 *  CAN FD TX Retry Attempt Constants
 *
 *  Used with the TXAT field in C1TXQCON and C1FIFOCONn to control
 *  how many times a message is retransmitted after arbitration loss
 *  or error (when RTXAT is enabled in C1CON).
 *
 * =================================================================== */

#define CAN_TXAT_DISABLED 0x00u  /* Retransmit disabled            */
#define CAN_TXAT_3_RETRIES 0x01u /* Retransmit up to 3 times      */
#define CAN_TXAT_UNLIMITED 0x02u /* Unlimited retransmissions      */
/* 0x03 is reserved                                                      */

/* =====================================================================
 *
 *  CAN FD TDC Mode Constants
 *
 *  Used with the TDCMOD field in C1TDC to select the transmitter
 *  delay compensation mode for CAN FD data-phase bit rates.
 *
 * =================================================================== */

#define CAN_TDCMOD_DISABLED 0x00u /* TDC disabled                   */
#define CAN_TDCMOD_MANUAL 0x01u   /* Manual: use TDCO only          */
#define CAN_TDCMOD_AUTO 0x02u     /* Auto: TDCV measured + TDCO     */
/* 0x03 is reserved                                                      */

/* *********************************************************************
 *
 *  C1CON - CAN Module Control Register
 *  Address: 0x0100 (32-bit)
 *
 *  Master control register for the CAN FD module.  Provides clock
 *  selection, protocol options, TX bandwidth sharing, abort control,
 *  and operating mode request/status.
 *
 * ********************************************************************* */

#define C1CONL SFR8(0x0100) /* byte 0: bits  7:0        */
#define C1CONH SFR8(0x0101) /* byte 1: bits 15:8        */
#define C1CONU SFR8(0x0102) /* byte 2: bits 23:16       */
#define C1CONT SFR8(0x0103) /* byte 3: bits 31:24       */
#define C1CON SFR32(0x0100) /* full 32-bit access       */

/* --- Byte 0 (C1CONL, offset +0) --- */

/* Bits 4:0 - DNCNT[4:0]: DeviceNet Filter Bit Number
 *   Number of bits at the start of the frame to apply the DeviceNet
 *   filter.  Set to 0 to disable DeviceNet filtering.                  */
#define C1CON_DNCNT_POS 0
#define C1CON_DNCNT_MASK (0x1Fu << 0)

/* Bit 5 - ISOCRCEN: ISO CRC Enable
 *   1 = CRC field uses ISO 11898-1:2015 CAN FD format (stuff bit
 *       count included in CRC calculation).
 *   0 = CRC field uses non-ISO (Bosch) CAN FD format.
 *   Should match all other nodes on the bus.                           */
#define C1CON_ISOCRCEN (1u << 5)

/* Bit 6 - PXEDIS: Protocol Exception Event Disable
 *   1 = Protocol exception is disabled; the module treats a received
 *       FDF=1 (in CAN 2.0 mode) as a form error.
 *   0 = Protocol exception is enabled per ISO 11898-1:2015.            */
#define C1CON_PXEDIS (1u << 6)

/* Bit 7 - CLKSEL: CAN System Clock Select
 *   1 = CAN module uses the system clock (Fosc) directly.
 *   0 = CAN module uses the clock selected by the CAN clock divider.   */
#define C1CON_CLKSEL (1u << 7)

/* --- Byte 1 (C1CONH, offset +1) --- */

/* Bit 8 - RTXAT: Restrict Retransmission Attempts
 *   1 = Retransmission attempts are restricted per TXAT field in
 *       the FIFO control registers.
 *   0 = Message retransmits unlimited on arbitration loss or error.     */
#define C1CON_RTXAT (1u << 8)

/* Bit 9 - WAKFIL: CAN Bus Line Wake-up Filter Enable
 *   1 = Wake-up filter enabled on the CAN bus line.
 *   0 = Wake-up filter disabled.                                       */
#define C1CON_WAKFIL (1u << 9)

/* Bits 11:10 - WFT[1:0]: Wake-up Filter Time
 *   Selects the wake-up filter time when WAKFIL = 1.
 *   00 = T00FILT, 01 = T01FILT, 10 = T10FILT, 11 = T11FILT
 *   (Actual times are implementation-defined; see DS40002213D.)         */
#define C1CON_WFT_POS 10
#define C1CON_WFT_MASK (0x03u << 10)

/* --- Byte 2 (C1CONU, offset +2) --- */

/* Bit 16 - ABAT: Abort All Pending Transmissions
 *   Write 1 = Request abort of all pending transmissions.
 *             Hardware clears this bit when all TX FIFOs have been
 *             flushed and pending transmissions aborted.
 *   0 = No abort request / abort complete.                             */
#define C1CON_ABAT (1u << 16)

/* Bits 23:20 - TXBWS[3:0]: Transmit Bandwidth Sharing
 *   Controls the delay (in arbitration bit times) between consecutive
 *   transmission attempts from this node, providing fair access to
 *   other bus participants.
 *   0000 = No delay (back-to-back transmissions)
 *   0001-1011 = Delay of (value * 2) bit times
 *   1100-1111 = Reserved                                               */
#define C1CON_TXBWS_POS 20
#define C1CON_TXBWS_MASK (0x0Fu << 20)

/* --- Byte 3 (C1CONT, offset +3) --- */

/* Bits 26:24 - OPMOD[2:0]: Operation Mode Status (read-only)
 *   Reports the current operating mode of the CAN module.
 *   Use CAN_MODE_* constants for comparison.                           */
#define C1CON_OPMOD_POS 24
#define C1CON_OPMOD_MASK (0x07u << 24)

/* Bits 30:28 - REQOP[2:0]: Request Operation Mode
 *   Write the desired mode value to request a mode transition.
 *   The OPMOD field reflects the current mode after the transition
 *   completes.  Use CAN_MODE_* constants.                              */
#define C1CON_REQOP_POS 28
#define C1CON_REQOP_MASK (0x07u << 28)

/* Bit 27 - ESIGM: Transmit Error Signal Mode
 *   1 = Error frames are transmitted with ESI bit set in CAN FD
 *       frames when the module is in error passive state.
 *   0 = ESI bit reflects the transmitter's error state.                */
#define C1CON_ESIGM (1u << 27)

/* Bit 28 overlaps REQOP[0] -- see REQOP definition above */

/* Bit 29 - SERRLOM: System Error to Listen-Only Mode
 *   1 = On a system error, the module automatically transitions
 *       to Listen-Only mode.
 *   0 = System error does not cause mode transition.                   */
#define C1CON_SERRLOM (1u << 29)

/* Bit 30 - STEF: Store in Transmit Event FIFO
 *   1 = Successfully transmitted messages are stored in the TEF.
 *   0 = Transmitted messages are not stored in the TEF.                */
#define C1CON_STEF (1u << 30)

/* Bit 31 - BUSY: CAN Module Busy (read-only)
 *   1 = Module is actively transmitting or receiving.
 *   0 = Module is idle.                                                */
#define C1CON_BUSY (1u << 31)

/* *********************************************************************
 *
 *  C1NBTCFG - Nominal Bit Time Configuration
 *  Address: 0x0104 (32-bit)
 *
 *  Configures the nominal (arbitration phase) bit timing.  The nominal
 *  bit time = (SJW+1 + TSEG1+1 + TSEG2+1) time quanta, where each
 *  time quantum = (BRP+1) / Fcan.
 *
 * ********************************************************************* */

#define C1NBTCFGL SFR8(0x0104) /* byte 0: BRP[7:0]         */
#define C1NBTCFGH SFR8(0x0105) /* byte 1: TSEG1[7:0]       */
#define C1NBTCFGU SFR8(0x0106) /* byte 2: TSEG2[6:0]       */
#define C1NBTCFGT SFR8(0x0107) /* byte 3: SJW[6:0]         */
#define C1NBTCFG SFR32(0x0104) /* full 32-bit access       */

/* Bits 7:0 - BRP[7:0]: Baud Rate Prescaler
 *   Nominal bit rate prescaler.  Time quantum = (BRP+1) / Fcan.
 *   Range: 0-255 (divides by 1-256).                                   */
#define C1NBTCFG_BRP_POS 0
#define C1NBTCFG_BRP_MASK (0xFFu << 0)

/* Bits 15:8 - TSEG1[7:0]: Time Segment 1 (Propagation + Phase 1)
 *   Nominal TSEG1.  Number of TQ = TSEG1 + 1.
 *   Range: 0-255 (1-256 TQ).                                           */
#define C1NBTCFG_TSEG1_POS 8
#define C1NBTCFG_TSEG1_MASK (0xFFu << 8)

/* Bits 22:16 - TSEG2[6:0]: Time Segment 2 (Phase 2)
 *   Nominal TSEG2.  Number of TQ = TSEG2 + 1.
 *   Range: 0-127 (1-128 TQ).                                           */
#define C1NBTCFG_TSEG2_POS 16
#define C1NBTCFG_TSEG2_MASK (0x7Fu << 16)

/* Bits 30:24 - SJW[6:0]: Synchronization Jump Width
 *   Nominal SJW.  Maximum adjustment = SJW + 1 TQ.
 *   Range: 0-127 (1-128 TQ).                                           */
#define C1NBTCFG_SJW_POS 24
#define C1NBTCFG_SJW_MASK (0x7Fu << 24)

/* *********************************************************************
 *
 *  C1DBTCFG - Data Bit Time Configuration
 *  Address: 0x0108 (32-bit)
 *
 *  Configures the data phase bit timing (used in CAN FD frames during
 *  the data field when BRS=1).  The data bit time = (SJW+1 + TSEG1+1
 *  + TSEG2+1) time quanta, where TQ = (BRP+1) / Fcan.
 *
 * ********************************************************************* */

#define C1DBTCFGL SFR8(0x0108) /* byte 0: BRP[7:0]         */
#define C1DBTCFGH SFR8(0x0109) /* byte 1: TSEG1[4:0]       */
#define C1DBTCFGU SFR8(0x010A) /* byte 2: TSEG2[3:0]       */
#define C1DBTCFGT SFR8(0x010B) /* byte 3: SJW[3:0]         */
#define C1DBTCFG SFR32(0x0108) /* full 32-bit access       */

/* Bits 7:0 - BRP[7:0]: Data Baud Rate Prescaler
 *   Data phase prescaler.  TQ = (BRP+1) / Fcan.
 *   Range: 0-255 (divides by 1-256).                                   */
#define C1DBTCFG_BRP_POS 0
#define C1DBTCFG_BRP_MASK (0xFFu << 0)

/* Bits 12:8 - TSEG1[4:0]: Data Time Segment 1
 *   Data TSEG1.  Number of TQ = TSEG1 + 1.
 *   Range: 0-31 (1-32 TQ).                                             */
#define C1DBTCFG_TSEG1_POS 8
#define C1DBTCFG_TSEG1_MASK (0x1Fu << 8)

/* Bits 19:16 - TSEG2[3:0]: Data Time Segment 2
 *   Data TSEG2.  Number of TQ = TSEG2 + 1.
 *   Range: 0-15 (1-16 TQ).                                             */
#define C1DBTCFG_TSEG2_POS 16
#define C1DBTCFG_TSEG2_MASK (0x0Fu << 16)

/* Bits 27:24 - SJW[3:0]: Data Synchronization Jump Width
 *   Data SJW.  Maximum adjustment = SJW + 1 TQ.
 *   Range: 0-15 (1-16 TQ).                                             */
#define C1DBTCFG_SJW_POS 24
#define C1DBTCFG_SJW_MASK (0x0Fu << 24)

/* *********************************************************************
 *
 *  C1TDC - Transmitter Delay Compensation
 *  Address: 0x010C (32-bit)
 *
 *  Controls transmitter delay compensation (TDC) for reliable data
 *  phase reception at high bit rates.  TDC compensates for the
 *  round-trip delay through the transceiver.
 *
 * ********************************************************************* */

#define C1TDCL SFR8(0x010C) /* byte 0: TDCV[5:0]        */
#define C1TDCH SFR8(0x010D) /* byte 1: TDCO[6:0]        */
#define C1TDCU SFR8(0x010E) /* byte 2: TDCMOD, EDGFLTEN */
#define C1TDCT SFR8(0x010F) /* byte 3: SID11EN          */
#define C1TDC SFR32(0x010C) /* full 32-bit access       */

/* Bits 5:0 - TDCV[5:0]: Transmitter Delay Compensation Value (read-only)
 *   In auto mode, hardware measures the transmitter-to-receiver loop
 *   delay and stores it here.  Read to verify TDC operation.
 *   Range: 0-63 time quanta.                                           */
#define C1TDC_TDCV_POS 0
#define C1TDC_TDCV_MASK (0x3Fu << 0)

/* Bits 14:8 - TDCO[6:0]: Transmitter Delay Compensation Offset
 *   Manual offset added to the measured delay (auto mode) or used
 *   as the sole compensation value (manual mode).
 *   Range: 0-127 time quanta.                                          */
#define C1TDC_TDCO_POS 8
#define C1TDC_TDCO_MASK (0x7Fu << 8)

/* Bits 17:16 - TDCMOD[1:0]: TDC Mode
 *   Selects the transmitter delay compensation mode.
 *   Use CAN_TDCMOD_* constants.                                        */
#define C1TDC_TDCMOD_POS 16
#define C1TDC_TDCMOD_MASK (0x03u << 16)

/* Bit 19 - EDGFLTEN: Edge Filter Enable
 *   1 = Enable edge filtering on the CAN bus input during the
 *       data phase to reject glitches.
 *   0 = Edge filtering disabled.                                       */
#define C1TDC_EDGFLTEN (1u << 19)

/* Bit 24 - SID11EN: SID[11] Enable (CAN FD Base Format)
 *   1 = In CAN FD Base format frames, the SID11 bit (12th ID bit)
 *       is included in RX filter matching and stored in messages.
 *   0 = SID11 bit is ignored (standard 11-bit SID only).               */
#define C1TDC_SID11EN (1u << 24)

/* *********************************************************************
 *
 *  C1TBC - Time Base Counter
 *  Address: 0x0110 (32-bit)
 *
 *  32-bit free-running counter for CAN message timestamping.  The
 *  counter increments at a rate determined by C1TSCON.TBCPRE when
 *  enabled via C1TSCON.TBCEN.
 *
 * ********************************************************************* */

#define C1TBCL SFR8(0x0110) /* byte 0: TBC[7:0]         */
#define C1TBCH SFR8(0x0111) /* byte 1: TBC[15:8]        */
#define C1TBCU SFR8(0x0112) /* byte 2: TBC[23:16]       */
#define C1TBCT SFR8(0x0113) /* byte 3: TBC[31:24]       */
#define C1TBC SFR32(0x0110) /* full 32-bit access       */

/* *********************************************************************
 *
 *  C1TSCON - Timestamp Control
 *  Address: 0x0114 (32-bit)
 *
 *  Controls the time base counter prescaler, resolution, and enable.
 *
 * ********************************************************************* */

#define C1TSCONL SFR8(0x0114) /* byte 0: TBCPRE[7:0]      */
#define C1TSCONH SFR8(0x0115) /* byte 1: TBCPRE[9:8]      */
#define C1TSCONU SFR8(0x0116) /* byte 2: TSRES, TSEOF     */
#define C1TSCONT SFR8(0x0117) /* byte 3: TBCEN            */
#define C1TSCON SFR32(0x0114) /* full 32-bit access       */

/* Bits 9:0 - TBCPRE[9:0]: Time Base Counter Prescaler
 *   TBC increment rate = Fcan / (TBCPRE + 1).
 *   Range: 0-1023 (divides by 1-1024).                                 */
#define C1TSCON_TBCPRE_POS 0
#define C1TSCON_TBCPRE_MASK (0x03FFu << 0)

/* Bit 16 - TSRES: Timestamp Resolution
 *   1 = Timestamp captured at start of frame (SOF).
 *   0 = Timestamp captured at end of frame (EOF).                      */
#define C1TSCON_TSRES (1u << 16)

/* Bit 17 - TSEOF: Timestamp at EOF
 *   1 = End-of-frame timestamping (overrides TSRES interpretation
 *       depending on implementation).
 *   0 = Start-of-frame timestamping.
 *   Refer to DS40002213D for exact interaction with TSRES.             */
#define C1TSCON_TSEOF (1u << 17)

/* Bit 24 - TBCEN: Time Base Counter Enable
 *   1 = Time base counter is enabled and running.
 *   0 = Time base counter is stopped and held at its current value.    */
#define C1TSCON_TBCEN (1u << 24)

/* *********************************************************************
 *
 *  C1VEC - Interrupt Vector Register
 *  Address: 0x0118 (32-bit, read-only)
 *
 *  Reports the highest-priority pending interrupt source and the
 *  filter hit number for the most recently received message.  Used
 *  for vectored interrupt handling.
 *
 * ********************************************************************* */

#define C1VECL SFR8(0x0118) /* byte 0: ICODE[6:0]       */
#define C1VECH SFR8(0x0119) /* byte 1: FILHIT[4:0]      */
#define C1VECU SFR8(0x011A) /* byte 2: TXCODE[6:0]      */
#define C1VECT SFR8(0x011B) /* byte 3: RXCODE[6:0]      */
#define C1VEC SFR32(0x0118) /* full 32-bit access       */

/* Bits 6:0 - ICODE[6:0]: Interrupt Flag Code (read-only)
 *   Index of the highest-priority pending CAN interrupt source.
 *   0x00 = No interrupt, 0x01 = Error, 0x02 = Wake-up, etc.
 *   Refer to DS40002213D Table 29-X for complete encoding.             */
#define C1VEC_ICODE_POS 0
#define C1VEC_ICODE_MASK (0x7Fu << 0)

/* Bits 12:8 - FILHIT[4:0]: Filter Hit Number (read-only)
 *   Number of the acceptance filter that matched the most recently
 *   received message.  Range: 0-11.                                    */
#define C1VEC_FILHIT_POS 8
#define C1VEC_FILHIT_MASK (0x1Fu << 8)

/* Bits 22:16 - TXCODE[6:0]: TX Interrupt Flag Code (read-only)
 *   Index of the FIFO that caused the TX interrupt.                    */
#define C1VEC_TXCODE_POS 16
#define C1VEC_TXCODE_MASK (0x7Fu << 16)

/* Bits 30:24 - RXCODE[6:0]: RX Interrupt Flag Code (read-only)
 *   Index of the FIFO that caused the RX interrupt.                    */
#define C1VEC_RXCODE_POS 24
#define C1VEC_RXCODE_MASK (0x7Fu << 24)

/* *********************************************************************
 *
 *  C1INT - Interrupt Register
 *  Address: 0x011C (32-bit)
 *
 *  Combined interrupt flags (bytes 0 and 2) and interrupt enables
 *  (bytes 1 and 3) for the CAN module.
 *
 *  Byte 0: Basic interrupt flags (TXIF, RXIF)
 *  Byte 1: Basic interrupt enables (TXIE, RXIE)
 *  Byte 2: Event interrupt flags
 *  Byte 3: Event interrupt enables
 *
 * ********************************************************************* */

#define C1INTL SFR8(0x011C) /* byte 0: basic flags      */
#define C1INTH SFR8(0x011D) /* byte 1: basic enables    */
#define C1INTU SFR8(0x011E) /* byte 2: event flags      */
#define C1INTT SFR8(0x011F) /* byte 3: event enables    */
#define C1INT SFR32(0x011C) /* full 32-bit access       */

/* --- Byte 0 (C1INTL): Basic Interrupt Flags --- */

/* Bit 0 - TXIF: Transmit Interrupt Flag (read-only)
 *   1 = At least one TX FIFO is not full (ready to accept data).
 *   0 = All TX FIFOs are full.  Cleared automatically by hardware.     */
#define C1INT_TXIF (1u << 0)

/* Bit 1 - RXIF: Receive Interrupt Flag (read-only)
 *   1 = At least one RX FIFO contains a message (not empty).
 *   0 = All RX FIFOs are empty.  Cleared automatically by hardware.    */
#define C1INT_RXIF (1u << 1)

/* --- Byte 1 (C1INTH): Basic Interrupt Enables --- */

/* Bit 8 - TXIE: Transmit Interrupt Enable
 *   1 = Interrupt enabled when TXIF is set.
 *   0 = TX interrupt disabled.                                         */
#define C1INT_TXIE (1u << 8)

/* Bit 9 - RXIE: Receive Interrupt Enable
 *   1 = Interrupt enabled when RXIF is set.
 *   0 = RX interrupt disabled.                                         */
#define C1INT_RXIE (1u << 9)

/* --- Byte 2 (C1INTU): Event Interrupt Flags --- */

/* Bit 16 - TXATIF: TX Attempt Interrupt Flag (read-only)
 *   1 = A TX attempt limit was reached on one or more FIFOs.           */
#define C1INT_TXATIF (1u << 16)

/* Bit 17 - TEFIF: TX Event FIFO Interrupt Flag (read-only)
 *   1 = TX Event FIFO has a pending event (not empty, overflow, etc.)  */
#define C1INT_TEFIF (1u << 17)

/* Bit 19 - WAKIF: Bus Wake-up Interrupt Flag
 *   1 = CAN bus activity detected (wake-up from Sleep).
 *   Cleared by software.                                               */
#define C1INT_WAKIF (1u << 19)

/* Bit 20 - IVMIF: Invalid Message Interrupt Flag
 *   1 = An invalid message was received (format error detected).
 *   Cleared by software.                                               */
#define C1INT_IVMIF (1u << 20)

/* Bit 21 - SERRIF: System Error Interrupt Flag
 *   1 = A system error occurred (access violation, ECC error, etc.).
 *   Cleared by software.                                               */
#define C1INT_SERRIF (1u << 21)

/* Bit 22 - CERRIF: CAN Bus Error Interrupt Flag
 *   1 = A CAN bus error occurred (error counter threshold crossed).
 *   Cleared by software.                                               */
#define C1INT_CERRIF (1u << 22)

/* Bit 23 - TBCIF: Time Base Counter Interrupt Flag
 *   1 = Time base counter overflow or comparison match.
 *   Cleared by software.                                               */
#define C1INT_TBCIF (1u << 23)

/* Bit 24 - RXOVIF: Receive FIFO Overflow Interrupt Flag (read-only)
 *   1 = At least one RX FIFO has overflowed.                           */
#define C1INT_RXOVIF (1u << 24)

/* Bit 25 - MODIF: Mode Change Interrupt Flag
 *   1 = The operating mode has changed (OPMOD != previous value).
 *   Cleared by software.                                               */
#define C1INT_MODIF (1u << 25)

/* --- Byte 3 (C1INTT): Event Interrupt Enables --- */

/* Bit 16+8=24 mapping note: enables are in byte 3 (bits 31:24 of the
 * 32-bit register), mirroring the flag positions from byte 2.
 * The enable bit for flag at bit N in byte2 is at bit (N+8) here.      */

/* Bit 24 - TXATIE: TX Attempt Interrupt Enable                         */
#define C1INT_TXATIE (1u << 24)
#define C1INT_TXATIE_B3 (1u << 0) /* relative to byte 3 alone      */

/* Bit 25 - TEFIE: TX Event FIFO Interrupt Enable                      */
#define C1INT_TEFIE (1u << 25)
#define C1INT_TEFIE_B3 (1u << 1) /* relative to byte 3 alone      */

/* Bit 27 - WAKIE: Bus Wake-up Interrupt Enable                        */
#define C1INT_WAKIE (1u << 27)
#define C1INT_WAKIE_B3 (1u << 3) /* relative to byte 3 alone      */

/* Bit 28 - IVMIE: Invalid Message Interrupt Enable                    */
#define C1INT_IVMIE (1u << 28)
#define C1INT_IVMIE_B3 (1u << 4) /* relative to byte 3 alone      */

/* Bit 29 - SERRIE: System Error Interrupt Enable                      */
#define C1INT_SERRIE (1u << 29)
#define C1INT_SERRIE_B3 (1u << 5) /* relative to byte 3 alone      */

/* Bit 30 - CERRIE: CAN Bus Error Interrupt Enable                     */
#define C1INT_CERRIE (1u << 30)
#define C1INT_CERRIE_B3 (1u << 6) /* relative to byte 3 alone      */

/* Bit 31 - TBCIE: Time Base Counter Interrupt Enable                  */
#define C1INT_TBCIE (1u << 31)
#define C1INT_TBCIE_B3 (1u << 7) /* relative to byte 3 alone      */

/* Note: RXOVIE and MODIE also exist in byte 3 at their respective
 * positions.  The exact bit mapping within byte 3 should be verified
 * against DS40002213D Table 29-9 for the precise enable layout.        */

/* *********************************************************************
 *
 *  C1RXIF - Receive FIFO Interrupt Flags
 *  Address: 0x0120 (32-bit)
 *
 *  Per-FIFO receive interrupt flags.  Bit n corresponds to FIFO n.
 *
 * ********************************************************************* */

#define C1RXIFL SFR8(0x0120)
#define C1RXIFH SFR8(0x0121)
#define C1RXIFU SFR8(0x0122)
#define C1RXIFT SFR8(0x0123)
#define C1RXIF SFR32(0x0120)

#define C1RXIF_RFIF1 (1u << 1) /* FIFO 1 receive interrupt flag */
#define C1RXIF_RFIF2 (1u << 2) /* FIFO 2 receive interrupt flag */
#define C1RXIF_RFIF3 (1u << 3) /* FIFO 3 receive interrupt flag */

/* *********************************************************************
 *
 *  C1TXIF - Transmit FIFO Interrupt Flags
 *  Address: 0x0124 (32-bit)
 *
 *  Per-FIFO transmit interrupt flags.  Bit n corresponds to FIFO n.
 *  TXQ is represented as FIFO 0.
 *
 * ********************************************************************* */

#define C1TXIFL SFR8(0x0124)
#define C1TXIFH SFR8(0x0125)
#define C1TXIFU SFR8(0x0126)
#define C1TXIFT SFR8(0x0127)
#define C1TXIF SFR32(0x0124)

#define C1TXIF_TFIF0 (1u << 0) /* TXQ transmit interrupt flag   */
#define C1TXIF_TFIF1 (1u << 1) /* FIFO 1 transmit interrupt flag*/
#define C1TXIF_TFIF2 (1u << 2) /* FIFO 2 transmit interrupt flag*/
#define C1TXIF_TFIF3 (1u << 3) /* FIFO 3 transmit interrupt flag*/

/* *********************************************************************
 *
 *  C1RXOVIF - Receive Overflow Interrupt Flags
 *  Address: 0x0128 (32-bit)
 *
 *  Per-FIFO receive overflow flags.
 *
 * ********************************************************************* */

#define C1RXOVIFL SFR8(0x0128)
#define C1RXOVIFH SFR8(0x0129)
#define C1RXOVIFU SFR8(0x012A)
#define C1RXOVIFT SFR8(0x012B)
#define C1RXOVIF SFR32(0x0128)

#define C1RXOVIF_RFOVIF1 (1u << 1) /* FIFO 1 overflow flag          */
#define C1RXOVIF_RFOVIF2 (1u << 2) /* FIFO 2 overflow flag          */
#define C1RXOVIF_RFOVIF3 (1u << 3) /* FIFO 3 overflow flag          */

/* *********************************************************************
 *
 *  C1TXATIF - Transmit Attempt Interrupt Flags
 *  Address: 0x012C (32-bit)
 *
 *  Per-FIFO transmit attempt limit flags (set when RTXAT enabled
 *  and the configured number of retries has been exhausted).
 *
 * ********************************************************************* */

#define C1TXATIFL SFR8(0x012C)
#define C1TXATIFH SFR8(0x012D)
#define C1TXATIFU SFR8(0x012E)
#define C1TXATIFT SFR8(0x012F)
#define C1TXATIF SFR32(0x012C)

#define C1TXATIF_TFATIF0 (1u << 0) /* TXQ attempt flag              */
#define C1TXATIF_TFATIF1 (1u << 1) /* FIFO 1 attempt flag           */
#define C1TXATIF_TFATIF2 (1u << 2) /* FIFO 2 attempt flag           */
#define C1TXATIF_TFATIF3 (1u << 3) /* FIFO 3 attempt flag           */

/* *********************************************************************
 *
 *  C1TXREQ - Transmit Request Register
 *  Address: 0x0130 (32-bit)
 *
 *  Per-FIFO transmit request bits.  Set by hardware when a FIFO has
 *  a message pending for transmission.
 *
 * ********************************************************************* */

#define C1TXREQL SFR8(0x0130)
#define C1TXREQH SFR8(0x0131)
#define C1TXREQU SFR8(0x0132)
#define C1TXREQT SFR8(0x0133)
#define C1TXREQ SFR32(0x0130)

#define C1TXREQ_TXREQ0 (1u << 0) /* TXQ has pending message       */
#define C1TXREQ_TXREQ1 (1u << 1) /* FIFO 1 has pending message    */
#define C1TXREQ_TXREQ2 (1u << 2) /* FIFO 2 has pending message    */
#define C1TXREQ_TXREQ3 (1u << 3) /* FIFO 3 has pending message    */

/* *********************************************************************
 *
 *  C1TREC - Transmit/Receive Error Count
 *  Address: 0x0134 (32-bit)
 *
 *  Contains the CAN error counters and bus-off / error-passive status.
 *
 * ********************************************************************* */

#define C1TRECL SFR8(0x0134) /* byte 0: RERRCNT[7:0]     */
#define C1TRECH SFR8(0x0135) /* byte 1: TERRCNT[7:0]     */
#define C1TRECU SFR8(0x0136) /* byte 2: status bits      */
#define C1TRECT SFR8(0x0137) /* byte 3: reserved         */
#define C1TREC SFR32(0x0134) /* full 32-bit access       */

/* Bits 7:0 - RERRCNT[7:0]: Receive Error Counter
 *   Current value of the CAN receive error counter (REC).
 *   Read-only.  Range: 0-255.                                          */
#define C1TREC_RERRCNT_POS 0
#define C1TREC_RERRCNT_MASK (0xFFu << 0)

/* Bits 15:8 - TERRCNT[7:0]: Transmit Error Counter
 *   Current value of the CAN transmit error counter (TEC).
 *   Read-only.  Range: 0-255.                                          */
#define C1TREC_TERRCNT_POS 8
#define C1TREC_TERRCNT_MASK (0xFFu << 8)

/* Bit 16 - EWARN: Error Warning Flag (read-only)
 *   1 = TEC or REC >= 96.                                              */
#define C1TREC_EWARN (1u << 16)

/* Bit 17 - RXWARN: Receive Error Warning (read-only)
 *   1 = REC >= 96.                                                     */
#define C1TREC_RXWARN (1u << 17)

/* Bit 18 - TXWARN: Transmit Error Warning (read-only)
 *   1 = TEC >= 96.                                                     */
#define C1TREC_TXWARN (1u << 18)

/* Bit 19 - RXBP: Receive Bus Passive (read-only)
 *   1 = REC >= 128 (error passive state for receiver).                 */
#define C1TREC_RXBP (1u << 19)

/* Bit 20 - TXBP: Transmit Bus Passive (read-only)
 *   1 = TEC >= 128 (error passive state for transmitter).              */
#define C1TREC_TXBP (1u << 20)

/* Bit 21 - TXBO: Transmit Bus Off (read-only)
 *   1 = TEC >= 256 (bus-off state).  The module will not transmit
 *       or receive until a bus-off recovery sequence completes.        */
#define C1TREC_TXBO (1u << 21)

/* *********************************************************************
 *
 *  C1BDIAG0 - Bus Diagnostic Register 0
 *  Address: 0x0138 (32-bit)
 *
 *  Contains separate error counters for nominal and data bit rate
 *  phases.  These counters are independent of the protocol error
 *  counters in C1TREC and can be cleared by writing.
 *
 * ********************************************************************* */

#define C1BDIAG0L SFR8(0x0138) /* byte 0: NRERRCNT[7:0]    */
#define C1BDIAG0H SFR8(0x0139) /* byte 1: NTERRCNT[7:0]    */
#define C1BDIAG0U SFR8(0x013A) /* byte 2: DRERRCNT[7:0]    */
#define C1BDIAG0T SFR8(0x013B) /* byte 3: DTERRCNT[7:0]    */
#define C1BDIAG0 SFR32(0x0138) /* full 32-bit access       */

/* Bits  7:0  - NRERRCNT[7:0]: Nominal Bit Rate Receive Error Count     */
#define C1BDIAG0_NRERRCNT_POS 0
#define C1BDIAG0_NRERRCNT_MASK (0xFFu << 0)

/* Bits 15:8  - NTERRCNT[7:0]: Nominal Bit Rate Transmit Error Count    */
#define C1BDIAG0_NTERRCNT_POS 8
#define C1BDIAG0_NTERRCNT_MASK (0xFFu << 8)

/* Bits 23:16 - DRERRCNT[7:0]: Data Bit Rate Receive Error Count        */
#define C1BDIAG0_DRERRCNT_POS 16
#define C1BDIAG0_DRERRCNT_MASK (0xFFu << 16)

/* Bits 31:24 - DTERRCNT[7:0]: Data Bit Rate Transmit Error Count       */
#define C1BDIAG0_DTERRCNT_POS 24
#define C1BDIAG0_DTERRCNT_MASK (0xFFu << 24)

/* *********************************************************************
 *
 *  C1BDIAG1 - Bus Diagnostic Register 1
 *  Address: 0x013C (32-bit)
 *
 *  Error type flags for both nominal and data bit rate phases.
 *  Each flag is set by hardware on the corresponding error type.
 *  Cleared by writing 0.
 *
 * ********************************************************************* */

#define C1BDIAG1L SFR8(0x013C) /* byte 0: nominal errors   */
#define C1BDIAG1H SFR8(0x013D) /* byte 1: nominal + data   */
#define C1BDIAG1U SFR8(0x013E) /* byte 2: data errors      */
#define C1BDIAG1T SFR8(0x013F) /* byte 3: misc error bits  */
#define C1BDIAG1 SFR32(0x013C) /* full 32-bit access       */

/* --- Nominal bit rate error flags --- */
#define C1BDIAG1_NBIT0ERR (1u << 0) /* Nominal bit 0 error           */
#define C1BDIAG1_NBIT1ERR (1u << 1) /* Nominal bit 1 error           */
#define C1BDIAG1_NACKERR (1u << 2)  /* Nominal ACK error             */
#define C1BDIAG1_NFORMERR (1u << 3) /* Nominal form error            */
#define C1BDIAG1_NSTUFERR (1u << 4) /* Nominal stuff error           */
#define C1BDIAG1_NCRCERR (1u << 5)  /* Nominal CRC error             */

/* --- Data bit rate error flags --- */
#define C1BDIAG1_DBIT0ERR (1u << 8)  /* Data bit 0 error              */
#define C1BDIAG1_DBIT1ERR (1u << 9)  /* Data bit 1 error              */
#define C1BDIAG1_DFORMERR (1u << 10) /* Data form error               */
#define C1BDIAG1_DSTUFERR (1u << 11) /* Data stuff error              */
#define C1BDIAG1_DCRCERR (1u << 12)  /* Data CRC error                */

/* --- Miscellaneous error flags --- */
#define C1BDIAG1_ESI (1u << 16)     /* ESI flag of received FD frame */
#define C1BDIAG1_DLCMM (1u << 17)   /* DLC mismatch                  */
#define C1BDIAG1_TXBOERR (1u << 24) /* Device went bus-off           */

/* *********************************************************************
 *
 *  C1TEFCON - TX Event FIFO Control
 *  Address: 0x0140 (32-bit)
 *
 *  Controls the Transmit Event FIFO (TEF), which stores records of
 *  successfully transmitted messages including timestamps.  The TEF
 *  must be enabled via C1CON.STEF = 1 for records to be stored.
 *
 * ********************************************************************* */

#define C1TEFCONL SFR8(0x0140) /* byte 0: enables/sizes    */
#define C1TEFCONH SFR8(0x0141) /* byte 1: FSIZE            */
#define C1TEFCONU SFR8(0x0142) /* byte 2: reserved         */
#define C1TEFCONT SFR8(0x0143) /* byte 3: UINC, FRESET     */
#define C1TEFCON SFR32(0x0140) /* full 32-bit access       */

/* Bit 0 - TEFFIE: TEF Full Interrupt Enable
 *   1 = Interrupt when the TEF is completely full.                     */
#define C1TEFCON_TEFFIE (1u << 0)

/* Bit 1 - TEFNEIE: TEF Not Empty Interrupt Enable
 *   1 = Interrupt when TEF transitions from empty to not-empty.        */
#define C1TEFCON_TEFNEIE (1u << 1)

/* Bit 2 - TEFHIE: TEF Half Full Interrupt Enable
 *   1 = Interrupt when TEF reaches half-full threshold.                */
#define C1TEFCON_TEFHIE (1u << 2)

/* Bit 3 - TEFOVIE: TEF Overflow Interrupt Enable
 *   1 = Interrupt on TEF overflow (message lost because TEF full).     */
#define C1TEFCON_TEFOVIE (1u << 3)

/* Bit 5 - TEFTSEN: TEF Timestamp Enable
 *   1 = Timestamps are stored with each TEF entry.
 *   0 = No timestamps in TEF entries (saves RAM space).                */
#define C1TEFCON_TEFTSEN (1u << 5)

/* Bits 12:8 - FSIZE[4:0]: FIFO Size
 *   Number of TEF entries = FSIZE + 1.
 *   Range: 0-31 (1-32 entries).                                        */
#define C1TEFCON_FSIZE_POS 8
#define C1TEFCON_FSIZE_MASK (0x1Fu << 8)

/* Bit 24 - UINC: User Increment (write-only, self-clearing)
 *   Write 1 = Advance the TEF read pointer by one entry.
 *   Read always returns 0.  Used after reading a TEF entry to move
 *   to the next entry in the FIFO.                                     */
#define C1TEFCON_UINC (1u << 24)

/* Bit 25 - FRESET: FIFO Reset (write-only, self-clearing)
 *   Write 1 = Reset the TEF head and tail pointers.  All entries
 *   are discarded.  Hardware clears this bit when the reset completes. */
#define C1TEFCON_FRESET (1u << 25)

/* *********************************************************************
 *
 *  C1TEFSTA - TX Event FIFO Status
 *  Address: 0x0144 (32-bit)
 *
 * ********************************************************************* */

#define C1TEFSTAL SFR8(0x0144) /* byte 0: status flags     */
#define C1TEFSTAH SFR8(0x0145) /* byte 1: reserved         */
#define C1TEFSTAU SFR8(0x0146) /* byte 2: reserved         */
#define C1TEFSTAT SFR8(0x0147) /* byte 3: reserved         */
#define C1TEFSTA SFR32(0x0144) /* full 32-bit access       */

/* Bit 0 - TEFFIF: TEF Full Interrupt Flag (read-only)
 *   1 = TEF is completely full.                                        */
#define C1TEFSTA_TEFFIF (1u << 0)

/* Bit 1 - TEFNEIF: TEF Not Empty Interrupt Flag (read-only)
 *   1 = TEF contains at least one unread entry.                        */
#define C1TEFSTA_TEFNEIF (1u << 1)

/* Bit 2 - TEFHIF: TEF Half Full Interrupt Flag (read-only)
 *   1 = TEF is at least half full.                                     */
#define C1TEFSTA_TEFHIF (1u << 2)

/* Bit 3 - TEFOVIF: TEF Overflow Interrupt Flag
 *   1 = TEF has overflowed (a TX event was lost).
 *   Cleared by software writing 0.                                     */
#define C1TEFSTA_TEFOVIF (1u << 3)

/* *********************************************************************
 *
 *  C1TEFUA - TX Event FIFO User Address
 *  Address: 0x0148 (32-bit)
 *
 *  Read-only.  Points to the address of the next TEF entry to be
 *  read in the CAN message RAM.
 *
 * ********************************************************************* */

#define C1TEFUAL SFR8(0x0148)
#define C1TEFUAH SFR8(0x0149)
#define C1TEFUAU SFR8(0x014A)
#define C1TEFUAT SFR8(0x014B)
#define C1TEFUA SFR32(0x0148)

/* *********************************************************************
 *
 *  C1FIFOBA - FIFO Base Address
 *  Address: 0x014C (32-bit)
 *
 *  Starting address of the CAN FIFO region in the CAN message RAM.
 *  All FIFO user-address pointers are relative to the address space
 *  beginning at this value.
 *
 * ********************************************************************* */

#define C1FIFOBAL SFR8(0x014C)
#define C1FIFOBAH SFR8(0x014D)
#define C1FIFOBAU SFR8(0x014E)
#define C1FIFOBAT SFR8(0x014F)
#define C1FIFOBA SFR32(0x014C)

/* *********************************************************************
 *
 *  C1TXQCON - TX Queue Control Register
 *  Address: 0x0150 (32-bit)
 *
 *  Controls the dedicated Transmit Queue (TXQ), which is a priority-
 *  based transmit FIFO.  Messages in the TXQ are transmitted in
 *  priority order (lowest CAN ID first).
 *
 * ********************************************************************* */

#define C1TXQCONL SFR8(0x0150) /* byte 0                   */
#define C1TXQCONH SFR8(0x0151) /* byte 1                   */
#define C1TXQCONU SFR8(0x0152) /* byte 2                   */
#define C1TXQCONT SFR8(0x0153) /* byte 3                   */
#define C1TXQCON SFR32(0x0150) /* full 32-bit access       */

/* Bit 0 - TXQNIE: TX Queue Not Full Interrupt Enable
 *   1 = Interrupt when TXQ transitions from full to not-full.          */
#define C1TXQCON_TXQNIE (1u << 0)

/* Bit 1 - TXQEIE: TX Queue Empty Interrupt Enable
 *   1 = Interrupt when TXQ becomes completely empty.                   */
#define C1TXQCON_TXQEIE (1u << 1)

/* Bits 4:3 - TXAT[1:0]: TX Attempts
 *   Controls retry behaviour.  Use CAN_TXAT_* constants.
 *   Only effective when C1CON.RTXAT = 1.                               */
#define C1TXQCON_TXAT_POS 3
#define C1TXQCON_TXAT_MASK (0x03u << 3)

/* Bit 5 - TXATIE: TX Attempts Exhausted Interrupt Enable
 *   1 = Interrupt when TX attempt limit is reached.                    */
#define C1TXQCON_TXATIE (1u << 5)

/* Bit 7 - TXEN: Transmit Enable
 *   1 = TXQ is enabled for transmitting.
 *   0 = TXQ is disabled (no messages sent from TXQ).                   */
#define C1TXQCON_TXEN (1u << 7)

/* Bits 12:8 - FSIZE[4:0]: FIFO Size
 *   Number of messages in TXQ = FSIZE + 1.
 *   Range: 0-31 (1-32 messages).                                       */
#define C1TXQCON_FSIZE_POS 8
#define C1TXQCON_FSIZE_MASK (0x1Fu << 8)

/* Bits 18:16 - PLSIZE[2:0]: Payload Size
 *   Use CAN_PLSIZE_* constants.                                        */
#define C1TXQCON_PLSIZE_POS 16
#define C1TXQCON_PLSIZE_MASK (0x07u << 16)

/* Bits 28:24 - TXPRI[4:0]: TX Priority
 *   Arbitration priority for this FIFO relative to other TX FIFOs.
 *   Lower values = higher priority (transmitted first).
 *   Range: 0-31.                                                       */
#define C1TXQCON_TXPRI_POS 24
#define C1TXQCON_TXPRI_MASK (0x1Fu << 24)

/* Bit 29 - FRESET: FIFO Reset (write-only, self-clearing)
 *   Write 1 = Reset TXQ FIFO pointers.                                */
#define C1TXQCON_FRESET (1u << 29)

/* Bit 30 - UINC: User Increment (write-only, self-clearing)
 *   Write 1 = Advance TXQ write pointer to next slot after writing
 *   a message object.                                                  */
#define C1TXQCON_UINC (1u << 30)

/* *********************************************************************
 *
 *  C1TXQSTA - TX Queue Status Register
 *  Address: 0x0154 (32-bit)
 *
 * ********************************************************************* */

#define C1TXQSTAL SFR8(0x0154) /* byte 0: status flags     */
#define C1TXQSTAH SFR8(0x0155) /* byte 1: TXQCI            */
#define C1TXQSTAU SFR8(0x0156) /* byte 2: reserved         */
#define C1TXQSTAT SFR8(0x0157) /* byte 3: reserved         */
#define C1TXQSTA SFR32(0x0154) /* full 32-bit access       */

/* Bit 0 - TXQNIF: TX Queue Not Full Interrupt Flag (read-only)
 *   1 = TXQ is not full; space available for new messages.             */
#define C1TXQSTA_TXQNIF (1u << 0)

/* Bit 1 - TXQEIF: TX Queue Empty Interrupt Flag (read-only)
 *   1 = TXQ is empty; no messages pending for transmission.            */
#define C1TXQSTA_TXQEIF (1u << 1)

/* Bit 4 - TXREQ: TX Request Pending (read-only)
 *   1 = TXQ has at least one message pending for transmission.
 *   0 = No pending transmissions in TXQ.                               */
#define C1TXQSTA_TXREQ (1u << 4)

/* Bit 5 - TXERR: TX Error (read-only)
 *   1 = A bus error occurred during the last transmission attempt.     */
#define C1TXQSTA_TXERR (1u << 5)

/* Bit 6 - TXLARB: TX Lost Arbitration (read-only)
 *   1 = The last transmission lost arbitration.                        */
#define C1TXQSTA_TXLARB (1u << 6)

/* Bit 7 - TXABT: TX Aborted (read-only)
 *   1 = The last transmission was aborted (by ABAT or attempt limit). */
#define C1TXQSTA_TXABT (1u << 7)

/* Bits 12:8 - TXQCI[4:0]: TX Queue Index (read-only)
 *   Points to the next message slot to be read by the CAN module
 *   for transmission.                                                  */
#define C1TXQSTA_TXQCI_POS 8
#define C1TXQSTA_TXQCI_MASK (0x1Fu << 8)

/* *********************************************************************
 *
 *  C1TXQUA - TX Queue User Address
 *  Address: 0x0158 (32-bit)
 *
 *  Read-only.  Points to the next available slot in the TXQ where
 *  the application should write the message object to be transmitted.
 *
 * ********************************************************************* */

#define C1TXQUAL SFR8(0x0158)
#define C1TXQUAH SFR8(0x0159)
#define C1TXQUAU SFR8(0x015A)
#define C1TXQUAT SFR8(0x015B)
#define C1TXQUA SFR32(0x0158)

/* *********************************************************************
 *
 *  C1FIFOCONn - FIFO Control Registers (n = 1, 2, 3)
 *
 *  Each of the three programmable FIFOs has an identical control
 *  register layout.  A FIFO can be configured as either TX or RX
 *  by setting/clearing the TXEN bit.
 *
 * ********************************************************************* */

/* --- FIFO 1 Control: C1FIFOCON1 @ 0x015C --- */

#define C1FIFOCON1L SFR8(0x015C)
#define C1FIFOCON1H SFR8(0x015D)
#define C1FIFOCON1U SFR8(0x015E)
#define C1FIFOCON1T SFR8(0x015F)
#define C1FIFOCON1 SFR32(0x015C)

/* --- FIFO 2 Control: C1FIFOCON2 @ 0x0168 --- */

#define C1FIFOCON2L SFR8(0x0168)
#define C1FIFOCON2H SFR8(0x0169)
#define C1FIFOCON2U SFR8(0x016A)
#define C1FIFOCON2T SFR8(0x016B)
#define C1FIFOCON2 SFR32(0x0168)

/* --- FIFO 3 Control: C1FIFOCON3 @ 0x0174 --- */

#define C1FIFOCON3L SFR8(0x0174)
#define C1FIFOCON3H SFR8(0x0175)
#define C1FIFOCON3U SFR8(0x0176)
#define C1FIFOCON3T SFR8(0x0177)
#define C1FIFOCON3 SFR32(0x0174)

/* --- Shared FIFOCONn bit definitions ---
 *
 * These bit masks/positions apply identically to C1FIFOCON1, C1FIFOCON2,
 * and C1FIFOCON3.  OR them into any C1FIFOCONn register.
 *
 * Byte 0 layout:
 *   Bit 0 - TFNRFNIE: FIFO Not Full (TX) / Not Empty (RX) Interrupt Enable
 *   Bit 1 - TFHRFHIE: FIFO Half Full Interrupt Enable
 *   Bit 2 - TFERFFIE: FIFO Full (TX) / Full (RX) Interrupt Enable
 *   Bit 3 - RXOVIE:   Receive Overflow Interrupt Enable (RX FIFOs only)
 *   Bit 4 - TXATIE:   TX Attempt Interrupt Enable (TX FIFOs only)
 *   Bits 6:5 - TXAT[1:0]: TX Attempts (use CAN_TXAT_* constants)
 *   Bit 7 - TXEN:     FIFO Direction (1=TX, 0=RX)
 *
 * Byte 1 layout:
 *   Bit 8 - RTREN:    Auto RTR Enable (respond to remote frames)
 *   Bit 9 - RXTSEN:   RX Timestamp Enable
 *   Bit 10 - TXABT:   (read in FIFOSTAn, but reserved in FIFOCONn)
 *   Bits 15:11 - reserved
 *
 * Byte 2 layout:
 *   Bits 20:16 - FSIZE[4:0]: FIFO Depth (messages = FSIZE+1)
 *   Bits 23:21 - PLSIZE[2:0]: Payload Size (use CAN_PLSIZE_*)
 *
 * Byte 3 layout:
 *   Bits 28:24 - TXPRI[4:0]: TX Priority (TX FIFOs only)
 *   Bit 29 - TXREQ:   TX Request (write 1 to initiate TX, TX FIFO only)
 *   Bit 30 - UINC:    User Increment (write-only, self-clearing)
 *   Bit 31 - FRESET:  FIFO Reset (write-only, self-clearing)
 */

/* Byte 0 bits */
#define C1FIFOCON_TFNRFNIE (1u << 0) /* Not-full/not-empty int en     */
#define C1FIFOCON_TFHRFHIE (1u << 1) /* Half-full int en              */
#define C1FIFOCON_TFERFFIE (1u << 2) /* Full int en                   */
#define C1FIFOCON_RXOVIE (1u << 3)   /* RX overflow int en            */
#define C1FIFOCON_TXATIE (1u << 4)   /* TX attempt int en             */
#define C1FIFOCON_TXAT_POS 5
#define C1FIFOCON_TXAT_MASK (0x03u << 5)
#define C1FIFOCON_TXEN (1u << 7) /* 1=TX FIFO, 0=RX FIFO         */

/* Byte 1 bits */
#define C1FIFOCON_RTREN (1u << 8)  /* Auto RTR enable               */
#define C1FIFOCON_RXTSEN (1u << 9) /* RX timestamp enable           */

/* Byte 2 bits */
#define C1FIFOCON_FSIZE_POS 16
#define C1FIFOCON_FSIZE_MASK (0x1Fu << 16)
#define C1FIFOCON_PLSIZE_POS 21
#define C1FIFOCON_PLSIZE_MASK (0x07u << 21)

/* Byte 3 bits */
#define C1FIFOCON_TXPRI_POS 24
#define C1FIFOCON_TXPRI_MASK (0x1Fu << 24)
#define C1FIFOCON_TXREQ (1u << 29)  /* TX request (TX FIFOs)    */
#define C1FIFOCON_UINC (1u << 30)   /* User increment (w/o)     */
#define C1FIFOCON_FRESET (1u << 31) /* FIFO reset (w/o)         */

/* *********************************************************************
 *
 *  C1FIFOSTAn - FIFO Status Registers (n = 1, 2, 3)
 *
 * ********************************************************************* */

/* --- FIFO 1 Status: C1FIFOSTA1 @ 0x0160 --- */

#define C1FIFOSTA1L SFR8(0x0160)
#define C1FIFOSTA1H SFR8(0x0161)
#define C1FIFOSTA1U SFR8(0x0162)
#define C1FIFOSTA1T SFR8(0x0163)
#define C1FIFOSTA1 SFR32(0x0160)

/* --- FIFO 2 Status: C1FIFOSTA2 @ 0x016C --- */

#define C1FIFOSTA2L SFR8(0x016C)
#define C1FIFOSTA2H SFR8(0x016D)
#define C1FIFOSTA2U SFR8(0x016E)
#define C1FIFOSTA2T SFR8(0x016F)
#define C1FIFOSTA2 SFR32(0x016C)

/* --- FIFO 3 Status: C1FIFOSTA3 @ 0x0178 --- */

#define C1FIFOSTA3L SFR8(0x0178)
#define C1FIFOSTA3H SFR8(0x0179)
#define C1FIFOSTA3U SFR8(0x017A)
#define C1FIFOSTA3T SFR8(0x017B)
#define C1FIFOSTA3 SFR32(0x0178)

/* --- Shared FIFOSTAn bit definitions ---
 *
 * Byte 0:
 *   Bit 0 - TFNRFNIF: Not-Full (TX) / Not-Empty (RX) Interrupt Flag
 *   Bit 1 - TFHRFHIF: Half-Full Interrupt Flag
 *   Bit 2 - TFERFFIF: Full Interrupt Flag
 *   Bit 3 - RXOVIF:   RX Overflow Interrupt Flag (cleared by sw)
 *   Bit 4 - TXATIF:   TX Attempt Interrupt Flag (cleared by sw)
 *   Bit 5 - TXERR:    TX Error (read-only)
 *   Bit 6 - TXLARB:   TX Lost Arbitration (read-only)
 *   Bit 7 - TXABT:    TX Aborted (read-only)
 *
 * Byte 1:
 *   Bits 12:8 - FIFOCI[4:0]: FIFO Message Index (read-only)
 */

#define C1FIFOSTA_TFNRFNIF (1u << 0) /* Not-full / not-empty flag     */
#define C1FIFOSTA_TFHRFHIF (1u << 1) /* Half-full flag                */
#define C1FIFOSTA_TFERFFIF (1u << 2) /* Full flag                     */
#define C1FIFOSTA_RXOVIF (1u << 3)   /* RX overflow flag              */
#define C1FIFOSTA_TXATIF (1u << 4)   /* TX attempt flag               */
#define C1FIFOSTA_TXERR (1u << 5)    /* TX error (read-only)          */
#define C1FIFOSTA_TXLARB (1u << 6)   /* TX lost arbitration (r/o)     */
#define C1FIFOSTA_TXABT (1u << 7)    /* TX aborted (read-only)        */

#define C1FIFOSTA_FIFOCI_POS 8
#define C1FIFOSTA_FIFOCI_MASK (0x1Fu << 8)

/* *********************************************************************
 *
 *  C1FIFOUAn - FIFO User Address Registers (n = 1, 2, 3)
 *
 *  Read-only.  For TX FIFOs, points to the next slot to write a
 *  message object.  For RX FIFOs, points to the next message to read.
 *
 * ********************************************************************* */

/* --- FIFO 1 User Address: C1FIFOUA1 @ 0x0164 --- */

#define C1FIFOUA1L SFR8(0x0164)
#define C1FIFOUA1H SFR8(0x0165)
#define C1FIFOUA1U SFR8(0x0166)
#define C1FIFOUA1T SFR8(0x0167)
#define C1FIFOUA1 SFR32(0x0164)

/* --- FIFO 2 User Address: C1FIFOUA2 @ 0x0170 --- */

#define C1FIFOUA2L SFR8(0x0170)
#define C1FIFOUA2H SFR8(0x0171)
#define C1FIFOUA2U SFR8(0x0172)
#define C1FIFOUA2T SFR8(0x0173)
#define C1FIFOUA2 SFR32(0x0170)

/* --- FIFO 3 User Address: C1FIFOUA3 @ 0x017C --- */

#define C1FIFOUA3L SFR8(0x017C)
#define C1FIFOUA3H SFR8(0x017D)
#define C1FIFOUA3U SFR8(0x017E)
#define C1FIFOUA3T SFR8(0x017F)
#define C1FIFOUA3 SFR32(0x017C)

/* *********************************************************************
 *
 *  C1FLTCONn - Filter Control Registers (n = 0, 1, 2)
 *
 *  Each 32-bit register controls four acceptance filters.  Per-filter
 *  fields are packed into 8-bit lanes:
 *
 *  C1FLTCON0 (0x0180): filters 0-3
 *    Byte 0: FLTEN0 (bit7), F0BP[4:0] (bits 4:0)  -> filter 0
 *    Byte 1: FLTEN1 (bit7), F1BP[4:0] (bits 4:0)  -> filter 1
 *    Byte 2: FLTEN2 (bit7), F2BP[4:0] (bits 4:0)  -> filter 2
 *    Byte 3: FLTEN3 (bit7), F3BP[4:0] (bits 4:0)  -> filter 3
 *
 *  C1FLTCON1 (0x0184): filters 4-7
 *  C1FLTCON2 (0x0188): filters 8-11
 *
 *  FLTENn: Filter Enable (1=enabled, 0=disabled)
 *  FnBP[4:0]: Filter Buffer Pointer - selects which FIFO receives
 *             messages matching this filter.  0=TXQ (unusual), 1-3=FIFO1-3.
 *
 * ********************************************************************* */

#define C1FLTCON0L SFR8(0x0180) /* filter 0 control         */
#define C1FLTCON0H SFR8(0x0181) /* filter 1 control         */
#define C1FLTCON0U SFR8(0x0182) /* filter 2 control         */
#define C1FLTCON0T SFR8(0x0183) /* filter 3 control         */
#define C1FLTCON0 SFR32(0x0180)

#define C1FLTCON1L SFR8(0x0184) /* filter 4 control         */
#define C1FLTCON1H SFR8(0x0185) /* filter 5 control         */
#define C1FLTCON1U SFR8(0x0186) /* filter 6 control         */
#define C1FLTCON1T SFR8(0x0187) /* filter 7 control         */
#define C1FLTCON1 SFR32(0x0184)

#define C1FLTCON2L SFR8(0x0188) /* filter 8 control         */
#define C1FLTCON2H SFR8(0x0189) /* filter 9 control         */
#define C1FLTCON2U SFR8(0x018A) /* filter 10 control        */
#define C1FLTCON2T SFR8(0x018B) /* filter 11 control        */
#define C1FLTCON2 SFR32(0x0188)

/* Per-filter bit masks (apply to the individual byte for that filter)  */
#define C1FLTCON_FLTEN (1u << 7) /* Filter enable bit        */
#define C1FLTCON_FBP_POS 0       /* Buffer pointer position  */
#define C1FLTCON_FBP_MASK 0x1Fu  /* Buffer pointer mask      */

/* Convenience: per-filter enable bits within the 32-bit register       */
#define C1FLTCON_FLTEN0 (1u << 7)  /* byte 0, bit 7            */
#define C1FLTCON_FLTEN1 (1u << 15) /* byte 1, bit 7            */
#define C1FLTCON_FLTEN2 (1u << 23) /* byte 2, bit 7            */
#define C1FLTCON_FLTEN3 (1u << 31) /* byte 3, bit 7            */

/* Convenience: buffer pointer positions within the 32-bit register     */
#define C1FLTCON_F0BP_POS 0  /* filter 0 BP in bits 4:0  */
#define C1FLTCON_F1BP_POS 8  /* filter 1 BP in bits 12:8 */
#define C1FLTCON_F2BP_POS 16 /* filter 2 BP in bits 20:16*/
#define C1FLTCON_F3BP_POS 24 /* filter 3 BP in bits 28:24*/

/* Address 0x018C is reserved (gap before filter objects)               */

/* *********************************************************************
 *
 *  C1FLTOBJn / C1MASKn - Filter Object and Mask Registers
 *
 *  12 filter/mask pairs for CAN message acceptance filtering.
 *  Each filter object and mask is a 32-bit register with this layout:
 *
 *  Filter Object (C1FLTOBJn):
 *    Bits  10:0  - SID[10:0]:  Standard Identifier bits to match
 *    Bits  28:11 - EID[17:0]:  Extended Identifier bits to match
 *    Bit   29    - SID11:      12th SID bit (CAN FD base format)
 *    Bit   30    - EXIDE:      Extended ID Filter Enable
 *                               1 = Match extended frames (29-bit ID)
 *                               0 = Match standard frames (11-bit ID)
 *
 *  Mask Register (C1MASKn):
 *    Bits  10:0  - MSID[10:0]: SID mask (1=must match, 0=don't care)
 *    Bits  28:11 - MEID[17:0]: EID mask (1=must match, 0=don't care)
 *    Bit   29    - MSID11:     SID11 mask bit
 *    Bit   30    - MIDE:       Identifier type mask
 *                               1 = EXIDE bit must match for acceptance
 *                               0 = Match both standard and extended
 *
 * ********************************************************************* */

/* Shared bit positions for all filter object and mask registers        */
#define C1FLTOBJ_SID_POS 0
#define C1FLTOBJ_SID_MASK (0x07FFu << 0) /* SID[10:0]            */
#define C1FLTOBJ_EID_POS 11
#define C1FLTOBJ_EID_MASK (0x3FFFFu << 11) /* EID[17:0]            */
#define C1FLTOBJ_SID11 (1u << 29)          /* SID11 bit            */
#define C1FLTOBJ_EXIDE (1u << 30)          /* Extended ID enable   */

#define C1MASK_MSID_POS 0
#define C1MASK_MSID_MASK (0x07FFu << 0) /* MSID[10:0]          */
#define C1MASK_MEID_POS 11
#define C1MASK_MEID_MASK (0x3FFFFu << 11) /* MEID[17:0]          */
#define C1MASK_MSID11 (1u << 29)          /* MSID11 mask         */
#define C1MASK_MIDE (1u << 30)            /* ID type mask enable */

/* --- Filter 0 --- */
#define C1FLTOBJ0L SFR8(0x018C)
#define C1FLTOBJ0H SFR8(0x018D)
#define C1FLTOBJ0U SFR8(0x018E)
#define C1FLTOBJ0T SFR8(0x018F)
#define C1FLTOBJ0 SFR32(0x018C)

#define C1MASK0L SFR8(0x0190)
#define C1MASK0H SFR8(0x0191)
#define C1MASK0U SFR8(0x0192)
#define C1MASK0T SFR8(0x0193)
#define C1MASK0 SFR32(0x0190)

/* --- Filter 1 --- */
#define C1FLTOBJ1L SFR8(0x0194)
#define C1FLTOBJ1H SFR8(0x0195)
#define C1FLTOBJ1U SFR8(0x0196)
#define C1FLTOBJ1T SFR8(0x0197)
#define C1FLTOBJ1 SFR32(0x0194)

#define C1MASK1L SFR8(0x0198)
#define C1MASK1H SFR8(0x0199)
#define C1MASK1U SFR8(0x019A)
#define C1MASK1T SFR8(0x019B)
#define C1MASK1 SFR32(0x0198)

/* --- Filter 2 --- */
#define C1FLTOBJ2L SFR8(0x019C)
#define C1FLTOBJ2H SFR8(0x019D)
#define C1FLTOBJ2U SFR8(0x019E)
#define C1FLTOBJ2T SFR8(0x019F)
#define C1FLTOBJ2 SFR32(0x019C)

#define C1MASK2L SFR8(0x01A0)
#define C1MASK2H SFR8(0x01A1)
#define C1MASK2U SFR8(0x01A2)
#define C1MASK2T SFR8(0x01A3)
#define C1MASK2 SFR32(0x01A0)

/* --- Filter 3 --- */
#define C1FLTOBJ3L SFR8(0x01A4)
#define C1FLTOBJ3H SFR8(0x01A5)
#define C1FLTOBJ3U SFR8(0x01A6)
#define C1FLTOBJ3T SFR8(0x01A7)
#define C1FLTOBJ3 SFR32(0x01A4)

#define C1MASK3L SFR8(0x01A8)
#define C1MASK3H SFR8(0x01A9)
#define C1MASK3U SFR8(0x01AA)
#define C1MASK3T SFR8(0x01AB)
#define C1MASK3 SFR32(0x01A8)

/* --- Filter 4 --- */
#define C1FLTOBJ4L SFR8(0x01AC)
#define C1FLTOBJ4H SFR8(0x01AD)
#define C1FLTOBJ4U SFR8(0x01AE)
#define C1FLTOBJ4T SFR8(0x01AF)
#define C1FLTOBJ4 SFR32(0x01AC)

#define C1MASK4L SFR8(0x01B0)
#define C1MASK4H SFR8(0x01B1)
#define C1MASK4U SFR8(0x01B2)
#define C1MASK4T SFR8(0x01B3)
#define C1MASK4 SFR32(0x01B0)

/* --- Filter 5 --- */
#define C1FLTOBJ5L SFR8(0x01B4)
#define C1FLTOBJ5H SFR8(0x01B5)
#define C1FLTOBJ5U SFR8(0x01B6)
#define C1FLTOBJ5T SFR8(0x01B7)
#define C1FLTOBJ5 SFR32(0x01B4)

#define C1MASK5L SFR8(0x01B8)
#define C1MASK5H SFR8(0x01B9)
#define C1MASK5U SFR8(0x01BA)
#define C1MASK5T SFR8(0x01BB)
#define C1MASK5 SFR32(0x01B8)

/* --- Filter 6 --- */
#define C1FLTOBJ6L SFR8(0x01BC)
#define C1FLTOBJ6H SFR8(0x01BD)
#define C1FLTOBJ6U SFR8(0x01BE)
#define C1FLTOBJ6T SFR8(0x01BF)
#define C1FLTOBJ6 SFR32(0x01BC)

#define C1MASK6L SFR8(0x01C0)
#define C1MASK6H SFR8(0x01C1)
#define C1MASK6U SFR8(0x01C2)
#define C1MASK6T SFR8(0x01C3)
#define C1MASK6 SFR32(0x01C0)

/* --- Filter 7 --- */
#define C1FLTOBJ7L SFR8(0x01C4)
#define C1FLTOBJ7H SFR8(0x01C5)
#define C1FLTOBJ7U SFR8(0x01C6)
#define C1FLTOBJ7T SFR8(0x01C7)
#define C1FLTOBJ7 SFR32(0x01C4)

#define C1MASK7L SFR8(0x01C8)
#define C1MASK7H SFR8(0x01C9)
#define C1MASK7U SFR8(0x01CA)
#define C1MASK7T SFR8(0x01CB)
#define C1MASK7 SFR32(0x01C8)

/* --- Filter 8 --- */
#define C1FLTOBJ8L SFR8(0x01CC)
#define C1FLTOBJ8H SFR8(0x01CD)
#define C1FLTOBJ8U SFR8(0x01CE)
#define C1FLTOBJ8T SFR8(0x01CF)
#define C1FLTOBJ8 SFR32(0x01CC)

#define C1MASK8L SFR8(0x01D0)
#define C1MASK8H SFR8(0x01D1)
#define C1MASK8U SFR8(0x01D2)
#define C1MASK8T SFR8(0x01D3)
#define C1MASK8 SFR32(0x01D0)

/* --- Filter 9 --- */
#define C1FLTOBJ9L SFR8(0x01D4)
#define C1FLTOBJ9H SFR8(0x01D5)
#define C1FLTOBJ9U SFR8(0x01D6)
#define C1FLTOBJ9T SFR8(0x01D7)
#define C1FLTOBJ9 SFR32(0x01D4)

#define C1MASK9L SFR8(0x01D8)
#define C1MASK9H SFR8(0x01D9)
#define C1MASK9U SFR8(0x01DA)
#define C1MASK9T SFR8(0x01DB)
#define C1MASK9 SFR32(0x01D8)

/* --- Filter 10 --- */
#define C1FLTOBJ10L SFR8(0x01DC)
#define C1FLTOBJ10H SFR8(0x01DD)
#define C1FLTOBJ10U SFR8(0x01DE)
#define C1FLTOBJ10T SFR8(0x01DF)
#define C1FLTOBJ10 SFR32(0x01DC)

#define C1MASK10L SFR8(0x01E0)
#define C1MASK10H SFR8(0x01E1)
#define C1MASK10U SFR8(0x01E2)
#define C1MASK10T SFR8(0x01E3)
#define C1MASK10 SFR32(0x01E0)

/* --- Filter 11 --- */
#define C1FLTOBJ11L SFR8(0x01E4)
#define C1FLTOBJ11H SFR8(0x01E5)
#define C1FLTOBJ11U SFR8(0x01E6)
#define C1FLTOBJ11T SFR8(0x01E7)
#define C1FLTOBJ11 SFR32(0x01E4)

#define C1MASK11L SFR8(0x01E8)
#define C1MASK11H SFR8(0x01E9)
#define C1MASK11U SFR8(0x01EA)
#define C1MASK11T SFR8(0x01EB)
#define C1MASK11 SFR32(0x01E8)

/* *********************************************************************
 *
 *  CAN FD Message Object Layout (in CAN Message RAM)
 *
 *  These bit definitions describe the format of TX and RX message
 *  objects stored in the CAN message RAM.  The application reads/writes
 *  these structures through the FIFO user-address pointers (C1TXQUA,
 *  C1FIFOUAn).
 *
 *  TX Message Object (word 0, 32-bit):
 *    Bits  10:0  - SID[10:0]:  Standard Identifier
 *    Bits  28:11 - EID[17:0]:  Extended Identifier
 *    Bit   29    - SID11:      12th SID bit (if SID11EN=1)
 *
 *  TX Message Object (word 1, 32-bit):
 *    Bits   3:0  - DLC[3:0]:   Data Length Code
 *    Bit    4    - IDE:         Identifier Extension (1=extended)
 *    Bit    5    - RTR:         Remote Transmission Request
 *    Bit    6    - BRS:         Bit Rate Switch (1=switch to data rate)
 *    Bit    7    - FDF:         FD Frame (1=CAN FD, 0=CAN 2.0)
 *    Bit    8    - ESI:         Error State Indicator
 *    Bits  15:9  - SEQ[6:0]:   Sequence number (echoed back in TEF)
 *
 *  Data bytes follow starting at word 2 (byte offset 8).
 *
 * ********************************************************************* */

/* Message object word 0 (TX/RX ID word) */
#define CAN_MSG_SID_POS 0
#define CAN_MSG_SID_MASK (0x07FFu << 0)
#define CAN_MSG_EID_POS 11
#define CAN_MSG_EID_MASK (0x3FFFFu << 11)
#define CAN_MSG_SID11 (1u << 29)

/* Message object word 1 (TX/RX control word) */
#define CAN_MSG_DLC_POS 0
#define CAN_MSG_DLC_MASK (0x0Fu << 0)
#define CAN_MSG_IDE (1u << 4) /* Extended ID frame           */
#define CAN_MSG_RTR (1u << 5) /* Remote TX request           */
#define CAN_MSG_BRS (1u << 6) /* Bit Rate Switch             */
#define CAN_MSG_FDF (1u << 7) /* CAN FD frame format         */
#define CAN_MSG_ESI (1u << 8) /* Error State Indicator       */
#define CAN_MSG_SEQ_POS 9
#define CAN_MSG_SEQ_MASK (0x7Fu << 9)

/* DLC-to-byte-count mapping for CAN FD:
 *   DLC 0-8:   0-8 bytes (same as CAN 2.0)
 *   DLC 9:    12 bytes
 *   DLC 10:   16 bytes
 *   DLC 11:   20 bytes
 *   DLC 12:   24 bytes
 *   DLC 13:   32 bytes
 *   DLC 14:   48 bytes
 *   DLC 15:   64 bytes                                                 */

/* *********************************************************************
 *
 *  Bit Timing Calculation Helpers
 *
 *  The CAN bit rate is determined by:
 *    Bit Rate = Fcan / (TQ_per_bit * (BRP + 1))
 *    TQ_per_bit = Sync_Seg(1) + TSEG1 + 1 + TSEG2 + 1
 *
 *  Fcan is the CAN module clock (system clock when CLKSEL=1).
 *
 *  Example: 500 kbit/s nominal @ 64 MHz Fcan
 *    BRP=0, TSEG1=62, TSEG2=15, SJW=15
 *    TQ_per_bit = 1 + 63 + 16 = 80
 *    Bit Rate = 64 MHz / (80 * 1) = 800 kHz ... adjust as needed.
 *
 *  Refer to DS40002213D Section 29.3 for detailed bit timing examples.
 *
 * ********************************************************************* */

#endif /* PIC18F_Q84_CAN_H */
