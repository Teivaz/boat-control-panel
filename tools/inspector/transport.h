#ifndef INSPECTOR_TRANSPORT_H
#define INSPECTOR_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

/* Unified return codes for any USB-I2C bridge backend. */
#define TRANSPORT_OK         0
#define TRANSPORT_WRITE_NACK (-1)
#define TRANSPORT_READ_NACK  (-2)
#define TRANSPORT_USB_ERR    (-3)
#define TRANSPORT_NOT_FOUND  (-4)
#define TRANSPORT_BAD_ARG    (-5)

/* Probe for an I2C bridge: CH347 first, then CP2112. The first one that
 * opens successfully is selected for the session. Returns TRANSPORT_OK,
 * TRANSPORT_NOT_FOUND, or TRANSPORT_USB_ERR. */
int transport_open(uint32_t baud_hz);

void transport_close(void);

/* Human-readable name of the active backend (e.g. "CH347", "CP2112"). */
const char* transport_name(void);

/* Plain I2C write: Start, addr+W, data..., Stop. */
int transport_i2c_write(uint8_t addr7, const uint8_t* data, size_t len);

/* I2C combined transaction: Start, addr+W, wdata..., (Repeated-)Start,
 * addr+R, read rdata..., Stop. */
int transport_i2c_write_read(uint8_t addr7,
                             const uint8_t* wdata, size_t wlen,
                             uint8_t* rdata, size_t rlen);

#endif /* INSPECTOR_TRANSPORT_H */
