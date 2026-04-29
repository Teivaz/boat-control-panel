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
#if I2C_FME
    RB1I2Cbits.PU = 0b11;
    RB2I2Cbits.PU = 0b11;
#else
    RB1I2Cbits.PU = 0b10;
    RB2I2Cbits.PU = 0b10;
#endif
    RB1I2Cbits.SLEW = 0b01;
    RB2I2Cbits.SLEW = 0b01;

    I2C1SCLPPS = 0x0A; /* RB2 -> SCL1 */
    RB2PPS = 0x37;
    I2C1SDAPPS = 0x09; /* RB1 -> SDA1 */
    RB1PPS = 0x38;
}

void i2c_bus_recover(void) {
}
