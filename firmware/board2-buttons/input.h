/*
 * File:   input.h
 * Author: teivaz
 *
 * Created on March 20, 2026, 12:41 AM
 */

#ifndef INPUT_H
#define INPUT_H

#include <xc.h>

typedef union
{
    struct
    {
        uint8_t b0 : 1;
        uint8_t b1 : 1;
        uint8_t b2 : 1;
        uint8_t b3 : 1;
        uint8_t b4 : 1;
        uint8_t b5 : 1;
        uint8_t b6 : 1;
        uint8_t b7 : 1;
    };
    uint8_t integer;
} InputState;

void input_init(void);
InputState input_state_current(void);

void _input_state_init(InputState *state);
void _input_state_interrupt_handler();
void _input_state_update(InputState *state);

#endif /* INPUT_H */
