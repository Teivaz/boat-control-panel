#include "button.h"
#include "comm.h"
#include "config.h"
#include "i2c.h"
#include "input.h"
#include "interrupt.h"
#include "led_effect.h"
#include "libcomm.h"
#include "rgbled.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

#define _XTAL_FREQ 64000000UL

static TaskController ctrl;

static void tick_init(void);

static void init(void) {
    rgbled_init();
    i2c_init();

    task_controller_init(&ctrl);
    input_init(&ctrl);
    button_init(&ctrl);
    led_effect_init(&ctrl);
    comm_init(&ctrl);
    config_init(&ctrl);
    tick_init();

    /* Interrupts enabled last. */
    interrupt_init();
}

/* TMR0 in 8-bit mode clocked from Fosc/4 with /128 prescaler gives
 * 16 MHz / 128 ≈ 125 kHz (~1.024 ms per count). Match value 124 in
 * TMR0H yields a 1 ms period interrupt. */
void __interrupt(irq(TMR0), base(8)) TMR0_ISR(void) {
    PIR3bits.TMR0IF = 0;
    task_controller_tick(&ctrl);
}

static void tick_init(void) {
    T0CON0bits.EN = 0;
    // Assign peripheral interrupt priority vectors
    IPR3bits.TMR0IP = 1;
    T0CON1bits.CS = 0b010;    /* Fosc/4 -> 16 MHz */
    T0CON1bits.CKPS = 0b0111; /* 16 MHz / 128 -> 125 kHz */
    PIE3bits.TMR0IE = 1;
    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 124; /* 124 cycles + 1 for trigger at 125 kHz => 1 kHz / 1ms */
    T0CON0bits.EN = 1;
}

void main(void) {
    init();

    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        button_set_trigger(i, config_get_button(i));
    }
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        led_effect_set(i, config_get_effect(i));
    }

    while (1) {
        task_controller_poll(&ctrl);
    }
}
