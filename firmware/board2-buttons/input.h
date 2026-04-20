/*
 * File:   input.h
 * Author: teivaz
 *
 * Created on March 20, 2026, 12:41 AM
 */

#ifndef INPUT_H
#define INPUT_H

#include <xc.h>

typedef union {
    struct {
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

typedef void (*InputChangeHandler)(uint8_t prev, uint8_t curr);

void input_init(void);
InputState input_state_current(void);

/* Register a listener invoked from the IOC ISR after each state change.
 * Receives the input-byte values before and after the update. */
void input_set_change_handler(InputChangeHandler handler);

#endif /* INPUT_H */
