#include "relay_out.h"

#include <xc.h>

/* MC74HC595A pair wired as 16-bit SIPO. nOUT_EN tied to GND, so the latched
 * state is always driven. Readme pinout:
 *   RA0 A (serial data)
 *   RA1 LATCH_CLK (rising edge copies shift register into the output latch)
 *   RA2 SHIFT_CLK (rising edge samples A into the shift register)
 *   RB1 nRST (active-low asynchronous clear of the shift register)
 * Bit 0 is the lowest bit and is transmitted last, so we shift MSB-first
 * from bit 15 down to bit 0. */

#define PIN_A          LATAbits.LATA0
#define PIN_LATCH_CLK  LATAbits.LATA1
#define PIN_SHIFT_CLK  LATAbits.LATA2
#define PIN_NRST       LATBbits.LATB1

void relay_out_init(void) {
    ANSELAbits.ANSELA0 = 0;
    ANSELAbits.ANSELA1 = 0;
    ANSELAbits.ANSELA2 = 0;
    ANSELBbits.ANSELB1 = 0;

    LATAbits.LATA0 = 0;
    LATAbits.LATA1 = 0;
    LATAbits.LATA2 = 0;
    LATBbits.LATB1 = 0;

    TRISAbits.TRISA0 = 0;
    TRISAbits.TRISA1 = 0;
    TRISAbits.TRISA2 = 0;
    TRISBbits.TRISB1 = 0;

    /* Pulse nRST to clear the shift register, then release and latch zero
     * so the board boots with every relay off. */
    PIN_NRST = 0;
    __asm("NOP");
    PIN_NRST = 1;
    relay_out_write(0);
}

void relay_out_write(uint16_t wire_mask) {
    PIN_LATCH_CLK = 0;
    for (int8_t i = 15; i >= 0; i--) {
        PIN_SHIFT_CLK = 0;
        PIN_A = (wire_mask >> i) & 1u;
        PIN_SHIFT_CLK = 1;
    }
    PIN_LATCH_CLK = 1;
}
