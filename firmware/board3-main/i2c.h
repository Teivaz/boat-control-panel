#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <xc.h>

typedef enum {
    I2C_RESULT_OK = 0,
    I2C_RESULT_BUSY,        /* bus held by another host               */
    I2C_RESULT_COLLISION,   /* arbitration lost mid-transaction       */
    I2C_RESULT_NACK,        /* addressed target did not ACK           */
    I2C_RESULT_TIMEOUT,     /* peripheral stalled                     */
} I2cResult;

/* RX handler: complete write-message body (cmd byte at [0] + payload). */
typedef void (*I2cRxHandler)(const uint8_t *data, uint8_t len);

/* Read handler: build the response for an incoming read request. `request`
 * is the write-phase payload (cmd + params); write up to `response_max`
 * bytes to `response` and return the count. Runs in ISR context. */
typedef uint8_t (*I2cReadHandler)(const uint8_t *request, uint8_t request_len,
                                  uint8_t *response, uint8_t response_max);

void      i2c_init             (void);
void      i2c_set_rx_handler   (I2cRxHandler   h);
void      i2c_set_read_handler (I2cReadHandler h);

/* Host-mode blocking write. Main context only. */
I2cResult i2c_transmit(uint8_t address, const uint8_t *data, uint8_t len);

/* Host-mode blocking write-then-read (repeated-start). The first phase
 * transmits `tx_len` bytes (typically a cmd id + optional selector), then
 * a repeated-start issues an address-read and `rx_len` bytes are clocked
 * into `rx_buf`. Main context only. */
I2cResult i2c_receive (uint8_t address,
                       const uint8_t *tx, uint8_t tx_len,
                       uint8_t *rx, uint8_t rx_len);

#endif /* I2C_H */
