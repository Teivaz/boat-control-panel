#include "rgbled.h"

/* 5 status LEDs on RB5, driven by the same LTST-E563C WS2812-like encoding
 * used on board2-buttons: SPI1 shifts out at 3.2 Mbps and each input bit is
 * expanded to a 4-bit nibble so the 312.5 ns SPI period produces the
 * correct high/low pulse widths. DMA1 feeds SPI1TXB from the expanded
 * buffer so writes are non-blocking. */

#define RGBLED_COUNT 5
#define RGBLED_BUFFER_LEN ((uint8_t)(4 * 3 * RGBLED_COUNT))

static uint8_t _rgbled_buffer[RGBLED_BUFFER_LEN] = {0};

static void _byte_to_sequence(uint8_t input_byte, uint8_t *dest)
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

static uint16_t _copy_to_buffer(RGBLedData *rgb, uint8_t count)
{
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

static void rgbled_spi_init(void)
{
    /* RB5 as SDO1 output. */
    LATBbits.LATB5 = 0;
    TRISBbits.TRISB5 = 0;
    ANSELBbits.ANSELB5 = 0;

    RB5PPS = 0x32; /* RB5 = SPI1 SDO */

    SPI1CON0bits.EN = 0;

    SPI1CLK = 0x00;        /* Fosc */
    SPI1CON0bits.LSBF = 1; /* LSB first */
    SPI1CON0bits.MST = 1;
    SPI1CON0bits.BMODE = 0;

    SPI1CON2bits.RXR = 0;
    SPI1CON2bits.TXR = 1; /* Transmit-only */

    SPI1STATUSbits.TXBE = 1;

    /* 3.2 Mbps: BAUD = (FOSC / (2 * Fbit)) - 1 = 9 */
    SPI1BAUD = 9;

    SPI1INTF = 0;
    SPI1CON0bits.EN = 1;
}

static void rgbled_dma_init(void)
{
    DMASELECT = 0;             /* DMA1 */
    DMAnCON1bits.DMODE = 0b00; /* destination stays fixed */
    DMAnCON1bits.SMR = 0b00;   /* source in SFR/GPR space   */
    DMAnCON1bits.SMODE = 0b01; /* source auto-increments    */
    DMAnCON1bits.SSTP = 1;     /* stop SIRQEN on completion */
    DMAnSSZ = RGBLED_BUFFER_LEN;
    DMAnSSA = (uint24_t)_rgbled_buffer;
    DMAnDSZ = 1;
    DMAnDSA = (uint16_t)&SPI1TXB;
    DMAnSIRQ = 0x19; /* SPI1TX */
    DMAnAIRQ = 0;
    DMA1PR = 0x01;
    PRLOCK = 0x55;
    PRLOCK = 0xAA;
    PRLOCKbits.PRLOCKED = 1;
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
