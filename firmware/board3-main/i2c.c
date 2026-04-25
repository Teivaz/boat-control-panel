#define _XTAL_FREQ 64000000UL

#include "i2c.h"

#include "libcomm.h"

/* ============================================================================
 * Async, interrupt-driven I2C driver for the main board.
 *
 * Public API: i2c_submit() enqueues a write or write-then-read transaction;
 * a completion callback fires from main context when it lands. Slave-side
 * (client mode) handlers behave exactly as before — registered via
 * i2c_set_rx_handler / i2c_set_read_handler.
 *
 * Designed to be liftable into libcomm later: no board-specific includes
 * (only libcomm.h for comm_address()), no DEVICE_TYPE checks, no peripheral
 * other than I2C1 plus the externally-driven ms tick.
 * ============================================================================
 */

/* ---------- Slave-side state -------------------------------------------- */

#define I2C_BUF_SIZE 8 /* longest inbound: relay_changed (1+7) */

typedef enum {
    SLAVE_IDLE,
    SLAVE_RX,
    SLAVE_TX,
} SlaveState;

static volatile SlaveState slave_state;
static volatile uint8_t slave_rx_buf[I2C_BUF_SIZE];
static volatile uint8_t slave_rx_len;
static volatile uint8_t slave_tx_buf[I2C_BUF_SIZE];
static volatile uint8_t slave_tx_len;
static volatile uint8_t slave_tx_pos;

static I2cRxHandler rx_handler;
static I2cReadHandler read_handler;

/* ---------- Host-side state --------------------------------------------- */

#define TRANSMIT_QUEUE_SIZE 8
#define TRANSMIT_QUEUE_MASK (TRANSMIT_QUEUE_SIZE - 1)
#define I2C_MAX_ATTEMPTS 3

#define I2C_DEADLINE_BUS_FREE_MS 10
#define I2C_DEADLINE_BYTE_MS 5
#define I2C_DEADLINE_STOP_MS 10
#define I2C_BACKOFF_MS 2

typedef enum {
    HOST_IDLE = 0,
    HOST_TX,
    HOST_RX,
    HOST_STOP,
    HOST_BACKOFF,
} HostState;

typedef struct {
    uint8_t address;
    uint8_t tx_len;
    uint8_t rx_len;
    uint8_t tx[I2C_TX_MAX];
    uint8_t* rx_buf;
    I2cCompletion cb;
    void* ctx;
    uint8_t attempts;
} TransmitTask;

static TransmitTask queue[TRANSMIT_QUEUE_SIZE];
static volatile uint8_t q_head; /* consumer */
static volatile uint8_t q_tail; /* producer */

static volatile HostState host_state;
static volatile uint8_t tx_idx;
static volatile uint8_t rx_idx;
static volatile uint16_t deadline_ms; /* 0 = inactive */
static volatile uint8_t completion_pending; /* run_in_main_loop in flight */
static volatile I2cResult pending_result;
static volatile uint8_t pending_rx_len;

static TaskController* g_ctrl;

/* ---------- Forward decls ----------------------------------------------- */

static void client_mode_enable(void);
static void client_mode_disable(void);
static void slave_reset(void);
static void slave_handle_event(void);
static void slave_handle_error(void);

static void try_dispatch(void);
static void start_transaction(TransmitTask* t);
static void start_read_phase(TransmitTask* t);
static void host_complete(I2cResult r);
static void dispatch_completion(void* ctx);
static void host_handle_event(void);
static void host_handle_error(void);

/* ============================================================================
 * Initialisation
 * ============================================================================
 */

void i2c_init(TaskController* ctrl) {
    g_ctrl = ctrl;

    q_head = q_tail = 0;
    host_state = HOST_IDLE;
    deadline_ms = 0;
    completion_pending = 0;

    /* RB1 = SDA, RB2 = SCL. Open-drain, I2C-specific slew + weak pulls. */
    LATBbits.LATB1 = 1;
    LATBbits.LATB2 = 1;
    ANSELBbits.ANSELB1 = 0;
    ANSELBbits.ANSELB2 = 0;
    TRISBbits.TRISB1 = 0;
    TRISBbits.TRISB2 = 0;
    ODCONBbits.ODCB1 = 1;
    ODCONBbits.ODCB2 = 1;
    RB1I2Cbits.TH = 0b01;
    RB2I2Cbits.TH = 0b01;
    RB1I2Cbits.PU = 0b10;
    RB2I2Cbits.PU = 0b10;
    RB1I2Cbits.SLEW = 0b01;
    RB2I2Cbits.SLEW = 0b01;

    I2C1SCLPPS = 0x0A; /* RB2 -> SCL1 */
    RB2PPS = 0x37;
    I2C1SDAPPS = 0x09; /* RB1 -> SDA1 */
    RB1PPS = 0x38;

    I2C1CLK = 0x01;
    I2C1BAUD = 31; /* 400 kHz @ Fosc=64 MHz */

    client_mode_enable();
}

void i2c_set_rx_handler(I2cRxHandler h) {
    rx_handler = h;
}

void i2c_set_read_handler(I2cReadHandler h) {
    read_handler = h;
}

/* ============================================================================
 * Submit / dispatch
 * ============================================================================
 */

I2cResult i2c_submit(uint8_t address, const uint8_t* tx, uint8_t tx_len, uint8_t* rx_buf, uint8_t rx_len,
                     I2cCompletion cb, void* ctx) {
    if (tx_len == 0 || tx_len > I2C_TX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    if (rx_len > 0 && rx_buf == 0) {
        return I2C_RESULT_BAD_ARG;
    }

    INTERRUPT_PUSH;
    uint8_t next = (uint8_t)((q_tail + 1) & TRANSMIT_QUEUE_MASK);
    if (next == q_head) {
        INTERRUPT_POP;
        return I2C_RESULT_QUEUE_FULL;
    }
    TransmitTask* t = &queue[q_tail];
    t->address = address;
    t->tx_len = tx_len;
    t->rx_len = rx_len;
    t->rx_buf = rx_buf;
    t->cb = cb;
    t->ctx = ctx;
    t->attempts = 0;
    for (uint8_t i = 0; i < tx_len; i++) {
        t->tx[i] = tx[i];
    }
    q_tail = next;
    try_dispatch();
    INTERRUPT_POP;
    return I2C_RESULT_OK;
}

/* Caller must have interrupts masked. */
static void try_dispatch(void) {
    if (host_state != HOST_IDLE) {
        return;
    }
    if (completion_pending) {
        /* Wait until the previous completion callback has been picked up
         * by the main loop; this caps us at one outstanding deferred
         * entry, which the libcomm queue (size 4) can always absorb. */
        return;
    }
    if (q_head == q_tail) {
        return;
    }
    if (slave_state != SLAVE_IDLE) {
        /* A slave transfer is in progress; the host_state will be picked
         * up again when slave PCIF returns to IDLE. */
        return;
    }
    start_transaction(&queue[q_head]);
}

/* Configure peripheral and kick off the write phase. */
static void start_transaction(TransmitTask* t) {
    /* Disable slave reception, switch to host mode. */
    PIE7bits.I2C1IE = 0;
    PIE7bits.I2C1EIE = 0;
    PIE7bits.I2C1RXIE = 0;
    PIE7bits.I2C1TXIE = 0;
    client_mode_disable();

    /* Bus-free check. BFRE=1 means no start/stop framing in progress. */
    if (!I2C1STAT0bits.BFRE) {
        /* Another master is using the bus — back off. */
        client_mode_enable();
        host_state = HOST_BACKOFF;
        deadline_ms = I2C_BACKOFF_MS;
        return;
    }

    t->attempts++;
    tx_idx = 0;
    rx_idx = 0;

    I2C1CON0bits.MODE = 0b100; /* host 7-bit */
    I2C1CON0bits.RSEN = (t->rx_len > 0) ? 1 : 0;
    I2C1CNTH = 0;
    I2C1CNTL = t->tx_len;
    I2C1ADB1 = (uint8_t)(t->address << 1); /* W */
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1STAT1bits.CLRBF = 1;

    /* Pre-load first byte so TXBE is low when the address phase ends. */
    I2C1TXB = t->tx[tx_idx++];

    /* Enable host interrupts. */
    I2C1PIEbits.PCIE = 1;
    I2C1ERRbits.NACKIE = 1;
    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;
    PIE7bits.I2C1RXIE = 1;
    PIE7bits.I2C1TXIE = 1;

    host_state = HOST_TX;
    deadline_ms = I2C_DEADLINE_BYTE_MS;

    I2C1CON0bits.EN = 1;
    I2C1CON0bits.S = 1; /* issue start */
}

/* Reconfigure for repeated-start read after the write phase has drained. */
static void start_read_phase(TransmitTask* t) {
    I2C1CON0bits.RSEN = 0;
    I2C1CNTH = 0;
    I2C1CNTL = t->rx_len;
    I2C1ADB1 = (uint8_t)((t->address << 1) | 0x01); /* R */
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    host_state = HOST_RX;
    deadline_ms = I2C_DEADLINE_BYTE_MS;
    I2C1CON0bits.S = 1;
}

/* Schedule callback (or retry/backoff), drop task from queue, dispatch next.
 * Caller is the I2C ISR. */
static void host_complete(I2cResult r) {
    TransmitTask* t = &queue[q_head];

    /* Retry transient errors. */
    if ((r == I2C_RESULT_BUSY || r == I2C_RESULT_TIMEOUT) && t->attempts < I2C_MAX_ATTEMPTS) {
        /* Tear the host setup down; back off then retry. */
        I2C1CON0bits.EN = 0;
        client_mode_enable();
        host_state = HOST_BACKOFF;
        deadline_ms = I2C_BACKOFF_MS;
        return;
    }

    /* Final outcome — schedule the user callback. */
    deadline_ms = 0;
    pending_result = r;
    pending_rx_len = (r == I2C_RESULT_OK) ? t->rx_len : 0;

    /* Restore client mode immediately so slave traffic can resume. */
    I2C1CON0bits.EN = 0;
    client_mode_enable();

    if (t->cb) {
        completion_pending = 1;
        (void)run_in_main_loop(g_ctrl, dispatch_completion, &queue[q_head]);
    } else {
        /* No callback — drop the task right away. */
        q_head = (uint8_t)((q_head + 1) & TRANSMIT_QUEUE_MASK);
    }

    host_state = HOST_IDLE;
    try_dispatch();
}

/* Main-context completion fan-out. */
static void dispatch_completion(void* ctx) {
    TransmitTask* t = (TransmitTask*)ctx;
    I2cCompletion cb = t->cb;
    void* user_ctx = t->ctx;
    uint8_t* rx_buf = t->rx_buf;

    INTERRUPT_PUSH;
    I2cResult r = pending_result;
    uint8_t rx_len = pending_rx_len;
    q_head = (uint8_t)((q_head + 1) & TRANSMIT_QUEUE_MASK);
    completion_pending = 0;
    /* Kick the next task if one was queued while we were waiting. */
    try_dispatch();
    INTERRUPT_POP;

    if (cb) {
        cb(r, rx_buf, rx_len, user_ctx);
    }
}

/* ============================================================================
 * ISR fan-out
 *
 * The I2C peripheral shares one interrupt group across TX/RX/main events;
 * dispatch by current host_state — when HOST_IDLE the slave path runs.
 * ============================================================================
 */

/* Dispatch by current peripheral mode: client (0b000) → slave path,
 * host (0b100) → host FSM. host_state alone isn't sufficient because
 * HOST_BACKOFF leaves the peripheral in client mode while the FSM is
 * "busy" — slave events during backoff must still be handled. */

void __interrupt(irq(I2C1TX, I2C1RX, I2C1), base(8)) I2C1_ISR(void) {
    if (I2C1CON0bits.MODE == 0b000) {
        slave_handle_event();
    } else {
        host_handle_event();
    }
}

void __interrupt(irq(I2C1E), base(8)) I2C1_ERROR_ISR(void) {
    if (I2C1CON0bits.MODE == 0b000) {
        slave_handle_error();
    } else {
        host_handle_error();
    }
}

/* ============================================================================
 * Host-side ISR
 * ============================================================================
 */

static void host_handle_event(void) {
    TransmitTask* t = &queue[q_head];

    if (I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        if (host_state == HOST_STOP || host_state == HOST_RX) {
            /* Final stop — read phase may have finished. */
            host_complete(I2C_RESULT_OK);
        } else {
            /* Unexpected stop in TX → treat as collision. */
            host_complete(I2C_RESULT_BUSY);
        }
        return;
    }

    if (host_state == HOST_TX && I2C1STAT1bits.TXBE) {
        if (tx_idx < t->tx_len) {
            I2C1TXB = t->tx[tx_idx++];
            deadline_ms = I2C_DEADLINE_BYTE_MS;
        } else {
            /* All write bytes drained. Either repeat-start read or stop. */
            if (t->rx_len > 0) {
                start_read_phase(t);
            } else {
                host_state = HOST_STOP;
                deadline_ms = I2C_DEADLINE_STOP_MS;
            }
        }
        return;
    }

    if (host_state == HOST_RX && I2C1STAT1bits.RXBF) {
        uint8_t byte = I2C1RXB;
        if (rx_idx < t->rx_len) {
            t->rx_buf[rx_idx++] = byte;
        }
        if (rx_idx >= t->rx_len) {
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

/* ============================================================================
 * ms tick — drives deadlines + backoff. ISR context (TMR0).
 * ============================================================================
 */

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

/* ============================================================================
 * Slave-side ISR (verbatim from the previous polled driver — proven path).
 * ============================================================================
 */

static void slave_handle_event(void) {
    if (I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        I2C1STAT1bits.CLRBF = 1;
        I2C1CNTL = 0;
        I2C1CNTH = 0;
        I2C1CON1bits.ACKDT = 0;

        if (slave_state == SLAVE_RX && slave_rx_len > 0 && rx_handler) {
            rx_handler((const uint8_t*)slave_rx_buf, slave_rx_len);
        }
        slave_reset();
        /* A host transaction may have been queued while the slave was busy. */
        try_dispatch();
    } else if (I2C1PIRbits.ADRIF) {
        I2C1PIRbits.ADRIF = 0;

        if (I2C1STAT0bits.R) {
            /* Read request: build response now. */
            slave_tx_pos = 0;
            slave_tx_len = 0;
            if (read_handler) {
                slave_tx_len = read_handler((const uint8_t*)slave_rx_buf, slave_rx_len, (uint8_t*)slave_tx_buf,
                                            I2C_BUF_SIZE);
            }
            slave_state = SLAVE_TX;
        } else {
            slave_rx_len = 0;
            slave_state = SLAVE_RX;
        }
    } else if (I2C1STAT0bits.R) {
        if (I2C1STAT1bits.TXBE && !I2C1CON1bits.ACKSTAT) {
            I2C1TXB = (slave_tx_pos < slave_tx_len) ? slave_tx_buf[slave_tx_pos++] : 0;
        }
    } else {
        if (I2C1STAT1bits.RXBF) {
            uint8_t byte = I2C1RXB;
            if (slave_rx_len < I2C_BUF_SIZE) {
                slave_rx_buf[slave_rx_len++] = byte;
            }
            I2C1CON1bits.ACKDT = 0;
            I2C1PIRbits.ACKTIF = 0;
        }
    }

    I2C1CON0bits.CSTR = 0;
}

static void slave_handle_error(void) {
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

    slave_reset();
    I2C1STAT1bits.CLRBF = 1;
    I2C1CON0bits.CSTR = 0;
}

/* ============================================================================
 * Bus recovery — bit-bang up to 9 SCL pulses + manual STOP.
 * Main context only; restores client mode on return.
 * ============================================================================
 */

#define I2C_RECOVER_HALF_US 5

void i2c_bus_recover(void) {
    INTERRUPT_PUSH;
    uint8_t pie7_saved = PIE7;
    PIE7bits.I2C1IE = 0;
    PIE7bits.I2C1EIE = 0;
    PIE7bits.I2C1RXIE = 0;
    PIE7bits.I2C1TXIE = 0;

    client_mode_disable();

    RB1PPS = 0x00;
    RB2PPS = 0x00;

    LATBbits.LATB1 = 1;
    LATBbits.LATB2 = 1;
    __delay_us(I2C_RECOVER_HALF_US);

    if (PORTBbits.RB1 == 0) {
        for (uint8_t i = 0; i < 9 && PORTBbits.RB1 == 0; i++) {
            LATBbits.LATB2 = 0;
            __delay_us(I2C_RECOVER_HALF_US);
            LATBbits.LATB2 = 1;
            __delay_us(I2C_RECOVER_HALF_US);
        }
        LATBbits.LATB1 = 0;
        __delay_us(I2C_RECOVER_HALF_US);
        LATBbits.LATB1 = 1;
        __delay_us(I2C_RECOVER_HALF_US);
    }

    RB2PPS = 0x37;
    RB1PPS = 0x38;

    client_mode_enable();
    PIE7 = pie7_saved;
    INTERRUPT_POP;
}

/* ============================================================================
 * Internal helpers
 * ============================================================================
 */

static void client_mode_enable(void) {
    I2C1CON0bits.EN = 0;
    I2C1CON0bits.MODE = 0b000;

    I2C1CON1 = 0x00;
    I2C1CON1bits.CSD = 1;
    I2C1CON2 = 0x00;

    I2C1CNTL = 0;
    I2C1CNTH = 0;

    const uint8_t addr = comm_address();
    I2C1ADR0 = (uint8_t)(addr << 1);
    I2C1ADR1 = (uint8_t)(addr << 1);
    I2C1ADR2 = (uint8_t)(addr << 1);
    I2C1ADR3 = (uint8_t)(addr << 1);

    I2C1STAT1bits.CLRBF = 1;
    I2C1PIRbits.PCIF = 0;
    I2C1PIRbits.ADRIF = 0;
    I2C1PIRbits.ACKTIF = 0;
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    slave_reset();

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

static void slave_reset(void) {
    slave_state = SLAVE_IDLE;
    slave_rx_len = 0;
    slave_tx_len = 0;
    slave_tx_pos = 0;
}
