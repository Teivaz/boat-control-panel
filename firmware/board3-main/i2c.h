#ifndef I2C_H
#define I2C_H

#include "task.h"

#include <stdint.h>
#include <xc.h>

typedef enum {
    I2C_RESULT_OK = 0,
    I2C_RESULT_BUSY,       /* bus held / arbitration lost after retries */
    I2C_RESULT_NACK,       /* target did not ACK address or data byte    */
    I2C_RESULT_TIMEOUT,    /* peripheral stalled mid-transfer            */
    I2C_RESULT_QUEUE_FULL, /* enqueue rejected                           */
    I2C_RESULT_BAD_ARG,    /* len == 0, buffer overrun, etc.             */
} I2cResult;

/* Host-side completion (main context). rx_buf is the same pointer the
 * caller passed to i2c_submit; rx_len is how many bytes were placed (0 on
 * write-only or any non-OK result). */
typedef void (*I2cCompletion)(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx);

/* Slave-side callbacks (ISR context — keep short). */
typedef void (*I2cRxHandler)(const uint8_t* data, uint8_t len);
typedef uint8_t (*I2cReadHandler)(const uint8_t* request, uint8_t request_len, uint8_t* response, uint8_t response_max);

/* Initialise pins, peripheral, queue. ctrl is used to defer completion
 * callbacks out of ISR context via run_in_main_loop. */
void i2c_init(TaskController* ctrl);

/* Slave-side hooks. */
void i2c_set_rx_handler(I2cRxHandler h);
void i2c_set_read_handler(I2cReadHandler h);

/* Maximum tx payload that the driver will buffer per task. CommMessage is
 * 8 bytes (1 id + 7 payload); 8 covers every current builder. */
#define I2C_TX_MAX 8

/* Submit a write or write-then-read.
 *  - tx is copied into the driver's queue entry (caller owns nothing).
 *  - rx_buf must remain valid until cb fires (typically file-static).
 *  - rx_len == 0 → write only; rx_buf may be NULL.
 *  - cb may be NULL for fire-and-forget writes.
 * Main context only. */
I2cResult i2c_submit(uint8_t address, const uint8_t* tx, uint8_t tx_len, uint8_t* rx_buf, uint8_t rx_len,
                     I2cCompletion cb, void* ctx);

/* Bit-bang up to 9 SCL pulses to free a stuck SDA, then issue a manual
 * STOP. No-op when SDA is already high. Main context only. */
void i2c_bus_recover(void);

/* Advance internal ms-resolution deadline / backoff timers. Called from
 * the board's 1 ms ISR (TMR0_ISR in main.c). ISR-callable. */
void i2c_tick_ms(void);

#endif /* I2C_H */
