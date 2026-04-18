#include "rgbled.h"

#define RGBLED_COUNT 7
#define RGBLED_BUFFER_LEN ((uint8_t)(4 * 3 * RGBLED_COUNT))

uint8_t _rgbled_buffer[RGBLED_BUFFER_LEN] = {0};

void _byte_to_sequence(uint8_t input_byte, uint8_t *dest)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        uint8_t output_byte = 0;
        for (uint8_t n = 0; n < 2; n++)
        {
            uint8_t bit = !!(input_byte & 0b10000000);
            input_byte <<= 1;
            uint8_t nibble = 0b0001 | (bit * 0b0110);
            output_byte |= nibble << (n * 4);
        }
        *dest++ = output_byte;
    }
}

/// Returns the size of the buffer
uint16_t _copy_to_buffer(RGBLedData *rgb, uint8_t count)
{
    // TODO: Synchronize
    uint8_t *iptr = (uint8_t *)rgb;
    uint8_t *iend = iptr + count * sizeof(RGBLedData);
    uint8_t *optr = _rgbled_buffer;
    uint8_t *oend = _rgbled_buffer + RGBLED_BUFFER_LEN;
    while (iptr < iend && optr < oend)
    {
        _byte_to_sequence(*iptr, optr);
        iptr += 1;
        optr += 4;
    }
    return (uint16_t)(optr - _rgbled_buffer);
}

void rgbled_spi_init(void)
{
    // Configure RC2 as SDO (output)
    LATCbits.LATC2 = 0;
    TRISCbits.TRISC2 = 0;   // RC2 is output
    ANSELCbits.ANSELC2 = 0; // RC2 is digital

    // PPS Configuration - RC2 is SDO1
    RC2PPS = 0x32; // RC2 = SPI1 SDO

    SPI1CON0bits.EN = 0; // Disable SPI during configuration

    SPI1CLK = 0x00;        // Fosc
    SPI1CON0bits.LSBF = 1; // SPI with standard mode (LSB first)
    SPI1CON0bits.MST = 1;
    SPI1CON0bits.BMODE = 0;

    SPI1CON2bits.RXR = 0;
    SPI1CON2bits.TXR = 1; // Transmit only mode

    SPI1STATUSbits.TXBE = 1;

    /* Calculate Baud Rate
     * Tbit = 312.5 ns => Fbit = 3.2 MHz
     * BAUD = (FOSC / 2 * Fbit) - 1
     * For 3.2 Mbps: (64 / 6.4) - 1 = 9
     */
    SPI1BAUD = 9;

    SPI1INTF = 0;
    // Enable SPI1
    SPI1CON0bits.EN = 1;
}

void rgbled_dma_init(void)
{
    DMASELECT = 0;             // Select DMA1
    DMAnCON1bits.DMODE = 0b00; // 0b00 => Destination Pointer (DMADPTR) remains unchanged after each transfer
    DMAnCON1bits.SMR = 0b00;   // 0b00 => SFR/GPR data space is DMA source memory
    DMAnCON1bits.SMODE = 0b01; // 0b01 => Source pointer increments
    DMAnCON1bits.SSTP = 1;     // 1 => Clear SIRQEN once all data transferred
    DMAnSSZ = RGBLED_BUFFER_LEN;
    DMAnSSA = (uint24_t)_rgbled_buffer;
    DMAnDSZ = 1;
    DMAnDSA = (uint16_t)&SPI1TXB;
    DMAnSIRQ = 0x19; // 0x19 => SPI1TX
    DMAnAIRQ = 0;
    // Change arbiter priority if needed and perform lock operation
    DMA1PR = 0x01;           // Change the priority only if needed
    PRLOCK = 0x55;           // This sequence
    PRLOCK = 0xAA;           // is mandatory
    PRLOCKbits.PRLOCKED = 1; // for DMA operation   
    DMAnCON0bits.EN = 1;
}

void rgbled_init(void)
{
    rgbled_spi_init();
    rgbled_dma_init();
}

void rgbled_set(RGBLedData *rgb, uint8_t count)
{
    DMASELECT = 0;
    if (DMAnCON0bits.XIP != 0)
    {
        return;
    }

    uint16_t byte_count = _copy_to_buffer(rgb, count);

    SPI1TCNT = byte_count;
    DMAnSSZ = byte_count;
    DMAnCON0bits.SIRQEN = 1;
}
