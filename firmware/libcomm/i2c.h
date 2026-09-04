/*
 * i2c.h — Shared I2C multi-master driver for PIC18F27/47/57Q84.
 *         Interrupt-driven alternative implementation.
 *
 * Async host + client on I2C1, Multi-Host 7-bit mode, 400 kHz Fast mode,
 * automatic arbitration handling and bounded retry.  Bus timeout uses the
 * peripheral BTO (LFINTOSC), not a software tick.  The driver owns the
 * I2C1, I2C1E, I2C1TX and I2C1RX IRQs internally.
 *
 * Unlike the original driver this one moves bytes with the CPU, one byte
 * per interrupt, and uses no DMA channel.  With CSD = 0 the peripheral
 * stretches SCL whenever software has not yet supplied or consumed a byte
 * (DS40002213D §37.3.11.1, §37.5.1), so software feeding is the mechanism
 * the hardware is designed around rather than a compromise.  See
 * ../../i2c-alternative-design.md for the full rationale.
 *
 * Pin/PPS/oscillator setup is the caller's responsibility — this driver
 * only programs the I2C1 module registers.
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
 *                    i2c_set_sync_cold_rx_handler, i2c_set_client_tx,
 *                    i2c_submit, i2c_poll, i2c_log_snapshot
 *
 * Prerequisites (caller-owned):
 *   SDA/SCL: TRIS, ANSEL=0, ODCON=1, RxxI2C TH/PU/SLEW, PPS routed
 *   FOSC = 64 MHz
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/* ── Configuration (override before including this header) ──────────── */

/* Baud divisor.  The PIC18F I2C host clock is
 *   Fosc / (4 * (BAUD + 1))   in Fast mode  (FME = 1)
 *   Fosc / (5 * (BAUD + 1))   in Standard mode (FME = 0)
 * At Fosc = 64 MHz (Eq. 37-1 / 37-2):
 *   BAUD = 39  (0x27) → 400.0 kHz   (FME = 1)
 *   BAUD = 79  (0x4F) → 200.0 kHz   (FME = 1)
 *   BAUD = 127 (0x7F) → 100.0 kHz   (FME = 0)
 * Override before compiling if the bus needs a different rate. */
#ifndef I2C_FME
#define I2C_FME 1
#endif

#if I2C_FME
#define I2C_BAUD 0x27
#else
#define I2C_BAUD 0x7F
#endif

/* Must be a power of two — the ring wraps with a mask, not a modulo. */
#ifndef I2C_QUEUE_SIZE
#define I2C_QUEUE_SIZE 16
#endif

#ifndef I2C_TX_MAX
#define I2C_TX_MAX 16
#endif

#ifndef I2C_RX_MAX
#define I2C_RX_MAX 16
#endif

#ifndef I2C_RETRY_COUNT
#define I2C_RETRY_COUNT 2
#endif

/* ── Types ──────────────────────────────────────────────────────────── */

typedef enum {
    I2C_RESULT_OK = 0,
    I2C_RESULT_BUSY,       /* bus held or arbitration lost               */
    I2C_RESULT_NACK,       /* target did not acknowledge                 */
    I2C_RESULT_TIMEOUT,    /* peripheral bus time-out (BTO)              */
    I2C_RESULT_QUEUE_FULL, /* i2c_submit rejected — queue full           */
    I2C_RESULT_BAD_ARG,    /* rejected — null pointer or bad length      */
    I2C_RESULT_BAD_CRC,    /* never produced here; owned by libcomm      */
} I2cResult;

/* Completion callback.  Fired from i2c_poll() (main-loop context) for
 * host transactions, and for cold (client) RX deliveries.
 *   result — I2C_RESULT_OK on success, or the failure reason.
 *   addr   — for host transactions: the target's I2C address.  For cold
 *            (client) RX deliveries: 0 (the receiving address is this
 *            device, and the sender is identified inside the payload).
 *   tx_buf — driver-owned buffer holding the transmitted bytes (host
 *            transactions only; NULL for cold RX).  Valid only while
 *            the callback is running.
 *   tx_len — bytes transmitted.  0 for cold RX.
 *   rx_buf — driver-owned buffer holding received bytes.  Valid only
 *            while the callback is running; copy out anything that
 *            must outlive the callback.
 *   rx_len — bytes *actually* received, which may be short of the number
 *            requested.  0 on write-only success and on any failure.
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

/* Wire-event log.  The driver records each transaction phase into an
 * internal ring buffer.  Boards that want to visualise bus activity
 * snapshot the ring from main context via i2c_log_snapshot.
 *
 * An entry is a (kind, result) pair rather than one fused enum: the
 * original driver encoded failures as `I2C_LOG_WA + result`, which made
 * the enum's declaration order load-bearing arithmetic that nothing in
 * the header explained.  Here `kind` says which phase and `result` says
 * how it ended, independently. */
typedef enum {
    I2C_LOG_CR, /* client received  (data = raw message, sender embedded) */
    I2C_LOG_CT, /* client responded (data = framed response + CRC)        */
    I2C_LOG_W,  /* host write phase (data = tx buffer)                    */
    I2C_LOG_R,  /* host read phase  (data = rx buffer)                    */
} I2cLogKind;

#define I2C_LOG_DATA_MAX 9u /* largest framed message: 1 id + 7 payload + 1 crc */

typedef struct {
    uint8_t kind;   /* I2cLogKind */
    uint8_t result; /* I2cResult  */
    uint8_t addr;   /* peer address; 0 for CR/CT */
    uint8_t req_id; /* per-transaction id, wraps at 0xFF */
    uint8_t len;
    uint8_t data[I2C_LOG_DATA_MAX];
} I2cLogEntry;

/* ── Main-loop API ──────────────────────────────────────────────────── */

/* Set the client-side cold RX handler.  May be called before or right
 * after i2c_init.  May be left unset (NULL) if that direction is unused. */
void i2c_set_cold_rx_handler(I2cCompletion cold_rx);

/* Set the synchronous cold-RX handler (see I2cSyncColdRxHandler).  If unset,
 * every client-RX transaction is queued for async delivery. */
void i2c_set_sync_cold_rx_handler(I2cSyncColdRxHandler handler);

/* Copy up to `capacity` most-recent log entries into `out`, newest first.
 * Returns the number of entries actually copied.  Main-context only —
 * reads are briefly interrupt-locked so an ISR append can't interleave. */
uint8_t i2c_log_snapshot(I2cLogEntry* out, uint8_t capacity);

/* One-time hardware init.  Configures I2C1 in Multi-Host 7-bit mode.
 * Caller must have set up pins and oscillator beforehand. */
void i2c_init(uint8_t client_addr);

/* Stage the data the client will transmit when next addressed for a read. */
I2cResult i2c_set_client_tx(uint8_t* tx, uint8_t tx_len);

/* Submit a host write or write-then-read.
 *   tx, tx_len  — required (1..I2C_TX_MAX).  Copied into the queue.
 *   rx_len      — write-then-read.  rx_len=0 means write-only.
 *   cb          — fired from i2c_poll().  cb may be NULL.
 * Main-loop context only. */
I2cResult i2c_submit(uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t rx_len, I2cCompletion cb);

/* Main-loop poll.  Fires the completion callback for one finished op and
 * starts the next queued op when the bus is free.  O(1) per call. */
void i2c_poll(void);

#endif /* I2C_H */
