
#include "i2c.h"

#include <xc.h>

#define DMA_TX_CHANNEL 1 // DMA2
#define DMA_RX_CHANNEL 2 // DMA3

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
} MessageTaskState;

typedef struct {
    uint8_t addr;
    uint8_t tx[I2C_TX_MAX];
    uint8_t tx_len;
    uint8_t rx[I2C_RX_MAX];
    uint8_t rx_len;
    MessageTaskState state;
    uint8_t retries;
    I2cCompletion cb;
    void* cb_ctx
} MessageTask;

typedef enum {
    FSM_IDLE,
    FSM_HOST_TX,
    FSM_HOST_RX,
    FSM_CLIENT_TX,
    FSM_CLIENT_RX,
} FSMState;

static volatile I2cCompletion g_cold_rx = 0;
static volatile FSMState g_fsm = FSM_IDLE;
static volatile uint8_t g_client_rx[I2C_RX_MAX] = {0};
static volatile uint8_t g_client_rx_len = 0;
static volatile uint8_t g_client_tx[I2C_TX_MAX] = {0};
static volatile uint8_t g_client_tx_len = 0;
static volatile MessageTask g_queue[I2C_QUEUE_SIZE] = {0};
static volatile uint8_t g_q_head = 0;
static volatile uint8_t g_q_tail = 0;

static void i2c_dma_init(void);
static void i2c_dma_set(MessageTask* task);
static void i2c_dma_rx(void);
static void i2c_dma_tx(void);
static void i2c_start_task(MessageTask* task);
static void prepend_completed_task(uint8_t addr, uint8_t* rx, uint8_t rx_len);
static void i2c_error_reset(void);
static void isr_on_address(void);
static void isr_on_stop(void);
static void isr_on_restart(void);
static void isr_on_nack(void);
static void isr_on_collision(void);
static void isr_on_timeout(void);

void i2c_set_cold_rx_handler(I2cCompletion cold_tx) {
    g_cold_rx = colt_tx;
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
    DMASELECT = DMA_TX_CHANNEL;
    DMAnCON1bits.DMODE = 0b00; /* Destination Pointer (DMADPTR) remains unchanged after each transfer */
    DMAnCON1bits.DSTP = 0;     /* SIRQEN bit is not cleared when destination counter reloads */
    DMAnCON1bits.SMR = 0b01;   /* Program Flash Memory is selected as the DMA source memory */
    DMAnCON1bits.SMODE = 0b01; /* Source Pointer (DMASPTR) is incremented after each transfer */
    DMAnCON1bits.SSTP = 1;     /* clear SIRQEN on completion */
    DMAnDSZ = 1;
    DMAnDSA = (uint16_t)&I2C1TXB;
    DMAnSSZ = 0x00;
    DMAnSSA = (uint24_t)0x00;
    DMAnSIRQ = 0x39; // I2C1TX
    DMAnAIRQ = 0;
    // Change arbiter priority if needed and perform lock operation
    DMA1PR = 0x01;           // Change the priority only if needed
    PRLOCK = 0x55;           // This sequence
    PRLOCK = 0xAA;           // is mandatory
    PRLOCKbits.PRLOCKED = 1; // for DMA operation   
    DMAnCON0bits.EN = 1;

    DMASELECT = DMA_RX_CHANNEL;
    DMAnCON1bits.DMODE = 0b01; /* Destination Pointer (DMADPTR) is incremented after each transfer */
    DMAnCON1bits.DSTP = 1;     /* SIRQEN bit is cleared when destination counter reloads */
    DMAnCON1bits.SMR = 0b00;   /* SFR/GPR data space is selected as the DMA source memory */
    DMAnCON1bits.SMODE = 0b00; /* Source Pointer (DMASPTR) remains unchanged after each transfer */
    DMAnCON1bits.SSTP = 0;     /* SIRQEN bit is not cleared when source counter reloads */
    DMAnDSZ = 0;
    DMAnDSA = (uint16_t)g_rx_buf;
    DMAnSSZ = 1;
    DMAnSSA = (uint24_t)&I2C1RXB;
    DMAnSIRQ = 0x38; // I2C1TX
    DMAnAIRQ = 0;
    // Change arbiter priority if needed and perform lock operation
    DMA1PR = 0x01;           // Change the priority only if needed
    PRLOCK = 0x55;           // This sequence
    PRLOCK = 0xAA;           // is mandatory
    PRLOCKbits.PRLOCKED = 1; // for DMA operation   
    DMAnCON0bits.EN = 1;
}

// Call this function when the device has won th
static void i2c_dma_set(MessageTask* task) {
    INTERRUPT_PUSH;
    if (task.tx_len) {
        DMASELECT = DMA_TX_CHANNEL;
        DMAnSSA = (uint24_t)task.tx;
        DMAnSSZ = task.tx_len;
        DMAnCON0bits.SIRQEN = 1;
    }
    if (task.rx_len) {
        DMASELECT = DMA_RX_CHANNEL;
        DMAnDSA = (uint16_t)task.rx;
        DMAnDSZ = task.tx_len;
        DMAnCON0bits.SIRQEN = 1;
    }
    INTERRUPT_POP;
}

static void i2c_dma_rx(void) {
    INTERRUPT_PUSH;
    DMASELECT = DMA_RX_CHANNEL;
    DMAnDSA = (uint16_t)g_client_rx_buf;
    DMAnDSZ = I2C_RX_MAX;
    DMAnCON0bits.SIRQEN = 1;
    I2CxCNT = I2C_RX_MAX;
    INTERRUPT_POP;
}

static void i2c_dma_tx(void) {
    if (g_client_tx_len == 0) {
        return;
    }
    INTERRUPT_PUSH;
    DMASELECT = DMA_TX_CHANNEL;
    DMAnDSA = (uint16_t)g_client_tx_buf;
    DMAnDSZ = g_client_tx_len;
    DMAnCON0bits.SIRQEN = 1;
    I2CxCNT = g_client_tx_len;
    INTERRUPT_POP;
}

void i2c_init(uint8_t addr) {
    g_client_rx_len = 0;
    g_client_tx_len = 0;
    g_fsm = FSM_IDLE;
    g_q_head = 0;
    g_q_tail = 0;

    i2c_dma_init();
    i2c_dma_rx();
    i2c_dma_tx();

    I2C1CON0bits.EN = 0;
    I2C1CON0bits.MODE = 0b100; // Host 7 bit
    I2C1CLK = 0x01;            /* FOSC */
    I2C1BAUD = I2C_BAUD;
    I2C1CON1bits.CSD = 0; // For multi master mode clock stretch should be enabled
    I2C1CON2bits.FME = I2C_FME;
    I2C1CON2bits.BFRET = 0b00; // 8 cycles for BFRE

    const uint8_t a = (uint8_t)(addr << 1);
    I2C1ADR0 = a;
    I2C1ADR1 = a;
    I2C1ADR2 = a;
    I2C1ADR3 = a;

    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.RSCIE = 1;
    I2C1PIEbits.ADRIE = 1;

    I2C1BTOC = 0x06;         // LFINTOSC as BTO clock source
    I2C1BTObits.TOBY32 = 1;  // time x 32
    I2C1BTObits.TOTIME = 35; // reset buts after ~35 ms

    I2C1ERRbits.BLCIE = 1;
    I2C1ERRbits.NACKIE = 1;
    I2C1ERRbits.BTOIE = 1;

    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;

    I2C1CON0bits.EN = 1;
}

void i2c_poll(void) {
    if (g_q_head != g_q_tail) {
        MessageTask* task = &g_queue[g_head];
        if (task->state == MT_FINISHED) {
            if (task->cb) {
                task->cb(task->rx, task->rx_len, task->cb_ctx);
            }
            INTERRUPT_PUSH;
            g_q_head = q_next(g_q_head);
            INTERRUPT_POP;
        }
    }

    if (g_q_head != g_q_tail) {
        MessageTask* task = &g_queue[g_head];
        INTERRUPT_PUSH;
        if (task->state == MT_IDLE) {
            i2c_start_task(task);
        }
        INTERRUPT_POP;
    }
}

static void i2c_start_task(MessageTask* task) {
    task->state = MT_RUNNING;
    I2C1ADB1 = (uint8_t)(task->addr << 1);
    if (task->rx_len > 0 && task->tx_len > 0) {
        I2C1CON0bits.RSEN = 1;
    }
    i2c_dma_set(task);
}

// Should be submitted from main loop
I2cResult i2c_submit(uint8_t addr, const uint8_t* tx, uint8_t tx_len, uint8_t rx_len, I2cCompletion cb, void* ctx) {
    if (tx == 0) {
        return I2C_RESULT_BAD_ARG;
    }
    if (tx_len == 0 || tx_len > I2C_TX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    if (rx_len > 0 && rx == 0) {
        return I2C_RESULT_BAD_ARG;
    }

    INTERRUPT_PUSH;
    if (q_next(g_q_tail) == g_q_head) {
        INTERRUPT_POP;
        return I2C_RESULT_QUEUE_FULL;
    }
    MessageTask* task = &g_queue[g_q_tail];
    task->state = MT_IDLE;
    g_q_tail = q_next(g_q_tail);
    INTERRUPT_POP;

    task->addr = addr;
    task->tx_len = tx_len;
    task->rx_len = rx_len;
    task->cb = cb;
    task->cb_ctx = ctx;
    task->retries = I2C_RETRY_COUNT;
    for (uint8_t i = 0; i < tx_len; i++) {
        task->tx[i] = tx[i];
    }

    return I2C_RESULT_OK;
}

static void prepend_completed_task(uint8_t addr, uint8_t* rx, uint8_t rx_len) {
    if (rx == 0) {
        return I2C_RESULT_BAD_ARG;
    }
    if (rx_len == 0 || rx_len > I2C_RX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }

    if (q_prev(g_q_head) == g_q_tail) {
        return I2C_RESULT_QUEUE_FULL;
    }
    g_q_head = q_prev(g_q_head);
    MessageTask* task = &g_queue[g_q_head];
    task->state = MT_FINISHED;

    task->addr = addr;
    task->tx_len = 0;
    task->rx_len = rx_len;
    task->cb = g_cold_rx;
    task->cb_ctx = 0;
    task->retries = I2C_RETRY_COUNT;
    for (uint8_t i = 0; i < rx_len; i++) {
        task->rx[i] = rx[i];
    }

    return I2C_RESULT_OK;
}

static void i2c_error_reset(void) {
    // TODO: Clear all errors
    I2C1STAT1bits.CLRBF = 0;
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

    if (I2C1PIEbits.RSCIE && I2C1PIRbits.RSCIF) {
        // RSCIF - Repeated Start Condition
        I2C1PIRbits.RSCIF = 0;
        isr_on_restart();
        return;
    }

    if (I2C1PIEbits.ACKTIE && I2C1PIRbits.ACKTIF) {
        // ACKTIF - Acknowledge Timeout
        I2C1PIRbits.ACKTIF = 0;
        I2C1CON0bits.CSTR = 0;
        return;
    }
}

static void isr_on_address(void) {
    if (g_fsm == FSM_HOST_TX) {
        // Reset started task
        MessageTask* task = &g_queue[g_head];
        task->state = MT_IDLE;
        g_fsm = FSM_IDLE;
    }

    if (g_fsm == FSM_IDLE) {
        const uint8_t has_tx = g_client_tx_len > 0;
        if (I2C1STAT0bits.R && has_tx) {
            // We are a client that has to transmit and has data
            g_fsm = FSM_CLIENT_RX;
            i2c_dma_tx();
            I2C1CON1bits.ACKDT = 0; // ACK
        } else if (I2C1STAT0bits.R && !has_tx) {
            // We are a client that has to transmit but has no data
            g_fsm = FSM_IDLE;
            I2C1CON1bits.ACKDT = 1; // NACK
        } else {
            // We are a client that has to recieve
            g_fsm = FSM_IDLE;
            i2c_dma_rx();
            I2C1CON1bits.ACKDT = fsm_client_rx();
        }
    }
    I2C1CON0bits.CSTR = 0; // Release clock
}

static void isr_on_stop(void) {
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        g_fsm = FSM_IDLE;
        MessageTask* task = &g_queue[g_head];
        task->state = MT_FINISHED;
    } else if (g_fsm == FSM_CLIENT_TX) {
        g_fsm = FSM_IDLE;
        g_client_tx_len = 0;
    } else if (g_fsm == FSM_CLIENT_RX) {
        g_fsm = FSM_IDLE;
        prepend_completed_task(0, g_client_rx, g_client_rx_len);
    }
    i2c_error_reset();
}

static void isr_on_restart(void) {
    if (g_fsm == FSM_HOST_TX) {
        // if have pending operation, then host is switching from write to read
        g_fsm == FSM_HOST_RX;
        MessageTask* task = &g_queue[g_head];
        I2C1CNT = task->rx_len;
    } else if (g_fsm == FSM_CLIENT_RX) {
        // If we are client, then we need to process isr_on_stop()
        g_fsm = FSM_IDLE;
        prepend_completed_task(0, g_client_rx, g_client_rx_len);
    }
}

static void isr_on_nack(void) {
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        MessageTask* task = &g_queue[g_head];
        if (--task->retries > 0) {
            // Retry the task
            g_fsm = FSM_IDLE;
            task->state = MT_IDLE;
        } else {
            // Drop the task
            g_q_head = q_next(g_q_head);
        }
    }
}

static void isr_on_collision(void) {
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        MessageTask* task = &g_queue[g_head];
        task->state = MT_IDLE;
        g_fsm = FSM_IDLE;
    }
}

static void isr_on_timeout(void) {
    if (g_fsm == FSM_HOST_TX || g_fsm == FSM_HOST_RX) {
        MessageTask* task = &g_queue[g_head];
        task->state = MT_IDLE;
        g_fsm = FSM_IDLE;
    }
    g_client_rx_len = 0;
    g_client_tx_len = 0;

    I2C1PIR = 0x00;         // Clear protocol interrupt flags
    I2C1ERRbits.BCLIF = 0;  // Clear bus collision flag
    I2C1ERRbits.BTOIF = 0;  // Clear bus timeout flag
    I2C1ERRbits.NACKIF = 0; // Clear NACK flag

    // Clear status register and buffers
    I2C1STAT1 = 0x00;
    I2C1STAT1bits.CLRBF = 1; // Clear all buffers

    I2C1CON0bits.EN = 0;

// ===== SILICON ERRATA WORKAROUND =====
// Same sequence as in I2C1_Initialize()
// Prevents I2C module from locking up during reset
#pragma message                                                                                                        \
    "Refer to erratum DS80000870F: https://www.microchip.com/content/dam/mchp/documents/MCU08/ProductDocuments/Errata/PIC18F27-47-57Q43-Silicon-Errata-and-Datasheet-Clarifications-80000870J.pdf"
    I2C1PIEbits.SCIE = 0;
    I2C1PIEbits.PCIE = 0;
    // Re-enable I2C module
    I2C1CON0bits.EN = 1;
    // Wait 1 microsecond for hardware to settle
    __delay_us(1);

    // Execute 6 NOP instructions for timing stability
    __nop();
    __nop();
    __nop();
    __nop();
    __nop();
    __nop();

    // Clear spurious Start and Stop condition flags
    I2C1PIRbits.SCIF = 0; // Clear Start Condition Interrupt Flag
    I2C1PIRbits.PCIF = 0; // Clear Stop Condition Interrupt Flag
    I2C1PIEbits.PCIE = 1;
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
