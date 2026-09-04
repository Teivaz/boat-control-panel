/*
 * hw_sim.h — models of the two discrete parts hanging off the switching
 * board's GPIO, so the bit-banged drivers can be tested against something
 * that behaves like the hardware rather than against register writes.
 *
 * Both are driven by mock register watchers (see pic_mock.h): the shift
 * register observes LATA and reconstructs the word clocked into it, and the
 * multiplexer intercepts reads of PORTA and answers with whatever channel the
 * select lines currently address.
 */

#ifndef HW_SIM_H
#define HW_SIM_H

#include <stdint.h>

/* ── MC74HC595A pair, 16 bits, wired to LATA/LATB ─────────────────────── */

/* Start observing.  Until the first latch pulse, sr_latched() returns 0. */
void sr_sim_attach(void);

/* The word most recently transferred into the output latch, in
 * shift-register pin order (bit N = SR pin N), which is *not* the protocol
 * bit order — translating between the two is what relay_out.c is for. */
uint16_t sr_latched(void);

/* How many times the latch has been pulsed. */
unsigned sr_latch_count(void);

/* ── SN74LV4051A pair, 8 channels each, commons on RA3/RA4 ────────────── */

/* Start answering PORTA reads.  `wire_state` is a 16-bit mask in *wire* bit
 * order — the same order relay_mon_read() is expected to return — which the
 * simulator maps back onto mux addresses using the board's documented
 * wiring.  A driver whose lookup tables disagree therefore reads back a
 * different mask than it was given. */
void mux_sim_attach(uint16_t wire_state);
void mux_sim_set(uint16_t wire_state);

#endif /* HW_SIM_H */
