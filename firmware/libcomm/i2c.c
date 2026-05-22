#include "i2c.h"

#include "libcomm.h"

#define _XTAL_FREQ 64000000UL

#include <xc.h>

#define DMA_TX_CHANNEL 1 /* DMA2 */
#define DMA_RX_CHANNEL 2 /* DMA3 */

#define Q_MASK (I2C_QUEUE_SIZE - 1u)

static inline uint8_t q_next(uint8_t idx) {
    return (uint8_t)((idx + 1u) & Q_MASK);
}

static inline uint8_t q_prev(uint8_t idx) {
    return (uint8_t)((idx + Q_MASK) & Q_MASK);
}

typedef enum {
    MT_IDLE,
    MT_RUNNING,
    MT_FINISHED,
    MT_FAILED,
} MessageTaskState;

typedef struct {
    uint8_t addr;
    uint8_t tx[I2C_TX_MAX];
    uint8_t tx_len;
    uint8_t rx[I2C_RX_MAX];
    uint8_t rx_len;
    MessageTaskState state;
    I2cResult result;
    uint8_t retries;
    uint8_t tx_done;  /* 1 once write phase has completed, for log phase */
    I2cCompletion cb;
} MessageTask;

typedef enum {
    FSM_IDLE,
    FSM_HOST_TX,
    FSM_HOST_RX,
    FSM_CLIENT_TX,
    FSM_CLIENT_RX,
} FSMState;

// TODO: use only one callback
static I2cCompletion g_cold_rx = 0;
static I2cSyncColdRxHandler g_sync_cold_rx = 0;

#define LOG_CAPACITY 8u
#define LOG_MASK     (LOG_CAPACITY - 1u)

static I2cLogEntry g_log[LOG_CAPACITY];
static uint8_t g_log_head = 0;
static uint8_t g_log_count = 0;

static volatile FSMState g_fsm = FSM_IDLE;
static volatile uint8_t g_client_rx[I2C_RX_MAX] = {0};
static volatile uint8_t g_client_tx[I2C_TX_MAX] = {0};
static volatile uint8_t g_client_tx_len = 0;
static MessageTask g_queue[I2C_QUEUE_SIZE] = {0};
static volatile uint8_t g_q_head = 0;
static volatile uint8_t g_q_tail = 0;

static void i2c_dma_init(void);
static void i2c_dma_set_host(MessageTask* task);
static void i2c_dma_client_rx(void);
static void i2c_dma_client_tx(void);
static void prepend_completed_task(uint8_t addr, const volatile uint8_t* rx, uint8_t rx_len);
static void isr_on_address(void);
static void isr_on_stop(void);
static void isr_on_restart(void);
static void isr_on_transmit_exhausted(void);
static void isr_on_nack(void);
static void isr_on_collision(void);
static void isr_on_timeout(void);
static void on_cold_rx_complete(void);
static void switch_to_host(void);
static FSMState arm_event(void);
static void switch_to_client(void);
static void disarm_event(I2cResult reason);

void i2c_set_cold_rx_handler(I2cCompletion cold_rx) {
    g_cold_rx = cold_rx;
}

void i2c_set_sync_cold_rx_handler(I2cSyncColdRxHandler handler) {
    g_sync_cold_rx = handler;
}

static void log_append(I2cLogKind kind, uint8_t addr, const uint8_t* data, uint8_t len) {
    uint8_t slot;
    if (g_log_count < LOG_CAPACITY) {
        slot = (uint8_t)((g_log_head + g_log_count) & LOG_MASK);
        g_log_count++;
    } else {
        slot = g_log_head;
        g_log_head = (uint8_t)((g_log_head + 1u) & LOG_MASK);
    }
    g_log[slot].kind = (uint8_t)kind;
    g_log[slot].addr = addr;
    uint8_t n = (len > I2C_LOG_DATA_MAX) ? (uint8_t)I2C_LOG_DATA_MAX : len;
    g_log[slot].len = n;
    for (uint8_t i = 0; i < n; i++) {
        g_log[slot].data[i] = data[i];
    }
}

static void log_host_phase(FSMState phase, I2cResult reason) {
    MessageTask* task = &g_queue[g_q_head];
    if (phase == FSM_HOST_TX) {
        I2cLogKind kind = (reason == I2C_RESULT_OK) ? I2C_LOG_WA : I2C_LOG_WN;
        log_append(kind, task->addr, task->tx, task->tx_len);
    } else if (phase == FSM_HOST_RX) {
        I2cLogKind kind = (reason == I2C_RESULT_OK) ? I2C_LOG_RA : I2C_LOG_RN;
        log_append(kind, task->addr, task->rx, task->rx_len);
    }
}

uint8_t i2c_log_snapshot(I2cLogEntry* out, uint8_t capacity) {
    INTERRUPT_PUSH;
    uint8_t count = (g_log_count < capacity) ? g_log_count : capacity;
    for (uint8_t i = 0; i < count; i++) {
        /* Newest first: snapshot[0] = most recent. */
        uint8_t idx = (uint8_t)((g_log_head + g_log_count - 1u - i) & LOG_MASK);
        out[i] = g_log[idx];
    }
    INTERRUPT_POP;
    return count;
}

I2cResult i2c_set_client_tx(uint8_t* tx, uint8_t tx_len) {
    if (g_fsm == FSM_CLIENT_TX) {
        return I2C_RESULT_BUSY;
    }
    if (tx == 0) {
        return I2C_RESULT_BAD_ARG;
    }
    if (tx_len == 0 || tx_len > I2C_TX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    INTERRUPT_PUSH;
    for (uint8_t i = 0; i < tx_len; i++) {
        g_client_tx[i] = tx[i];
    }
    g_client_tx_len = tx_len;
    INTERRUPT_POP;
    /* Log the staged response.  tx_len bytes include the trailing CRC
     * appended by comm_respond; the renderer verifies and displays CT+/CT-. */
    log_append(I2C_LOG_CT, 0, tx, tx_len);
    return I2C_RESULT_OK;
}

static void i2c_dma_init(void) {
    /* Host TX channel: SFR/GPR -> I2C1TXB, source increments. */
    DMASELECT = DMA_TX_CHANNEL;
    DMAnCON1bits.DMODE = 0b00;
    DMAnCON1bits.DSTP = 0;
    DMAnCON1bits.SMR = 0b00;
    DMAnCON1bits.SMODE = 0b01;
    DMAnCON1bits.SSTP = 1;
    DMAnDSZ = 1;
    DMAnDSA = (uint16_t)&I2C1TXB;
    DMAnSSZ = 0;
    DMAnSSA = 0;
    DMAnSIRQ = 0x39; /* I2C1TX request */
    DMAnAIRQ = 0x3b; /* I2C1E */
    DMAnAIRQ = 0;    /* unwire abort: stale NACK/BTO/BCL must not kill in-flight transfers */
    DMAnCON0bits.EN = 1;

    /* Host RX channel: I2C1RXB -> GPR, destination increments. */
    DMASELECT = DMA_RX_CHANNEL;
    DMAnCON1bits.DMODE = 0b01;
    DMAnCON1bits.DSTP = 1;
    DMAnCON1bits.SMR = 0b00;
    DMAnCON1bits.SMODE = 0b00;
    DMAnCON1bits.SSTP = 0;
    DMAnDSZ = 0;
    DMAnDSA = 0;
    DMAnSSZ = 1;
    DMAnSSA = (uint24_t)&I2C1RXB;
    DMAnSIRQ = 0x38; /* I2C1RX request */
    DMAnAIRQ = 0x3b; /* I2C1E */
    DMAnAIRQ = 0;    /* unwire abort: stale NACK/BTO/BCL must not kill in-flight transfers */
    DMAnCON0bits.EN = 1;

    DMA2PR = 0x02;
    DMA3PR = 0x03;
    PRLOCK = 0x55;
    PRLOCK = 0xAA;
    PRLOCKbits.PRLOCKED = 1;
}

static void i2c_dma_set_host(MessageTask* task) {
    INTERRUPT_PUSH;
    if (task->tx_len) {
        DMASELECT = DMA_TX_CHANNEL;
        DMAnCON0bits.EN = 0;
        DMAnSSA = (uint24_t)task->tx;
        DMAnSSZ = task->tx_len;
        DMAnCON0bits.SIRQEN = 1;
        DMAnCON0bits.AIRQEN = 1;
        DMAnCON0bits.EN = 1;
    }
    if (task->rx_len) {
        DMASELECT = DMA_RX_CHANNEL;
        DMAnCON0bits.EN = 0;
        DMAnDSA = (uint16_t)task->rx;
        DMAnDSZ = task->rx_len;
        DMAnCON0bits.SIRQEN = 1;
        DMAnCON0bits.AIRQEN = 1;
        DMAnCON0bits.EN = 1;
    }
    INTERRUPT_POP;
}

static void i2c_dma_client_rx(void) {
    INTERRUPT_PUSH;
    DMASELECT = DMA_RX_CHANNEL;
    DMAnCON0bits.EN = 0;
    DMAnDSA = (uint16_t)g_client_rx;
    DMAnDSZ = I2C_RX_MAX;
    DMAnCON0bits.SIRQEN = 1;
    DMAnCON0bits.AIRQEN = 1;
    DMAnCON0bits.EN = 1;
    INTERRUPT_POP;
}

/*
 * Load the client TX path: byte 0 written directly to I2C1TXB, DMA armed to
 * deliver bytes 1..g_client_tx_len-1 on subsequent TXBE rising edges.
 *
 * Why byte 0 is direct-write and not DMA:
 *   The window between CNT being written and the peripheral's first TXB->SR
 *   move (at the 9th falling of the address byte) is only a handful of CPU
 *   cycles. Even with raised DMA priority, the DMA transfer occasionally
 *   doesn't complete in time — the byte appears one slot late on the wire.
 *   Writing TXB directly guarantees byte 0 is in place before CSTR=0 drops.
 *
 * Bytes 1..N-1 stay on DMA because each TXBE rising edge happens at the 9th
 * falling SCL of the prior byte — a full byte period (~20 us at 400 kHz) of
 * headroom, which is trivial even at default DMA priority.
 *
 * Preconditions:
 *   - CLRBF has just been set, so TXBE=1.
 *   - I2C1CNT is still 0 (caller writes it after this returns, generating the
 *     first TXIF edge — harmless here because byte 0 is already in TXB).
 *
 * On return for N > 1:
 *   - TXB holds g_client_tx[0] (TXBE=0).
 *   - DMA armed with SCNT=N-1, SPTR=&g_client_tx[1], SIRQEN=1, EN=1.
 *   DMAnCON1.SSTP=1 disarms the channel after the last transfer.
 *
 * Static config (CON1, DSA=&I2C1TXB, DSZ, SIRQ=I2C1TX, AIRQ=0) lives in
 * i2c_dma_init and is not touched here.
 */
static void i2c_dma_client_tx(void) {
    if (g_client_tx_len == 0) {
        return;
    }
    INTERRUPT_PUSH;
    I2C1TXB = g_client_tx[0];
    if (g_client_tx_len > 1) {
        DMASELECT = DMA_TX_CHANNEL;
        DMAnCON0bits.EN = 0;
        DMAnSSA = (uint24_t)&g_client_tx[1];
        DMAnSSZ = (uint16_t)(g_client_tx_len - 1u);
        DMAnCON0bits.SIRQEN = 1;
        DMAnCON0bits.EN = 1;
    }
    INTERRUPT_POP;
}

static void disarm_event(I2cResult reason) {
    /* OK finishes the task; every non-OK reason (BUSY / NACK / TIMEOUT)
     * decrements `retries` and either re-queues the task as MT_IDLE or
     * gives up as MT_FAILED.  Having BUSY decrement too (rather than
     * retrying forever) bounds arbitration-loss / collision cases and
     * lets i2c_poll emit one WN/RN per task when the retries are
     * exhausted — the single source of failure logging. */
    MessageTask* task = &g_queue[g_q_head];
    if (reason == I2C_RESULT_OK) {
        task->state = MT_FINISHED;
        task->result = I2C_RESULT_OK;
    } else if (task->retries == 0) {
        task->state = MT_FAILED;
        task->result = reason;
    } else {
        task->retries--;
        task->state = MT_IDLE;
    }
    I2C1CON0bits.RSEN = 0;
    /* Unload both DMA channels so no partial / dirty state survives into
     * the next transaction (e.g. TX DMA armed with stale SSA/SCNT after an
     * address NACK, or RX DMA holding residual DCNT after BTO).  The next
     * state entry re-arms from scratch: i2c_dma_set_host for host ops,
     * i2c_dma_client_rx for client reception, i2c_dma_client_tx for client
     * reply — each does EN=0 -> config -> SIRQEN=1 -> EN=1. */
    DMASELECT = DMA_TX_CHANNEL;
    DMAnCON0bits.SIRQEN = 0;
    DMAnCON0bits.EN = 0;
    DMASELECT = DMA_RX_CHANNEL;
    DMAnCON0bits.SIRQEN = 0;
    DMAnCON0bits.EN = 0;
}

static void switch_to_client(void) {
    I2C1CON0bits.MODE = 0b000;
    I2C1CON1bits.ACKCNT = 0;
    I2C1STAT1bits.CLRBF = 1;
    i2c_dma_client_rx();
}

static FSMState arm_event(void) {
    MessageTask* task = &g_queue[g_q_head];
    i2c_dma_set_host(task);
    task->state = MT_RUNNING;
    I2C1ADB1 = (uint8_t)(task->addr << 1);
    if (task->tx_len > 0) {
        I2C1CNTH = 0;
        I2C1CNTL = task->tx_len;
        I2C1CON0bits.RSEN = task->rx_len > 0;
        return FSM_HOST_TX;
    }
    if (task->rx_len > 0) {
        I2C1ADB1 |= 0b1; // Read only
        I2C1CNTH = 0;
        I2C1CNTL = task->rx_len;
        return FSM_HOST_RX;
    }
    return FSM_IDLE;
}

static void switch_to_host(void) {
    I2C1CON0bits.MODE = 0b100;
    I2C1CON1bits.ACKCNT = 0;
    I2C1STAT1bits.CLRBF = 1;
}

void i2c_init(uint8_t addr) {
    g_client_tx_len = 0;
    g_fsm = FSM_IDLE;
    g_q_head = 0;
    g_q_tail = 0;

    I2C1CON0bits.EN = 0;
    I2C1CON0bits.MODE = 0b110;
    I2C1CLK = 0x01; /* FOSC */
    I2C1BAUD = I2C_BAUD;
    I2C1CON1bits.CSD = 0; /* multi-master: clock-stretch on data enabled */
    I2C1CON2bits.FME = I2C_FME;
    /* Bus-free wait time before re-entering arbitration. 0b11 = 32 BAUD
     * periods (vs 0b00 = 6) gives marginal bridges like the CH347 more
     * time to drive a clean rising edge between transactions. Cost is
     * only a few hundred µs of host-side throughput we don't care about. */
    I2C1CON2bits.BFRET = 0b11;

    const uint8_t a = (uint8_t)(addr << 1);
    I2C1ADR0 = a;
    I2C1ADR1 = a;
    I2C1ADR2 = a;
    I2C1ADR3 = a;

    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.RSCIE = 1;
    I2C1PIEbits.ADRIE = 1;
    I2C1PIEbits.CNTIE = 1;

    I2C1BTOC = 0x06;         /* LFINTOSC as BTO clock source */
    I2C1BTObits.TOBY32 = 1;  /* x32 */
    I2C1BTObits.TOTIME = 35; /* ~35 ms */

    I2C1ERRbits.BCLIE = 1;
    I2C1ERRbits.NACKIE = 1;
    I2C1ERRbits.BTOIE = 1;

    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;

    i2c_dma_init();
    i2c_dma_client_rx();
    i2c_dma_client_tx();

    I2C1CON0bits.EN = 1;
}

void i2c_poll(void) {
    I2cCompletion callback = 0;
    uint8_t addr = 0;
    uint8_t tx[I2C_TX_MAX];
    uint8_t tx_len = 0;
    uint8_t rx[I2C_RX_MAX];
    uint8_t rx_len = 0;
    I2cResult result = 0;

    INTERRUPT_PUSH;
    if (g_q_head != g_q_tail) {
        uint8_t cur = g_q_head;
        MessageTask* task = &g_queue[cur];
        result = task->result;
        addr = task->addr;
        if (task->state == MT_FAILED) {
            callback = task->cb;
            tx_len = task->tx_len;
            for (uint8_t i = 0; i < tx_len; i++) {
                tx[i] = task->tx[i];
            }
            rx_len = 0;
            /* Emit the WN/RN the error ISRs deferred.  We log from here
             * (main context, inside the critical section) because calling
             * log_append from I2C1_ERROR_ISR destabilises the chip — see
             * CLAUDE.md.  tx_done distinguishes "write phase failed before
             * it finished" (tx_len > 0 and !tx_done -> WN) from "write OK,
             * read phase failed" (tx_done -> RN).  A pure read-only task
             * (tx_len == 0) also logs RN. */
            if (task->tx_len > 0 && !task->tx_done) {
                log_append(I2C_LOG_WN, task->addr, task->tx, task->tx_len);
            } else if (task->rx_len > 0) {
                log_append(I2C_LOG_RN, task->addr, task->rx, task->rx_len);
            }
            g_q_head = q_next(cur);
        }
        else if (task->state == MT_FINISHED) {
            callback = task->cb;
            tx_len = task->tx_len;
            for (uint8_t i = 0; i < tx_len; i++) {
                tx[i] = task->tx[i];
            }
            rx_len = task->rx_len;
            for (uint8_t i = 0; i < rx_len; i++) {
                rx[i] = task->rx[i];
            }
            g_q_head = q_next(cur);
        }
    }
    if (g_q_head != g_q_tail) {
        MessageTask* task = &g_queue[g_q_head];
        if (task->state == MT_IDLE && g_fsm == FSM_IDLE) {
            switch_to_host();
            g_fsm = arm_event();
            if (g_fsm == FSM_IDLE) {
                i2c_dma_client_rx();
            }
            else {
                I2C1CON0bits.S = 1;
            }
        }
    }
    INTERRUPT_POP;

    if (callback) {
        callback(result, addr, tx_len ? tx : 0, tx_len, rx, rx_len);
    }
}

I2cResult i2c_submit(uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t rx_len, I2cCompletion cb) {
    if (tx == 0) {
        return I2C_RESULT_BAD_ARG;
    }
    if (tx_len == 0 || tx_len > I2C_TX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    if (rx_len > I2C_RX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }

    INTERRUPT_PUSH;
    if (q_next(g_q_tail) == g_q_head) {
        INTERRUPT_POP;
        return I2C_RESULT_QUEUE_FULL;
    }
    MessageTask* task = &g_queue[g_q_tail];
    task->addr = addr;
    task->tx_len = tx_len;
    task->rx_len = rx_len;
    task->cb = cb;
    task->retries = I2C_RETRY_COUNT;
    task->tx_done = 0;
    for (uint8_t i = 0; i < tx_len; i++) {
        task->tx[i] = tx[i];
    }
    task->result = I2C_RESULT_OK;
    task->state = MT_IDLE;
    g_q_tail = q_next(g_q_tail);
    INTERRUPT_POP;

    return I2C_RESULT_OK;
}

static void prepend_completed_task(uint8_t addr, const volatile uint8_t* rx, uint8_t rx_len) {
    if (rx == 0 || rx_len == 0 || rx_len > I2C_RX_MAX) {
        return;
    }
    if (q_prev(g_q_head) == g_q_tail) {
        return; /* queue full — drop the cold message */
    }
    g_q_head = q_prev(g_q_head);
    MessageTask* task = &g_queue[g_q_head];

    task->addr = addr;
    task->tx_len = 0;
    task->rx_len = rx_len;
    task->cb = g_cold_rx;
    task->retries = 0;
    for (uint8_t i = 0; i < rx_len; i++) {
        task->rx[i] = rx[i];
    }
    task->state = MT_FINISHED;
    task->result = I2C_RESULT_OK;
}

void __interrupt(high_priority, irq(I2C1), base(8)) I2C1_ISR(void) {
    // byte counter has reached zero.  All planned bytes have been transferred
    if (I2C1PIEbits.CNTIE && I2C1PIRbits.CNTIF) {
        I2C1PIRbits.CNTIF = 0;
        isr_on_transmit_exhausted();
        return;
    }

    // Address detected, set on the 8th falling SCL edge for a matching received address byte
    if (I2C1PIEbits.ADRIE && I2C1PIRbits.ADRIF) {
        I2C1PIRbits.ADRIF = 0;
        isr_on_address();
        return;
    }

    // Stop condition detected
    if (I2C1PIEbits.PCIE && I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        isr_on_stop();
        return;
    }

    // RSCIF - Repeated Start Condition
    if (I2C1PIEbits.RSCIE && I2C1PIRbits.RSCIF) {
        I2C1PIRbits.RSCIF = 0;
        isr_on_restart();
        return;
    }

    // ACKTIF - Acknowledge Timeout
    if (I2C1PIEbits.ACKTIE && I2C1PIRbits.ACKTIF) {
        I2C1PIRbits.ACKTIF = 0;
        I2C1CON0bits.CSTR = 0;
        return;
    }
}

static void isr_on_address(void) {
    switch (g_fsm)
    {
    case FSM_HOST_TX:
    case FSM_HOST_RX:
        // We have likely lost arbitration.  WN/RN log is emitted from
        // i2c_poll on the final failure dispatch.
        disarm_event(I2C_RESULT_BUSY);
        switch_to_client();
        break;
    case FSM_CLIENT_RX:
    case FSM_CLIENT_TX:
        I2C1STAT1bits.CLRBF = 1;
        break;
    case FSM_IDLE:
        break;
    }
    if (I2C1STAT0bits.R && g_client_tx_len > 0) {
        g_fsm = FSM_CLIENT_TX;
        /* Client TX setup:
         *   1. CLRBF — known TX state: TXBE=1, TXIF=0, TXB empty.
         *   2. i2c_dma_client_tx — writes byte 0 directly to TXB (racing the
         *      peripheral's first TXB->SR move is unreliable, so byte 0 is
         *      off the DMA's critical path) and arms DMA for bytes 1..N-1.
         *   3. Write CNT = N - 1. The peripheral checks (TXBE && CNT > 0)
         *      at every 8th SCL falling edge and stretches if true. With CNT
         *      initialised to N - 1 it decrements to 0 just before the last
         *      byte's 8th falling — so the last byte's check sees CNT == 0
         *      and the bus releases cleanly, letting the host clock the 9th
         *      bit for its terminal NACK + STOP. With CNT = N the last byte
         *      always stretches until BTO (~36 ms) recovers.
         *   4. Drop CSTR at end of isr_on_address — peripheral resumes.
         *
         * Side effect of CNT = N - 1: CNTIF fires after byte N - 2 (not the
         * last byte). The CLIENT_TX path of isr_on_transmit_exhausted then
         * runs early; clearing CSTR there is a benign no-op because no
         * stretch is in effect at that point. End-of-transaction cleanup
         * happens via PCIF in isr_on_stop, unchanged.
         *
         * For N == 1 the initial CNT is 0; the stretch check then trivially
         * fails on the only byte and the path collapses to the same shape.
         *
         * CNT write is safe here because CSTR is stretching (DS §37.5.11). */
        I2C1STAT1bits.CLRBF = 1;
        i2c_dma_client_tx();
        I2C1CNTH = 0;
        I2C1CNTL = (uint8_t)(g_client_tx_len - 1u);
        I2C1CON1bits.ACKDT = 0;
    }
    else if (I2C1STAT0bits.R) {
        I2C1CON1bits.ACKDT = 1;
    }
    else {
        g_fsm = FSM_CLIENT_RX;
        i2c_dma_client_rx();
        I2C1CON1bits.ACKDT = 0;
    }
    I2C1CON0bits.CSTR = 0;
}

static void isr_on_transmit_exhausted(void) {
    MessageTask* task = &g_queue[g_q_head];
    switch(g_fsm) {
        case FSM_IDLE:
            break;
        case FSM_HOST_TX:
            task->tx_done = 1;
            log_host_phase(FSM_HOST_TX, I2C_RESULT_OK);
            if (task->rx_len > 0) {
                g_fsm = FSM_HOST_RX;
                I2C1ADB1 |= 0b1;
                /* Clear RSEN so the 9th-falling of the terminal RX byte
                 * auto-issues Stop (§37.5.1: CNT=0 ∧ RSEN=0) rather than
                 * setting MDR (pause-for-Restart) as RSEN=1 would.  Without
                 * this the bus hangs after the master's terminal NACK until
                 * BTO.  CNT is left alone — the peripheral produces the
                 * correct ACK/NACK pattern (ACK middle bytes, NACK the
                 * final one) on its own, observed on the wire.  ACKCNT=1
                 * belts-and-braces the terminal NACK. */
                I2C1CON0bits.RSEN = 0;
                I2C1CON1bits.ACKCNT = 1;
                I2C1CON0bits.S = 1; // Restart (MDR=1 from prior 9th-falling)
            }
            else {
                g_fsm = FSM_IDLE;
                disarm_event(I2C_RESULT_OK);
                switch_to_client();
            }
            break;
        case FSM_HOST_RX:
            /* CNTIF fires at the 9th falling of the last RX byte (CNT just
             * decremented to 0).  Hardware auto-issues Stop next because
             * RSEN=0; isr_on_stop's HOST_RX branch logs RA and disarms.
             * Keep FSM at HOST_RX so that branch runs. */
            break;
        case FSM_CLIENT_RX:
            break;
        case FSM_CLIENT_TX:
            /* CNTIF fires once per client-TX when the last planned byte is
             * shifted out. The peripheral auto-stretches SCL on CNT=0, so
             * fall through to the unconditional CSTR=0 below — otherwise
             * the master can't clock the terminal NACK or STOP and the bus
             * hangs until BTO recovers (~36 ms). isr_on_stop /
             * isr_on_restart handle the teardown after the master finishes. */
            break;
    }
    I2C1CON0bits.CSTR = 0;
}

static void isr_on_nack(void) {
    switch(g_fsm) {
        case FSM_IDLE:
            break;
        case FSM_HOST_TX:
            /* Log emitted from i2c_poll main-context dispatch (tx_done=0 on
             * the failed task selects WN).  Calling log_host_phase from
             * I2C1_ERROR_ISR destabilises the chip — see CLAUDE.md. */
            g_fsm = FSM_IDLE;
            disarm_event(I2C_RESULT_NACK);
            switch_to_client();
            break;
        case FSM_HOST_RX:
            /* Host's own terminal NACK (ACKCNT=1 on the last-read byte with
             * CNT=0) is spec-correct "I have enough" signalling, not a
             * failure.  Let isr_on_stop complete the transaction as OK. */
            if (I2C1CNTL == 0 && I2C1CNTH == 0) {
                break;
            }
            /* RN logged from i2c_poll main-context dispatch. */
            g_fsm = FSM_IDLE;
            disarm_event(I2C_RESULT_NACK);
            switch_to_client();
            break;
        case FSM_CLIENT_RX:
        case FSM_CLIENT_TX:
            break;
    }
}

static void isr_on_restart(void) {
    switch(g_fsm) {
        case FSM_IDLE:
            break;
        case FSM_HOST_RX:
        case FSM_HOST_TX:
            /* Unexpected restart while we're the host — treat as arbitration
             * loss / bus glitch; log emitted from i2c_poll on final fail. */
            g_fsm = FSM_IDLE;
            disarm_event(I2C_RESULT_BUSY);
            switch_to_client();
            break;
        case FSM_CLIENT_RX:
            g_fsm = FSM_IDLE;
            on_cold_rx_complete();
            i2c_dma_client_rx();
            break;
        case FSM_CLIENT_TX:
            g_fsm = FSM_IDLE;
            i2c_dma_client_rx();
            break;
    }
}

static void on_cold_rx_complete(void) {
    /* DMA was set up for I2C_RX_MAX; what we actually received is
     * I2C_RX_MAX minus the DMA destination count remaining. */
    DMASELECT = DMA_RX_CHANNEL;
    uint8_t remaining = (uint8_t)DMAnDCNT;
    uint8_t received = (uint8_t)(I2C_RX_MAX - remaining);
    if (received == 0) {
        return;
    }
    log_append(I2C_LOG_CR, 0, (const uint8_t*)g_client_rx, received);
    /* Invalidate any previous TX response before dispatch. If the request
     * is a read whose write-phase CRC validates, the sync handler will
     * call comm_respond → i2c_set_client_tx, repopulating g_client_tx_len.
     * If the CRC fails (or the id isn't a read), len stays 0 and the
     * following read-phase address gets NACK'd in isr_on_address rather
     * than echoing stale bytes from the previous transaction. */
    g_client_tx_len = 0;
    /* Offer to the synchronous handler first.  It runs in this ISR, so an
     * urgent handler (e.g. a read-request dispatcher that stages the reply
     * via i2c_set_client_tx before the read-phase address arrives) can
     * return 0 to keep the bytes off the async cold-RX queue. */
    if (g_sync_cold_rx && g_sync_cold_rx((uint8_t*)g_client_rx, received) == 0) {
        return;
    }
    // When receiving the client does not transmit address, set provisional 0x00
    prepend_completed_task(0x00, g_client_rx, received);
}

static void isr_on_stop(void) {
    switch (g_fsm) {
        case FSM_IDLE:
            // Irrelevant stop
            break;
        case FSM_HOST_TX:
        case FSM_HOST_RX:
            log_host_phase(g_fsm, I2C_RESULT_OK);
            g_fsm = FSM_IDLE;
            disarm_event(I2C_RESULT_OK);
            switch_to_client();
            break;
        case FSM_CLIENT_RX:
            g_fsm = FSM_IDLE;
            on_cold_rx_complete();
            i2c_dma_client_rx();
            break;
        case FSM_CLIENT_TX:
            g_fsm = FSM_IDLE;
            i2c_dma_client_rx();
            break;
    }
}

static void isr_on_collision(void) {
    switch (g_fsm) {
        case FSM_IDLE:
            break;
        case FSM_HOST_TX:
        case FSM_HOST_RX:
            /* Log emitted from i2c_poll — see isr_on_nack. */
            g_fsm = FSM_IDLE;
            disarm_event(I2C_RESULT_BUSY);
            switch_to_client();
            break;
        case FSM_CLIENT_TX:
        case FSM_CLIENT_RX:
            g_fsm = FSM_IDLE;
            i2c_dma_client_rx();
            break;
    }
}

static void isr_on_timeout(void) {
    switch (g_fsm) {
    case FSM_IDLE:
        break;
    case FSM_HOST_TX:
    case FSM_HOST_RX:
        /* Log emitted from i2c_poll — see isr_on_nack. */
        g_fsm = FSM_IDLE;
        disarm_event(I2C_RESULT_TIMEOUT);
        switch_to_client();
        break;
    case FSM_CLIENT_TX:
    case FSM_CLIENT_RX:
        g_fsm = FSM_IDLE;
        i2c_dma_client_rx();
        break;
    }

    I2C1PIR = 0x00;
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.BTOIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1STAT1 = 0x00;
    I2C1STAT1bits.CLRBF = 1;
    I2C1CON0bits.EN = 0;
    I2C1CON1bits.ACKCNT = 0;

    /* ===== SILICON ERRATA WORKAROUND =====
    * Same sequence as in the init path. Prevents I2C from locking up
    * during reset. */
    #pragma message "Refer to erratum DS80000870F: https://www.microchip.com/content/dam/mchp/documents/MCU08/ProductDocuments/Errata/PIC18F27-47-57Q43-Silicon-Errata-and-Datasheet-Clarifications-80000870J.pdf"
    I2C1PIEbits.SCIE = 0;
    I2C1PIEbits.PCIE = 0;
    I2C1CON0bits.EN = 1;
    __delay_us(1);
    __nop();
    __nop();
    __nop();
    __nop();
    __nop();
    __nop();
    I2C1PIRbits.SCIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.RSCIE = 1;
    i2c_dma_client_rx();
}

void __interrupt(high_priority, irq(I2C1E), base(8)) I2C1_ERROR_ISR(void) {
    if (I2C1ERRbits.NACKIF) {
        I2C1ERRbits.NACKIF = 0;
        isr_on_nack();
    }
    if (I2C1ERRbits.BTOIF) {
        I2C1ERRbits.BTOIF = 0;
        isr_on_timeout();
    }
    if (I2C1ERRbits.BCLIF) {
        I2C1ERRbits.BCLIF = 0;
        isr_on_collision();
    }
}
