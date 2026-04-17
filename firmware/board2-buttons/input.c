#include "input.h"
#include "interrupt.h"

#define _INPUT_0 PORTAbits.RA7
#define _INPUT_1 PORTAbits.RA6
#define _INPUT_2 PORTAbits.RA0
#define _INPUT_3 PORTAbits.RA1
#define _INPUT_4 PORTAbits.RA2
#define _INPUT_5 PORTAbits.RA3
#define _INPUT_6 PORTAbits.RA4
#define _INPUT_7 PORTAbits.RA5

static InputState          _global_input_state;
static InputChangeHandler  _change_handler;

void input_init(void)
{
    LATA = 0x00;
    ODCONA = 0x00; // Open-Drain Control Register. Default 0
    TRISA = 0xFF;
    ANSELA = 0x00;
    WPUA = 0xFF;    // Weak Pull-Up Register. Default unset
    SLRCONA = 0xFF; // Slew Rate Control Register. Default set
    INLVLA = 0xFF;  // Input Level Control Register. Default set

    _input_state_init(&_global_input_state);
    interrupt_set_handler_IOC(&_input_state_interrupt_handler);
    PIE0bits.IOCIE = 1;
    IOCAP = 0xFF;
    IOCAN = 0xFF;
}

InputState input_state_current(void)
{
    const uint8_t interrupt_state = GIE;
    GIE = 0;
    InputState result = _global_input_state;
    GIE = interrupt_state;
    return result;
}

void input_set_change_handler(InputChangeHandler handler)
{
    _change_handler = handler;
}

void _input_state_init(InputState *state)
{
    state->integer = 0x00;
}

void _input_state_interrupt_handler(void)
{
    IOCAF = 0x00;
    const uint8_t prev = _global_input_state.integer;
    _input_state_update(&_global_input_state);
    const uint8_t curr = _global_input_state.integer;
    if (curr != prev && _change_handler) {
        _change_handler(prev, curr);
    }
}

void _input_state_update(InputState *state)
{
    state->b0 = !_INPUT_0;
    state->b1 = !_INPUT_1;
    state->b2 = !_INPUT_2;
    state->b3 = !_INPUT_3;
    state->b4 = !_INPUT_4;
    state->b5 = !_INPUT_5;
    state->b6 = !_INPUT_6;
    state->b7 = !_INPUT_7;
}
