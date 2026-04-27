#include "button_fx.h"
#include "comm.h"
#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
#include "display_text.h"
#include "i2c.h"
#include "i2c_board.h"
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

    task_controller_init(&ctrl);

    display_init();
    rgbled_init();

    i2c_pins_init();
    i2c_init(comm_address());
    comm_interface_init();

    config_init(&ctrl);
    config_mode_init(&ctrl);
    comm_init();
    controller_init(&ctrl);
    button_fx_init(&ctrl);
    indicator_init(&ctrl);
    display_text_init(&ctrl);
    tick_init();

    /* Interrupts enabled last. */
    interrupt_init();
}

void __interrupt(irq(TMR0), base(8)) TMR0_ISR(void) {
    PIR3bits.TMR0IF = 0;
    task_controller_tick(&ctrl);
    i2c_tick_ms();
}

void __interrupt(irq(I2C1TX, I2C1RX, I2C1), base(8)) I2C1_ISR(void) {
    i2c_isr();
}

void __interrupt(irq(I2C1E), base(8)) I2C1_ERROR_ISR(void) {
    i2c_error_isr();
}

static void tick_init(void) {
    T0CON0bits.EN = 0;
    IPR3bits.TMR0IP = 1;
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

void main(void) {
    init();

    CommButtonEffect effect;
    CommButtonOutputEffect oe = {.mode = 1, .color = 0};
    comm_button_effect_init(&effect);
    comm_button_effect_set(&effect, 1, oe);

    comm_send_button_effect(0, &effect, 0, 0);
    while (1) {
        i2c_poll();
        task_controller_poll(&ctrl);
    }
}
