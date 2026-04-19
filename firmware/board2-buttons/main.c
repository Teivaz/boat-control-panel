#include "rgbled.h"
#include "input.h"
#include "interrupt.h"
#include "i2c.h"
#include "task.h"
#include "task_ids.h"
#include "button.h"
#include "led_effect.h"
#include "comm.h"
#include "libcomm.h"
#include "config.h"

#include <xc.h>

#define _XTAL_FREQ 64000000UL

static TaskController ctrl;

static void tick_isr (void);
static void tick_init(void);

void init(void) {
    rgbled_init();
    input_init();
    i2c_init();

    task_controller_init(&ctrl);
    button_init(&ctrl);
    led_effect_init(&ctrl);
    comm_init(&ctrl);
    config_init(&ctrl);
    tick_init();

    // Interrupts should be enabled last
    interrupt_init();
}

void send_button_event(uint8_t button_id) {
    uint8_t cur  = input_state_current().integer;
    uint8_t prev = cur ^ (uint8_t)(1u << button_id);
    comm_send_button_changed(prev, cur);
}

/* TMR0 in 8-bit mode clocked from Fosc/4 with /128 prescaler gives
 * 16 MHz / 128 ≈ 125 kHz (~1.024 ms per count). Match value 124 in
 * TMR0H yields a 1 ms period interrupt. */
static void tick_isr(void) {
    task_controller_tick(&ctrl);
}

static void tick_init(void) {
    T0CON0bits.EN   = 0;
    T0CON1bits.CS   = 0b010; // Fosc/4 -> 16MHz
    T0CON1bits.CKPS = 0b0111; // divide by 128 -> 125 kHz (8 us)
    interrupt_set_handler_TMR0(tick_isr);
    PIE3bits.TMR0IE = 1;

    INTERRUPT_PUSH;
    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 124; // on 125th tick the elapsed time would be exactly 1ms
    T0CON0bits.EN = 1;
    INTERRUPT_POP;
}

void main(void) {
    init();

    for(uint8_t i = 0; i < BUTTON_COUNT; i++) {
        button_set_trigger(i, config_get_button(i));
    }
    for(uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        led_effect_set(i, config_get_effect(i));
    }

    while (1) {
        task_controller_poll(&ctrl);
    }
}
