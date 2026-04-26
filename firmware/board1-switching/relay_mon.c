#include "relay_mon.h"

#include <xc.h>

/* Two SN74LV4051A muxes addressed in parallel (A/B/C shared). Each mux
 * routes one of its 8 inputs to its COM pin: mux 0 -> RA3, mux 1 -> RA4.
 * nINH is tied to GND, so the muxes are always enabled.
 *
 * Readme pinout:
 *   RB3 A
 *   RB4 B
 *   RB5 C
 *   RA3 0COM
 *   RA4 1COM
 *
 * The wire-bit-position to (mux, mux-address) layout is embedded in the
 * two 8-element lookup tables below — one per mux — so a single pass
 * through the 8 addresses fills all 16 wire bits. */

#define SEL_A LATBbits.LATB3
#define SEL_B LATBbits.LATB4
#define SEL_C LATBbits.LATB5
#define COM_0 PORTAbits.RA3
#define COM_1 PORTAbits.RA4

/* mux0_to_wire[addr] = the wire bit that mux 0 channel `addr` monitors. */
static const uint8_t mux0_to_wire[8] = {
    6, /* addr 0 -> anchor_light   (wire 6)  */
    8, /* addr 1 -> tricolor_light (wire 8)  */
    9, /* addr 2 -> fresh_water    (wire 9)  */
    5, /* addr 3 -> steaming_light (wire 5)  */
    4, /* addr 4 -> stern_light    (wire 4)  */
    1, /* addr 5 -> main           (wire 1)  */
    3, /* addr 6 -> bow_light      (wire 3)  */
    2, /* addr 7 -> autopilot      (wire 2)  */
};

/* mux1_to_wire[addr] = the wire bit that mux 1 channel `addr` monitors. */
static const uint8_t mux1_to_wire[8] = {
    15, /* addr 0 -> usb          (wire 15) */
    7,  /* addr 1 -> cabin_lights (wire 7)  */
    0,  /* addr 2 -> instruments  (wire 0)  */
    14, /* addr 3 -> deck_lights  (wire 14) */
    13, /* addr 4 -> aux2         (wire 13) */
    10, /* addr 5 -> fridge       (wire 10) */
    12, /* addr 6 -> aux1         (wire 12) */
    11, /* addr 7 -> inverter     (wire 11) */
};

void relay_mon_init(void) {
    /* Select pins RB3/RB4/RB5: digital outputs. */
    ANSELBbits.ANSELB3 = 0;
    ANSELBbits.ANSELB4 = 0;
    ANSELBbits.ANSELB5 = 0;
    LATBbits.LATB3 = 0;
    LATBbits.LATB4 = 0;
    LATBbits.LATB5 = 0;
    TRISBbits.TRISB3 = 0;
    TRISBbits.TRISB4 = 0;
    TRISBbits.TRISB5 = 0;

    /* COM inputs RA3/RA4: digital inputs. Even though the SN74LV4051A is
     * analog-capable, the upstream signal is a logic-level relay-power
     * indicator so reading them as digital is sufficient. */
    ANSELAbits.ANSELA3 = 0;
    ANSELAbits.ANSELA4 = 0;
    TRISAbits.TRISA3 = 1;
    TRISAbits.TRISA4 = 1;
}

uint16_t relay_mon_read(void) {
    uint16_t result = 0;
    for (uint8_t addr = 0; addr < 8; addr++) {
        SEL_A = (addr >> 0) & 1u;
        SEL_B = (addr >> 1) & 1u;
        SEL_C = (addr >> 2) & 1u;
        /* SN74LV4051A typical enable/transition time < 14 ns at 3.3 V.
         * A single NOP at 64 MHz (~15.6 ns) is just enough for it.*/
        __asm("NOP");
        if (COM_0) {
            result |= (uint16_t)(1u << mux0_to_wire[addr]);
        }
        if (COM_1) {
            result |= (uint16_t)(1u << mux1_to_wire[addr]);
        }
    }
    return result;
}
