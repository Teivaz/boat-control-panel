#include "adc.h"
#include "comm.h"
#include "config.h"
#include "controller.h"
#include "i2c.h"
#include "i2c_board.h"
#include "interrupt.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "relay_mon.h"
#include "relay_out.h"
#include "sensors.h"
#include "task.h"

#include <xc.h>

#define _XTAL_FREQ 64000000UL

static TaskController ctrl;

static void tick_init(void);

static void init(void) {
    relay_out_init();
    relay_mon_init();
    adc_init();
    i2c_pins_init();
    i2c_init(comm_address());
    comm_interface_init();

    task_controller_init(&ctrl);
    config_init(&ctrl);
    comm_init();
    sensors_init(&ctrl);
    controller_init(&ctrl);
    tick_init();

    /* Interrupts enabled last. */
    interrupt_init();
}

/* TMR0 in 8-bit mode clocked from Fosc/4 with /128 prescaler gives
 * 16 MHz / 128 = 125 kHz (8 us per count). Match value 124 in TMR0H
 * yields a 1 ms period interrupt.
 *
 * ISR context. */
void __interrupt(irq(TMR0), base(8)) TMR0_ISR(void) {
    PIR3bits.TMR0IF = 0;
    task_controller_tick(&ctrl);
}

static void tick_init(void) {
    T0CON0bits.EN = 0;
    IPR3bits.TMR0IP = 1;
    T0CON1bits.CS = 0b010;    // Fosc/4 -> 16 MHz
    T0CON1bits.CKPS = 0b0111; // 16 MHz / 128 -> 125 kHz
    PIE3bits.TMR0IE = 1;

    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 124; // 124 cycles + 1 for trigger at 125 kHz => 1 kHz / 1ms
    T0CON0bits.EN = 1;
}

void main(void) {
    init();
    while (1) {
        i2c_poll();
        task_controller_poll(&ctrl);
    }
}
