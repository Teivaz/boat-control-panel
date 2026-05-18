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

#define PIN_A LATAbits.LATA0
#define PIN_LATCH_CLK LATAbits.LATA1
#define PIN_SHIFT_CLK LATAbits.LATA2
#define PIN_NRST LATBbits.LATB1

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

/* Wire-bit (protocol) → internal shift-register pin number.
 *
 * The readme's "Address" column doesn't match the actual hardware. Empirical
 * mapping from sr_mask bit to physical channel (verified on the dev board):
 *
 *   sr_mask bit | channel              sr_mask bit | channel
 *   ----------- | -----------          ----------- | -----------
 *           0   | instruments                  8   | tricolor_light
 *           1   | main                         9   | fresh_water_pump
 *           2   | autopilot                   10   | fridge
 *           3   | bow_light                   11   | inverter
 *           4   | stern_light                 12   | aux1
 *           5   | steaming_light              13   | aux2
 *           6   | anchor_light                14   | deck_lights
 *           7   | cabin_lights                15   | usb
 *
 * Each entry below is the sr_mask bit for that protocol bit's channel.
 * The mux-side monitoring table in the readme matches the hardware directly,
 * so only this output LUT is rebuilt from observation. */
static const uint8_t wire_to_sr[16] = {
    1,  /* wire 0  → main             */
    0,  /* wire 1  → instruments      */
    2,  /* wire 2  → autopilot        */
    3,  /* wire 3  → bow_light        */
    4,  /* wire 4  → stern_light      */
    5,  /* wire 5  → steaming_light   */
    6,  /* wire 6  → anchor_light     */
    8,  /* wire 7  → tricolor_light   */
    11, /* wire 8  → inverter         */
    9,  /* wire 9  → fresh_water_pump */
    10, /* wire 10 → fridge           */
    14, /* wire 11 → deck_lights      */
    7,  /* wire 12 → cabin_lights     */
    15, /* wire 13 → usb              */
    12, /* wire 14 → aux1             */
    13, /* wire 15 → aux2             */
};

void relay_out_write(uint16_t wire_mask) {
    /* Translate wire bits to the SIPO chain's pin layout. The shift order
     * below puts wire-mask bit i at SR pin i unmodified, which doesn't
     * match the readme's wiring — remap first so each wire bit drives the
     * right physical coil. */
    uint16_t sr_mask = 0;
    for (uint8_t i = 0; i < 16; i++) {
        if (wire_mask & (uint16_t)(1u << i)) {
            sr_mask |= (uint16_t)(1u << wire_to_sr[i]);
        }
    }
    PIN_LATCH_CLK = 0;
    for (int8_t i = 15; i >= 0; i--) {
        PIN_SHIFT_CLK = 0;
        PIN_A = (sr_mask >> i) & 1u;
        PIN_SHIFT_CLK = 1;
    }
    PIN_LATCH_CLK = 1;
}
