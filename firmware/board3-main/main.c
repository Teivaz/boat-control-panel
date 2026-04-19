#include "comm.h"
#include "config.h"
#include "controller.h"
#include "display.h"
#include "display_text.h"
#include "i2c.h"
#include "interrupt.h"
#include "libcomm.h"
#include "rgbled.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

#define _XTAL_FREQ 64000000UL

static TaskController ctrl;

static void tick_isr(void);
static void tick_init(void);

static void init(void) {
    /* RA7 is the config-mode switch: digital input with weak pull-up. */
    TRISAbits.TRISA7 = 1;
    ANSELAbits.ANSELA7 = 0;
    WPUAbits.WPUA7 = 1;

    display_init();
    rgbled_init();
    i2c_init();

    task_controller_init(&ctrl);
    config_init(&ctrl);
    comm_init();
    controller_init(&ctrl);
    display_text_init(&ctrl);
    tick_init();

    /* Interrupts enabled last. */
    interrupt_init();
}

/* TMR0 in 8-bit mode clocked from Fosc/4 with /128 prescaler gives
 * 16 MHz / 128 ≈ 125 kHz (~1.024 ms per count). Match value 124 in TMR0H
 * yields a 1 ms period interrupt. */
static void tick_isr(void) {
    task_controller_tick(&ctrl);
}

static void tick_init(void) {
    T0CON0bits.EN = 0;
    T0CON1bits.CS = 0b010;    /* Fosc/4 -> 16 MHz */
    T0CON1bits.CKPS = 0b0111; /* /128     -> 125 kHz */
    interrupt_set_handler_TMR0(tick_isr);
    PIE3bits.TMR0IE = 1;

    INTERRUPT_PUSH;
    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 124;
    T0CON0bits.EN = 1;
    INTERRUPT_POP;
}

void main(void) {
    init();
    while (1) {
        task_controller_poll(&ctrl);
    }
}
