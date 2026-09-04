#include "hw_sim.h"

#include "pic_mock.h"

#include <xc.h>

#define ADDR_LATA 0x4BEu
#define ADDR_LATB 0x4BFu
#define ADDR_PORTA 0x4CEu

/* ── Shift register ───────────────────────────────────────────────────── */
/*
 * relay_out.c drives:
 *   RA0 serial data, RA1 latch clock, RA2 shift clock, RB1 active-low reset
 *
 * The watcher runs before each LATA access completes, so what it samples is
 * the state left by the *previous* access.  Clocking one bit takes three
 * accesses (clk low, data, clk high), which means the rising edge is always
 * observed on the following access with the data line already settled — the
 * same instant the real 74HC595 samples it.
 */

static struct {
    uint8_t prev;      /* last sampled LATA */
    uint8_t prev_rstb; /* last sampled nRST from LATB */
    uint16_t shift;
    uint16_t latched;
    unsigned latches;
    uint8_t primed;
} sr;

#define SR_DATA   0x01u /* RA0 */
#define SR_LATCH  0x02u /* RA1 */
#define SR_SHIFT  0x04u /* RA2 */
#define SR_NRST   0x02u /* RB1 */

static void sr_watch(unsigned addr, void* ctx) {
    (void)addr;
    (void)ctx;
    const uint8_t now = pic_sfr[ADDR_LATA];
    const uint8_t rstb = (uint8_t)(pic_sfr[ADDR_LATB] & SR_NRST);

    if (sr.primed) {
        /* Active-low asynchronous clear. */
        if (sr.prev_rstb && !rstb) {
            sr.shift = 0;
        }
        /* Shift clock rising edge samples the data line, MSB-first. */
        if (!(sr.prev & SR_SHIFT) && (now & SR_SHIFT)) {
            sr.shift = (uint16_t)((sr.shift << 1) | ((now & SR_DATA) ? 1u : 0u));
        }
        /* Latch clock rising edge copies shift register to the outputs. */
        if (!(sr.prev & SR_LATCH) && (now & SR_LATCH)) {
            sr.latched = sr.shift;
            sr.latches++;
        }
    }
    sr.prev = now;
    sr.prev_rstb = rstb;
    sr.primed = 1;
}

void sr_sim_attach(void) {
    sr.prev = 0;
    sr.prev_rstb = 0;
    sr.shift = 0;
    sr.latched = 0;
    sr.latches = 0;
    sr.primed = 0;
    pic_watch(ADDR_LATA, sr_watch, 0);
}

uint16_t sr_latched(void) {
    /* One more sample so the final latch pulse — which is the last LATA write
     * of relay_out_write and therefore has nothing after it — is seen. */
    sr_watch(ADDR_LATA, 0);
    return sr.latched;
}

unsigned sr_latch_count(void) {
    sr_watch(ADDR_LATA, 0);
    return sr.latches;
}

/* ── Multiplexers ─────────────────────────────────────────────────────── */
/*
 * Two SN74LV4051A share the select lines RB3/RB4/RB5 (A/B/C).  Mux 0's common
 * is RA3, mux 1's is RA4.  The per-address wiring below is the board's
 * documented map — deliberately written out here rather than reused from
 * relay_mon.c, so the driver's copy is checked against the readme rather than
 * against itself.
 */

static const uint8_t mux0_wire[8] = {6, 7, 9, 5, 4, 0, 3, 2};
static const uint8_t mux1_wire[8] = {13, 12, 1, 11, 15, 10, 14, 8};

static uint16_t mux_state;

static void mux_watch(unsigned addr, void* ctx) {
    (void)addr;
    (void)ctx;
    const uint8_t latb = pic_sfr[ADDR_LATB];
    const uint8_t sel = (uint8_t)(((latb >> 3) & 1u) | (((latb >> 4) & 1u) << 1) | (((latb >> 5) & 1u) << 2));

    uint8_t porta = pic_sfr[ADDR_PORTA];
    porta &= (uint8_t)~0x18u; /* clear RA3 / RA4 */
    if (mux_state & (uint16_t)(1u << mux0_wire[sel])) {
        porta |= 0x08u; /* RA3 */
    }
    if (mux_state & (uint16_t)(1u << mux1_wire[sel])) {
        porta |= 0x10u; /* RA4 */
    }
    pic_sfr[ADDR_PORTA] = porta;
}

void mux_sim_attach(uint16_t wire_state) {
    mux_state = wire_state;
    pic_watch(ADDR_PORTA, mux_watch, 0);
}

void mux_sim_set(uint16_t wire_state) {
    mux_state = wire_state;
}
