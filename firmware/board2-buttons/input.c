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

static volatile InputState input_state;
static InputChangeHandler change_handler;

static void sample_pins(InputState* state);
static void ioc_handler(void);

void input_init(void) {
    LATA = 0x00;
    ODCONA = 0x00;
    TRISA = 0xFF;
    ANSELA = 0x00;
    WPUA = 0xFF;
    SLRCONA = 0xFF;
    INLVLA = 0xFF;

    input_state.integer = 0x00;
    interrupt_set_handler_IOC(ioc_handler);
    PIE0bits.IOCIE = 1;
    IOCAP = 0xFF;
    IOCAN = 0xFF;
}

InputState input_state_current(void) {
    INTERRUPT_PUSH;
    InputState result = input_state;
    INTERRUPT_POP;
    return result;
}

void input_set_change_handler(InputChangeHandler handler) {
    change_handler = handler;
}

static void ioc_handler(void) {
    IOCAF = 0x00;
    const uint8_t prev = input_state.integer;
    InputState next = input_state;
    sample_pins(&next);
    input_state = next;
    const uint8_t curr = next.integer;
    if (curr != prev && change_handler) {
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
