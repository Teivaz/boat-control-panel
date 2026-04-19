#define _XTAL_FREQ 64000000UL

#include "display.h"

#include <xc.h>

/* Pin assignments from board3-main/readme.md:
 *   RB0 D/!C         (1 = data, 0 = command)
 *   RB3 !RST
 *   RB4 !WR          (write strobe; data latched on the rising edge)
 *   RC0..RC7 D0..D7
 *
 * The SSD1322 is configured for 8080-mode parallel; nCS is tied to ground
 * and !RD is pulled up — only the above four lines are driven from the
 * MCU. A byte is sent by setting D/!C, placing the data on PORTC, and
 * pulsing !WR low-then-high (the rising edge latches). */

#define DC_PIN LATBbits.LATB0
#define RST_PIN LATBbits.LATB3
#define WR_PIN LATBbits.LATB4

#define DC_COMMAND 0
#define DC_DATA 1

static void bus_write(uint8_t byte, uint8_t dc) {
    DC_PIN = dc;
    LATC = byte;
    WR_PIN = 0;   /* !WR low — data setup window */
    __asm("NOP"); /* >= 60 ns low pulse (single NOP at 64 MHz is ~62.5 ns) */
    WR_PIN = 1;   /* !WR rising edge latches the byte */
}

void display_send_cmd(uint8_t cmd) {
    bus_write(cmd, DC_COMMAND);
}

void display_send_data(const uint8_t* data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        bus_write(data[i], DC_DATA);
    }
}

/* SSD1322 init sequence for a 256x64 panel (e.g. NHD-3.12-25664). Values
 * match the controller's datasheet defaults for the common monochrome use
 * case; display remap (0xA0) may need tuning on hardware once the panel
 * orientation is verified. */
static const uint8_t init_seq[] = {
    /* cmd,  arg_count, args... */
    0xFD, 1, 0x12,       /* command lock: unlock                     */
    0xAE, 0,             /* display off                              */
    0xB3, 1, 0x91,       /* clock divide ratio / oscillator freq     */
    0xCA, 1, 0x3F,       /* multiplex ratio (64MUX)                  */
    0xA2, 1, 0x00,       /* display offset                           */
    0xA1, 1, 0x00,       /* start line                               */
    0xA0, 2, 0x14, 0x11, /* remap + dual-COM mode                    */
    0xAB, 1, 0x01,       /* function selection: internal VDD         */
    0xB4, 2, 0xA0, 0xB5, /* display enhancement A                    */
    0xC1, 1, 0x9F,       /* contrast current                         */
    0xC7, 1, 0x0F,       /* master current                           */
    0xB9, 0,             /* select default linear grayscale table    */
    0xB1, 1, 0xE2,       /* phase length                             */
    0xD1, 2, 0x82, 0x20, /* display enhancement B                    */
    0xBB, 1, 0x1F,       /* pre-charge voltage                       */
    0xB6, 1, 0x08,       /* second pre-charge period                 */
    0xBE, 1, 0x07,       /* VCOMH                                    */
    0xA6, 0,             /* normal display                           */
    0xA9, 0,             /* exit partial display                     */
    0xAF, 0,             /* display on                               */
};

static void gpio_init(void) {
    /* PORTC: 8-bit data bus, all digital outputs. */
    ANSELC = 0x00;
    LATC = 0x00;
    TRISC = 0x00;

    /* RB0 D/!C, RB3 !RST, RB4 !WR strobe — all digital outputs. */
    ANSELBbits.ANSELB0 = 0;
    ANSELBbits.ANSELB3 = 0;
    ANSELBbits.ANSELB4 = 0;
    TRISBbits.TRISB0 = 0;
    TRISBbits.TRISB3 = 0;
    TRISBbits.TRISB4 = 0;

    /* Idle state: strobe and !RST high, D/!C command. */
    DC_PIN = DC_COMMAND;
    WR_PIN = 1;
    RST_PIN = 1;
}

static void hw_reset(void) {
    RST_PIN = 1;
    __delay_ms(1);
    RST_PIN = 0;
    __delay_ms(1); /* datasheet min: 100 us */
    RST_PIN = 1;
    __delay_ms(2); /* tRES: allow internal reset to settle */
}

void display_init(void) {
    gpio_init();
    hw_reset();

    const uint8_t* p = init_seq;
    const uint8_t* end = init_seq + sizeof(init_seq);
    while (p < end) {
        uint8_t cmd = *p++;
        uint8_t nargs = *p++;
        display_send_cmd(cmd);
        if (nargs) {
            display_send_data(p, nargs);
            p += nargs;
        }
    }
}
