#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <xc.h>

typedef enum {
    I2C_RESULT_OK = 0,
    I2C_RESULT_BUSY,
    I2C_RESULT_COLLISION,
    I2C_RESULT_NACK,
    I2C_RESULT_TIMEOUT,
} I2cResult;

typedef void (*I2cRxHandler)(const uint8_t *data, uint8_t len);
typedef uint8_t (*I2cReadHandler)(const uint8_t *request, uint8_t request_len,
                                  uint8_t *response, uint8_t response_max);

void i2c_init(void);
void i2c_set_rx_handler(I2cRxHandler h);
void i2c_set_read_handler(I2cReadHandler h);

/* Host-mode blocking write. Main context only. */
I2cResult i2c_transmit(uint8_t address, const uint8_t *data, uint8_t len);

#endif /* I2C_H */
