/*
 * i2c.h — Shared I2C multi-master driver for PIC18F27/47/57Q84.
 *
 * Async, DMA-driven host + client on I2C1.  400 kHz Fast mode,
 * 7-bit addressing, multi-master with automatic bus arbitration and
 * collision retry.  Bus timeout uses the peripheral BTO (LFINTOSC),
 * not a software tick.  The driver owns I2C1 / I2C1E IRQs internally.
 *
 * Pin/PPS/oscillator setup is the caller's responsibility — this
 * driver only programs the I2C1 module registers.
 *
 * Host side:
 *   i2c_submit() enqueues a write or write-then-read; the driver owns
 *   both the TX (copied) and RX (returned in the completion) buffers.
 *   Completion callbacks fire from i2c_poll() in main-loop context.
 *
 * Client side:
 *   i2c_init(addr) enables client mode on the given 7-bit address.
 *   Incoming writes are delivered to the cold-rx handler asynchronously
 *   from i2c_poll().  Replies to incoming reads come from the buffer
 *   that the application has loaded with i2c_set_client_tx() — this
 *   driver does not synthesise per-request responses.
 *
 * Context rules:
 *   Main loop only:  i2c_init, i2c_set_cold_rx_handler,
 *                    i2c_set_client_tx, i2c_submit, i2c_poll
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
 * Fosc / (4 * (BAUD + 1)).  At Fosc = 64 MHz:
 *   BAUD = 39 (0x27)  → 400 kHz
 * Fosc / (5 * (BAUD + 1)).  At Fosc = 64 MHz:
 *   BAUD = 127 (0x7F) → 100 kHz
 * Override before compiling if the bus needs a different rate. */
#ifndef I2C_FME
#define I2C_FME 1
#endif

#if I2C_FME
#define I2C_BAUD 0x27
#else
#define I2C_BAUD 0x7F
#endif

#ifndef I2C_QUEUE_SIZE
#define I2C_QUEUE_SIZE 16
#endif

#ifndef I2C_TX_MAX
#define I2C_TX_MAX 16
#endif

#ifndef I2C_RX_MAX
#define I2C_RX_MAX 16
#endif

#ifndef I2C_CLIENT_BUF_SIZE
#define I2C_CLIENT_BUF_SIZE 16
#endif

#ifndef I2C_RETRY_COUNT
#define I2C_RETRY_COUNT 1
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

/* Completion callback.  Fired from i2c_poll() (main-loop context) for
 * host transactions, and for cold (client) RX deliveries.
 *   result — I2C_RESULT_OK on success, or the failure reason.
 *   addr   — for host transactions: the target's I2C address.  For cold
 *            (client) RX deliveries: 0 (the receiving address is this
 *            device).
 *   tx_buf — driver-owned buffer holding the transmitted bytes (host
 *            transactions only; NULL for cold RX).  Valid only while
 *            the callback is running.
 *   tx_len — bytes transmitted.  0 for cold RX.
 *   rx_buf — driver-owned buffer holding received bytes.  Valid only
 *            while the callback is running; copy out anything that
 *            must outlive the callback.
 *   rx_len — bytes received.  0 on write-only success or any failure.
 *            For host reads, callers should treat rx_len == 0 as
 *            failure since the protocol expects > 0 bytes back. */
typedef void (*I2cCompletion)(I2cResult result, uint8_t addr, uint8_t* tx_buf, uint8_t tx_len, uint8_t* rx_buf,
                              uint8_t rx_len);

/* Synchronous cold-RX handler.  Fired from ISR context the moment a
 * client-RX transaction completes (stop or restart), before the next
 * address byte can arrive.  Lets the app intercept urgent commands — for
 * example, a read request whose response must be staged via
 * i2c_set_client_tx() before the read-phase address triggers client TX.
 *
 * Return 0 if the request was handled synchronously (the driver will skip
 * the asynchronous cold-RX queue).  Return 1 to defer: the driver will
 * queue the bytes for main-loop delivery via the I2cCompletion registered
 * with i2c_set_cold_rx_handler().  ISR-callable only — must not block. */
typedef uint8_t (*I2cSyncColdRxHandler)(uint8_t* data, uint8_t len);

/* Wire-event log.  The driver records each transaction into an internal
 * ring buffer (CR for client receive, WA/WN for host-write ack/nack,
 * RA/RN for host-read ack/nack).  Boards that want to visualise bus
 * activity snapshot the ring from main context via i2c_log_snapshot.
 * kind is one of I2cLogKind; addr is the peer address (0 for client-RX,
 * target address for host ops); data[0..len-1] is the payload (received
 * bytes for CR, tx buffer for WA/WN, rx buffer for RA/RN). */
typedef enum {
    I2C_LOG_CR,  /* client received (data = raw message, sender info embedded) */
    I2C_LOG_CT,  /* client transmitted a response (data = framed response + CRC) */
    I2C_LOG_WA,  /* host write acknowledged (data = tx buffer) */
    I2C_LOG_WN,  /* host write not acknowledged / aborted */
    I2C_LOG_RA,  /* host read acknowledged (data = rx buffer) */
    I2C_LOG_RN,  /* host read not acknowledged / aborted */
} I2cLogKind;

#define I2C_LOG_DATA_MAX 9u   /* big enough for the largest framed message
                                 (relay_changed: 1 id + 7 payload + 1 crc) */

typedef struct {
    uint8_t kind;  /* I2cLogKind */
    uint8_t addr;
    uint8_t len;
    uint8_t data[I2C_LOG_DATA_MAX];
} I2cLogEntry;

/* ── Main-loop API ──────────────────────────────────────────────────── */

/* Set the client-side cold RX handler.  May be called before or right
 * after i2c_init. May be left unset (NULL) if that direction is unused. */
void i2c_set_cold_rx_handler(I2cCompletion cold_tx);

/* Set the synchronous cold-RX handler (see I2cSyncColdRxHandler).  If unset,
 * every client-RX transaction is queued for async delivery as before. */
void i2c_set_sync_cold_rx_handler(I2cSyncColdRxHandler handler);

/* Copy up to `capacity` most-recent log entries into `out`, newest first.
 * Returns the number of entries actually copied.  Main-context only —
 * reads are briefly interrupt-locked so an ISR append can't interleave. */
uint8_t i2c_log_snapshot(I2cLogEntry* out, uint8_t capacity);

/* One-time hardware init.  Configures I2C1 at 400 kHz.
 * Caller must have set up pins and oscillator beforehand. */
void i2c_init(uint8_t client_addr);

/* Set the data that the client is expected to transmit */
I2cResult i2c_set_client_tx(uint8_t* tx, uint8_t tx_len);

/* Submit a host write or write-then-read.
 *   tx, tx_len  — required (1..I2C_TX_MAX).  Copied into the queue.
 *   rx_len      — write-then-read.  rx_len=0 means write-only.
 *   cb          — fired from i2c_poll().  cb may be NULL.
 * Main-loop context only. */
I2cResult i2c_submit(uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t rx_len, I2cCompletion cb);

/* Main-loop poll.  Fires the completion callback for finished ops and
 * starts the next queued op when the bus is free.  O(1) per call. */
void i2c_poll(void);

#endif /* I2C_H */
