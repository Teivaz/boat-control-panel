#define _XTAL_FREQ 64000000UL

#include "button_fx.h"
#include "comm.h"
#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
#include "display_text.h"
#include "i2c.h"
#include "i2c_board.h"
#include "i2c_log.h"
#include "indicator.h"
#include "interrupt.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "rgbled.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

static TaskController ctrl;

static void tick_init(void);

static void init(void) {
    /* RA7 is the config-mode switch: digital input with weak pull-up. */
    TRISAbits.TRISA7 = 1;
    ANSELAbits.ANSELA7 = 0;
    WPUAbits.WPUA7 = 1;

    // Low level system without dependencies
    task_controller_init(&ctrl);
    config_init(&ctrl);
    i2c_pins_init();
    rgbled_init();
    display_init();

    // Higher level systems that might depend on the low level
    i2c_init(comm_address());
    comm_interface_init();
    i2c_log_init();
    comm_init();

    // High level systems that schedule tasks
    config_mode_init(&ctrl);
    controller_init(&ctrl);
    button_fx_init(&ctrl);
    indicator_init(&ctrl);
    display_text_init(&ctrl);

    tick_init();

    /* Interrupts enabled last. */
    interrupt_init();

    /* Only now is it safe to answer on the bus - see i2c_start(). */
    i2c_start();
}

void __interrupt(low_priority, irq(TMR0), base(8)) TMR0_ISR(void) {
    PIR3bits.TMR0IF = 0;
    task_controller_tick(&ctrl);
}

static void tick_init(void) {
    T0CON0bits.EN = 0;
    /* Priority is owned by interrupt_init's IPR clear-and-promote step;
     * TMR0 ends up low-priority correctly there. */
    T0CON1bits.CS = 0b010;    /* Fosc/4 -> 16 MHz */
    T0CON1bits.CKPS = 0b0111; /* /128     -> 125 kHz */
    PIE3bits.TMR0IE = 1;

    INTERRUPT_PUSH;
    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 124;
    T0CON0bits.EN = 1;
    INTERRUPT_POP;
}

/* XXX DIAGNOSTIC — TEMPORARY, REMOVE ME.
 *
 * Blink the latched reset cause at boot, readable with no I2C bridge and no
 * display. A *count* rather than a colour, because indicator.c drives these
 * same LEDs and its own output collides with any colour scheme: render_normal
 * paints active nav lights white, render_error paints them red. It never
 * blinks a deliberate count, and this runs before init() so nothing else is
 * touching the LEDs yet.
 *
 * Every restart replays the blinks, so a reset loop is unmistakable: the
 * pattern repeats instead of happening once at power-up.
 *
 *   1  !BOR    brown-out          (expected on a power-off)
 *   2  !POR    power-on
 *   3  !RMCLR  MCLR / programmer
 *   4  !RI     software RESET — comm_on_reset, or an __asm("RESET") stub in
 *              interrupt.c firing on an unclaimed vector
 *   5  STKOVF  hardware stack overflow
 *   6  STKUNF  stack underflow
 *   7          none of the above — re-entered main() with no reset recorded,
 *              i.e. a wild jump */
static void show_reset_cause(uint8_t cause) {
    RGBLedData leds[5];
    uint8_t n;
    if (cause & 0x80) {
        n = 5;
    } else if (cause & 0x40) {
        n = 6;
    } else if (!(cause & 0x04)) {
        n = 4;
    } else if (!(cause & 0x08)) {
        n = 3;
    } else if (!(cause & 0x01)) {
        n = 1;
    } else if (!(cause & 0x02)) {
        n = 2;
    } else {
        n = 7;
    }

    rgbled_init();
    /* Dark lead-in so the first blink is unambiguous. */
    for (uint8_t i = 0; i < 5; i++) {
        leds[i].red = 0;
        leds[i].green = 0;
        leds[i].blue = 0;
    }
    rgbled_set(leds, 5);
    for (uint8_t d = 0; d < 10; d++) {
        __delay_ms(100);
    }

    for (uint8_t blink = 0; blink < n; blink++) {
        for (uint8_t i = 0; i < 5; i++) {
            leds[i].red = 0x30;
            leds[i].green = 0x00;
            leds[i].blue = 0x30;
        }
        rgbled_set(leds, 5);
        for (uint8_t d = 0; d < 3; d++) {
            __delay_ms(100);
        }
        for (uint8_t i = 0; i < 5; i++) {
            leds[i].red = 0;
            leds[i].green = 0;
            leds[i].blue = 0;
        }
        rgbled_set(leds, 5);
        for (uint8_t d = 0; d < 3; d++) {
            __delay_ms(100);
        }
    }
    /* Trailing gap so a repeat (reset loop) reads as a separate group. */
    for (uint8_t d = 0; d < 8; d++) {
        __delay_ms(100);
    }
}

/* XXX DIAGNOSTIC — TEMPORARY, REMOVE ME.
 *
 * Second blink group, in cyan: the region the board was executing when the PC
 * went wild. Read after the purple reset-cause group, separated by a long gap.
 *
 *   1 i2c_poll (main)          2 (unused)         3 I2C1_ISR
 *   4 I2C1TX_ISR               5 I2C1RX_ISR       6 I2C1E_ISR
 *   7 client-RX sync dispatch (ISR)               8 cold_rx_dispatch (main)
 *   9 controller_on_button_changed (main)        10 display refresh (main)
 *  11 indicator refresh (main) */
static void show_trace(uint8_t n) {
    RGBLedData leds[5];
    for (uint8_t d = 0; d < 15; d++) {
        __delay_ms(100);
    }
    if (n > 20) {
        n = 20; /* uninitialised persistent RAM on the very first power-up */
    }
    for (uint8_t blink = 0; blink < n; blink++) {
        for (uint8_t i = 0; i < 5; i++) {
            leds[i].red = 0x00;
            leds[i].green = 0x30;
            leds[i].blue = 0x30;
        }
        rgbled_set(leds, 5);
        for (uint8_t d = 0; d < 3; d++) {
            __delay_ms(100);
        }
        for (uint8_t i = 0; i < 5; i++) {
            leds[i].red = 0;
            leds[i].green = 0;
            leds[i].blue = 0;
        }
        rgbled_set(leds, 5);
        for (uint8_t d = 0; d < 3; d++) {
            __delay_ms(100);
        }
    }
    for (uint8_t d = 0; d < 8; d++) {
        __delay_ms(100);
    }
}

void main(void) {
    /* XXX DIAGNOSTIC — TEMPORARY, REMOVE ME. Latch PCON0 before anything
     * can disturb it; readable at config 0x22 and shown as an LED colour. */
    comm_reset_cause_latch();
    show_reset_cause(comm_reset_cause());
    show_trace(g_trace);
    TRACE(0);
    init();

    CommButtonEffect effect;
    CommButtonOutputEffect oe = {.mode = 1, .color = 0};
    comm_button_effect_init(&effect);
    comm_button_effect_set(&effect, 1, oe);

    // comm_send_button_effect(COMM_ADDRESS_BUTTON_BOARD_L, &effect, 0, 0);
    while (1) {
        i2c_poll();
        task_controller_poll(&ctrl);
    }
}
