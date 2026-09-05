/*
 * i2c.c — interrupt-driven (DMA-free) implementation of the driver
 *         declared in i2c.h, which carries the API contract and the
 *         rationale for moving bytes with the CPU instead of the DMA.
 *
 * Section references (§N) are DS40002213D's own numbering.
 *
 * Four interrupt vectors, all high priority so they cannot preempt each
 * other:
 *   I2C1    — protocol events: CNTIF, ADRIF, PCIF, RSCIF
 *   I2C1E   — errors: NACKIF, BTOIF, BCLIF
 *   I2C1TX  — "TXB is empty and CNT != 0": hand the peripheral one byte
 *   I2C1RX  — "RXB holds a byte": take it
 *
 * The two byte movers are deliberately the shortest code in the file: one
 * bounds check, one buffer access, one counter step.  Everything that
 * decides anything happens in the other two vectors or in main context.
 */

#include "i2c.h"

#include "libcomm.h" /* INTERRUPT_PUSH / INTERRUPT_POP */

#define _XTAL_FREQ 64000000UL

#include <xc.h>

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
    uint8_t rx_len; /* bytes requested — survives a retry */
    uint8_t rx_got; /* bytes the wire actually delivered */
    MessageTaskState state;
    I2cResult result;
    uint8_t retries;
    uint8_t tx_done; /* 1 once the write phase completed, for failure logging */
    uint8_t req_id;  /* driver-assigned per-transaction id (wraps at 0xFF) */
    I2cCompletion cb;
} MessageTask;

typedef enum {
    FSM_IDLE,
    FSM_HOST_TX,
    FSM_HOST_RX,
    FSM_CLIENT_TX,
    FSM_CLIENT_RX,
} FSMState;

static I2cCompletion g_cold_rx = 0;
static I2cSyncColdRxHandler g_sync_cold_rx = 0;

#define LOG_CAPACITY 8u
#define LOG_MASK     (LOG_CAPACITY - 1u)

static I2cLogEntry g_log[LOG_CAPACITY];
static uint8_t g_log_head = 0;
static uint8_t g_log_count = 0;

static volatile FSMState g_fsm = FSM_IDLE;

/* Host byte cursors.  These replace every DMAnSCNT / DMAnDCNT read in the
 * original driver, which is what makes the received-length calculation
 * exact — DMAnDCNT famously never reads zero (§16.3.3), so
 * `window - DCNT` reports 0 for a message that exactly fills the window. */
static volatile uint8_t g_tx_pos = 0;
static volatile uint8_t g_rx_pos = 0;

static volatile uint8_t g_client_rx[I2C_RX_MAX] = {0};
static volatile uint8_t g_client_rx_pos = 0;
static volatile uint8_t g_client_rx_ovf = 0;
static volatile uint8_t g_client_tx[I2C_TX_MAX] = {0};
static volatile uint8_t g_client_tx_len = 0;
static volatile uint8_t g_client_tx_pos = 0;

static MessageTask g_queue[I2C_QUEUE_SIZE] = {0};
static volatile uint8_t g_q_head = 0;
static volatile uint8_t g_q_tail = 0;

/* Cursor for the transaction currently on the wire.  It exists because it
 * cannot be g_q_head: prepend_completed_task injects an inbound client
 * message ahead of the head from ISR context, so g_q_head moves while a
 * transaction is in flight.  Every ISR that services that transaction must
 * address it by a cursor the injection cannot disturb, or it writes its
 * completion into the inbound message's slot and destroys it (rx_got in
 * particular, which i2c_poll then reads as a zero-length payload and drops).
 *
 * Latched in arm_head_task from g_q_head at the moment the transaction is
 * armed; queue discipline (which slot to pop, where to inject) stays with
 * g_q_head. */
static volatile uint8_t g_q_active = 0;

/* Monotonic per-transaction id, wraps at 0xFF.  Assigned at i2c_submit
 * (host transactions), prepend_completed_task (cold-rx delivered as a
 * pseudo-task), and i2c_set_client_tx (client-mode response).  Lets the
 * log render correlate the W/R phases of the same host op via a shared
 * id, and tag CR/CT in chronological order. */
static volatile uint8_t g_next_req_id = 0;

static uint8_t next_req_id(void) {
    /* Safe from either context: in an ISR the saved GIE is already 0 and
     * the restore is a no-op. */
    INTERRUPT_PUSH;
    uint8_t id = g_next_req_id++;
    INTERRUPT_POP;
    return id;
}

static void byte_irq(uint8_t tx_on, uint8_t rx_on);
static void enter_client(void);
static void enter_host(void);
static FSMState arm_head_task(void);
static void finish(I2cResult reason);
static void prepend_completed_task(uint8_t addr, const volatile uint8_t* rx, uint8_t rx_len);
static void on_client_rx_complete(void);
static void isr_on_address(void);
static void isr_on_count_zero(void);
static void isr_on_stop(void);
static void isr_on_restart(void);
static void isr_on_nack(void);
static void isr_on_collision(void);
static void isr_on_timeout(void);

/* ── Registration ───────────────────────────────────────────────────── */

void i2c_set_cold_rx_handler(I2cCompletion cold_rx) {
    g_cold_rx = cold_rx;
}

void i2c_set_sync_cold_rx_handler(I2cSyncColdRxHandler handler) {
    g_sync_cold_rx = handler;
}

/* ── Wire log ───────────────────────────────────────────────────────── */

static void log_append(I2cLogKind kind, I2cResult result, uint8_t addr, uint8_t req_id, const uint8_t* data,
                       uint8_t len) {
    uint8_t slot;
    if (g_log_count < LOG_CAPACITY) {
        slot = (uint8_t)((g_log_head + g_log_count) & LOG_MASK);
        g_log_count++;
    } else {
        slot = g_log_head;
        g_log_head = (uint8_t)((g_log_head + 1u) & LOG_MASK);
    }
    g_log[slot].kind = (uint8_t)kind;
    g_log[slot].result = (uint8_t)result;
    g_log[slot].addr = addr;
    g_log[slot].req_id = req_id;
    uint8_t n = (len > I2C_LOG_DATA_MAX) ? (uint8_t)I2C_LOG_DATA_MAX : len;
    g_log[slot].len = n;
    for (uint8_t i = 0; i < n; i++) {
        g_log[slot].data[i] = data[i];
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

/* ── Byte-mover arming ──────────────────────────────────────────────── */

/*
 * I2CxTXIF is set whenever TXBE = 1 AND I2CxCNT != 0 AND (SMA = 1 OR
 * MMA = 1) (§37.3.14.1).  Note what that condition does NOT
 * say: it does not mention direction.  During a host *read* MMA is set and
 * CNT is the read length, so TXIF is asserted for the whole reception with
 * nothing legitimate to send.  The flag is read-only and clears only on a
 * write to I2CxTXB or CLRBF, so an unmasked vector would spin.
 *
 * Hence the vectors are armed per phase rather than left enabled: TX only
 * while we are the one transmitting, RX only while we are receiving.  This
 * is the exact role SIRQEN played for the DMA channels in the original
 * driver.
 */
static void byte_irq(uint8_t tx_on, uint8_t rx_on) {
    PIE7bits.I2C1TXIE = tx_on ? 1 : 0;
    PIE7bits.I2C1RXIE = rx_on ? 1 : 0;
}

/* ── Role entry ─────────────────────────────────────────────────────── */

/*
 * Park the peripheral in the client role and make it ready to be addressed.
 *
 * MODE is deliberately never written: the module stays in Multi-Host 7-bit
 * (0b110) for its entire life, set once in i2c_init.  Multi-Host is the only
 * mode where the device is host and client simultaneously (Table 37-1) — in
 * pure Host mode (0b100) the peripheral performs no client address matching
 * at all, so every peer that addressed us during a host transaction would go
 * unanswered.  Staying in 0b110 also keeps I2CxADB1 writable, which pure
 * Client mode (0b0xx) does not (§37.5.15 note 1) — arm_head_task depends on
 * that.
 *
 * Both acknowledge sources are set to ACK.  ACKCNT rather than ACKDT is the
 * one that actually matters for client reception: I2CxCNT is 0 throughout
 * (we never preload it as a client), and §37.3.4 selects
 * ACKCNT as the response whenever CNT == 0.  A stale ACKCNT = 1 left over
 * from a host read would NACK the first byte of every inbound write.
 */
static void enter_client(void) {
    g_fsm = FSM_IDLE;
    g_client_rx_pos = 0;
    g_client_rx_ovf = 0;
    g_client_tx_pos = 0;
    I2C1CON1bits.ACKDT = 0;
    I2C1CON1bits.ACKCNT = 0;
    I2C1CNTH = 0;
    I2C1CNTL = 0;
    I2C1STAT1bits.CLRBF = 1;
    /* RX stays armed while idle: I2CxRXIF cannot be set unless SMA or MMA
     * is set (§37.3.14.1), so an armed-but-idle RX vector never fires. */
    byte_irq(0, 1);
}

/* Reset the peripheral's buffer state for the host role.  See enter_client
 * for why MODE is not written here either. */
static void enter_host(void) {
    I2C1CON1bits.ACKCNT = 0;
    I2C1STAT1bits.CLRBF = 1;
}

/*
 * Program the peripheral for the task at the head of the queue and return
 * the state to enter.  The caller issues the Start.
 *
 * I2CxCNT is the exact byte count, not count-1.  For transmission CNT is
 * decremented on the 9th falling SCL edge as a byte leaves I2CxTXB
 * (§37.5.11), and TXIF is gated on CNT != 0 (§37.3.14.1) — so CNT = N
 * produces exactly N feed interrupts and delivers all N bytes, while
 * CNT = N-1 silently drops the last one (which is always the CRC).
 */
static FSMState arm_head_task(void) {
    g_q_active = g_q_head;
    MessageTask* task = &g_queue[g_q_active];
    task->state = MT_RUNNING;
    task->rx_got = 0;
    g_tx_pos = 0;
    g_rx_pos = 0;

    if (task->tx_len > 0) {
        I2C1ADB1 = (uint8_t)(task->addr << 1); /* R/W = 0 → write */
        I2C1CNTH = 0;
        I2C1CNTL = task->tx_len;
        /* RSEN = 1 makes the 9th falling of the last written byte set MDR
         * and stretch, ready for the Restart, instead of auto-Stopping
         * (§37.3.12). */
        I2C1CON0bits.RSEN = task->rx_len > 0;
        I2C1CON1bits.ACKCNT = 0;
        byte_irq(1, 0);
        return FSM_HOST_TX;
    }

    if (task->rx_len > 0) {
        /* Pure read.  Unreachable through i2c_submit, which rejects
         * tx_len == 0, but kept correct rather than latent: ACKCNT must be
         * 1 here or the host ACKs its own final byte (§37.3.4). */
        I2C1ADB1 = (uint8_t)((task->addr << 1) | 0b1); /* R/W = 1 → read */
        I2C1CNTH = 0;
        I2C1CNTL = task->rx_len;
        I2C1CON0bits.RSEN = 0;
        I2C1CON1bits.ACKCNT = 1;
        byte_irq(0, 1);
        return FSM_HOST_RX;
    }

    return FSM_IDLE;
}

/*
 * The single terminal for a host transaction.  OK finishes the task; every
 * non-OK reason (BUSY / NACK / TIMEOUT) decrements `retries` and either
 * re-queues the task as MT_IDLE or gives up as MT_FAILED.  Having BUSY
 * decrement too (rather than retrying forever) bounds arbitration-loss and
 * collision cases and lets i2c_poll emit one failure log per task when the
 * retries are exhausted — the single source of failure logging.
 *
 * Callers follow this with enter_client().
 */
static void finish(I2cResult reason) {
    MessageTask* task = &g_queue[g_q_active];
    if (reason == I2C_RESULT_OK) {
        task->state = MT_FINISHED;
        task->result = I2C_RESULT_OK;
        task->rx_got = g_rx_pos;
    } else if (task->retries == 0) {
        task->state = MT_FAILED;
        task->result = reason;
        task->rx_got = 0;
    } else {
        task->retries--;
        task->state = MT_IDLE;
        task->rx_got = 0;
    }
    I2C1CON0bits.RSEN = 0;
    byte_irq(0, 0);
}

/* ── Client response staging ────────────────────────────────────────── */

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
    g_client_tx_pos = 0;
    g_client_tx_len = tx_len;
    INTERRUPT_POP;
    /* Log the staged response.  tx_len bytes include the trailing CRC
     * appended by comm_respond; the renderer verifies and displays it. */
    log_append(I2C_LOG_CT, I2C_RESULT_OK, 0, next_req_id(), tx, tx_len);
    return I2C_RESULT_OK;
}

/* ── Init ───────────────────────────────────────────────────────────── */

/*
 * Follows the multi-host checklist in.  Pins, PPS and the
 * oscillator are the board's job and must already be done.
 */
void i2c_init(uint8_t addr) {
    g_client_tx_len = 0;
    g_client_tx_pos = 0;
    g_client_rx_pos = 0;
    g_client_rx_ovf = 0;
    g_fsm = FSM_IDLE;
    g_q_head = 0;
    g_q_active = 0;
    g_q_tail = 0;

    I2C1CON0bits.EN = 0;

    /* Multi-Host, 7-bit.  Written once, never again — see enter_client. */
    I2C1CON0bits.MODE = 0b110;

    I2C1CLK = 0x01; /* FOSC (§37.5.9).  Required even for the client role:
                     * BFRE is derived from it (§37.5.4 note 2). */
    I2C1BAUD = I2C_BAUD;
    I2C1CON2bits.FME = I2C_FME;

    /* Multi-host prerequisites (§37.4.3): the device cannot be addressed as
     * a client without both of these. */
    I2C1CON1bits.CSD = 0; /* clock stretching enabled — also what makes the
                           * byte-at-a-time feed safe: TXU and RXO can only
                           * be set when CSD = 1 (§37.5.2 note 3). */
    I2C1PIEbits.ADRIE = 1;

    /* Bus-free wait before re-entering arbitration, in I2CxCLK pulses:
     * 0b00 = 8, 0b01 = 16, 0b10 = 32, 0b11 = 64 (§37.5.3).
     * With I2CxCLK = FOSC = 64 MHz even the maximum is ~1 us, so this knob
     * cannot produce the inter-transaction gap a marginal USB-I2C bridge
     * might want; that would need a slower I2CxCLK or software spacing.
     * Maximum is chosen because it costs nothing. */
    I2C1CON2bits.BFRET = 0b11;

    const uint8_t a = (uint8_t)(addr << 1);
    I2C1ADR0 = a;
    I2C1ADR1 = a;
    I2C1ADR2 = a;
    I2C1ADR3 = a;

    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.RSCIE = 1;
    I2C1PIEbits.CNTIE = 1;
    /* ACKTIE and WRIE stay disabled: both are interrupt-and-hold sources
     * that would stretch on every byte (§37.3.11.2) and neither carries
     * information this driver acts on. */

    /* Bus time-out.  Verbatim from the datasheet's own 35 ms worked example
     * (Example 37-1), which is also the SMBus host requirement.
     *
     * §37.3.7 prose and both worked examples say TOBY32 = 1 multiplies by
     * 32; the §37.5.12 register table says the opposite, and the conflict
     * survives unchanged into Rev. F.  TOBY32 = 0 with
     * TOTIME = 35 is therefore 35 ms under the reading that two of three
     * sources support.  To settle it on the bench: hold SCL low with the
     * bus otherwise idle and time BTOIF — 35 ms confirms the prose,
     * ~1.1 s confirms the register table. */
    I2C1BTOC = 0x06;         /* LFINTOSC, ~1 ms base period (§37.5.13) */
    I2C1BTObits.TOBY32 = 0;  /* no x32 multiplier */
    I2C1BTObits.TOTIME = 35; /* 35 * 1 ms */
    /* TOREC stays 0 — software resets the module in isr_on_timeout.  The
     * datasheet recommends TOREC = 1 for client mode (§37.3.7), but the
     * hardware reset it performs is exactly the operation erratum
     * DS80000870F says can wedge the module, and the manual sequence below
     * is the one that has been shown to work on this board.  Revisit only
     * with a bench measurement. */

    I2C1PIR = 0x00;
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.BTOIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1ERRbits.BCLIE = 1;
    I2C1ERRbits.NACKIE = 1;
    I2C1ERRbits.BTOIE = 1;
    I2C1STAT1 = 0x00;
    I2C1STAT1bits.CLRBF = 1;

    /* All four vectors this driver owns must run at high priority: that is
     * what makes them unable to preempt each other, which is why the FSM
     * needs no reentrancy guard.
     *
     * Set here so i2c_init is correct standalone, but the board's
     * interrupt_init must promote all four as well — it clears IPR7
     * wholesale, and boards call i2c_init before interrupt_init, so these
     * two writes are wiped on every board that does. */
    IPR7bits.I2C1TXIP = 1;
    IPR7bits.I2C1RXIP = 1;
    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;

    enter_client();
}

/* Bring the peripheral on the bus.  Split from i2c_init because enabling it
 * makes this device start ACKing its address immediately, and an address
 * matched before the ISRs can run leaves the peripheral clock-stretching for
 * an RXB read that never comes.  TOREC = 0 means the bus time-out is
 * recovered by isr_on_timeout, so with interrupts still masked nothing
 * releases SCL and the whole bus stops -- every board on it, not just this
 * one.  The window is not theoretical: a board seeding EEPROM defaults after
 * a config-layout change spends tens of milliseconds between i2c_init and
 * interrupt_init, and any other master addressing it in that window wedges
 * the bus until power is cycled.
 *
 * Call once, after interrupt_init(). */
void i2c_start(void) {
    I2C1CON0bits.EN = 1;
}

/* ── Main-loop poll ─────────────────────────────────────────────────── */

void i2c_poll(void) {
    I2cCompletion callback = 0;
    uint8_t addr = 0;
    uint8_t tx[I2C_TX_MAX];
    uint8_t tx_len = 0;
    uint8_t rx[I2C_RX_MAX];
    uint8_t rx_len = 0;
    I2cResult result = I2C_RESULT_OK;

    INTERRUPT_PUSH;
    if (g_q_head != g_q_tail) {
        uint8_t cur = g_q_head;
        MessageTask* task = &g_queue[cur];
        if (task->state == MT_FAILED || task->state == MT_FINISHED) {
            result = task->result;
            addr = task->addr;
            callback = task->cb;
            tx_len = task->tx_len;
            for (uint8_t i = 0; i < tx_len; i++) {
                tx[i] = task->tx[i];
            }
            rx_len = (task->state == MT_FINISHED) ? task->rx_got : 0;
            for (uint8_t i = 0; i < rx_len; i++) {
                rx[i] = task->rx[i];
            }
            if (task->state == MT_FAILED) {
                /* Emit the failure entry the error ISRs deferred.  We log
                 * from here (main context, inside the critical section)
                 * because calling log_append from I2C1_ERROR_ISR
                 * destabilises the chip — see ../../CLAUDE.md.  tx_done
                 * distinguishes "write phase failed before it finished"
                 * from "write OK, read phase failed".  A pure read-only
                 * task (tx_len == 0) also logs as a read. */
                if (task->tx_len > 0 && !task->tx_done) {
                    log_append(I2C_LOG_W, task->result, task->addr, task->req_id, task->tx, task->tx_len);
                } else if (task->rx_len > 0) {
                    log_append(I2C_LOG_R, task->result, task->addr, task->req_id, task->rx, task->rx_len);
                }
            }
            g_q_head = q_next(cur);
        }
    }

    if (g_q_head != g_q_tail) {
        MessageTask* task = &g_queue[g_q_head];
        /* Multi-Host arbitration rule (§37.4.3): "Client
         * hardware has priority over host hardware in Multi-Host mode.
         * Host mode communication can only be initiated when SMA = 0."
         *
         * g_fsm alone is not sufficient: it only becomes FSM_CLIENT_RX at
         * ADRIF, i.e. the 8th falling SCL edge of a matching address
         * (§37.5.6).  Between another master's Start and that edge g_fsm is
         * still FSM_IDLE, so without the SMA check we would tear down client
         * mode in the middle of a reception addressed to us.  SMA is set on
         * that same edge and cleared on any Restart/Stop (§37.5.4), so it
         * covers the window g_fsm cannot.
         *
         * Leaving the task MT_IDLE costs no retry budget — the next
         * i2c_poll simply tries again once the bus is ours. */
        if (task->state == MT_IDLE && g_fsm == FSM_IDLE && !I2C1STAT0bits.SMA) {
            enter_host();
            g_fsm = arm_head_task();
            if (g_fsm == FSM_IDLE) {
                enter_client();
            } else {
                I2C1CON0bits.S = 1;
            }
        }
    }
    INTERRUPT_POP;

    if (callback) {
        callback(result, addr, tx_len ? tx : 0, tx_len, rx, rx_len);
    }
}

/* ── Submission ─────────────────────────────────────────────────────── */

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
    task->rx_got = 0;
    task->cb = cb;
    task->req_id = g_next_req_id++;
    task->retries = I2C_RETRY_COUNT;
    task->tx_done = 0;
    for (uint8_t i = 0; i < tx_len; i++) {
        task->tx[i] = tx[i];
    }
    /* Zero the rx span we're about to read into so that any byte the bus
     * doesn't actually deliver reads as 0 — the protocol layer's CRC check
     * fails instead of silently consuming bytes left from a previous use of
     * this slot.  Otherwise, for fixed-size 2-byte payloads, stale data
     * could CRC-validate as a valid frame of the wrong type. */
    for (uint8_t i = 0; i < rx_len; i++) {
        task->rx[i] = 0;
    }
    task->result = I2C_RESULT_OK;
    task->state = MT_IDLE;
    g_q_tail = q_next(g_q_tail);
    INTERRUPT_POP;

    return I2C_RESULT_OK;
}

/* Inject a completed cold-RX message at the head of the queue so that it
 * and host completions drain through the one dispatch path in i2c_poll. */
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
    task->rx_got = rx_len;
    task->cb = g_cold_rx;
    task->req_id = g_next_req_id++;
    task->retries = 0;
    task->tx_done = 0;
    for (uint8_t i = 0; i < rx_len; i++) {
        task->rx[i] = rx[i];
    }
    task->state = MT_FINISHED;
    task->result = I2C_RESULT_OK;
}

/* ── Byte movers ────────────────────────────────────────────────────── */

/*
 * I2C1TX: the peripheral has room for one byte and I2CxCNT is not yet
 * zero, so it wants one.  With CSD = 0 it is stretching SCL while it waits
 * (§37.3.11.1 for the client role, MDR per §37.5.1 for the host role), so
 * there is no deadline here — only an obligation to write I2CxTXB, since
 * that (or CLRBF) is the only thing that clears the read-only flag.
 */
void __interrupt(high_priority, irq(I2C1TX), base(8)) I2C1TX_ISR(void) {
    switch (g_fsm) {
        case FSM_HOST_TX: {
            MessageTask* task = &g_queue[g_q_active];
            if (g_tx_pos < task->tx_len) {
                I2C1TXB = task->tx[g_tx_pos++];
                return;
            }
            break;
        }
        case FSM_CLIENT_TX:
            if (g_client_tx_pos < g_client_tx_len) {
                I2C1TXB = g_client_tx[g_client_tx_pos++];
                return;
            }
            break;
        default:
            break;
    }
    /* Nothing legitimate to send.  CNT = N is chosen precisely so this
     * cannot happen during a well-formed transfer; if it does, feed a
     * filler rather than spin the vector forever.  0xFF is what the
     * peripheral itself substitutes on an underflow (§37.3.9). */
    I2C1TXB = 0xFF;
}

/*
 * I2C1RX: a byte has landed in I2CxRXB.  Read it unconditionally — the
 * read is what clears RXBF / I2CxRXIF and releases any stretch the full
 * buffer is holding (§37.3.10) — then place it, or drop it if we have no
 * business receiving right now.
 */
void __interrupt(high_priority, irq(I2C1RX), base(8)) I2C1RX_ISR(void) {
    uint8_t b = I2C1RXB;
    switch (g_fsm) {
        case FSM_HOST_RX: {
            MessageTask* task = &g_queue[g_q_active];
            if (g_rx_pos < task->rx_len) {
                task->rx[g_rx_pos++] = b;
            }
            break;
        }
        case FSM_CLIENT_RX:
            if (g_client_rx_pos < I2C_RX_MAX) {
                g_client_rx[g_client_rx_pos++] = b;
            } else {
                /* Longer than anything the protocol defines.  Mark it so
                 * on_client_rx_complete drops the whole message rather than
                 * handing a silently truncated frame to the dispatcher. */
                g_client_rx_ovf = 1;
            }
            break;
        default:
            break;
    }
}

/* ── Protocol event vector ──────────────────────────────────────────── */

void __interrupt(high_priority, irq(I2C1), base(8)) I2C1_ISR(void) {
    /* Byte counter reached zero — all planned bytes have been transferred. */
    if (I2C1PIEbits.CNTIE && I2C1PIRbits.CNTIF) {
        I2C1PIRbits.CNTIF = 0;
        isr_on_count_zero();
        return;
    }

    /* A transaction's terminating event is checked before the address that
     * may follow it, because on the wire it happened first and only one
     * flag is serviced per entry.
     *
     * This is load-bearing for client reads.  A write-then-read puts
     * Restart and the read-phase address a few microseconds apart, so both
     * RSCIF and ADRIF are routinely pending together.  isr_on_restart is
     * what runs on_client_rx_complete -> the sync dispatcher ->
     * comm_respond, i.e. it is what stages the reply; isr_on_address is
     * what consumes it.  Servicing the address first finds g_client_tx_len
     * still 0 and NACKs the read, so the host gets a zero-length response
     * and every read fails while writes are unaffected.
     *
     * Returning after one flag is fine: the vector re-fires immediately
     * while any other flag is still set, and the client is stretching via
     * CSTR throughout, so there is no deadline. */
    if (I2C1PIEbits.RSCIE && I2C1PIRbits.RSCIF) {
        I2C1PIRbits.RSCIF = 0;
        isr_on_restart();
        return;
    }

    if (I2C1PIEbits.PCIE && I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        isr_on_stop();
        return;
    }

    /* Address match, on the 8th falling SCL edge of a matching address. */
    if (I2C1PIEbits.ADRIE && I2C1PIRbits.ADRIF) {
        I2C1PIRbits.ADRIF = 0;
        isr_on_address();
        return;
    }
}

/*
 * A matching address arrived.  ADRIF is an interrupt-and-hold source, so
 * SCL is stretched for the whole of this handler and every register write
 * below happens in a quiet bus window (§37.3.11.2).
 */
static void isr_on_address(void) {
    switch (g_fsm) {
        case FSM_HOST_TX:
        case FSM_HOST_RX:
            /* Being addressed while we thought we were the host means we
             * lost arbitration.  The failure log is emitted from i2c_poll
             * on the final dispatch, never from here. */
            finish(I2C_RESULT_BUSY);
            enter_client();
            break;
        case FSM_CLIENT_RX:
        case FSM_CLIENT_TX:
            I2C1STAT1bits.CLRBF = 1;
            break;
        case FSM_IDLE:
            break;
    }

    if (I2C1STAT0bits.R && g_client_tx_len > 0) {
        /* Read request with a reply staged.
         *
         * Byte 0 is written straight to I2CxTXB here rather than left to
         * the TX vector.  §37.4.1.3.1 step 4 says hardware will set CSTR
         * and stretch while TXB is empty, but CSTR is already set by ADRIF
         * and §37.5.1 note 3 warns that a single source clearing it is not
         * enough when several sources have set it.  Writing the first byte
         * before dropping CSTR removes the question entirely; bytes
         * 1..N-1 then arrive on TXIF, one per byte period (~22 us at
         * 400 kHz), with no timing pressure at all.
         *
         * I2CxCNT = N exactly, for the reasons in arm_head_task.  CNT = N
         * does not stretch the final byte: CNT for byte k is decremented on
         * byte k-1's 9th falling edge, so by the time the last byte's 8th
         * falling edge arrives (when the stretch condition TXBE && CNT != 0
         * is evaluated, §37.3.11.1) CNT is already 0.  The bus releases and
         * the master clocks its terminal NACK and Stop.
         *
         * Writing CNT is safe here because CSTR is stretching (§37.5.11
         * note 1). */
        g_fsm = FSM_CLIENT_TX;
        I2C1STAT1bits.CLRBF = 1;
        I2C1TXB = g_client_tx[0];
        g_client_tx_pos = 1;
        I2C1CNTH = 0;
        I2C1CNTL = g_client_tx_len;
        I2C1CON1bits.ACKDT = 0;
        byte_irq(1, 0);
    } else if (I2C1STAT0bits.R) {
        /* Read request with nothing staged — NACK it rather than clock out
         * whatever happens to be in the buffer. */
        I2C1CON1bits.ACKDT = 1;
    } else {
        g_fsm = FSM_CLIENT_RX;
        g_client_rx_pos = 0;
        g_client_rx_ovf = 0;
        /* CNT stays 0 for client reception, so ACKCNT is the acknowledge
         * source, not ACKDT (§37.3.4).  Both are set. */
        I2C1CNTH = 0;
        I2C1CNTL = 0;
        I2C1CON1bits.ACKDT = 0;
        I2C1CON1bits.ACKCNT = 0;
        byte_irq(0, 1);
    }

    /* §37.5.2 note 1: "software writes to ACKDT must be followed by a
     * minimum SDA setup time before clearing CSTR."  The datasheet never
     * quantifies it; four cycles is 250 ns at 64 MHz, comfortably beyond
     * the 100 ns the I2C specification asks of a data setup and free
     * against a ~22 us byte period. */
    __nop();
    __nop();
    __nop();
    __nop();
    I2C1CON0bits.CSTR = 0;
}

/* I2CxCNT hit zero, on the 9th falling SCL edge (§37.3.14.2). */
static void isr_on_count_zero(void) {
    MessageTask* task = &g_queue[g_q_active];
    switch (g_fsm) {
        case FSM_IDLE:
            break;

        case FSM_HOST_TX:
            task->tx_done = 1;
            log_append(I2C_LOG_W, I2C_RESULT_OK, task->addr, task->req_id, task->tx, task->tx_len);
            if (task->rx_len > 0) {
                /* Turn the write phase into the read phase.  RSEN = 1 has
                 * already caused hardware to set MDR and stretch, waiting
                 * for S (§37.3.12); clearing RSEN now means the
                 * 9th falling of the final read byte auto-issues Stop rather
                 * than parking on MDR again, which would hang the bus until
                 * BTO.  ACKCNT = 1 supplies the terminal NACK, which is the
                 * response used once CNT reaches 0 (§37.3.4). */
                g_fsm = FSM_HOST_RX;
                g_rx_pos = 0;
                I2C1ADB1 = (uint8_t)((task->addr << 1) | 0b1);
                I2C1CNTH = 0;
                I2C1CNTL = task->rx_len;
                I2C1CON0bits.RSEN = 0;
                I2C1CON1bits.ACKCNT = 1;
                byte_irq(0, 1);
                I2C1CON0bits.S = 1; /* Restart */
            } else {
                g_fsm = FSM_IDLE;
                finish(I2C_RESULT_OK);
                enter_client();
            }
            break;

        case FSM_HOST_RX:
            /* Hardware auto-issues Stop next because RSEN = 0; isr_on_stop
             * logs the read and finishes the task.  Stay in HOST_RX so that
             * branch runs. */
            break;

        case FSM_CLIENT_RX:
            /* Expected and meaningless: we never preload CNT as a client,
             * and §37.3.12.1 has hardware set CNTIF if no count is loaded by
             * the 9th falling edge after the address.  Clearing the flag in
             * the vector is the whole job. */
            break;

        case FSM_CLIENT_TX:
            /* The last byte is moving TXB → shift register right now and
             * still has eight SCL periods to go on the wire.  Tearing down
             * here would clear I2CxTXB out from under it (§37.5.5).
             * Teardown belongs to isr_on_stop / isr_on_restart. */
            break;
    }
}

static void on_client_rx_complete(void) {
    uint8_t received = g_client_rx_pos;
    uint8_t overflowed = g_client_rx_ovf;
    g_client_rx_pos = 0;
    g_client_rx_ovf = 0;

    if (overflowed || received == 0) {
        /* A frame longer than I2C_RX_MAX is not anything this protocol
         * defines; delivering its first I2C_RX_MAX bytes would just be a
         * truncated frame that might still CRC-validate. */
        return;
    }

    log_append(I2C_LOG_CR, I2C_RESULT_OK, 0, next_req_id(), (const uint8_t*)g_client_rx, received);

    /* Invalidate any previous staged response before dispatch.  If the
     * request is a read whose CRC validates, the sync handler will call
     * comm_respond -> i2c_set_client_tx and repopulate it.  If the CRC
     * fails (or the id isn't a read), the length stays 0 and the following
     * read-phase address gets NACKed in isr_on_address rather than echoing
     * stale bytes from the previous transaction. */
    g_client_tx_len = 0;
    g_client_tx_pos = 0;

    /* Offer to the synchronous handler first.  It runs in this ISR, so an
     * urgent handler (e.g. a read-request dispatcher that stages the reply
     * before the read-phase address arrives) can return 0 to keep the bytes
     * off the async cold-RX queue.
     *
     * This pointer is called from this one vector and no other — see the
     * "one function pointer, one vector" rule in ../../CLAUDE.md. */
    if (g_sync_cold_rx && g_sync_cold_rx((uint8_t*)g_client_rx, received) == 0) {
        return;
    }

    /* A client does not learn the sender's address from the bus; use 0. */
    prepend_completed_task(0x00, g_client_rx, received);
}

/* Take any received byte the RX vector has not serviced yet.
 *
 * The final byte's RXIF and the transaction's PCIF can be pending at the
 * same moment, and all four I2C vectors share one priority level, so the
 * hardware vector order — not arrival order — decides which runs first.
 * When Stop wins, the last byte is still sitting in RXB and completing the
 * task here would drop it.  For a framed response that byte is the CRC, so
 * the loss is not partial data but a read that always fails validation.
 *
 * Reading I2CxRXB is also what clears RXBF (§37.3.10), so draining here
 * leaves the buffer in the state enter_client expects. */
static void drain_rx(MessageTask* task) {
    while (I2C1STAT1bits.RXBF && g_rx_pos < task->rx_len) {
        task->rx[g_rx_pos++] = I2C1RXB;
    }
}

static void isr_on_stop(void) {
    MessageTask* task = &g_queue[g_q_active];
    switch (g_fsm) {
        case FSM_IDLE:
            break; /* someone else's transaction */

        case FSM_HOST_TX:
            log_append(I2C_LOG_W, I2C_RESULT_OK, task->addr, task->req_id, task->tx, task->tx_len);
            g_fsm = FSM_IDLE;
            finish(I2C_RESULT_OK);
            enter_client();
            break;

        case FSM_HOST_RX:
            drain_rx(task);
            /* g_rx_pos, not task->rx_len: log what the wire delivered. */
            log_append(I2C_LOG_R, I2C_RESULT_OK, task->addr, task->req_id, task->rx, g_rx_pos);
            g_fsm = FSM_IDLE;
            finish(I2C_RESULT_OK);
            enter_client();
            break;

        case FSM_CLIENT_RX:
            g_fsm = FSM_IDLE;
            on_client_rx_complete();
            enter_client();
            break;

        case FSM_CLIENT_TX:
            g_fsm = FSM_IDLE;
            g_client_tx_len = 0;
            enter_client();
            break;
    }
}

static void isr_on_restart(void) {
    switch (g_fsm) {
        case FSM_IDLE:
        case FSM_HOST_TX:
        case FSM_HOST_RX:
            /* Our own Restart between the write and read phases; the state
             * change was already made in isr_on_count_zero. */
            break;

        case FSM_CLIENT_RX:
            /* Write phase of somebody's read request just ended.  This is
             * the latency-critical path: the reply must be staged before the
             * read-phase address arrives a few microseconds from now. */
            g_fsm = FSM_IDLE;
            on_client_rx_complete();
            enter_client();
            break;

        case FSM_CLIENT_TX:
            g_fsm = FSM_IDLE;
            g_client_tx_len = 0;
            enter_client();
            break;
    }
}

/* ── Error vector ───────────────────────────────────────────────────── */

/*
 * Nothing in this vector may call log_append or any function pointer: both
 * destabilise the part (see ../../CLAUDE.md).  Failures are recorded when
 * i2c_poll dispatches the failed task.
 */
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

static void isr_on_nack(void) {
    switch (g_fsm) {
        case FSM_IDLE:
            break;

        case FSM_HOST_TX:
            g_fsm = FSM_IDLE;
            finish(I2C_RESULT_NACK);
            enter_client();
            break;

        case FSM_HOST_RX:
            /* Our own terminal NACK (ACKCNT = 1 applied once CNT reached 0)
             * is spec-correct "I have enough" signalling, not a failure —
             * and §37.5.8 note 4 confirms NACKIF is set for it.  Let
             * isr_on_stop complete the transaction as OK. */
            if (I2C1CNTL == 0 && I2C1CNTH == 0) {
                break;
            }
            g_fsm = FSM_IDLE;
            finish(I2C_RESULT_NACK);
            enter_client();
            break;

        case FSM_CLIENT_TX:
            /* The master's terminal NACK.  Retire the staged reply but stay
             * in CLIENT_TX: the Stop or Restart that follows does the
             * teardown. */
            g_client_tx_len = 0;
            byte_irq(0, 0);
            I2C1STAT1bits.CLRBF = 1;
            break;

        case FSM_CLIENT_RX:
            break;
    }
}

static void isr_on_collision(void) {
    switch (g_fsm) {
        case FSM_IDLE:
            break;

        case FSM_HOST_TX:
        case FSM_HOST_RX:
            /* Arbitration lost (§37.4.3.2).  If we lost during the
             * addressing phase the winner may be addressing us as a client,
             * so returning to the client role immediately is required, not
             * merely tidy. */
            g_fsm = FSM_IDLE;
            finish(I2C_RESULT_BUSY);
            enter_client();
            break;

        case FSM_CLIENT_TX:
            g_fsm = FSM_IDLE;
            g_client_tx_len = 0;
            enter_client();
            break;

        case FSM_CLIENT_RX:
            g_fsm = FSM_IDLE;
            g_client_rx_pos = 0;
            enter_client();
            break;
    }
}

static void isr_on_timeout(void) {
    switch (g_fsm) {
        case FSM_IDLE:
            break;

        case FSM_HOST_TX:
        case FSM_HOST_RX:
            g_fsm = FSM_IDLE;
            finish(I2C_RESULT_TIMEOUT);
            break;

        case FSM_CLIENT_TX:
            g_fsm = FSM_IDLE;
            g_client_tx_len = 0;
            break;

        case FSM_CLIENT_RX:
            g_fsm = FSM_IDLE;
            break;
    }

    /* TOREC = 0, so hardware has done nothing but raise BTOIF (§37.3.7)
     * — the module reset is ours to perform. */
    byte_irq(0, 0);
    I2C1PIR = 0x00;
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.BTOIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1STAT1 = 0x00;
    I2C1STAT1bits.CLRBF = 1;
    I2C1CON0bits.EN = 0;
    I2C1CON1bits.ACKCNT = 0;

    /* ===== SILICON ERRATA WORKAROUND =====
     * Prevents I2C from locking up during the re-enable. */
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

    enter_client();
}
