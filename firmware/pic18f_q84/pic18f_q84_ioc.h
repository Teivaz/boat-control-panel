/*
 * PIC18F27/47/57Q84 Interrupt-on-Change (IOC) Registers
 *
 * Each IOC-capable GPIO pin can generate an interrupt on:
 *   - Rising edge  (positive edge, controlled by IOCxP register)
 *   - Falling edge (negative edge, controlled by IOCxN register)
 *   - Both edges   (set both IOCxP and IOCxN bits for that pin)
 *
 * When an enabled edge is detected, the corresponding flag bit in IOCxF
 * is set. Flag bits must be cleared in software by writing 0 to the bit.
 *
 * IOC-capable ports:
 *   PORTA : IOC on all 8 pins (RA7:RA0)         - all packages
 *   PORTB : IOC on all 8 pins (RB7:RB0)         - all packages
 *   PORTC : IOC on all 8 pins (RC7:RC0)         - all packages
 *   PORTE : IOC on 4 pins (RE3:RE0)             - 48-pin only
 *
 *   PORTD : No IOC capability
 *   PORTF : No IOC capability
 *
 * Master interrupt control:
 *   The master IOC interrupt flag is IOCIF (bit 7) in PIR0.
 *   The master IOC interrupt enable is IOCIE (bit 7) in PIE0.
 *   IOCIF is read-only and is set when any IOCxF flag is set while
 *   its corresponding edge enable (IOCxP or IOCxN) is also set.
 *   IOCIF clears automatically when all individual IOCxF flags with
 *   active edge enables are cleared.
 *
 * Typical usage:
 *   1. Configure pin as digital input (clear ANSELx, set TRISx)
 *   2. Set the desired edge enable bit(s) in IOCxP and/or IOCxN
 *   3. Clear the flag in IOCxF
 *   4. Enable IOCIE in PIE0 and GIE/PEIE in INTCON
 *   5. In the ISR, read IOCxF to determine which pin(s) triggered,
 *      then clear the serviced flag bits before returning
 *
 * Reference: Microchip DS40002213D
 *            "PIC18F27/47/57Q84 Data Sheet"
 */

#ifndef PIC18F_Q84_IOC_H
#define PIC18F_Q84_IOC_H

/* ========================================================================
 *  PORTA Interrupt-on-Change
 * ========================================================================
 *  Available on all packages (28/40/44/48-pin).
 *  8 IOC channels: RA7:RA0.
 * ======================================================================== */

#define IOCAP SFR8(0x0405) /* Bits 7:0 - IOCAP7:IOCAP0               */
                           /*   Positive edge (rising) enable         */
                           /*   1 = Interrupt on low-to-high          */
                           /*   0 = Rising edge detect disabled       */

#define IOCAN SFR8(0x0406) /* Bits 7:0 - IOCAN7:IOCAN0               */
                           /*   Negative edge (falling) enable        */
                           /*   1 = Interrupt on high-to-low          */
                           /*   0 = Falling edge detect disabled      */

#define IOCAF SFR8(0x0407) /* Bits 7:0 - IOCAF7:IOCAF0               */
                           /*   Interrupt flags                       */
                           /*   1 = Edge detected (write 0 to clear)  */
                           /*   0 = No edge detected                  */

/* PORTA IOC pin bit masks */
#define IOCAP0_bm 0x01 /* RA0 positive edge enable mask           */
#define IOCAP1_bm 0x02 /* RA1 positive edge enable mask           */
#define IOCAP2_bm 0x04 /* RA2 positive edge enable mask           */
#define IOCAP3_bm 0x08 /* RA3 positive edge enable mask           */
#define IOCAP4_bm 0x10 /* RA4 positive edge enable mask           */
#define IOCAP5_bm 0x20 /* RA5 positive edge enable mask           */
#define IOCAP6_bm 0x40 /* RA6 positive edge enable mask           */
#define IOCAP7_bm 0x80 /* RA7 positive edge enable mask           */

#define IOCAN0_bm 0x01 /* RA0 negative edge enable mask           */
#define IOCAN1_bm 0x02 /* RA1 negative edge enable mask           */
#define IOCAN2_bm 0x04 /* RA2 negative edge enable mask           */
#define IOCAN3_bm 0x08 /* RA3 negative edge enable mask           */
#define IOCAN4_bm 0x10 /* RA4 negative edge enable mask           */
#define IOCAN5_bm 0x20 /* RA5 negative edge enable mask           */
#define IOCAN6_bm 0x40 /* RA6 negative edge enable mask           */
#define IOCAN7_bm 0x80 /* RA7 negative edge enable mask           */

#define IOCAF0_bm 0x01 /* RA0 interrupt flag mask                 */
#define IOCAF1_bm 0x02 /* RA1 interrupt flag mask                 */
#define IOCAF2_bm 0x04 /* RA2 interrupt flag mask                 */
#define IOCAF3_bm 0x08 /* RA3 interrupt flag mask                 */
#define IOCAF4_bm 0x10 /* RA4 interrupt flag mask                 */
#define IOCAF5_bm 0x20 /* RA5 interrupt flag mask                 */
#define IOCAF6_bm 0x40 /* RA6 interrupt flag mask                 */
#define IOCAF7_bm 0x80 /* RA7 interrupt flag mask                 */

/* PORTA IOC pin bit positions */
#define IOCAP0_bp 0 /* RA0 positive edge enable position       */
#define IOCAP1_bp 1 /* RA1 positive edge enable position       */
#define IOCAP2_bp 2 /* RA2 positive edge enable position       */
#define IOCAP3_bp 3 /* RA3 positive edge enable position       */
#define IOCAP4_bp 4 /* RA4 positive edge enable position       */
#define IOCAP5_bp 5 /* RA5 positive edge enable position       */
#define IOCAP6_bp 6 /* RA6 positive edge enable position       */
#define IOCAP7_bp 7 /* RA7 positive edge enable position       */

#define IOCAN0_bp 0 /* RA0 negative edge enable position       */
#define IOCAN1_bp 1 /* RA1 negative edge enable position       */
#define IOCAN2_bp 2 /* RA2 negative edge enable position       */
#define IOCAN3_bp 3 /* RA3 negative edge enable position       */
#define IOCAN4_bp 4 /* RA4 negative edge enable position       */
#define IOCAN5_bp 5 /* RA5 negative edge enable position       */
#define IOCAN6_bp 6 /* RA6 negative edge enable position       */
#define IOCAN7_bp 7 /* RA7 negative edge enable position       */

#define IOCAF0_bp 0 /* RA0 interrupt flag position             */
#define IOCAF1_bp 1 /* RA1 interrupt flag position             */
#define IOCAF2_bp 2 /* RA2 interrupt flag position             */
#define IOCAF3_bp 3 /* RA3 interrupt flag position             */
#define IOCAF4_bp 4 /* RA4 interrupt flag position             */
#define IOCAF5_bp 5 /* RA5 interrupt flag position             */
#define IOCAF6_bp 6 /* RA6 interrupt flag position             */
#define IOCAF7_bp 7 /* RA7 interrupt flag position             */

/* ========================================================================
 *  PORTB Interrupt-on-Change
 * ========================================================================
 *  Available on all packages (28/40/44/48-pin).
 *  8 IOC channels: RB7:RB0.
 * ======================================================================== */

#define IOCBP SFR8(0x040D) /* Bits 7:0 - IOCBP7:IOCBP0               */
                           /*   Positive edge (rising) enable         */

#define IOCBN SFR8(0x040E) /* Bits 7:0 - IOCBN7:IOCBN0               */
                           /*   Negative edge (falling) enable        */

#define IOCBF SFR8(0x040F) /* Bits 7:0 - IOCBF7:IOCBF0               */
                           /*   Interrupt flags (write 0 to clear)    */

/* PORTB IOC pin bit masks */
#define IOCBP0_bm 0x01 /* RB0 positive edge enable mask           */
#define IOCBP1_bm 0x02 /* RB1 positive edge enable mask           */
#define IOCBP2_bm 0x04 /* RB2 positive edge enable mask           */
#define IOCBP3_bm 0x08 /* RB3 positive edge enable mask           */
#define IOCBP4_bm 0x10 /* RB4 positive edge enable mask           */
#define IOCBP5_bm 0x20 /* RB5 positive edge enable mask           */
#define IOCBP6_bm 0x40 /* RB6 positive edge enable mask           */
#define IOCBP7_bm 0x80 /* RB7 positive edge enable mask           */

#define IOCBN0_bm 0x01 /* RB0 negative edge enable mask           */
#define IOCBN1_bm 0x02 /* RB1 negative edge enable mask           */
#define IOCBN2_bm 0x04 /* RB2 negative edge enable mask           */
#define IOCBN3_bm 0x08 /* RB3 negative edge enable mask           */
#define IOCBN4_bm 0x10 /* RB4 negative edge enable mask           */
#define IOCBN5_bm 0x20 /* RB5 negative edge enable mask           */
#define IOCBN6_bm 0x40 /* RB6 negative edge enable mask           */
#define IOCBN7_bm 0x80 /* RB7 negative edge enable mask           */

#define IOCBF0_bm 0x01 /* RB0 interrupt flag mask                 */
#define IOCBF1_bm 0x02 /* RB1 interrupt flag mask                 */
#define IOCBF2_bm 0x04 /* RB2 interrupt flag mask                 */
#define IOCBF3_bm 0x08 /* RB3 interrupt flag mask                 */
#define IOCBF4_bm 0x10 /* RB4 interrupt flag mask                 */
#define IOCBF5_bm 0x20 /* RB5 interrupt flag mask                 */
#define IOCBF6_bm 0x40 /* RB6 interrupt flag mask                 */
#define IOCBF7_bm 0x80 /* RB7 interrupt flag mask                 */

/* PORTB IOC pin bit positions */
#define IOCBP0_bp 0 /* RB0 positive edge enable position       */
#define IOCBP1_bp 1 /* RB1 positive edge enable position       */
#define IOCBP2_bp 2 /* RB2 positive edge enable position       */
#define IOCBP3_bp 3 /* RB3 positive edge enable position       */
#define IOCBP4_bp 4 /* RB4 positive edge enable position       */
#define IOCBP5_bp 5 /* RB5 positive edge enable position       */
#define IOCBP6_bp 6 /* RB6 positive edge enable position       */
#define IOCBP7_bp 7 /* RB7 positive edge enable position       */

#define IOCBN0_bp 0 /* RB0 negative edge enable position       */
#define IOCBN1_bp 1 /* RB1 negative edge enable position       */
#define IOCBN2_bp 2 /* RB2 negative edge enable position       */
#define IOCBN3_bp 3 /* RB3 negative edge enable position       */
#define IOCBN4_bp 4 /* RB4 negative edge enable position       */
#define IOCBN5_bp 5 /* RB5 negative edge enable position       */
#define IOCBN6_bp 6 /* RB6 negative edge enable position       */
#define IOCBN7_bp 7 /* RB7 negative edge enable position       */

#define IOCBF0_bp 0 /* RB0 interrupt flag position             */
#define IOCBF1_bp 1 /* RB1 interrupt flag position             */
#define IOCBF2_bp 2 /* RB2 interrupt flag position             */
#define IOCBF3_bp 3 /* RB3 interrupt flag position             */
#define IOCBF4_bp 4 /* RB4 interrupt flag position             */
#define IOCBF5_bp 5 /* RB5 interrupt flag position             */
#define IOCBF6_bp 6 /* RB6 interrupt flag position             */
#define IOCBF7_bp 7 /* RB7 interrupt flag position             */

/* ========================================================================
 *  PORTC Interrupt-on-Change
 * ========================================================================
 *  Available on all packages (28/40/44/48-pin).
 *  8 IOC channels: RC7:RC0.
 * ======================================================================== */

#define IOCCP SFR8(0x0415) /* Bits 7:0 - IOCCP7:IOCCP0               */
                           /*   Positive edge (rising) enable         */

#define IOCCN SFR8(0x0416) /* Bits 7:0 - IOCCN7:IOCCN0               */
                           /*   Negative edge (falling) enable        */

#define IOCCF SFR8(0x0417) /* Bits 7:0 - IOCCF7:IOCCF0               */
                           /*   Interrupt flags (write 0 to clear)    */

/* PORTC IOC pin bit masks */
#define IOCCP0_bm 0x01 /* RC0 positive edge enable mask           */
#define IOCCP1_bm 0x02 /* RC1 positive edge enable mask           */
#define IOCCP2_bm 0x04 /* RC2 positive edge enable mask           */
#define IOCCP3_bm 0x08 /* RC3 positive edge enable mask           */
#define IOCCP4_bm 0x10 /* RC4 positive edge enable mask           */
#define IOCCP5_bm 0x20 /* RC5 positive edge enable mask           */
#define IOCCP6_bm 0x40 /* RC6 positive edge enable mask           */
#define IOCCP7_bm 0x80 /* RC7 positive edge enable mask           */

#define IOCCN0_bm 0x01 /* RC0 negative edge enable mask           */
#define IOCCN1_bm 0x02 /* RC1 negative edge enable mask           */
#define IOCCN2_bm 0x04 /* RC2 negative edge enable mask           */
#define IOCCN3_bm 0x08 /* RC3 negative edge enable mask           */
#define IOCCN4_bm 0x10 /* RC4 negative edge enable mask           */
#define IOCCN5_bm 0x20 /* RC5 negative edge enable mask           */
#define IOCCN6_bm 0x40 /* RC6 negative edge enable mask           */
#define IOCCN7_bm 0x80 /* RC7 negative edge enable mask           */

#define IOCCF0_bm 0x01 /* RC0 interrupt flag mask                 */
#define IOCCF1_bm 0x02 /* RC1 interrupt flag mask                 */
#define IOCCF2_bm 0x04 /* RC2 interrupt flag mask                 */
#define IOCCF3_bm 0x08 /* RC3 interrupt flag mask                 */
#define IOCCF4_bm 0x10 /* RC4 interrupt flag mask                 */
#define IOCCF5_bm 0x20 /* RC5 interrupt flag mask                 */
#define IOCCF6_bm 0x40 /* RC6 interrupt flag mask                 */
#define IOCCF7_bm 0x80 /* RC7 interrupt flag mask                 */

/* PORTC IOC pin bit positions */
#define IOCCP0_bp 0 /* RC0 positive edge enable position       */
#define IOCCP1_bp 1 /* RC1 positive edge enable position       */
#define IOCCP2_bp 2 /* RC2 positive edge enable position       */
#define IOCCP3_bp 3 /* RC3 positive edge enable position       */
#define IOCCP4_bp 4 /* RC4 positive edge enable position       */
#define IOCCP5_bp 5 /* RC5 positive edge enable position       */
#define IOCCP6_bp 6 /* RC6 positive edge enable position       */
#define IOCCP7_bp 7 /* RC7 positive edge enable position       */

#define IOCCN0_bp 0 /* RC0 negative edge enable position       */
#define IOCCN1_bp 1 /* RC1 negative edge enable position       */
#define IOCCN2_bp 2 /* RC2 negative edge enable position       */
#define IOCCN3_bp 3 /* RC3 negative edge enable position       */
#define IOCCN4_bp 4 /* RC4 negative edge enable position       */
#define IOCCN5_bp 5 /* RC5 negative edge enable position       */
#define IOCCN6_bp 6 /* RC6 negative edge enable position       */
#define IOCCN7_bp 7 /* RC7 negative edge enable position       */

#define IOCCF0_bp 0 /* RC0 interrupt flag position             */
#define IOCCF1_bp 1 /* RC1 interrupt flag position             */
#define IOCCF2_bp 2 /* RC2 interrupt flag position             */
#define IOCCF3_bp 3 /* RC3 interrupt flag position             */
#define IOCCF4_bp 4 /* RC4 interrupt flag position             */
#define IOCCF5_bp 5 /* RC5 interrupt flag position             */
#define IOCCF6_bp 6 /* RC6 interrupt flag position             */
#define IOCCF7_bp 7 /* RC7 interrupt flag position             */

/* ========================================================================
 *  PORTE Interrupt-on-Change (48-pin devices only)
 * ========================================================================
 *  4 IOC channels: RE3:RE0.
 *  RE3 is the MCLR pin (input-only) but still has IOC capability.
 *
 *  Note: Although PORTE exists on 40/44-pin devices, the IOC registers
 *  for PORTE are only implemented on the 48-pin PIC18F57Q84.
 * ======================================================================== */

#define IOCEP SFR8(0x0425) /* Bits 3:0 - IOCEP3:IOCEP0               */
                           /*   Positive edge (rising) enable         */

#define IOCEN SFR8(0x0426) /* Bits 3:0 - IOCEN3:IOCEN0               */
                           /*   Negative edge (falling) enable        */

#define IOCEF SFR8(0x0427) /* Bits 3:0 - IOCEF3:IOCEF0               */
                           /*   Interrupt flags (write 0 to clear)    */

/* PORTE IOC pin bit masks */
#define IOCEP0_bm 0x01 /* RE0 positive edge enable mask           */
#define IOCEP1_bm 0x02 /* RE1 positive edge enable mask           */
#define IOCEP2_bm 0x04 /* RE2 positive edge enable mask           */
#define IOCEP3_bm 0x08 /* RE3 positive edge enable mask (MCLR)    */

#define IOCEN0_bm 0x01 /* RE0 negative edge enable mask           */
#define IOCEN1_bm 0x02 /* RE1 negative edge enable mask           */
#define IOCEN2_bm 0x04 /* RE2 negative edge enable mask           */
#define IOCEN3_bm 0x08 /* RE3 negative edge enable mask (MCLR)    */

#define IOCEF0_bm 0x01 /* RE0 interrupt flag mask                 */
#define IOCEF1_bm 0x02 /* RE1 interrupt flag mask                 */
#define IOCEF2_bm 0x04 /* RE2 interrupt flag mask                 */
#define IOCEF3_bm 0x08 /* RE3 interrupt flag mask (MCLR)          */

/* PORTE IOC pin bit positions */
#define IOCEP0_bp 0 /* RE0 positive edge enable position       */
#define IOCEP1_bp 1 /* RE1 positive edge enable position       */
#define IOCEP2_bp 2 /* RE2 positive edge enable position       */
#define IOCEP3_bp 3 /* RE3 positive edge enable position       */

#define IOCEN0_bp 0 /* RE0 negative edge enable position       */
#define IOCEN1_bp 1 /* RE1 negative edge enable position       */
#define IOCEN2_bp 2 /* RE2 negative edge enable position       */
#define IOCEN3_bp 3 /* RE3 negative edge enable position       */

#define IOCEF0_bp 0 /* RE0 interrupt flag position             */
#define IOCEF1_bp 1 /* RE1 interrupt flag position             */
#define IOCEF2_bp 2 /* RE2 interrupt flag position             */
#define IOCEF3_bp 3 /* RE3 interrupt flag position             */

#endif /* PIC18F_Q84_IOC_H */
