#define _XTAL_FREQ 64000000UL

#include "i2c_board.h"

#include "i2c.h"
#include "libcomm.h"

#include <xc.h>

#define I2C_RECOVER_HALF_US 5

void i2c_pins_init(void) {
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
}

void i2c_bus_recover(void) {
    INTERRUPT_PUSH;
    uint8_t pie7_saved = PIE7;
    PIE7bits.I2C1IE = 0;
    PIE7bits.I2C1EIE = 0;
    PIE7bits.I2C1RXIE = 0;
    PIE7bits.I2C1TXIE = 0;

    I2C1CON0bits.EN = 0;

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

    /* Re-init the I2C module after bit-bang recovery. */
    i2c_init(comm_address());

    PIE7 = pie7_saved;
    INTERRUPT_POP;
}
