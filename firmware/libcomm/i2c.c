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
    I2cCompletion cb;
    void* cb_ctx;
} MessageTask;

typedef enum {
    FSM_IDLE,
    FSM_HOST_TX,
    FSM_HOST_RX,
    FSM_CLIENT_TX,
    FSM_CLIENT_RX,
} FSMState;

static I2cCompletion g_cold_rx = 0;
static I2cReadRequestHandler g_read_request = 0;
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
static void i2c_start_task(MessageTask* task);
static void prepend_completed_task(uint8_t addr, const volatile uint8_t* rx, uint8_t rx_len);
static void host_finish(MessageTaskState final_state, I2cResult result);
static void isr_on_address(void);
static void isr_on_stop(void);
static void isr_on_restart(void);
static void isr_on_nack(void);
static void isr_on_collision(void);
static void isr_on_timeout(void);

void i2c_set_cold_rx_handler(I2cCompletion cold_rx) {
    g_cold_rx = cold_rx;
}

void i2c_set_read_request_handler(I2cReadRequestHandler handler) {
    g_read_request = handler;
}

I2cResult i2c_set_client_tx(uint8_t* tx, uint8_t tx_len) {
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
    DMAnAIRQ = 0;

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
    DMAnAIRQ = 0;

    /* Lock the DMA arbiter once both channels are configured.  The
    * arbiter is global; the lock applies to all priority registers. */
   DMA2PR = 0x02;
   DMA3PR = 0x03;
   PRLOCK = 0x55;
   PRLOCK = 0xAA;
   PRLOCKbits.PRLOCKED = 1;
   DMAnCON0bits.EN = 1;
}

static void i2c_dma_set_host(MessageTask* task) {
    INTERRUPT_PUSH;
    if (task->tx_len) {
        DMASELECT = DMA_TX_CHANNEL;
        DMAnCON0bits.EN = 0;
        DMAnSSA = (uint24_t)task->tx;
        DMAnSSZ = task->tx_len;
        DMAnCON0bits.SIRQEN = 0;
        DMAnCON0bits.EN = 1;
    }
    if (task->rx_len) {
        DMASELECT = DMA_RX_CHANNEL;
        DMAnCON0bits.EN = 0;
        DMAnDSA = (uint16_t)task->rx;
        DMAnDSZ = task->rx_len;
        DMAnCON0bits.SIRQEN = 1;
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
    DMAnCON0bits.EN = 1;
    INTERRUPT_POP;
}

static void i2c_dma_client_tx(void) {
    if (g_client_tx_len == 0) {
        return;
    }
    INTERRUPT_PUSH;
    DMASELECT = DMA_TX_CHANNEL;
    DMAnCON0bits.EN = 0;
    DMAnSSA = (uint24_t)g_client_tx;
    DMAnSSZ = g_client_tx_len;
    DMAnCON0bits.SIRQEN = 1;
    DMAnCON0bits.EN = 1;
    I2C1CNTH = 0;
    I2C1CNTL = g_client_tx_len;
    INTERRUPT_POP;
}

void i2c_init(uint8_t addr) {
    g_client_tx_len = 0;
    g_fsm = FSM_IDLE;
    g_q_head = 0;
    g_q_tail = 0;

    I2C1CON0bits.EN = 0;
    I2C1CON0bits.MODE = 0b000;
    I2C1CLK = 0x01; /* FOSC */
    I2C1BAUD = I2C_BAUD;
    I2C1CON1bits.CSD = 0; /* multi-master: clock-stretch on data enabled */
    // I2C1CON1bits.ACKCNT = 1; /* NACK last byte of host reads */
    I2C1CON2bits.FME = I2C_FME;
    /* BFRET = 0b10 (32 FOSC/4 cycles ≈ 2 µs at FOSC = 64 MHz) gives the
     * peripheral enough bus-free margin to safely back start conditions
     * after a STOP, especially during rapid client→host transitions. */
    I2C1CON2bits.BFRET = 0b10;

    const uint8_t a = (uint8_t)(addr << 1);
    I2C1ADR0 = a;
    I2C1ADR1 = a;
    I2C1ADR2 = a;
    I2C1ADR3 = a;

    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.RSCIE = 1;
    I2C1PIEbits.ADRIE = 1;

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
    if (g_q_head != g_q_tail) {
        INTERRUPT_PUSH;
        uint8_t cur = g_q_head;
        INTERRUPT_POP;
        MessageTask* task = &g_queue[cur];
        if (task->state == MT_FINISHED || task->state == MT_FAILED) {
            uint8_t rx_len = (task->state == MT_FINISHED) ? task->rx_len : 0;
            if (task->cb) {
                task->cb(task->result, task->rx, rx_len, task->cb_ctx);
            }
            INTERRUPT_PUSH;
            if (g_q_head == cur) {
                g_q_head = q_next(cur);
            }
            INTERRUPT_POP;
        }
    }

    if (g_q_head != g_q_tail) {
        MessageTask* task = &g_queue[g_q_head];
        INTERRUPT_PUSH;
        if (task->state == MT_IDLE && g_fsm == FSM_IDLE) {
            i2c_start_task(task);
        }
        INTERRUPT_POP;
    }
}

static void i2c_start_task(MessageTask* task) {
    task->state = MT_RUNNING;
    g_fsm = FSM_HOST_TX;
    I2C1CON0bits.MODE = 0b100;
    I2C1ADB1 = (uint8_t)(task->addr << 1);
    if (task->rx_len > 0 && task->tx_len > 0) {
        I2C1CON0bits.RSEN = 1; /* repeated-start at end of TX phase */
    } else {
        I2C1CON0bits.RSEN = 0;
    }
    I2C1CNTH = 0;
    I2C1CNTL = task->tx_len;
    i2c_dma_set_host(task);
    I2C1CON0bits.S = 1; /* issue start */
    if (task->tx_len) {
        DMASELECT = DMA_TX_CHANNEL;
        DMAnCON0bits.SIRQEN = 1;
    }
}

I2cResult i2c_submit(uint8_t addr, const uint8_t* tx, uint8_t tx_len, uint8_t rx_len, I2cCompletion cb, void* ctx) {
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
    task->cb_ctx = ctx;
    task->retries = I2C_RETRY_COUNT;
    for (uint8_t i = 0; i < tx_len; i++) {
        task->tx[i] = tx[i];
    }
    task->result = I2C_RESULT_OK; /* overwritten by host_finish on completion */
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
    task->cb_ctx = 0;
    task->retries = 0;
    for (uint8_t i = 0; i < rx_len; i++) {
        task->rx[i] = rx[i];
    }
    task->state = MT_FINISHED;
    task->result = I2C_RESULT_OK;
}

static void host_finish(MessageTaskState final_state, I2cResult result) {
    if (g_fsm != FSM_HOST_TX && g_fsm != FSM_HOST_RX) {
        return;
    }
    MessageTask* task = &g_queue[g_q_head];
    task->state = final_state;
    task->result = result;
    g_fsm = FSM_IDLE;
    I2C1CON0bits.MODE = 0b000;
}

void __interrupt(irq(I2C1), base(8)) I2C1_ISR(void) {
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
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        /* Lost arbitration to a competing master mid-transaction; let
         * the started task retry on the next poll. */
        MessageTask* task = &g_queue[g_q_head];
        task->state = MT_IDLE;
        g_fsm = FSM_IDLE;
        I2C1CON0bits.MODE = 0b000;
    }

    if (g_fsm == FSM_IDLE) {
        const uint8_t has_tx = g_client_tx_len > 0;
        if (I2C1STAT0bits.R && has_tx) {
            /* Master reads from us — shift out preloaded client TX. */
            g_fsm = FSM_CLIENT_TX;
            i2c_dma_client_tx();
            I2C1CON1bits.ACKDT = 0; /* ACK */
        } else if (I2C1STAT0bits.R) {
            /* Master reads from us but we have nothing prepared. */
            I2C1CON1bits.ACKDT = 1; /* NACK */
        } else {
            /* Master writes to us — receive into the cold-rx buffer. */
            g_fsm = FSM_CLIENT_RX;
            i2c_dma_client_rx();
            I2C1CON1bits.ACKDT = 0; /* ACK */
        }
    }
    I2C1CON0bits.CSTR = 0; /* release clock */
}

static void isr_on_stop(void) {
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        host_finish(MT_FINISHED, I2C_RESULT_OK);
    } else if (g_fsm == FSM_CLIENT_TX) {
        g_fsm = FSM_IDLE;
        g_client_tx_len = 0;
    } else if (g_fsm == FSM_CLIENT_RX) {
        /* DMA was set up for I2C_RX_MAX; what we actually received is
         * I2C_RX_MAX minus the DMA destination count remaining. */
        DMASELECT = DMA_RX_CHANNEL;
        uint8_t remaining = (uint8_t)DMAnDCNT;
        uint8_t received = (uint8_t)(I2C_RX_MAX - remaining);
        prepend_completed_task(0x00, g_client_rx, received);
        g_fsm = FSM_IDLE;
        i2c_dma_client_rx(); /* re-arm for the next inbound message */
    }
}

static void isr_on_restart(void) {
    if (g_fsm == FSM_HOST_TX) {
        /* Switching from write to read phase of the host transaction. */
        g_fsm = FSM_HOST_RX;
        MessageTask* task = &g_queue[g_q_head];
        I2C1CNTH = 0;
        I2C1CNTL = task->rx_len;
    } else if (g_fsm == FSM_CLIENT_RX) {
        /* The master issued a repeated start without a stop; treat the
         * accumulated bytes as a cold message and prepare for the next
         * address phase to choose direction again. */
        DMASELECT = DMA_RX_CHANNEL;
        uint8_t remaining = (uint8_t)DMAnDCNT;
        uint8_t received = (uint8_t)(I2C_RX_MAX - remaining);
        if (g_read_request && received > 0) {
            g_read_request(g_client_rx, received);
        }
        prepend_completed_task(0, g_client_rx, received);
        g_fsm = FSM_IDLE;
        i2c_dma_client_rx();
    }
}

static void isr_on_nack(void) {
    if (g_fsm != FSM_HOST_TX && g_fsm != FSM_HOST_RX) {
        return;
    }
    MessageTask* task = &g_queue[g_q_head];
    if (task->retries > 0) {
        task->retries--;
        task->state = MT_IDLE;
        g_fsm = FSM_IDLE;
    } else {
        host_finish(MT_FAILED, I2C_RESULT_NACK);
    }
}

static void isr_on_collision(void) {
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        MessageTask* task = &g_queue[g_q_head];
        task->state = MT_IDLE; /* will be retried by i2c_poll */
        g_fsm = FSM_IDLE;
    }
}

static void isr_on_timeout(void) {
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        host_finish(MT_FAILED, I2C_RESULT_TIMEOUT);
    } else {
        g_fsm = FSM_IDLE;
    }

    I2C1PIR = 0x00;
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.BTOIF = 0;
    I2C1ERRbits.NACKIF = 0;

    I2C1STAT1 = 0x00;
    I2C1STAT1bits.CLRBF = 1;

    I2C1CON0bits.EN = 0;

/* ===== SILICON ERRATA WORKAROUND =====
 * Same sequence as in the init path. Prevents I2C from locking up
 * during reset. */
#pragma message                                                                                                        \
    "Refer to erratum DS80000870F: https://www.microchip.com/content/dam/mchp/documents/MCU08/ProductDocuments/Errata/PIC18F27-47-57Q43-Silicon-Errata-and-Datasheet-Clarifications-80000870J.pdf"
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
}

void __interrupt(irq(I2C1E), base(8)) I2C1_ERROR_ISR(void) {
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
