#ifndef I2C_H
#define I2C_H

/*
 * i2c.h — Interrupt-driven I2C multi-master driver, host + client.
 *
 * Target:  PIC18F27/47/57Q84  (DS40002213D)
 * Bus:     400 kHz Fast mode, 7-bit addressing, multi-master with
 *          automatic bus arbitration and collision retry.
 * HW used: I2C1 module only.  No DMA, no Timer1 — the 1 ms tick
 *          is driven externally by the caller.
 *
 * Design overview
 * ───────────────
 * The driver owns I2C1 and switches between two operating modes:
 *
 *   • Client (slave) — peripheral idles in 7-bit client mode listening
 *     on a programmed address.  Inbound writes from another master are
 *     assembled into a buffer and delivered to the rx handler at the
 *     Stop condition.  Inbound read requests are answered by calling
 *     the read handler; the response is shifted out byte-by-byte.
 *     Both handlers run in ISR context — keep them short.
 *     Client mode is optional: if no address is configured before
 *     i2c_init(), the peripheral stays in host mode and ignores
 *     inbound traffic.
 *
 *   • Host (master) — when a host operation is queued via
 *     i2c_submit(), the next i2c_poll() waits for the bus to be free
 *     and for any in-progress client transaction to finish, then
 *     switches MODE to host, drives the transaction, and switches
 *     back to client mode on completion.  Operations may be pure
 *     writes (rx_len = 0) or write-then-read with a repeated start
 *     (rx_len > 0).
 *
 * Ops are queued in a fixed-size ring buffer.  i2c_poll() pumps one
 * step per call: it fires the completion callback for any finished op,
 * advances the queue, and starts the next op when the bus is free.
 * Bus events are handled in the I2C1 ISR (status flags + per-byte
 * shifting); a 1 ms tick handler enforces a per-op software timeout
 * and a bus-collision back-off.
 *
 * Context rules
 * ─────────────
 *   Main loop only:  i2c_init, i2c_set_client, i2c_submit, i2c_poll
 *   ISR only:        i2c_tick_ms (call from a 1 ms tick),
 *                    i2c_isr     (call from the I2C1 vectors)
 *   Never mixed.
 *
 * Pin / oscillator prerequisites (caller-owned)
 * ─────────────────────────────────────────────
 *   • SDA/SCL pins: TRIS = 1 (input until peripheral takes over),
 *     ANSEL = 0, ODCON = 1, RBxI2C / RCxI2C set for I2C levels and
 *     slew rate, PPS routed to/from I2C1SCL / I2C1SDA.
 *   • FOSC = 64 MHz; the driver assumes I2C1CLK source = FOSC and
 *     programs I2C1BAUD for 400 kHz on that basis.
 */

#include <stdint.h>

/* ── Configuration (override before including this header) ──────────── */

/* Host op queue depth — must be a power of 2. */
#ifndef I2C_QUEUE_SIZE
#define I2C_QUEUE_SIZE 8
#endif

/* Max bytes per host-op TX payload.  Each queue slot embeds one buffer
 * of this size; the caller's tx is copied in at submit time. */
#ifndef I2C_TX_MAX
#define I2C_TX_MAX 8
#endif

/* Client (slave) buffer size — longest inbound message, including the
 * command byte.  The driver assembles up to this many bytes per
 * client-side transaction.  Bytes beyond the limit are dropped. */
#ifndef I2C_CLIENT_BUF_SIZE
#define I2C_CLIENT_BUF_SIZE 8
#endif

/* Retry budget for transient host errors (BUS_BUSY / TIMEOUT). */
#ifndef I2C_RETRY_COUNT
#define I2C_RETRY_COUNT 3
#endif

/* ── Types ──────────────────────────────────────────────────────────── */

typedef enum {
    I2C_STATUS_PENDING = 0, /* queued, not yet started               */
    I2C_STATUS_ACTIVE,      /* transfer in progress on the bus       */
    I2C_STATUS_OK,          /* completed successfully                */
    I2C_STATUS_NACK,        /* client did not acknowledge            */
    I2C_STATUS_BUS_BUSY,    /* arbitration lost, retries exhausted   */
    I2C_STATUS_TIMEOUT,     /* software / hardware timeout           */
    I2C_STATUS_QUEUE_FULL,  /* i2c_submit return only                */
    I2C_STATUS_BAD_ARG,     /* i2c_submit return only                */
} I2CStatus;

/*
 * Host completion callback.  Fired from i2c_poll() (main-loop context)
 * once a queued op has reached a terminal state.
 *
 *   status  — terminal result (OK / NACK / BUS_BUSY / TIMEOUT)
 *   rx_buf  — same pointer the caller passed to i2c_submit()
 *   rx_len  — bytes actually received (0 on write-only or non-OK)
 *   ctx     — opaque pointer the caller passed to i2c_submit()
 */
typedef void (*I2CCompletion)(I2CStatus status, uint8_t* rx_buf, uint8_t rx_len, void* ctx);

/*
 * Client RX handler — invoked from ISR context when a master has
 * finished writing to us (Stop condition).  data points into a driver
 * buffer that is reused on the next inbound message; copy if needed.
 */
typedef void (*I2CClientRxHandler)(const uint8_t* data, uint8_t len);

/*
 * Client read handler — invoked from ISR context when a master
 * addresses us with R/W = 1.  request[] is whatever the master wrote
 * during the preceding write phase (typically a register / command
 * byte).  Write up to response_max bytes into response[] and return
 * the actual number; the driver shifts them out on the bus.
 */
typedef uint8_t (*I2CClientReadHandler)(const uint8_t* request, uint8_t request_len, uint8_t* response,
                                        uint8_t response_max);

/* One queued host operation.  Internal — do not access directly. */
typedef struct {
    uint8_t addr;            /* 7-bit client address                 */
    uint8_t tx_len;          /* bytes in tx[] (1..I2C_TX_MAX)        */
    uint8_t rx_len;          /* 0 = write only; else write-then-read */
    uint8_t tx[I2C_TX_MAX];  /* TX payload, copied at submit time    */
    uint8_t* rx_buf;         /* caller-owned RX buffer, lives until cb */
    I2CCompletion cb;        /* may be NULL                          */
    void* cb_ctx;
    uint8_t retries;         /* remaining retries for transient errors */
    volatile I2CStatus status;
} I2COp;

/* Internal host FSM. */
typedef enum {
    I2C_HOST_IDLE = 0,
    I2C_HOST_TX,
    I2C_HOST_RX,
    I2C_HOST_STOP,
    I2C_HOST_BACKOFF,
} I2CHostState;

/* Internal client FSM. */
typedef enum {
    I2C_CLIENT_IDLE = 0,
    I2C_CLIENT_RX,
    I2C_CLIENT_TX,
} I2CClientState;

/*
 * Driver instance.  Allocate one statically and pass to every call.
 * Members are documented as internal — do not poke them from outside
 * the driver.  Volatile fields are written in ISR context.
 */
typedef struct {
    /* Host queue + FSM */
    I2COp queue[I2C_QUEUE_SIZE];
    volatile uint8_t q_head;
    volatile uint8_t q_tail;
    volatile uint8_t host_state;     /* I2CHostState */
    volatile uint8_t tx_idx;
    volatile uint8_t rx_idx;
    volatile uint16_t deadline_ms;   /* 0 = inactive */

    /* Client config (set via i2c_set_client before i2c_init). */
    uint8_t client_addr;             /* 0 = client disabled */
    I2CClientRxHandler client_rx_cb;
    I2CClientReadHandler client_read_cb;

    /* Client FSM + buffers (volatile — ISR-managed). */
    volatile uint8_t client_state;   /* I2CClientState */
    volatile uint8_t client_rx_len;
    volatile uint8_t client_tx_len;
    volatile uint8_t client_tx_pos;
    volatile uint8_t client_rx_buf[I2C_CLIENT_BUF_SIZE];
    volatile uint8_t client_tx_buf[I2C_CLIENT_BUF_SIZE];
} I2CDriver;

/* ── Main-loop API ──────────────────────────────────────────────────── */

/*
 * Configure the optional client (slave) role.  Must be called before
 * i2c_init().  addr = 0 disables client mode entirely (host-only).
 * The handlers run in ISR context; either may be NULL.
 */
void i2c_set_client(I2CDriver* drv, uint8_t addr, I2CClientRxHandler rx, I2CClientReadHandler read);

/*
 * One-time hardware init.  Configures I2C1 at 400 kHz, enables the
 * client address (if set), and arms the relevant interrupts.
 * Caller must have configured pins (TRIS / ANSEL / ODCON / PPS /
 * RxxI2C) and oscillator (FOSC = 64 MHz) beforehand.
 *
 * Call once from main() before the super-loop.
 */
void i2c_init(I2CDriver* drv);

/*
 * Submit a host write or write-then-read.
 *
 *   tx, tx_len  — required (1..I2C_TX_MAX).  Copied into the queue.
 *   rx_buf, rx_len — write-then-read with repeated start.  rx_len = 0
 *                   means "write only"; rx_buf may be NULL in that case.
 *                   When rx_len > 0, rx_buf must remain valid until the
 *                   completion callback fires (typically file-static).
 *   cb, ctx     — fired from i2c_poll() in main-loop context.  cb may
 *                 be NULL for fire-and-forget writes.
 *
 * Returns I2C_STATUS_OK on enqueue, I2C_STATUS_QUEUE_FULL when the
 * ring is full, or I2C_STATUS_BAD_ARG for invalid lengths.
 *
 * Main-loop context only.
 */
I2CStatus i2c_submit(I2CDriver* drv, uint8_t addr, const uint8_t* tx, uint8_t tx_len, uint8_t* rx_buf, uint8_t rx_len,
                     I2CCompletion cb, void* ctx);

/*
 * Main-loop poll — call every super-loop iteration.
 *
 *  1. Fires the completion callback for the head op when its terminal
 *     status is set by the ISR.
 *  2. Starts the next queued op when the bus is free and no client
 *     transaction is in progress.
 *
 * O(1) work per call.
 */
void i2c_poll(I2CDriver* drv);

/* ── ISR entry points ───────────────────────────────────────────────── */

/* Call from a 1 ms tick ISR (e.g. TMR0).  Drives per-op timeout and
 * bus-collision back-off. */
void i2c_tick_ms(I2CDriver* drv);

/* Call from the I2C1 interrupt vectors.  A single function handles
 * I2C1, I2C1E, I2C1RX and I2C1TX — wire all four to one dispatcher,
 * e.g.:
 *
 *   void __interrupt(irq(I2C1, I2C1E, I2C1RX, I2C1TX), base(8))
 *   I2C1_ISR(void) { i2c_isr(&drv); }
 */
void i2c_isr(I2CDriver* drv);

#endif /* I2C_H */
