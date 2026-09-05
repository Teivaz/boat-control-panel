#ifndef I2C_LOG_H
#define I2C_LOG_H

#include "u8g2.h"

/* Right-half log view of the most recent I2C wire events, rendered in
 * config mode.  The ring buffer itself lives in the driver (libcomm/i2c.c);
 * this module just reads it via i2c_log_snapshot() and draws.  Each line
 * is one of:
 *   CR <bytes>           — client received (raw message, sender in payload)
 *   WA/WN <addr> <bytes> — host write ack / nack, with tx buffer
 *   RA/RN <addr> <bytes> — host read ack / nack, with rx buffer
 *
 * Visibility is gated by I2C_LOG_VISIBLE.  Define it (e.g.
 * -DI2C_LOG_VISIBLE=1 in the Makefile) to overlay the log on the
 * config-mode screen for debugging.  Defaults off — the overlay would
 * otherwise crowd the right half of every config screen. */
#ifndef I2C_LOG_VISIBLE
/* XXX DIAGNOSTIC — TEMPORARY, REVERT TO 0. */
#define I2C_LOG_VISIBLE 1
#endif

void i2c_log_init(void);

/* Draw the log on the right half of the display.  Caller chooses when
 * (typically only in config mode).  No-op when I2C_LOG_VISIBLE is 0. */
void i2c_log_render(u8g2_t* g);

#endif /* I2C_LOG_H */
