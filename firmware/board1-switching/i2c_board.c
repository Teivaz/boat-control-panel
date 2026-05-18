#define _XTAL_FREQ 64000000UL

#include "i2c_board.h"

#include <xc.h>

void i2c_pins_init(void) {
    LATCbits.LATC3 = 1;
    LATCbits.LATC4 = 1;
    ANSELCbits.ANSELC3 = 0;
    ANSELCbits.ANSELC4 = 0;
    TRISCbits.TRISC3 = 0;
    TRISCbits.TRISC4 = 0;
    ODCONCbits.ODCC3 = 1;
    ODCONCbits.ODCC4 = 1;
#if I2C_FME
    RC3I2Cbits.SLEW = 0b01;
    RC4I2Cbits.SLEW = 0b01;
#else
    SLRCONCbits.SLRC3 = 0b0;
    SLRCONCbits.SLRC4 = 0b0;
    RC3I2Cbits.SLEW = 0b00;
    RC4I2Cbits.SLEW = 0b00;
#endif
    RC3I2Cbits.PU = 0b10;
    RC4I2Cbits.PU = 0b10;
    RC3I2Cbits.TH = 0b01;
    RC4I2Cbits.TH = 0b01;

    I2C1SCLPPS = 0x13; /* RC3 -> SCL1 */
    RC3PPS = 0x37;
    I2C1SDAPPS = 0x14; /* RC4 -> SDA1 */
    RC4PPS = 0x38;
}

void i2c_bus_recover(void) {
}
