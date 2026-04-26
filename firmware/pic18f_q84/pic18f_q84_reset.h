/*
 * pic18f_q84_reset.h
 *
 * Reset, Brown-out Reset (BOR), and Voltage Regulator control registers
 * for PIC18F27/47/57Q84 family.
 *
 * Covers:
 *   - VREGCON : Voltage regulator power-mode selection
 *   - BORCON  : Brown-out reset enable and ready status
 *
 * Note: PCON0 and PCON1 (reset cause / status flags) are located in the
 *       CPU core register header (pic18f_q84_cpu.h) because they are
 *       architecturally part of the CPU subsystem.
 *
 * Reference: DS40002213D - PIC18F27/47/57Q84 Data Sheet
 *            Section 6 - Resets
 *
 * Requires: SFR8(addr) macro defined externally (provides volatile
 *           uint8_t access to an absolute SFR address).
 */

#ifndef PIC18F_Q84_RESET_H
#define PIC18F_Q84_RESET_H

/* ===================================================================== */
/*  VREGCON - Voltage Regulator Control Register                         */
/*  Address: 0x0048                                                      */
/*                                                                       */
/*  Controls the internal voltage regulator operating mode.  The LDO     */
/*  regulator can be configured to stay active in all power modes or     */
/*  switch to a low-power mode during Sleep to reduce current draw.      */
/*                                                                       */
/*  Bit 7-2: Unimplemented, read as 0                                   */
/*  Bits 1:0 - VREGPM[1:0]: Voltage Regulator Power Mode Select         */
/*      00 = Normal power mode - LDO regulator active in all modes       */
/*      01 = Low-power mode in Sleep, normal power mode in Run           */
/*      10 = Reserved                                                    */
/*      11 = Reserved                                                    */
/* ===================================================================== */
#define VREGCON SFR8(0x0048)

/* VREGPM - Voltage Regulator Power Mode Select (bits 1:0) */
#define VREGCON_VREGPM1 (1u << 1) /* VREGPM bit 1 */
#define VREGCON_VREGPM0 (1u << 0) /* VREGPM bit 0 */
#define VREGCON_VREGPM_MASK 0x03u /* Mask for VREGPM[1:0] field */

/* VREGPM field values */
#define VREGPM_NORMAL 0x00u   /* Normal power mode, LDO active in all modes */
#define VREGPM_LP_SLEEP 0x01u /* Low-power in Sleep, normal in Run          */

/* ===================================================================== */
/*  BORCON - Brown-out Reset Control Register                            */
/*  Address: 0x0049                                                      */
/*                                                                       */
/*  Provides software control over the Brown-out Reset (BOR) circuit     */
/*  and a read-only status bit indicating whether the BOR module is      */
/*  active and ready.                                                    */
/*                                                                       */
/*  The SBOREN bit is only effective when the BOREN configuration bits   */
/*  are set to "BOR controlled by software" mode (BOREN = 01).  In      */
/*  other BOREN modes the hardware overrides this bit.                   */
/*                                                                       */
/*  Bit 7   - SBOREN: Software BOR Enable                               */
/*      1 = BOR is enabled (when BOREN config bits permit SW control)    */
/*      0 = BOR is disabled                                              */
/*  Bits 6:1: Unimplemented, read as 0                                  */
/*  Bit 0   - BORRDY: BOR Ready Status (read-only)                      */
/*      1 = BOR circuit is active and ready                              */
/*      0 = BOR circuit is not ready or is disabled                      */
/* ===================================================================== */
#define BORCON SFR8(0x0049)

/* SBOREN - Software Brown-out Reset Enable (bit 7) */
#define BORCON_SBOREN (1u << 7)

/* BORRDY - Brown-out Reset Ready Status, read-only (bit 0) */
#define BORCON_BORRDY (1u << 0)

#endif /* PIC18F_Q84_RESET_H */
