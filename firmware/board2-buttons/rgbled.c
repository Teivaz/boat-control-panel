#include "rgbled.h"

#include <xc.h>

#define RGBLED_COUNT 7
#define RGBLED_BUFFER_LEN ((uint8_t)(4 * 3 * RGBLED_COUNT))

static uint8_t frame_buffer[RGBLED_BUFFER_LEN] = {0};

static void spi_init(void);
static void dma_init(void);
static void byte_to_sequence(uint8_t input_byte, uint8_t* dest);
static uint16_t copy_to_buffer(const RGBLedData* rgb, uint8_t count);

void rgbled_init(void) {
    spi_init();
    dma_init();
}

void rgbled_set(const RGBLedData* rgb, uint8_t count) {
    DMASELECT = 0;
    if (DMAnCON0bits.XIP != 0) {
        return;
    }

    uint16_t byte_count = copy_to_buffer(rgb, count);

    SPI1TCNT = byte_count;
    DMAnSSZ = byte_count;
    DMAnCON0bits.SIRQEN = 1;
}

/* Encodes one 8-bit intensity as four SPI bytes. Each source bit becomes a
 * 4-bit symbol (0b0001 for 0, 0b0111 for 1) so the WS2812-style timing is
 * produced by the SPI shift register running at 3.2 MHz. */
static void byte_to_sequence(uint8_t input_byte, uint8_t* dest) {
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t output_byte = 0;
        for (uint8_t n = 0; n < 2; n++) {
            uint8_t bit = !!(input_byte & 0b10000000);
            input_byte <<= 1;
            uint8_t nibble = 0b0001 | (bit * 0b0110);
            output_byte |= nibble << (n * 4);
        }
        *dest++ = output_byte;
    }
}

static uint16_t copy_to_buffer(const RGBLedData* rgb, uint8_t count) {
    const uint8_t* iptr = (const uint8_t*)rgb;
    const uint8_t* iend = iptr + count * sizeof(RGBLedData);
    uint8_t* optr = frame_buffer;
    uint8_t* oend = frame_buffer + RGBLED_BUFFER_LEN;
    while (iptr < iend && optr < oend) {
        byte_to_sequence(*iptr, optr);
        iptr += 1;
        optr += 4;
    }
    return (uint16_t)(optr - frame_buffer);
}

static void spi_init(void) {
    LATCbits.LATC2 = 0;
    TRISCbits.TRISC2 = 0;
    ANSELCbits.ANSELC2 = 0;

    RC2PPS = 0x32; /* RC2 -> SPI1 SDO */

    SPI1CON0bits.EN = 0;

    SPI1CLK = 0x00; /* Fosc */
    SPI1CON0bits.LSBF = 1;
    SPI1CON0bits.MST = 1;
    SPI1CON0bits.BMODE = 0;

    SPI1CON2bits.RXR = 0;
    SPI1CON2bits.TXR = 1; /* transmit-only */

    SPI1STATUSbits.TXBE = 1;

    /* 3.2 Mbps: BAUD = Fosc / (2 * Fbit) - 1 = 64 / 6.4 - 1 = 9. */
    SPI1BAUD = 9;

    SPI1INTF = 0;
    SPI1CON0bits.EN = 1;
}

static void dma_init(void) {
    DMASELECT = 0;
    DMAnCON1bits.DMODE = 0b00; /* destination pointer static */
    DMAnCON1bits.SMR = 0b00;   /* source is SFR/GPR */
    DMAnCON1bits.SMODE = 0b01; /* source pointer increments */
    DMAnCON1bits.SSTP = 1;     /* clear SIRQEN on completion */
    DMAnSSZ = RGBLED_BUFFER_LEN;
    DMAnSSA = (uint24_t)frame_buffer;
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
