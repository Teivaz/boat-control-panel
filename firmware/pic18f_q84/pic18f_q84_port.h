/*
 * PIC18F27/47/57Q84 I/O Port Registers
 *
 * GPIO control for PORTA through PORTF. Each port provides registers for:
 *   - PORTx   : Read pin input levels
 *   - LATx    : Output latch (read-modify-write safe)
 *   - TRISx   : Data direction (1 = input, 0 = output)
 *   - ANSELx  : Analog/digital select (1 = analog, 0 = digital)
 *   - WPUx    : Weak pull-up enable
 *   - ODCONx  : Open-drain control
 *   - SLRCONx : Slew rate limiting
 *   - INLVLx  : Input level select (ST vs TTL)
 *
 * Package availability:
 *   PIC18F27Q84  (28-pin)     : PORTA, PORTB, PORTC
 *   PIC18F47Q84  (40/44-pin)  : PORTA, PORTB, PORTC, PORTD, PORTE
 *   PIC18F57Q84  (48-pin)     : PORTA, PORTB, PORTC, PORTD, PORTE, PORTF
 *
 * All registers for all ports are defined here. On smaller packages the
 * pins are not bonded out; reads return 0 and writes have no effect.
 *
 * Reference: Microchip DS40002213D
 *            "PIC18F27/47/57Q84 Data Sheet"
 */

#ifndef PIC18F_Q84_PORT_H
#define PIC18F_Q84_PORT_H

/* ========================================================================
 *  PORT Registers - Read GPIO Pin Input Levels
 * ========================================================================
 *  Reading PORTx returns the actual logic level present on each pin.
 *  Note: if TRIS bit = 0 (output) and ANSELx = 0, the read reflects
 *  the driven output. If ANSELx = 1, digital input reads as 0.
 * ======================================================================== */

#define PORTA SFR8(0x04CE) /* Bits 7:0 - RA7:RA0 pin levels           */
#define PORTB SFR8(0x04CF) /* Bits 7:0 - RB7:RB0 pin levels           */
#define PORTC SFR8(0x04D0) /* Bits 7:0 - RC7:RC0 pin levels           */
#define PORTD SFR8(0x04D1) /* Bits 7:0 - RD7:RD0 (40/44/48-pin only) */
#define PORTE SFR8(0x04D2) /* Bits 2:0 - RE2:RE0; Bit 3 - RE3        */
                           /*   RE3 is input-only (MCLR pin)          */
                           /*   (40/44/48-pin only)                   */
#define PORTF SFR8(0x04D3) /* Bits 7:0 - RF7:RF0 (48-pin only)       */

/* ========================================================================
 *  LAT Registers - Output Data Latch
 * ========================================================================
 *  Writing to LATx sets the output latch without the read-modify-write
 *  hazard that can occur when writing directly to PORTx. Always use
 *  LATx for setting outputs.
 *
 *  Reading LATx returns the latch contents, not the pin level.
 * ======================================================================== */

#define LATA SFR8(0x04BE) /* Bits 7:0 - LATA7:LATA0                 */
#define LATB SFR8(0x04BF) /* Bits 7:0 - LATB7:LATB0                 */
#define LATC SFR8(0x04C0) /* Bits 7:0 - LATC7:LATC0                 */
#define LATD SFR8(0x04C1) /* Bits 7:0 - LATD7:LATD0                 */
#define LATE SFR8(0x04C2) /* Bits 2:0 - LATE2:LATE0                 */
#define LATF SFR8(0x04C3) /* Bits 7:0 - LATF7:LATF0                 */

/* ========================================================================
 *  TRIS Registers - Data Direction Control
 * ========================================================================
 *  1 = Pin configured as Input  (default on POR)
 *  0 = Pin configured as Output
 *
 *  All TRISx bits default to 1 (input) after any reset.
 *  TRISE3 is hardwired to 1; RE3/MCLR is always an input.
 * ======================================================================== */

#define TRISA SFR8(0x04C6) /* Bits 7:0 - TRISA7:TRISA0               */
#define TRISB SFR8(0x04C7) /* Bits 7:0 - TRISB7:TRISB0               */
#define TRISC SFR8(0x04C8) /* Bits 7:0 - TRISC7:TRISC0               */
#define TRISD SFR8(0x04C9) /* Bits 7:0 - TRISD7:TRISD0               */
#define TRISE SFR8(0x04CA) /* Bits 2:0 - TRISE2:TRISE0               */
#define TRISF SFR8(0x04CB) /* Bits 7:0 - TRISF7:TRISF0               */

/* ========================================================================
 *  ANSEL Registers - Analog/Digital Pin Function Select
 * ========================================================================
 *  1 = Analog input  (pin disconnected from digital input buffer;
 *                      digital reads return 0; overrides digital output)
 *  0 = Digital I/O
 *
 *  Pins with analog capability default to analog mode after reset.
 *  Clear the ANSEL bit before using a pin for digital I/O.
 *
 *  Note: The ANSEL registers are NOT contiguous; each port's analog
 *  control register sits within that port's configuration block.
 * ======================================================================== */

#define ANSELA SFR8(0x0400) /* Bits 7:0 - ANSELA7:ANSELA0             */
#define ANSELB SFR8(0x0408) /* Bits 7:0 - ANSELB7:ANSELB0             */
#define ANSELC SFR8(0x0410) /* Bits 7:0 - ANSELC7:ANSELC0             */
#define ANSELD SFR8(0x0418) /* Bits 7:0 - ANSELD7:ANSELD0             */
#define ANSELE SFR8(0x0420) /* Bits 2:0 - ANSELE2:ANSELE0             */
#define ANSELF SFR8(0x0428) /* Bits 7:0 - ANSELF7:ANSELF0             */

/* ========================================================================
 *  WPU Registers - Weak Pull-Up Enable
 * ========================================================================
 *  1 = Internal weak pull-up enabled (only effective when pin is input;
 *      i.e., corresponding TRISx bit = 1)
 *  0 = Pull-up disabled
 *
 *  Pull-ups are individually controllable per pin. They are automatically
 *  disabled on pins configured as outputs.
 *
 *  WPUE3 controls the pull-up on RE3/MCLR (always input).
 * ======================================================================== */

#define WPUA SFR8(0x0401) /* Bits 7:0 - WPUA7:WPUA0                 */
#define WPUB SFR8(0x0409) /* Bits 7:0 - WPUB7:WPUB0                 */
#define WPUC SFR8(0x0411) /* Bits 7:0 - WPUC7:WPUC0                 */
#define WPUD SFR8(0x0419) /* Bits 7:0 - WPUD7:WPUD0                 */
#define WPUE SFR8(0x0421) /* Bits 3:0 - WPUE3:WPUE0                 */
                          /*   Bit 3 = WPUE3 (RE3/MCLR pull-up)     */
#define WPUF SFR8(0x0429) /* Bits 7:0 - WPUF7:WPUF0                 */

/* ========================================================================
 *  ODCON Registers - Open-Drain Output Control
 * ========================================================================
 *  1 = Open-drain output (pin can only sink current; requires external
 *      pull-up for logic high)
 *  0 = Push-pull output  (default; pin actively drives both high and low)
 *
 *  Open-drain mode is useful for wired-OR buses (e.g., I2C).
 * ======================================================================== */

#define ODCONA SFR8(0x0402) /* Bits 7:0 - ODCONA7:ODCONA0             */
#define ODCONB SFR8(0x040A) /* Bits 7:0 - ODCONB7:ODCONB0             */
#define ODCONC SFR8(0x0412) /* Bits 7:0 - ODCONC7:ODCONC0             */
#define ODCOND SFR8(0x041A) /* Bits 7:0 - ODCOND7:ODCOND0             */
#define ODCONE SFR8(0x0422) /* Bits 2:0 - ODCONE2:ODCONE0             */
#define ODCONF SFR8(0x042A) /* Bits 7:0 - ODCONF7:ODCONF0             */

/* ========================================================================
 *  SLRCON Registers - Slew Rate Control
 * ========================================================================
 *  1 = Limited slew rate  (reduces EMI, default on many pins)
 *  0 = Maximum slew rate  (fastest edge transitions)
 * ======================================================================== */

#define SLRCONA SFR8(0x0403) /* Bits 7:0 - SLRCONA7:SLRCONA0           */
#define SLRCONB SFR8(0x040B) /* Bits 7:0 - SLRCONB7:SLRCONB0           */
#define SLRCONC SFR8(0x0413) /* Bits 7:0 - SLRCONC7:SLRCONC0           */
#define SLRCOND SFR8(0x041B) /* Bits 7:0 - SLRCOND7:SLRCOND0           */
#define SLRCONE SFR8(0x0423) /* Bits 2:0 - SLRCONE2:SLRCONE0           */
#define SLRCONF SFR8(0x042B) /* Bits 7:0 - SLRCONF7:SLRCONF0           */

/* ========================================================================
 *  INLVL Registers - Input Level Control
 * ========================================================================
 *  1 = ST (Schmitt Trigger) input thresholds
 *  0 = TTL compatible input thresholds
 *
 *  ST inputs have higher noise immunity with hysteresis.
 *  TTL inputs have a lower threshold (~0.8V for VIL, ~2.0V for VIH).
 * ======================================================================== */

#define INLVLA SFR8(0x0404) /* Bits 7:0 - INLVLA7:INLVLA0             */
#define INLVLB SFR8(0x040C) /* Bits 7:0 - INLVLB7:INLVLB0             */
#define INLVLC SFR8(0x0414) /* Bits 7:0 - INLVLC7:INLVLC0             */
#define INLVLD SFR8(0x041C) /* Bits 7:0 - INLVLD7:INLVLD0             */
#define INLVLE SFR8(0x0424) /* Bits 3:0 - INLVLE3:INLVLE0             */
#define INLVLF SFR8(0x042C) /* Bits 7:0 - INLVLF7:INLVLF0             */

/* ========================================================================
 *  Pin Bit Masks and Bit Positions - PORTA
 * ======================================================================== */

#define RA0_bm 0x01 /* Pin RA0 bit mask                        */
#define RA1_bm 0x02 /* Pin RA1 bit mask                        */
#define RA2_bm 0x04 /* Pin RA2 bit mask                        */
#define RA3_bm 0x08 /* Pin RA3 bit mask                        */
#define RA4_bm 0x10 /* Pin RA4 bit mask                        */
#define RA5_bm 0x20 /* Pin RA5 bit mask                        */
#define RA6_bm 0x40 /* Pin RA6 bit mask                        */
#define RA7_bm 0x80 /* Pin RA7 bit mask                        */

#define RA0_bp 0 /* Pin RA0 bit position                    */
#define RA1_bp 1 /* Pin RA1 bit position                    */
#define RA2_bp 2 /* Pin RA2 bit position                    */
#define RA3_bp 3 /* Pin RA3 bit position                    */
#define RA4_bp 4 /* Pin RA4 bit position                    */
#define RA5_bp 5 /* Pin RA5 bit position                    */
#define RA6_bp 6 /* Pin RA6 bit position                    */
#define RA7_bp 7 /* Pin RA7 bit position                    */

/* ========================================================================
 *  Pin Bit Masks and Bit Positions - PORTB
 * ======================================================================== */

#define RB0_bm 0x01 /* Pin RB0 bit mask                        */
#define RB1_bm 0x02 /* Pin RB1 bit mask                        */
#define RB2_bm 0x04 /* Pin RB2 bit mask                        */
#define RB3_bm 0x08 /* Pin RB3 bit mask                        */
#define RB4_bm 0x10 /* Pin RB4 bit mask                        */
#define RB5_bm 0x20 /* Pin RB5 bit mask                        */
#define RB6_bm 0x40 /* Pin RB6 bit mask                        */
#define RB7_bm 0x80 /* Pin RB7 bit mask                        */

#define RB0_bp 0 /* Pin RB0 bit position                    */
#define RB1_bp 1 /* Pin RB1 bit position                    */
#define RB2_bp 2 /* Pin RB2 bit position                    */
#define RB3_bp 3 /* Pin RB3 bit position                    */
#define RB4_bp 4 /* Pin RB4 bit position                    */
#define RB5_bp 5 /* Pin RB5 bit position                    */
#define RB6_bp 6 /* Pin RB6 bit position                    */
#define RB7_bp 7 /* Pin RB7 bit position                    */

/* ========================================================================
 *  Pin Bit Masks and Bit Positions - PORTC
 * ======================================================================== */

#define RC0_bm 0x01 /* Pin RC0 bit mask                        */
#define RC1_bm 0x02 /* Pin RC1 bit mask                        */
#define RC2_bm 0x04 /* Pin RC2 bit mask                        */
#define RC3_bm 0x08 /* Pin RC3 bit mask                        */
#define RC4_bm 0x10 /* Pin RC4 bit mask                        */
#define RC5_bm 0x20 /* Pin RC5 bit mask                        */
#define RC6_bm 0x40 /* Pin RC6 bit mask                        */
#define RC7_bm 0x80 /* Pin RC7 bit mask                        */

#define RC0_bp 0 /* Pin RC0 bit position                    */
#define RC1_bp 1 /* Pin RC1 bit position                    */
#define RC2_bp 2 /* Pin RC2 bit position                    */
#define RC3_bp 3 /* Pin RC3 bit position                    */
#define RC4_bp 4 /* Pin RC4 bit position                    */
#define RC5_bp 5 /* Pin RC5 bit position                    */
#define RC6_bp 6 /* Pin RC6 bit position                    */
#define RC7_bp 7 /* Pin RC7 bit position                    */

/* ========================================================================
 *  Pin Bit Masks and Bit Positions - PORTD (40/44/48-pin only)
 * ======================================================================== */

#define RD0_bm 0x01 /* Pin RD0 bit mask                        */
#define RD1_bm 0x02 /* Pin RD1 bit mask                        */
#define RD2_bm 0x04 /* Pin RD2 bit mask                        */
#define RD3_bm 0x08 /* Pin RD3 bit mask                        */
#define RD4_bm 0x10 /* Pin RD4 bit mask                        */
#define RD5_bm 0x20 /* Pin RD5 bit mask                        */
#define RD6_bm 0x40 /* Pin RD6 bit mask                        */
#define RD7_bm 0x80 /* Pin RD7 bit mask                        */

#define RD0_bp 0 /* Pin RD0 bit position                    */
#define RD1_bp 1 /* Pin RD1 bit position                    */
#define RD2_bp 2 /* Pin RD2 bit position                    */
#define RD3_bp 3 /* Pin RD3 bit position                    */
#define RD4_bp 4 /* Pin RD4 bit position                    */
#define RD5_bp 5 /* Pin RD5 bit position                    */
#define RD6_bp 6 /* Pin RD6 bit position                    */
#define RD7_bp 7 /* Pin RD7 bit position                    */

/* ========================================================================
 *  Pin Bit Masks and Bit Positions - PORTE (40/44/48-pin only)
 * ========================================================================
 *  RE0-RE2 : Standard I/O pins
 *  RE3     : Input-only (MCLR pin); no TRIS/LAT/ANSEL/ODCON control
 * ======================================================================== */

#define RE0_bm 0x01 /* Pin RE0 bit mask                        */
#define RE1_bm 0x02 /* Pin RE1 bit mask                        */
#define RE2_bm 0x04 /* Pin RE2 bit mask                        */
#define RE3_bm 0x08 /* Pin RE3 bit mask (input-only, MCLR)     */

#define RE0_bp 0 /* Pin RE0 bit position                    */
#define RE1_bp 1 /* Pin RE1 bit position                    */
#define RE2_bp 2 /* Pin RE2 bit position                    */
#define RE3_bp 3 /* Pin RE3 bit position (input-only)       */

/* ========================================================================
 *  Pin Bit Masks and Bit Positions - PORTF (48-pin only)
 * ======================================================================== */

#define RF0_bm 0x01 /* Pin RF0 bit mask                        */
#define RF1_bm 0x02 /* Pin RF1 bit mask                        */
#define RF2_bm 0x04 /* Pin RF2 bit mask                        */
#define RF3_bm 0x08 /* Pin RF3 bit mask                        */
#define RF4_bm 0x10 /* Pin RF4 bit mask                        */
#define RF5_bm 0x20 /* Pin RF5 bit mask                        */
#define RF6_bm 0x40 /* Pin RF6 bit mask                        */
#define RF7_bm 0x80 /* Pin RF7 bit mask                        */

#define RF0_bp 0 /* Pin RF0 bit position                    */
#define RF1_bp 1 /* Pin RF1 bit position                    */
#define RF2_bp 2 /* Pin RF2 bit position                    */
#define RF3_bp 3 /* Pin RF3 bit position                    */
#define RF4_bp 4 /* Pin RF4 bit position                    */
#define RF5_bp 5 /* Pin RF5 bit position                    */
#define RF6_bp 6 /* Pin RF6 bit position                    */
#define RF7_bp 7 /* Pin RF7 bit position                    */

#endif /* PIC18F_Q84_PORT_H */
