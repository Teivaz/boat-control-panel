#include "i2c.h"

#include "libcomm.h"

/* Longest inbound message is relay_changed-class (unused here) — 8 B covers
 * every command this board receives. */
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
static I2cResult host_transmit(uint8_t address, const uint8_t* data, uint8_t len);

void i2c_init(void) {
    /* RC3 = SCL, RC4 = SDA. Open-drain, I2C-specific slew + weak pulls. */
    LATCbits.LATC3 = 1;
    LATCbits.LATC4 = 1;
    ANSELCbits.ANSELC3 = 0;
    ANSELCbits.ANSELC4 = 0;
    TRISCbits.TRISC3 = 0;
    TRISCbits.TRISC4 = 0;
    ODCONCbits.ODCC3 = 1;
    ODCONCbits.ODCC4 = 1;
    RC3I2Cbits.TH = 0b01;
    RC4I2Cbits.TH = 0b01;
    RC3I2Cbits.PU = 0b10;
    RC4I2Cbits.PU = 0b10;
    RC3I2Cbits.SLEW = 0b01; // Fast mode 400kHz
    RC4I2Cbits.SLEW = 0b01; // Fast mode 400kHz

    I2C1SCLPPS = 0x13; /* RC3 -> SCL1 */
    RC3PPS = 0x37;
    I2C1SDAPPS = 0x14; /* RC4 -> SDA1 */
    RC4PPS = 0x38;

    I2C1CLK = 0x01; // Use Fosc
    /* 400 kHz: BAUD = 64 MHz / (5 * 400 kHz) - 1 */
    I2C1BAUD = 31; // ~400 kHz

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
 * Host-mode transmit (relay_changed push)
 * ============================================================================
 */

I2cResult i2c_transmit(uint8_t address, const uint8_t* data, uint8_t len) {
    if (len == 0) {
        return I2C_RESULT_OK;
    }

    uint16_t timeout = I2C_POLL_MAX;
    while (!I2C1STAT0bits.BFRE) {
        if (--timeout == 0) {
            return I2C_RESULT_BUSY;
        }
    }

    uint8_t pie7_saved = PIE7;
    PIE7bits.I2C1IE = 0;
    PIE7bits.I2C1EIE = 0;
    PIE7bits.I2C1RXIE = 0;
    PIE7bits.I2C1TXIE = 0;

    I2cResult result = host_transmit(address, data, len);

    client_mode_disable();
    client_mode_enable();
    PIE7 = pie7_saved;
    return result;
}

static I2cResult host_transmit(uint8_t address, const uint8_t* data, uint8_t len) {
    client_mode_disable();
    I2C1CON0bits.MODE = 0b100;
    I2C1CNTH = 0;
    I2C1CNTL = len;
    I2C1ADB1 = (uint8_t)(address << 1);
    I2C1ERRbits.BCLIF = 0;
    I2C1ERRbits.NACKIF = 0;
    I2C1PIRbits.PCIF = 0;
    I2C1CON0bits.EN = 1;
    I2C1CON0bits.S = 1;

    for (uint8_t i = 0; i < len; i++) {
        uint16_t timeout = I2C_POLL_MAX;
        while (!I2C1STAT1bits.TXBE) {
            if (I2C1ERRbits.BCLIF) {
                return I2C_RESULT_COLLISION;
            }
            if (I2C1ERRbits.NACKIF) {
                return I2C_RESULT_NACK;
            }
            if (--timeout == 0) {
                return I2C_RESULT_TIMEOUT;
            }
        }
        I2C1TXB = data[i];
    }

    uint16_t timeout = I2C_POLL_MAX;
    while (!I2C1PIRbits.PCIF) {
        if (I2C1ERRbits.BCLIF) {
            return I2C_RESULT_COLLISION;
        }
        if (I2C1ERRbits.NACKIF) {
            return I2C_RESULT_NACK;
        }
        if (--timeout == 0) {
            return I2C_RESULT_TIMEOUT;
        }
    }
    return I2C_RESULT_OK;
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
