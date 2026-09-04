#include "bus.h"

#include "i2c.h"
#include "test_support.h"

#include <xc.h>

#if BUS_CAN_MOVE_BYTES
void I2C1TX_ISR(void);
void I2C1RX_ISR(void);
#endif

void bus_reset(uint8_t addr) {
    test_reset_hardware();
    i2c_init(addr);
    /* i2c_init leaves the module enabled and parked in the client role. */
}

void bus_count_zero(void) {
    I2C1PIRbits.CNTIF = 1;
    I2C1_ISR();
}

void bus_address_match(uint8_t read) {
    I2C1STAT0bits.R = read ? 1 : 0;
    I2C1STAT0bits.SMA = 1; /* set by hardware on the same 8th falling edge */
    I2C1PIRbits.ADRIF = 1;
    I2C1_ISR();
}

void bus_stop(void) {
    I2C1STAT0bits.SMA = 0; /* cleared by hardware on Stop (§37.5.4) */
    I2C1PIRbits.PCIF = 1;
    I2C1_ISR();
}

void bus_restart(void) {
    I2C1STAT0bits.SMA = 0; /* cleared by hardware on Restart too */
    I2C1PIRbits.RSCIF = 1;
    I2C1_ISR();
}

void bus_nack(void) {
    I2C1ERRbits.NACKIF = 1;
    I2C1_ERROR_ISR();
}

void bus_collision(void) {
    I2C1ERRbits.BCLIF = 1;
    I2C1_ERROR_ISR();
}

void bus_timeout(void) {
    I2C1ERRbits.BTOIF = 1;
    I2C1_ERROR_ISR();
}

uint8_t bus_deliver_rx(const uint8_t* bytes, uint8_t len) {
#if BUS_CAN_MOVE_BYTES
    for (uint8_t i = 0; i < len; i++) {
        I2C1RXB = bytes[i];
        I2C1RX_ISR();
    }
    return 1;
#else
    (void)bytes;
    (void)len;
    return 0;
#endif
}

uint8_t bus_collect_tx(uint8_t* out, uint8_t max) {
#if BUS_CAN_MOVE_BYTES
    uint8_t n = 0;
    /* Byte 0 is written directly to I2CxTXB by the address handler; every
     * later byte arrives when the peripheral raises TXIF as the previous one
     * moves out of the buffer. */
    while (n < max) {
        out[n++] = I2C1TXB;
        if (I2C1CNTL == 0 && I2C1CNTH == 0) {
            break;
        }
        /* Emulate the 9th falling edge: TXB -> shift register, CNT down. */
        bus_set_cnt((uint16_t)(((uint16_t)I2C1CNTH << 8) | I2C1CNTL) - 1u);
        if (I2C1CNTL == 0 && I2C1CNTH == 0) {
            break;
        }
        I2C1TX_ISR();
    }
    return n;
#else
    (void)out;
    (void)max;
    return 0;
#endif
}

void bus_set_rx_remaining(uint8_t remaining) {
    DMASELECT = 2; /* DMA3, the driver's receive channel */
    DMAnDCNT = remaining;
}

void bus_set_cnt(uint16_t value) {
    I2C1CNTH = (uint8_t)(value >> 8);
    I2C1CNTL = (uint8_t)value;
}
