#ifndef I2C_LOG_H
#define I2C_LOG_H

#include "u8g2.h"

/* Right-half log view of the most recent I2C wire events, rendered in
 * config mode.  The ring buffer itself lives in the driver (libcomm/i2c.c);
 * this module just reads it via i2c_log_snapshot() and draws.  Each line
 * is one of:
 *   CR <bytes>           — client received (raw message, sender in payload)
 *   WA/WN <addr> <bytes> — host write ack / nack, with tx buffer
 *   RA/RN <addr> <bytes> — host read ack / nack, with rx buffer */
void i2c_log_init(void);

/* Draw the log on the right half of the display.  Caller chooses when
 * (typically only in config mode). */
void i2c_log_render(u8g2_t* g);

#endif /* I2C_LOG_H */
