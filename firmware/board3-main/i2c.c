#include "i2c.h"

#include "libcomm.h"

/* Inbound: button_changed (4 B) and relay_changed (8 B) are the longest. */
#define I2C_BUF_SIZE 8

#define I2C_POLL_MAX 10000u

typedef enum {
    STATE_IDLE,
    STATE_RX,
    STATE_TX,
} ClientState;

static volatile ClientState state;
static volatile uint8_t rx_buf[I2C_BUF_SIZE];
static volatile uint8_t rx_len;
static volatile uint8_t tx_buf[I2C_BUF_SIZE];
static volatile uint8_t tx_len;
static volatile uint8_t tx_pos;

static I2cRxHandler rx_handler;
static I2cReadHandler read_handler;

static void client_mode_enable(void);
static void client_mode_disable(void);
static void reset_state(void);
static void handle_event(void);
static void handle_error(void);
static uint8_t host_begin(uint8_t pie7_saved_out[1]);
static void host_end(uint8_t pie7_saved);

void i2c_init(void) {
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
    RB1I2Cbits.PU = 0b01;
    RB2I2Cbits.PU = 0b01;

    I2C1SCLPPS = 0x0A; /* RB2 -> SCL1 */
    RB2PPS = 0x37;
    I2C1SDAPPS = 0x09; /* RB1 -> SDA1 */
    RB1PPS = 0x38;

    /* 400 kHz: BAUD = 64 MHz / (2 * 400 kHz) - 1 = 79 */
    I2C1BAUD = 79;

    client_mode_enable();
}

void i2c_set_rx_handler(I2cRxHandler h) {
    rx_handler = h;
}
void i2c_set_read_handler(I2cReadHandler h) {
    read_handler = h;
}

/* ============================================================================
 * Client-mode ISR entry points
 * ============================================================================
 */

void __interrupt(irq(I2C1TX, I2C1RX, I2C1), base(8)) I2C1_ISR(void) {
    handle_event();
}

void __interrupt(irq(I2C1E), base(8)) I2C1_ERROR_ISR(void) {
    handle_error();
}

static void handle_event(void) {
    if (I2C1PIRbits.PCIF) {
        I2C1PIRbits.PCIF = 0;
        I2C1STAT1bits.CLRBF = 1;
        I2C1CNTL = 0;
        I2C1CNTH = 0;
        I2C1CON1bits.ACKDT = 0;

        if (state == STATE_RX && rx_len > 0 && rx_handler) {
            rx_handler((const uint8_t*)rx_buf, rx_len);
        }
        reset_state();
    } else if (I2C1PIRbits.ADRIF) {
        I2C1PIRbits.ADRIF = 0;

        if (I2C1STAT0bits.R) {
            tx_pos = 0;
            tx_len = 0;
            if (read_handler) {
                tx_len = read_handler((const uint8_t*)rx_buf, rx_len, (uint8_t*)tx_buf, I2C_BUF_SIZE);
            }
            state = STATE_TX;
        } else {
            rx_len = 0;
            state = STATE_RX;
        }
    } else if (I2C1STAT0bits.R) {
        if (I2C1STAT1bits.TXBE && !I2C1CON1bits.ACKSTAT) {
            I2C1TXB = (tx_pos < tx_len) ? tx_buf[tx_pos++] : 0;
        }
    } else {
        if (I2C1STAT1bits.RXBF) {
            uint8_t byte = I2C1RXB;
            if (rx_len < I2C_BUF_SIZE) {
                rx_buf[rx_len++] = byte;
            }
            I2C1CON1bits.ACKDT = 0;
            I2C1PIRbits.ACKTIF = 0;
        }
    }

    I2C1CON0bits.CSTR = 0;
}

static void handle_error(void) {
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

    reset_state();
    I2C1STAT1bits.CLRBF = 1;
    I2C1CON0bits.CSTR = 0;
}

/* ============================================================================
 * Host-mode transactions
 * ============================================================================
 */

static uint8_t host_begin(uint8_t pie7_saved_out[1]) {
    uint16_t timeout = I2C_POLL_MAX;
    while (!I2C1STAT0bits.BFRE) {
        if (--timeout == 0) {
            return 0;
        }
    }
    pie7_saved_out[0] = PIE7;
    PIE7bits.I2C1IE = 0;
    PIE7bits.I2C1EIE = 0;
    PIE7bits.I2C1RXIE = 0;
    PIE7bits.I2C1TXIE = 0;
    client_mode_disable();
    return 1;
}

static void host_end(uint8_t pie7_saved) {
    client_mode_disable();
    client_mode_enable();
    PIE7 = pie7_saved;
}

I2cResult i2c_transmit(uint8_t address, const uint8_t* data, uint8_t len) {
    if (len == 0) {
        return I2C_RESULT_OK;
    }

    uint8_t pie7_saved;
    if (!host_begin(&pie7_saved)) {
        return I2C_RESULT_BUSY;
    }

    I2C1CON0bits.MODE = 0b100;
    I2C1CNTH = 0;
    I2C1CNTL = len;
    I2C1ADB1 = (uint8_t)(address << 1);
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1CON0bits.EN = 1;
    I2C1CON0bits.S = 1;

    I2cResult result = I2C_RESULT_OK;
    for (uint8_t i = 0; i < len; i++) {
        uint16_t timeout = I2C_POLL_MAX;
        while (!I2C1STAT1bits.TXBE) {
            if (I2C1ERRbits.BCLIF) {
                result = I2C_RESULT_COLLISION;
                goto done;
            }
            if (I2C1ERRbits.NACKIF) {
                result = I2C_RESULT_NACK;
                goto done;
            }
            if (--timeout == 0) {
                result = I2C_RESULT_TIMEOUT;
                goto done;
            }
        }
        I2C1TXB = data[i];
    }

    {
        uint16_t timeout = I2C_POLL_MAX;
        while (!I2C1PIRbits.PCIF) {
            if (I2C1ERRbits.BCLIF) {
                result = I2C_RESULT_COLLISION;
                break;
            }
            if (I2C1ERRbits.NACKIF) {
                result = I2C_RESULT_NACK;
                break;
            }
            if (--timeout == 0) {
                result = I2C_RESULT_TIMEOUT;
                break;
            }
        }
    }

done:
    host_end(pie7_saved);
    return result;
}

/* Write-phase first (tx/tx_count), repeated-start, then read rx_count bytes.
 * Both phases run under the same bus lock; client ISRs are masked throughout.
 */
I2cResult i2c_receive(uint8_t address, const uint8_t* tx, uint8_t tx_count, uint8_t* rx, uint8_t rx_count) {
    if (rx_count == 0) {
        return I2C_RESULT_OK;
    }

    uint8_t pie7_saved;
    if (!host_begin(&pie7_saved)) {
        return I2C_RESULT_BUSY;
    }

    I2cResult result = I2C_RESULT_OK;

    I2C1CON0bits.MODE = 0b100;
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1CON0bits.EN = 1;

    /* Write phase (suppresses the stop via RSEN=1 so the next start is a
     * repeated start). If tx_count == 0 we still need an addressed-write start
     * so callers who only want a read should pass tx_count=0 with an empty tx —
     * we skip the write frame in that case and just do an address-read. */
    if (tx_count > 0) {
        I2C1CNTH = 0;
        I2C1CNTL = tx_count;
        I2C1ADB1 = (uint8_t)(address << 1);
        I2C1CON0bits.RSEN = 1;
        I2C1CON0bits.S = 1;

        for (uint8_t i = 0; i < tx_count; i++) {
            uint16_t timeout = I2C_POLL_MAX;
            while (!I2C1STAT1bits.TXBE) {
                if (I2C1ERRbits.BCLIF) {
                    result = I2C_RESULT_COLLISION;
                    goto done;
                }
                if (I2C1ERRbits.NACKIF) {
                    result = I2C_RESULT_NACK;
                    goto done;
                }
                if (--timeout == 0) {
                    result = I2C_RESULT_TIMEOUT;
                    goto done;
                }
            }
            I2C1TXB = tx[i];
        }

        /* Wait for the byte count to drain before issuing the repeated start.
         */
        {
            uint16_t timeout = I2C_POLL_MAX;
            while (I2C1CNTL != 0 || !I2C1STAT1bits.TXBE) {
                if (I2C1ERRbits.BCLIF) {
                    result = I2C_RESULT_COLLISION;
                    goto done;
                }
                if (I2C1ERRbits.NACKIF) {
                    result = I2C_RESULT_NACK;
                    goto done;
                }
                if (--timeout == 0) {
                    result = I2C_RESULT_TIMEOUT;
                    goto done;
                }
            }
        }
    }

    /* Read phase. */
    I2C1CON0bits.RSEN = 0;
    I2C1CNTH = 0;
    I2C1CNTL = rx_count;
    I2C1ADB1 = (uint8_t)((address << 1) | 0x01);
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1CON0bits.S = 1;

    for (uint8_t i = 0; i < rx_count; i++) {
        uint16_t timeout = I2C_POLL_MAX;
        while (!I2C1STAT1bits.RXBF) {
            if (I2C1ERRbits.BCLIF) {
                result = I2C_RESULT_COLLISION;
                goto done;
            }
            if (I2C1ERRbits.NACKIF) {
                result = I2C_RESULT_NACK;
                goto done;
            }
            if (--timeout == 0) {
                result = I2C_RESULT_TIMEOUT;
                goto done;
            }
        }
        rx[i] = I2C1RXB;
    }

    {
        uint16_t timeout = I2C_POLL_MAX;
        while (!I2C1PIRbits.PCIF) {
            if (I2C1ERRbits.BCLIF) {
                result = I2C_RESULT_COLLISION;
                break;
            }
            if (--timeout == 0) {
                result = I2C_RESULT_TIMEOUT;
                break;
            }
        }
    }

done:
    host_end(pie7_saved);
    return result;
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
    reset_state();

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

static void reset_state(void) {
    state = STATE_IDLE;
    rx_len = 0;
    tx_len = 0;
    tx_pos = 0;
}
