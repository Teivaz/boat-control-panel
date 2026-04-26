/*
 * i2c.c — Shared I2C multi-master driver implementation.
 *
 * See i2c.h for the design overview and API contract.
 *
 * Pure byte-level interrupts, no DMA.  Protocol payloads are small
 * enough (<=8 bytes) that per-byte ISR overhead is negligible at
 * 400 kHz.  Pin/oscillator setup is the caller's job; only the I2C1
 * module itself is programmed here.
 */

#include "i2c.h"

#include <xc.h>

/* ── Internal constants ────────────────────────────────────────────── */

/* Fosc / (4 * (BAUD + 1)) = 400 kHz at Fosc = 64 MHz → BAUD = 39.
 * Override before compiling if the bus needs a different rate. */
#ifndef I2C_BAUD
#define I2C_BAUD 39
#endif

#define I2C_DEADLINE_BYTE_MS 5
#define I2C_DEADLINE_STOP_MS 10
#define I2C_BACKOFF_MS 2

/* ── Queue helpers ─────────────────────────────────────────────────── */

#define Q_MASK (I2C_QUEUE_SIZE - 1u)

static inline uint8_t q_next(uint8_t idx) {
    return (uint8_t)((idx + 1u) & Q_MASK);
}

/* ── Internal types (file-scope only) ──────────────────────────────── */

typedef enum {
    HOST_IDLE = 0,
    HOST_TX,
    HOST_RX,
    HOST_STOP,
    HOST_BACKOFF,
} HostState;

typedef enum {
    CLIENT_IDLE = 0,
    CLIENT_RX,
    CLIENT_TX,
} ClientState;

typedef struct {
    uint8_t addr;
    uint8_t tx_len;
    uint8_t rx_len;
    uint8_t tx[I2C_TX_MAX];
    uint8_t* rx_buf;
    I2cCompletion cb;
    void* cb_ctx;
    uint8_t retries;
    volatile I2cResult result;
    volatile uint8_t done;
} I2cOp;

/* ── Static state ──────────────────────────────────────────────────── */

/* Host queue + FSM */
static I2cOp queue[I2C_QUEUE_SIZE];
static volatile uint8_t q_head;
static volatile uint8_t q_tail;
static volatile uint8_t host_state;     /* HostState */
static volatile uint8_t tx_idx;
static volatile uint8_t rx_idx;
static volatile uint16_t deadline_ms;

/* Client config */
static uint8_t client_addr;
static I2cRxHandler client_rx_cb;
static I2cReadHandler client_read_cb;

/* Client FSM + buffers */
static volatile uint8_t client_state;   /* ClientState */
static volatile uint8_t client_rx_len;
static volatile uint8_t client_tx_len;
static volatile uint8_t client_tx_pos;
static volatile uint8_t client_rx_buf[I2C_CLIENT_BUF_SIZE];
static volatile uint8_t client_tx_buf[I2C_CLIENT_BUF_SIZE];

/* ── Forward declarations ──────────────────────────────────────────── */

static void client_mode_enable(void);
static void client_mode_disable(void);
static void client_reset(void);
static void client_handle_event(void);
static void client_handle_error(void);

static void try_dispatch(void);
static void start_transaction(I2cOp* op);
static void start_read_phase(I2cOp* op);
static void host_complete(I2cResult r);
static void host_handle_event(void);
static void host_handle_error(void);

/* ── Public API: handler setters ───────────────────────────────────── */

void i2c_set_rx_handler(I2cRxHandler h) {
    client_rx_cb = h;
}

void i2c_set_read_handler(I2cReadHandler h) {
    client_read_cb = h;
}

/* ── Public API: init ──────────────────────────────────────────────── */

void i2c_init(uint8_t addr) {
    q_head = 0;
    q_tail = 0;
    host_state = HOST_IDLE;
    tx_idx = 0;
    rx_idx = 0;
    deadline_ms = 0;
    client_state = CLIENT_IDLE;
    client_rx_len = 0;
    client_tx_len = 0;
    client_tx_pos = 0;

    client_addr = addr;

    I2C1CON0bits.EN = 0;
    I2C1CLK = 0x01;        /* FOSC */
    I2C1BAUD = I2C_BAUD;

    if (client_addr != 0) {
        client_mode_enable();
    } else {
        I2C1CON0bits.MODE = 0b100; /* host 7-bit */
    }
}

/* ── Public API: submit ────────────────────────────────────────────── */

I2cResult i2c_submit(uint8_t address, const uint8_t* tx, uint8_t tx_len, uint8_t* rx_buf, uint8_t rx_len,
                     I2cCompletion cb, void* ctx) {
    if (tx_len == 0 || tx_len > I2C_TX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    if (rx_len > 0 && rx_buf == 0) {
        return I2C_RESULT_BAD_ARG;
    }

    GIE = 0;
    if (q_next(q_tail) == q_head) {
        GIE = 1;
        return I2C_RESULT_QUEUE_FULL;
    }

    I2cOp* op = &queue[q_tail];
    op->addr = address;
    op->tx_len = tx_len;
    op->rx_len = rx_len;
    op->rx_buf = rx_buf;
    op->cb = cb;
    op->cb_ctx = ctx;
    op->retries = I2C_RETRY_COUNT;
    op->result = I2C_RESULT_OK;
    op->done = 0;
    for (uint8_t i = 0; i < tx_len; i++) {
        op->tx[i] = tx[i];
    }

    q_tail = q_next(q_tail);
    GIE = 1;
    return I2C_RESULT_OK;
}

/* ── Public API: poll ──────────────────────────────────────────────── */

void i2c_poll(void) {
    /* Phase 1 — deliver completion for the head op. */
    if (host_state == HOST_IDLE && q_head != q_tail) {
        I2cOp* op = &queue[q_head];
        if (op->done) {
            I2cResult r = op->result;
            uint8_t actual_rx = (r == I2C_RESULT_OK) ? op->rx_len : 0;
            if (op->cb) {
                op->cb(r, op->rx_buf, actual_rx, op->cb_ctx);
            }
            GIE = 0;
            q_head = q_next(q_head);
            GIE = 1;
        }
    }

    /* Phase 2 — start next op if bus is free. */
    if (host_state == HOST_IDLE && q_head != q_tail) {
        try_dispatch();
    }
}

/* ── Dispatch & start ──────────────────────────────────────────────── */

static void try_dispatch(void) {
    if (host_state != HOST_IDLE) {
        return;
    }
    if (q_head == q_tail) {
        return;
    }
    if (client_state != CLIENT_IDLE) {
        return;
    }
    I2cOp* op = &queue[q_head];
    start_transaction(op);
}

static void start_transaction(I2cOp* op) {
    if (client_addr != 0 && !I2C1STAT0bits.BFRE) {
        host_state = HOST_BACKOFF;
        deadline_ms = I2C_BACKOFF_MS;
        return;
    }

    op->retries--;

    PIE7bits.I2C1IE = 0;
    PIE7bits.I2C1EIE = 0;
    PIE7bits.I2C1RXIE = 0;
    PIE7bits.I2C1TXIE = 0;

    client_mode_disable();
    tx_idx = 0;
    rx_idx = 0;

    I2C1CON0bits.MODE = 0b100;
    I2C1CON0bits.RSEN = (op->rx_len > 0) ? 1 : 0;
    I2C1CNTH = 0;
    I2C1CNTL = op->tx_len;
    I2C1ADB1 = (uint8_t)(op->addr << 1);
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

    host_state = HOST_TX;
    deadline_ms = I2C_DEADLINE_BYTE_MS;

    I2C1CON0bits.EN = 1;
    I2C1CON0bits.S = 1;
}

static void start_read_phase(I2cOp* op) {
    I2C1CON0bits.RSEN = 0;
    I2C1CNTH = 0;
    I2C1CNTL = op->rx_len;
    I2C1ADB1 = (uint8_t)((op->addr << 1) | 0x01);
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    host_state = HOST_RX;
    deadline_ms = I2C_DEADLINE_BYTE_MS;
    I2C1CON0bits.S = 1;
}

static void host_complete(I2cResult r) {
    I2cOp* op = &queue[q_head];

    if ((r == I2C_RESULT_BUSY || r == I2C_RESULT_TIMEOUT) && op->retries > 0) {
        /* Retry: re-arm and back off. */
        I2C1CON0bits.EN = 0;
        if (client_addr != 0) {
            client_mode_enable();
        }
        host_state = HOST_BACKOFF;
        deadline_ms = I2C_BACKOFF_MS;
        return;
    }

    deadline_ms = 0;
    op->result = r;
    op->done = 1;

    I2C1CON0bits.EN = 0;
    if (client_addr != 0) {
        client_mode_enable();
    }
    host_state = HOST_IDLE;
}

/* ── ISR entry points ──────────────────────────────────────────────── */

void i2c_isr(void) {
    if (I2C1CON0bits.MODE == 0b000) {
        client_handle_event();
    } else {
        host_handle_event();
    }
}

void i2c_error_isr(void) {
    if (I2C1CON0bits.MODE == 0b000) {
        client_handle_error();
    } else {
        host_handle_error();
    }
}

/* ── Host ISR ──────────────────────────────────────────────────────── */

static void host_handle_event(void) {
    if (host_state == HOST_IDLE || host_state == HOST_BACKOFF) {
        I2C1PIRbits.PCIF = 0;
        return;
    }
    I2cOp* op = &queue[q_head];

    if (I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        if (host_state == HOST_STOP || host_state == HOST_RX) {
            host_complete(I2C_RESULT_OK);
        } else {
            host_complete(I2C_RESULT_BUSY);
        }
        return;
    }

    if (host_state == HOST_TX && I2C1STAT1bits.TXBE) {
        if (tx_idx < op->tx_len) {
            I2C1TXB = op->tx[tx_idx++];
            deadline_ms = I2C_DEADLINE_BYTE_MS;
        } else if (op->rx_len > 0) {
            start_read_phase(op);
        } else {
            host_state = HOST_STOP;
            deadline_ms = I2C_DEADLINE_STOP_MS;
        }
        return;
    }

    if (host_state == HOST_RX && I2C1STAT1bits.RXBF) {
        uint8_t b = I2C1RXB;
        if (rx_idx < op->rx_len) {
            op->rx_buf[rx_idx++] = b;
        }
        if (rx_idx >= op->rx_len) {
            host_state = HOST_STOP;
            deadline_ms = I2C_DEADLINE_STOP_MS;
        } else {
            deadline_ms = I2C_DEADLINE_BYTE_MS;
        }
        return;
    }
}

static void host_handle_error(void) {
    if (I2C1ERRbits.NACKIF) {
        I2C1ERRbits.NACKIF = 0;
        host_complete(I2C_RESULT_NACK);
        return;
    }
    if (I2C1ERRbits.BCLIF) {
        I2C1ERRbits.BCLIF = 0;
        host_complete(I2C_RESULT_BUSY);
        return;
    }
}

/* ── Client ISR ────────────────────────────────────────────────────── */

static void client_handle_event(void) {
    if (I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        I2C1STAT1bits.CLRBF = 1;
        I2C1CNTL = 0;
        I2C1CNTH = 0;
        I2C1CON1bits.ACKDT = 0;

        if (client_state == CLIENT_RX && client_rx_len > 0 && client_rx_cb) {
            client_rx_cb((const uint8_t*)client_rx_buf, client_rx_len);
        }
        client_reset();
        try_dispatch();
    } else if (I2C1PIRbits.ADRIF) {
        I2C1PIRbits.ADRIF = 0;

        if (I2C1STAT0bits.R) {
            client_tx_pos = 0;
            client_tx_len = 0;
            if (client_read_cb) {
                client_tx_len = client_read_cb((const uint8_t*)client_rx_buf, client_rx_len,
                                               (uint8_t*)client_tx_buf, I2C_CLIENT_BUF_SIZE);
            }
            client_state = CLIENT_TX;
        } else {
            client_rx_len = 0;
            client_state = CLIENT_RX;
        }
    } else if (I2C1STAT0bits.R) {
        if (I2C1STAT1bits.TXBE && !I2C1CON1bits.ACKSTAT) {
            I2C1TXB = (client_tx_pos < client_tx_len) ? client_tx_buf[client_tx_pos++] : 0;
        }
    } else {
        if (I2C1STAT1bits.RXBF) {
            uint8_t b = I2C1RXB;
            if (client_rx_len < I2C_CLIENT_BUF_SIZE) {
                client_rx_buf[client_rx_len++] = b;
            }
            I2C1CON1bits.ACKDT = 0;
            I2C1PIRbits.ACKTIF = 0;
        }
    }

    I2C1CON0bits.CSTR = 0;
}

static void client_handle_error(void) {
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

    client_reset();
    I2C1STAT1bits.CLRBF = 1;
    I2C1CON0bits.CSTR = 0;
}

/* ── Tick ──────────────────────────────────────────────────────────── */

void i2c_tick_ms(void) {
    if (deadline_ms == 0) {
        return;
    }
    if (--deadline_ms != 0) {
        return;
    }
    if (host_state == HOST_BACKOFF) {
        host_state = HOST_IDLE;
        try_dispatch();
        return;
    }
    if (host_state != HOST_IDLE) {
        host_complete(I2C_RESULT_TIMEOUT);
    }
}

/* ── Internal helpers ──────────────────────────────────────────────── */

static void client_mode_enable(void) {
    I2C1CON0bits.EN = 0;
    I2C1CON0bits.MODE = 0b000;

    I2C1CON1 = 0x00;
    I2C1CON1bits.CSD = 1;
    I2C1CON2 = 0x00;

    I2C1CNTL = 0;
    I2C1CNTH = 0;

    const uint8_t a = (uint8_t)(client_addr << 1);
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
    client_reset();

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

static void client_reset(void) {
    client_state = CLIENT_IDLE;
    client_rx_len = 0;
    client_tx_len = 0;
    client_tx_pos = 0;
}
