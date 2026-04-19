#ifndef RELAY_MON_H
#define RELAY_MON_H

#include <stdint.h>

/* Reads both SN74LV4051A multiplexers to recover the physical state of all
 * 16 relay channels. The returned mask uses the same wire-bit numbering as
 * relay_out.h — the controller handles translation to protocol relay bits.
 *
 * Read time is ~8 mux-settle periods; safe to call from the main-context
 * polling task. */
void relay_mon_init(void);
uint16_t relay_mon_read(void);

#endif /* RELAY_MON_H */
