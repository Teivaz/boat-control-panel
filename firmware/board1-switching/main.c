#include "adc.h"
#include "comm.h"
#include "config.h"
#include "controller.h"
#include "i2c.h"
#include "interrupt.h"
#include "libcomm.h"
#include "relay_mon.h"
#include "relay_out.h"
#include "sensors.h"
#include "task.h"

#include <xc.h>

#define _XTAL_FREQ 64000000UL

static TaskController ctrl;

static void tick_isr (void);
static void tick_init(void);

static void init(void) {
    relay_out_init();
    relay_mon_init();
    adc_init();
    i2c_init();

    task_controller_init(&ctrl);
    config_init    (&ctrl);
    comm_init      ();
    sensors_init   (&ctrl);
    controller_init(&ctrl);
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
    T0CON0bits.EN   = 0;
    T0CON1bits.CS   = 0b010;   /* Fosc/4 -> 16 MHz */
    T0CON1bits.CKPS = 0b0111;  /* /128     -> 125 kHz */
    interrupt_set_handler_TMR0(tick_isr);
    PIE3bits.TMR0IE = 1;

    INTERRUPT_PUSH;
    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 124;
    T0CON0bits.EN   = 1;
    INTERRUPT_POP;
}

void main(void) {
    init();
    while (1) {
        task_controller_poll(&ctrl);
    }
}
