#ifndef INSPECTOR_CH347_H
#define INSPECTOR_CH347_H

#include "transport.h"

/* CH347 USB-to-I2C driver — host-side, libusb-1.0.
 *
 * Tested against CH347T in Mode 1 (vendor SPI+I2C+UART, PID 0x55DB).
 * The constants in ch347.c (VID/PID, interface number, endpoint addresses,
 * I2C stream opcodes) come from the publicly documented CH341/CH347 vendor
 * protocol; if your CH347 is configured in a different mode, adjust them
 * there. Returns the shared TRANSPORT_* codes. */

int ch347_open(uint32_t baud_hz);
void ch347_close(void);
int ch347_i2c_write(uint8_t addr7, const uint8_t* data, size_t len);
int ch347_i2c_write_read(uint8_t addr7,
                         const uint8_t* wdata, size_t wlen,
                         uint8_t* rdata, size_t rlen);

#endif /* INSPECTOR_CH347_H */
