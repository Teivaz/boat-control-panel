#ifndef I2C_LOG_H
#define I2C_LOG_H

#include "u8g2.h"

/* Ring-buffered log of the most recent I2C wire events, rendered on the
 * right half of the OLED in config mode.  Each event line is one of:
 *   CR <bytes>           — client received (raw message, sender info in payload)
 *   WA/WN <addr> <bytes> — host write ack / nack, with tx buffer
 *   RA/RN <addr> <bytes> — host read ack / nack, with rx buffer
 * i2c_log_init() registers the driver hook via i2c_set_logger. */
void i2c_log_init(void);

/* Draw the log on the right half of the display.  Caller chooses when
 * (typically only in config mode). */
void i2c_log_render(u8g2_t* g);

#endif /* I2C_LOG_H */
