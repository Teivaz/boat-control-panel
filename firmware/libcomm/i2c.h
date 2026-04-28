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

/* Baud divisor for I2C Fast mode.  The PIC18F I2C host clock is
 * Fosc / (5 * (BAUD + 1)).  At Fosc = 64 MHz:
 *   BAUD = 31  → 400 kHz
 *   BAUD = 7F  → 100 kHz
 * Override before compiling if the bus needs a different rate. */
#ifndef I2C_FME
#define I2C_FME 1
#endif

#if I2C_FME
#define I2C_BAUD 0x31
#else
#define I2C_BAUD 0x7F
#endif

#ifndef I2C_QUEUE_SIZE
#define I2C_QUEUE_SIZE 16
#endif

#ifndef I2C_TX_MAX
#define I2C_TX_MAX 8
#endif

#ifndef I2C_RX_MAX
#define I2C_RX_MAX 8
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
typedef void (*I2cCompletion)(uint8_t* rx_buf, uint8_t rx_len, void* ctx);

/* ── Main-loop API ──────────────────────────────────────────────────── */

/* Set the client-side cold RX handlers.  May be called before or right
 * after i2c_init. May be left unset (NULL) if that direction is unused. */
void i2c_set_cold_rx_handler(I2cCompletion cold_tx);

/* One-time hardware init.  Configures I2C1 at 400 kHz.
 * Caller must have set up pins and oscillator beforehand. */
void i2c_init(uint8_t client_addr);

/* Set the data that the client is expected to transmit */
I2cResult i2c_set_client_tx(uint8_t* tx, uint8_t tx_len);

/* Submit a host write or write-then-read.
 *   tx, tx_len  — required (1..I2C_TX_MAX).  Copied into the queue.
 *   rx_len      — write-then-read.  rx_len=0 means write-only.
 *   cb, ctx     — fired from i2c_poll().  cb may be NULL.
 * Main-loop context only. */
I2cResult i2c_submit(uint8_t addr, const uint8_t* tx, uint8_t tx_len, uint8_t rx_len, I2cCompletion cb, void* ctx);

/* Main-loop poll.  Fires the completion callback for finished ops and
 * starts the next queued op when the bus is free.  O(1) per call. */
void i2c_poll(void);

#endif /* I2C_H */
