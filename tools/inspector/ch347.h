#ifndef INSPECTOR_CH347_H
#define INSPECTOR_CH347_H

#include <stdint.h>
#include <stddef.h>

/* CH347 USB-to-I2C driver — host-side, libusb-1.0.
 *
 * Tested against CH347T in Mode 1 (vendor SPI+I2C+UART, PID 0x55DB).
 * The constants in ch347.c (VID/PID, interface number, endpoint addresses,
 * I2C stream opcodes) come from the publicly documented CH341/CH347 vendor
 * protocol; if your CH347 is configured in a different mode, adjust them
 * there. */

#define CH347_OK         0
#define CH347_WRITE_NACK (-1)
#define CH347_READ_NACK  (-2)
#define CH347_USB_ERR    (-3)
#define CH347_NOT_FOUND  (-4)
#define CH347_BAD_ARG    (-5)

/* Open the first CH347 we find. Returns 0 / CH347_NOT_FOUND / CH347_USB_ERR. */
int ch347_open(void);

void ch347_close(void);

/* Write `len` bytes to the 7-bit I2C address `addr`. Returns CH347_OK on full
 * ACK, CH347_WRITE_NACK if the slave NACK'd any byte (incl. the address). */
int ch347_i2c_write(uint8_t addr7, const uint8_t* data, size_t len);

/* I2C combined transaction: write `wlen` bytes (Start+addr+W+data),
 * repeated-Start, read `rlen` bytes (addr+R), Stop. Returns CH347_OK,
 * CH347_WRITE_NACK if the write phase NACK'd, or CH347_READ_NACK if the
 * slave NACK'd the read-phase address. */
int ch347_i2c_write_read(uint8_t addr7,
                         const uint8_t* wdata, size_t wlen,
                         uint8_t* rdata, size_t rlen);

#endif /* INSPECTOR_CH347_H */
