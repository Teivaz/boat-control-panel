#ifndef RELAY_OUT_H
#define RELAY_OUT_H

#include <stdint.h>

/* Drives the two MC74HC595A shift registers wired in series as a single
 * 16-bit output latch. `wire_mask` bit N sets the Nth relay coil; the bit
 * layout matches the "Bit" column in board1-switching/readme.md (not the
 * protocol relay indices — the controller handles that translation). */
void relay_out_init (void);
void relay_out_write(uint16_t wire_mask);

#endif /* RELAY_OUT_H */
