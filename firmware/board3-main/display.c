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

static u8g2_t u8g2;

static void bus_write(uint8_t byte) {
    LATC = byte;
    WR_PIN = 0;   /* !WR low — data setup window */
    __asm("NOP"); /* >= 60 ns low pulse (single NOP at 64 MHz is ~62.5 ns) */
    WR_PIN = 1;   /* !WR rising edge latches the byte */
}

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
    DC_PIN = 0;
    WR_PIN = 1;
    RST_PIN = 1;
}

/* u8g2 byte callback for an 8080-mode parallel bus. nCS is tied low so
 * the start/end transfer messages have no work to do. */
static uint8_t byte_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    (void)u8x8;
    switch (msg) {
        case U8X8_MSG_BYTE_SEND: {
            const uint8_t* data = (const uint8_t*)arg_ptr;
            for (uint8_t i = 0; i < arg_int; i++) {
                bus_write(data[i]);
            }
            break;
        }
        case U8X8_MSG_BYTE_INIT:
        case U8X8_MSG_BYTE_START_TRANSFER:
        case U8X8_MSG_BYTE_END_TRANSFER:
            break;
        case U8X8_MSG_BYTE_SET_DC:
            DC_PIN = arg_int;
            break;
        default:
            return 0;
    }
    return 1;
}

/* u8g2 gpio/delay callback. The byte bus pins are already configured by
 * display_init; here we only service the !RST line and the delay calls
 * u8g2 makes during display initialisation. */
static uint8_t gpio_and_delay_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    (void)u8x8;
    (void)arg_ptr;
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;
        case U8X8_MSG_DELAY_NANO:
            __asm("NOP");
            break;
        case U8X8_MSG_DELAY_100NANO:
            __delay_us(1);
            break;
        case U8X8_MSG_DELAY_10MICRO:
            for (uint8_t i = 0; i < arg_int; i++) {
                __delay_us(10);
            }
            break;
        case U8X8_MSG_DELAY_MILLI:
            for (uint8_t i = 0; i < arg_int; i++) {
                __delay_ms(1);
            }
            break;
        case U8X8_MSG_GPIO_RESET:
            RST_PIN = arg_int;
            break;
        default:
            return 0;
    }
    return 1;
}

void display_init(void) {
    gpio_init();
    u8g2_Setup_ssd1322_nhd_256x64_f(&u8g2, U8G2_R0, byte_cb, gpio_and_delay_cb);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFont(&u8g2, u8g2_font_unifont_tr);
}

u8g2_t* display_u8g2(void) {
    return &u8g2;
}
