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
    I2C1PIR = 0x00;        // Clear protocol interrupt flags
    I2C1ERRbits.BCLIF = 0; // Clear bus collision flag
    I2C1ERRbits.BTOIF = 0; // Clear bus timeout flag
    I2C1ERRbits.NACKIF = 0;// Clear NACK flag

    // Clear status register and buffers
    I2C1STAT1 = 0x00;
    I2C1STAT1bits.CLRBF = 1;  // Clear all buffers
    
    I2C1CON0bits.EN = 0;

    PIE7bits.I2C1IE = 1;
    PIE7bits.I2C1EIE = 1;
    PIE7bits.I2C1RXIE = 1;
    PIE7bits.I2C1TXIE = 1;

    I2C1PIEbits.PCIE = 1;
    I2C1PIEbits.RSCIE = 1;
    I2C1PIEbits.CNTIE = 1;
    I2C1ERRbits.NACKIE = 1;

    // ===== SILICON ERRATA WORKAROUND =====
    // Same sequence as in I2C1_Initialize()
    // Prevents I2C module from locking up during reset
    #pragma message "Refer to erratum DS80000870F: https://www.microchip.com/content/dam/mchp/documents/MCU08/ProductDocuments/Errata/PIC18F27-47-57Q43-Silicon-Errata-and-Datasheet-Clarifications-80000870J.pdf"
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
    I2C1PIRbits.SCIF = 0;   // Clear Start Condition Interrupt Flag
    I2C1PIRbits.PCIF = 0;   // Clear Stop Condition Interrupt Flag
    I2C1PIEbits.PCIE = 1;
}
