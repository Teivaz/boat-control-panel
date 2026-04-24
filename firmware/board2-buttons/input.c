#include "input.h"

#include "interrupt.h"
#include "libcomm.h"

#define PIN_INPUT_0 PORTAbits.RA7
#define PIN_INPUT_1 PORTAbits.RA6
#define PIN_INPUT_2 PORTAbits.RA0
#define PIN_INPUT_3 PORTAbits.RA1
#define PIN_INPUT_4 PORTAbits.RA2
#define PIN_INPUT_5 PORTAbits.RA3
#define PIN_INPUT_6 PORTAbits.RA4
#define PIN_INPUT_7 PORTAbits.RA5

static TaskController* ctrl;
static volatile InputState input_state;
/* State already observed by dispatch_pending. Updated only from main loop. */
static uint8_t last_notified;
static InputChangeHandler change_handler;

static void sample_pins(InputState* state);
static void dispatch_pending(void* ctx); /* main-loop callback */

void input_init(TaskController* controller) {
    ctrl = controller;
    LATA = 0x00;
    ODCONA = 0x00;
    TRISA = 0xFF;
    ANSELA = 0x00;
    WPUA = 0xFF;
    SLRCONA = 0xFF;
    INLVLA = 0xFF;

    input_state.integer = 0x00;
    last_notified = 0x00;
    PIE0bits.IOCIE = 1;
    IOCAP = 0xFF;
    IOCAN = 0xFF;
}

InputState input_state_current(void) {
    INTERRUPT_PUSH;
    const InputState result = input_state;
    INTERRUPT_POP;
    return result;
}

void input_set_change_handler(InputChangeHandler handler) {
    change_handler = handler;
}

/* ISR context. Captures the new pin values and hands dispatch off to the
 * main loop — run_in_main_loop is idempotent against re-queues because
 * dispatch_pending compares last_notified against the latest sampled state,
 * so a dropped queue slot (full queue) is self-healing on the next edge. */
void __interrupt(irq(IOC), base(8)) IOC_ISR(void) {
    PIR0bits.IOCIF = 0;
    IOCAF = 0x00;
    InputState next = input_state;
    sample_pins(&next);
    input_state = next;
    run_in_main_loop(ctrl, dispatch_pending, 0);
}

static void dispatch_pending(void* ctx) {
    (void)ctx;
    const uint8_t prev = last_notified;
    const uint8_t curr = input_state_current().integer;
    if (curr == prev) {
        return;
    }
    last_notified = curr;
    if (change_handler) {
        change_handler(prev, curr);
    }
}

static void sample_pins(InputState* state) {
    state->b0 = !PIN_INPUT_0;
    state->b1 = !PIN_INPUT_1;
    state->b2 = !PIN_INPUT_2;
    state->b3 = !PIN_INPUT_3;
    state->b4 = !PIN_INPUT_4;
    state->b5 = !PIN_INPUT_5;
    state->b6 = !PIN_INPUT_6;
    state->b7 = !PIN_INPUT_7;
}
