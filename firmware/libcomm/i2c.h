/*
 * i2c.h — Shared I2C multi-master driver for PIC18F27/47/57Q84.
 *
 * Async, interrupt-driven host + client (slave) on I2C1.
 * 400 kHz Fast mode, 7-bit addressing, multi-master with automatic
 * bus arbitration and collision retry.
 *
 * Pin/PPS/oscillator setup is the caller's responsibility — this
 * driver only programs the I2C1 module registers.  Boards define
 * their own __interrupt handlers and call i2c_isr / i2c_error_isr.
 *
 * Host side: i2c_submit() enqueues a write or write-then-read.
 * Completion callbacks fire from i2c_poll() in main-loop context.
 *
 * Client side: i2c_init(addr) enables client mode on the given
 * 7-bit address.  Incoming writes are delivered to the rx handler;
 * incoming reads are answered by the read handler.  Both handlers
 * run in ISR context — keep them short.
 *
 * Context rules:
 *   Main loop only:  i2c_init, i2c_set_rx_handler, i2c_set_read_handler,
 *                    i2c_submit, i2c_poll
 *   ISR only:        i2c_tick_ms, i2c_isr, i2c_error_isr
 *
 * Prerequisites (caller-owned):
 *   SDA/SCL: TRIS, ANSEL=0, ODCON=1, RxxI2C TH/PU/SLEW, PPS routed
 *   FOSC = 64 MHz
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/* ── Configuration (override before including this header) ──────────── */

#ifndef I2C_QUEUE_SIZE
#define I2C_QUEUE_SIZE 8
#endif

#ifndef I2C_TX_MAX
#define I2C_TX_MAX 8
#endif

#ifndef I2C_CLIENT_BUF_SIZE
#define I2C_CLIENT_BUF_SIZE 8
#endif

#ifndef I2C_RETRY_COUNT
#define I2C_RETRY_COUNT 3
#endif

/* ── Types ──────────────────────────────────────────────────────────── */

typedef enum {
    I2C_RESULT_OK = 0,
    I2C_RESULT_BUSY,       /* bus held or arbitration lost               */
    I2C_RESULT_NACK,       /* target did not acknowledge                 */
    I2C_RESULT_TIMEOUT,    /* software / hardware timeout                */
    I2C_RESULT_QUEUE_FULL, /* i2c_submit rejected — queue full           */
    I2C_RESULT_BAD_ARG,    /* i2c_submit rejected — invalid lengths      */
} I2cResult;

/* Host completion callback.  Fired from i2c_poll() (main-loop context).
 * rx_buf is the pointer the caller passed to i2c_submit; rx_len is
 * actual bytes received (0 on write-only or non-OK result). */
typedef void (*I2cCompletion)(I2cResult result, uint8_t* rx_buf, uint8_t rx_len, void* ctx);

/* Client RX handler — invoked from ISR when a master finishes writing
 * to us (Stop condition).  data points into a driver buffer that is
 * reused on the next message; copy if needed. */
typedef void (*I2cRxHandler)(const uint8_t* data, uint8_t len);

/* Client read handler — invoked from ISR when a master addresses us
 * with R/W=1.  request[] is the preceding write-phase bytes (command +
 * params).  Write up to response_max bytes into response[] and return
 * the count.  Return 0 to decline. */
typedef uint8_t (*I2cReadHandler)(const uint8_t* request, uint8_t request_len, uint8_t* response, uint8_t response_max);

/* ── Main-loop API ──────────────────────────────────────────────────── */

/* Set the slave-side handlers.  May be called before or after i2c_init.
 * Either may be left unset (NULL) if that direction is unused. */
void i2c_set_rx_handler(I2cRxHandler h);
void i2c_set_read_handler(I2cReadHandler h);

/* One-time hardware init.  Configures I2C1 at 400 kHz.
 * client_addr = 0 disables client mode (host-only).
 * Caller must have set up pins and oscillator beforehand. */
void i2c_init(uint8_t client_addr);

/* Submit a host write or write-then-read.
 *   tx, tx_len  — required (1..I2C_TX_MAX).  Copied into the queue.
 *   rx_buf, rx_len — write-then-read.  rx_len=0 means write-only.
 *                   When rx_len > 0, rx_buf must remain valid until cb.
 *   cb, ctx     — fired from i2c_poll().  cb may be NULL.
 * Main-loop context only. */
I2cResult i2c_submit(uint8_t address, const uint8_t* tx, uint8_t tx_len, uint8_t* rx_buf, uint8_t rx_len,
                     I2cCompletion cb, void* ctx);

/* Main-loop poll.  Fires the completion callback for finished ops and
 * starts the next queued op when the bus is free.  O(1) per call. */
void i2c_poll(void);

/* ── ISR entry points ───────────────────────────────────────────────── */

/* 1 ms tick — drives per-op timeout and bus-collision back-off. */
void i2c_tick_ms(void);

/* Call from the I2C1/I2C1TX/I2C1RX interrupt vector. */
void i2c_isr(void);

/* Call from the I2C1E interrupt vector. */
void i2c_error_isr(void);

#endif /* I2C_H */
