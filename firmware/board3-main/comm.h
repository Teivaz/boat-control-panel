#ifndef COMM_H
#define COMM_H

#include <stdint.h>

/* Wires i2c RX/read handlers to main-board dispatch: routes button_changed
 * and channel_changed to the controller, handles universal config / reset on
 * behalf of this device. Call after i2c_init(). */
void comm_init(void);

/* XXX DIAGNOSTIC — TEMPORARY, REMOVE ME. Tracing whether board 3 resets or
 * hangs under multi-master traffic. Readable via config_read: 0x20/0x21 are
 * the 16-bit uptime (low/high), 0x22 is PCON0 as latched at boot. */
void comm_tick_liveness(void);
void comm_reset_cause_latch(void);
uint8_t comm_reset_cause(void);

#endif /* COMM_H */
