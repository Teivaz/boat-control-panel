#ifndef INSPECTOR_CP2112_H
#define INSPECTOR_CP2112_H

#include "transport.h"

/* Silicon Labs CP2112 USB-to-I2C bridge — HID-class. VID 0x10C4 / PID 0xEA90.
 * Communicates via HID reports on interrupt endpoints (EP 0x01 OUT / 0x81 IN).
 * Returns the shared TRANSPORT_* codes. */

int cp2112_open(uint32_t baud_hz);
void cp2112_close(void);
int cp2112_i2c_write(uint8_t addr7, const uint8_t* data, size_t len);
int cp2112_i2c_write_read(uint8_t addr7,
                          const uint8_t* wdata, size_t wlen,
                          uint8_t* rdata, size_t rlen);

#endif /* INSPECTOR_CP2112_H */
