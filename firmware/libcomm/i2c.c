/*
 * i2c.c — Interrupt-driven I2C multi-master driver, host + client.
 *
 * See i2c.h for the design overview and API contract.
 *
 * Implementation notes
 * ────────────────────
 *  - Pure byte-level interrupts, no DMA.  Protocol payloads are small
 *    enough (<=8 bytes) that per-byte ISR overhead is negligible at
 *    400 kHz.
 *  - The MPLAB-supplied chip headers (xc.h) place the I2C1 interrupt
 *    enables in PIE7 / PIR7 (offset 0x4A5 / 0x4B5).  We use the
 *    bit-named accessors here for clarity.
 *  - Pin / oscillator setup is the caller's job; the driver only
 *    programs the I2C1 module itself.
 */

#include "i2c.h"

#include "libcomm.h"

#include <xc.h>

/* Baud divisor: Fosc / (4 * (BAUD + 1)).  At Fosc = 64 MHz, BAUD = 39
 * yields 400 kHz.  Override before compiling i2c.c if the bus needs to
 * run slower for signal-integrity reasons. */
#ifndef I2C_BAUD
#define I2C_BAUD 39
#endif

/* Per-byte software deadline (ms) — guards against a peripheral that
 * stalls mid-byte (e.g. clock-stretching client that never releases). */
#define I2C_DEADLINE_BYTE_MS 5
/* Stop-condition deadline — bus turn-around after the last byte. */
#define I2C_DEADLINE_STOP_MS 10
/* Back-off after a lost arbitration / busy bus. */
#define I2C_BACKOFF_MS 2

/* ── Queue helpers ──────────────────────────────────────────────────── */

#define Q_MASK (I2C_QUEUE_SIZE - 1u)

static inline uint8_t q_next(uint8_t idx) {
    return (uint8_t)((idx + 1u) & Q_MASK);
}

static inline uint8_t q_empty(const I2CDriver* drv) {
    return drv->q_head == drv->q_tail;
}

static inline uint8_t q_full(const I2CDriver* drv) {
    return q_next(drv->q_tail) == drv->q_head;
}

/* ── Forward decls ──────────────────────────────────────────────────── */

static void client_mode_enable(I2CDriver* drv);
static void client_mode_disable(void);
static void client_reset(I2CDriver* drv);
static void client_handle_event(I2CDriver* drv);
static void client_handle_error(I2CDriver* drv);

static void try_dispatch(I2CDriver* drv);
static void start_transaction(I2CDriver* drv, I2COp* op);
static void start_read_phase(I2COp* op);
static void host_complete(I2CDriver* drv, I2CStatus status);
static void host_handle_event(I2CDriver* drv);
static void host_handle_error(I2CDriver* drv);

/* ── Public API: configuration & init ───────────────────────────────── */

void i2c_set_client(I2CDriver* drv, uint8_t addr, I2CClientRxHandler rx, I2CClientReadHandler read) {
    drv->client_addr = addr;
    drv->client_rx_cb = rx;
    drv->client_read_cb = read;
}

void i2c_init(I2CDriver* drv) {
    /* Zero everything except the client config the caller just set. */
    uint8_t saved_addr = drv->client_addr;
    I2CClientRxHandler saved_rx = drv->client_rx_cb;
    I2CClientReadHandler saved_read = drv->client_read_cb;

    /* sizeof(I2CDriver) is well above 256 bytes (queue alone is many
     * slots), so the loop counter must be wider than uint8_t. */
    for (uint16_t i = 0; i < sizeof(*drv); i++) {
        ((volatile uint8_t*)drv)[i] = 0;
    }

    drv->client_addr = saved_addr;
    drv->client_rx_cb = saved_rx;
    drv->client_read_cb = saved_read;

    /* Common peripheral setup — applies to both modes.
     * I2C1CLK = 0 selects FOSC (64 MHz); paired with I2C_BAUD = 39
     * that yields the documented 400 kHz Fast-mode rate. */
    I2C1CON0bits.EN = 0;
    I2C1CLK = 0x00;
    I2C1BAUD = I2C_BAUD;

    if (drv->client_addr != 0) {
        client_mode_enable(drv);
    } else {
        /* Pure host mode: peripheral stays disabled until the first
         * start_transaction() flips it on. */
        I2C1CON0bits.MODE = 0b100;
    }
}

/* ── Public API: submit ─────────────────────────────────────────────── */

I2CStatus i2c_submit(I2CDriver* drv, uint8_t addr, const uint8_t* tx, uint8_t tx_len, uint8_t* rx_buf, uint8_t rx_len,
                     I2CCompletion cb, void* ctx) {
    if (tx_len == 0 || tx_len > I2C_TX_MAX) {
        return I2C_STATUS_BAD_ARG;
    }
    if (rx_len > 0 && rx_buf == 0) {
        return I2C_STATUS_BAD_ARG;
    }

    INTERRUPT_PUSH;
    if (q_full(drv)) {
        INTERRUPT_POP;
        return I2C_STATUS_QUEUE_FULL;
    }

    I2COp* op = &drv->queue[drv->q_tail];
    op->addr = addr;
    op->tx_len = tx_len;
    op->rx_len = rx_len;
    op->rx_buf = rx_buf;
    op->cb = cb;
    op->cb_ctx = ctx;
    op->retries = I2C_RETRY_COUNT;
    op->status = I2C_STATUS_PENDING;
    for (uint8_t i = 0; i < tx_len; i++) {
        op->tx[i] = tx[i];
    }

    drv->q_tail = q_next(drv->q_tail);
    INTERRUPT_POP;
    return I2C_STATUS_OK;
}

/* ── Public API: poll ───────────────────────────────────────────────── */

void i2c_poll(I2CDriver* drv) {
    /* Phase 1 — deliver completion for the head op when its terminal
     * status has been written by the ISR. */
    if (drv->host_state == I2C_HOST_IDLE && !q_empty(drv)) {
        I2COp* op = &drv->queue[drv->q_head];
        I2CStatus s = op->status;
        if (s != I2C_STATUS_PENDING && s != I2C_STATUS_ACTIVE) {
            uint8_t actual_rx = (s == I2C_STATUS_OK) ? op->rx_len : 0;
            if (op->cb) {
                op->cb(s, op->rx_buf, actual_rx, op->cb_ctx);
            }
            INTERRUPT_PUSH;
            drv->q_head = q_next(drv->q_head);
            INTERRUPT_POP;
        }
    }

    /* Phase 2 — kick the next op if the bus is free. */
    if (drv->host_state == I2C_HOST_IDLE && !q_empty(drv)) {
        try_dispatch(drv);
    }
}

/* ── Dispatch & start ───────────────────────────────────────────────── */

/* Caller may be main or ISR; relies on its caller to mask interrupts
 * if invoked from main context. */
static void try_dispatch(I2CDriver* drv) {
    if (drv->host_state != I2C_HOST_IDLE) {
        return;
    }
    if (q_empty(drv)) {
        return;
    }
    if (drv->client_state != I2C_CLIENT_IDLE) {
        /* A slave transaction is in flight — wait for its PCIF; the
         * client ISR will retry dispatch when it returns to IDLE. */
        return;
    }
    I2COp* op = &drv->queue[drv->q_head];
    if (op->status != I2C_STATUS_PENDING) {
        return;
    }
    start_transaction(drv, op);
}

/*
 * Configure peripheral and kick off the write phase of an op.
 *
 * BFRE must be sampled while the peripheral is still in its previous
 * (client) mode — the bit reflects line state only when the module is
 * enabled.  Reading after EN=0 returns stale 0 and would trap us in
 * BACKOFF forever.
 */
static void start_transaction(I2CDriver* drv, I2COp* op) {
    if (drv->client_addr != 0 && !I2C1STAT0bits.BFRE) {
        drv->host_state = I2C_HOST_BACKOFF;
        drv->deadline_ms = I2C_BACKOFF_MS;
        return;
    }

    op->status = I2C_STATUS_ACTIVE;

    /* Mask the I2C interrupt group while we tear down client mode and
     * rebuild for host mode.  Other peripherals (TMR0/IOC) stay live. */
    PIE7bits.I2C1IE = 0;
    PIE7bits.I2C1EIE = 0;
    PIE7bits.I2C1RXIE = 0;
    PIE7bits.I2C1TXIE = 0;

    client_mode_disable();
    drv->tx_idx = 0;
    drv->rx_idx = 0;

    I2C1CON0bits.MODE = 0b100;                       /* 7-bit host */
    I2C1CON0bits.RSEN = (op->rx_len > 0) ? 1 : 0;    /* repeat-start for read */
    I2C1CNTH = 0;
    I2C1CNTL = op->tx_len;
    I2C1ADB1 = (uint8_t)(op->addr << 1);             /* W phase */
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1STAT1bits.CLRBF = 1;

    I2C1PIEbits.PCIE = 1;
    I2C1ERRbits.NACKIE = 1;
    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;
    PIE7bits.I2C1RXIE = 1;
    PIE7bits.I2C1TXIE = 1;

    drv->host_state = I2C_HOST_TX;
    drv->deadline_ms = I2C_DEADLINE_BYTE_MS;

    I2C1CON0bits.EN = 1;
    I2C1CON0bits.S = 1;
}

/* Repeat-start into the read phase after the write bytes have drained. */
static void start_read_phase(I2COp* op) {
    I2C1CON0bits.RSEN = 0;
    I2C1CNTH = 0;
    I2C1CNTL = op->rx_len;
    I2C1ADB1 = (uint8_t)((op->addr << 1) | 0x01); /* R phase */
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1CON0bits.S = 1;
}

/*
 * Finalise the current transfer.  Caller is the I2C ISR (or tick on
 * timeout).  Either retries the op (transient errors with budget
 * remaining) or sets the terminal status; either way returns to client
 * mode so inbound traffic can resume.
 */
static void host_complete(I2CDriver* drv, I2CStatus status) {
    I2COp* op = &drv->queue[drv->q_head];

    if ((status == I2C_STATUS_BUS_BUSY || status == I2C_STATUS_TIMEOUT) && op->retries > 0) {
        op->retries--;
        op->status = I2C_STATUS_PENDING;
        I2C1CON0bits.EN = 0;
        if (drv->client_addr != 0) {
            client_mode_enable(drv);
        }
        drv->host_state = I2C_HOST_BACKOFF;
        drv->deadline_ms = I2C_BACKOFF_MS;
        return;
    }

    drv->deadline_ms = 0;
    op->status = status;

    I2C1CON0bits.EN = 0;
    if (drv->client_addr != 0) {
        client_mode_enable(drv);
    }
    drv->host_state = I2C_HOST_IDLE;
}

/* ── ISR ────────────────────────────────────────────────────────────── */

void i2c_isr(I2CDriver* drv) {
    /* Dispatch by current peripheral mode.  HOST_BACKOFF leaves the
     * peripheral in client mode while the host FSM is "busy" — slave
     * events during backoff still need the client path.  Both error
     * and event handlers are invoked unconditionally; each is a no-op
     * when its flags are clear. */
    if (I2C1CON0bits.MODE == 0b000) {
        client_handle_error(drv);
        client_handle_event(drv);
    } else {
        host_handle_error(drv);
        host_handle_event(drv);
    }
}

/* ── Host ISR helpers ───────────────────────────────────────────────── */

static void host_handle_event(I2CDriver* drv) {
    /* If the error handler already completed the op (host_state ==
     * IDLE) or we're in BACKOFF, do not act on stale event flags. */
    if (drv->host_state == I2C_HOST_IDLE || drv->host_state == I2C_HOST_BACKOFF) {
        I2C1PIRbits.PCIF = 0;
        return;
    }
    I2COp* op = &drv->queue[drv->q_head];

    if (I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        if (drv->host_state == I2C_HOST_STOP || drv->host_state == I2C_HOST_RX) {
            host_complete(drv, I2C_STATUS_OK);
        } else {
            /* Unexpected stop in TX → treat as collision/busy. */
            host_complete(drv, I2C_STATUS_BUS_BUSY);
        }
        return;
    }

    if (drv->host_state == I2C_HOST_TX && I2C1STAT1bits.TXBE) {
        if (drv->tx_idx < op->tx_len) {
            I2C1TXB = op->tx[drv->tx_idx++];
            drv->deadline_ms = I2C_DEADLINE_BYTE_MS;
        } else if (op->rx_len > 0) {
            drv->host_state = I2C_HOST_RX;
            drv->deadline_ms = I2C_DEADLINE_BYTE_MS;
            start_read_phase(op);
        } else {
            drv->host_state = I2C_HOST_STOP;
            drv->deadline_ms = I2C_DEADLINE_STOP_MS;
        }
        return;
    }

    if (drv->host_state == I2C_HOST_RX && I2C1STAT1bits.RXBF) {
        uint8_t b = I2C1RXB;
        if (drv->rx_idx < op->rx_len) {
            op->rx_buf[drv->rx_idx++] = b;
        }
        if (drv->rx_idx >= op->rx_len) {
            drv->host_state = I2C_HOST_STOP;
            drv->deadline_ms = I2C_DEADLINE_STOP_MS;
        } else {
            drv->deadline_ms = I2C_DEADLINE_BYTE_MS;
        }
        return;
    }
}

static void host_handle_error(I2CDriver* drv) {
    if (I2C1ERRbits.NACKIF) {
        I2C1ERRbits.NACKIF = 0;
        host_complete(drv, I2C_STATUS_NACK);
        return;
    }
    if (I2C1ERRbits.BCLIF) {
        I2C1ERRbits.BCLIF = 0;
        host_complete(drv, I2C_STATUS_BUS_BUSY);
        return;
    }
}

/* ── Client (slave) ISR helpers ─────────────────────────────────────── */

static void client_handle_event(I2CDriver* drv) {
    if (I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        I2C1STAT1bits.CLRBF = 1;
        I2C1CNTL = 0;
        I2C1CNTH = 0;
        I2C1CON1bits.ACKDT = 0;

        if (drv->client_state == I2C_CLIENT_RX && drv->client_rx_len > 0 && drv->client_rx_cb) {
            drv->client_rx_cb((const uint8_t*)drv->client_rx_buf, drv->client_rx_len);
        }
        client_reset(drv);
        try_dispatch(drv);
    } else if (I2C1PIRbits.ADRIF) {
        I2C1PIRbits.ADRIF = 0;

        if (I2C1STAT0bits.R) {
            /* Read request from master — build response now. */
            drv->client_tx_pos = 0;
            drv->client_tx_len = 0;
            if (drv->client_read_cb) {
                drv->client_tx_len = drv->client_read_cb((const uint8_t*)drv->client_rx_buf, drv->client_rx_len,
                                                         (uint8_t*)drv->client_tx_buf, I2C_CLIENT_BUF_SIZE);
            }
            drv->client_state = I2C_CLIENT_TX;
        } else {
            drv->client_rx_len = 0;
            drv->client_state = I2C_CLIENT_RX;
        }
    } else if (I2C1STAT0bits.R) {
        if (I2C1STAT1bits.TXBE && !I2C1CON1bits.ACKSTAT) {
            I2C1TXB = (drv->client_tx_pos < drv->client_tx_len) ? drv->client_tx_buf[drv->client_tx_pos++] : 0;
        }
    } else {
        if (I2C1STAT1bits.RXBF) {
            uint8_t b = I2C1RXB;
            if (drv->client_rx_len < I2C_CLIENT_BUF_SIZE) {
                drv->client_rx_buf[drv->client_rx_len++] = b;
            }
            I2C1CON1bits.ACKDT = 0;
            I2C1PIRbits.ACKTIF = 0;
        }
    }

    I2C1CON0bits.CSTR = 0;
}

static void client_handle_error(I2CDriver* drv) {
    if (I2C1ERRbits.BCLIF) {
        I2C1ERRbits.BCLIF = 0;
    }
    if (I2C1STAT1bits.TXWE) {
        I2C1STAT1bits.TXWE = 0;
    }
    if (I2C1CON1bits.RXO) {
        I2C1CON1bits.RXO = 0;
    }
    if (I2C1CON1bits.TXU) {
        I2C1CON1bits.TXU = 0;
    }
    if (I2C1STAT1bits.RXRE) {
        I2C1STAT1bits.RXRE = 0;
    }
    I2C1ERRbits.NACKIF = 0;

    client_reset(drv);
    I2C1STAT1bits.CLRBF = 1;
    I2C1CON0bits.CSTR = 0;
}

/* ── Tick — ms deadline & back-off ──────────────────────────────────── */

void i2c_tick_ms(I2CDriver* drv) {
    if (drv->deadline_ms == 0) {
        return;
    }
    if (--drv->deadline_ms != 0) {
        return;
    }
    if (drv->host_state == I2C_HOST_BACKOFF) {
        drv->host_state = I2C_HOST_IDLE;
        return;
    }
    if (drv->host_state != I2C_HOST_IDLE) {
        host_complete(drv, I2C_STATUS_TIMEOUT);
    }
}

/* ── Mode enable / disable ──────────────────────────────────────────── */

static void client_mode_enable(I2CDriver* drv) {
    I2C1CON0bits.EN = 0;
    I2C1CON0bits.MODE = 0b000; /* 7-bit client */

    I2C1CON1 = 0x00;
    I2C1CON1bits.CSD = 1;
    I2C1CON2 = 0x00;

    I2C1CNTL = 0;
    I2C1CNTH = 0;

    const uint8_t a = (uint8_t)(drv->client_addr << 1);
    I2C1ADR0 = a;
    I2C1ADR1 = a;
    I2C1ADR2 = a;
    I2C1ADR3 = a;

    I2C1STAT1bits.CLRBF = 1;
    I2C1PIRbits.PCIF = 0;
    I2C1PIRbits.ADRIF = 0;
    I2C1PIRbits.ACKTIF = 0;
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    client_reset(drv);

    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;
    PIE7bits.I2C1RXIE = 1;
    PIE7bits.I2C1TXIE = 1;
    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.ADRIE = 1;
    I2C1ERRbits.NACKIE = 1;

    I2C1CON0bits.EN = 1;
}

static void client_mode_disable(void) {
    I2C1CON0bits.EN = 0;
}

static void client_reset(I2CDriver* drv) {
    drv->client_state = I2C_CLIENT_IDLE;
    drv->client_rx_len = 0;
    drv->client_tx_len = 0;
    drv->client_tx_pos = 0;
}
