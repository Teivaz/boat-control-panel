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

#include <xc.h>

#define _XTAL_FREQ 64000000UL

static TaskController ctrl;

static void tick_isr (void);
static void tick_init(void);

void init(void)
{
    rgbled_init();
    input_init();
    i2c_init();

    task_controller_init(&ctrl);
    button_init(&ctrl);
    led_effect_init(&ctrl);
    comm_init();
    tick_init();

    // Interrupts should be enabled last
    interrupt_init();
}

void send_button_event(uint8_t button_id)
{
    uint8_t cur  = input_state_current().integer;
    uint8_t prev = cur ^ (uint8_t)(1u << button_id);
    comm_send_button_changed(prev, cur);

    CommButtonOutputEffect eff = led_effect_get(button_id);
    eff.mode = (eff.mode + 1) & 0x03;
    led_effect_set(button_id, eff);
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

void main(void)
{
    init();

    button_set_trigger(6, comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1000));
    button_set_trigger(5, comm_button_trigger_make(COMM_BUTTON_MODE_RELEASE, 1000));
    button_set_trigger(4, comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 500));
    button_set_trigger(3, comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 100));
    button_set_trigger(2, comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1));
    button_set_trigger(1, comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 0));
    button_set_trigger(0, comm_button_trigger_make(COMM_BUTTON_MODE_CHANGE, 0));

    CommButtonOutputEffect white = { .color=COMM_EFFECT_COLOR_WHITE, .mode = COMM_EFFECT_MODE_DISABLED };
    CommButtonOutputEffect red = { .color=COMM_EFFECT_COLOR_RED, .mode = COMM_EFFECT_MODE_DISABLED };
    CommButtonOutputEffect green = { .color=COMM_EFFECT_COLOR_GREEN, .mode = COMM_EFFECT_MODE_DISABLED };
    CommButtonOutputEffect blue = { .color=COMM_EFFECT_COLOR_BLUE, .mode = COMM_EFFECT_MODE_DISABLED };
    led_effect_set(6, blue);
    led_effect_set(5, green);
    led_effect_set(4, red);
    led_effect_set(3, white);
    led_effect_set(2, blue);
    led_effect_set(1, green);
    led_effect_set(0, red);

    while (1)
    {
        task_controller_poll(&ctrl);
    }
}
