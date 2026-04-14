#include "timer_update.h"
#include "interrupt.h"


void timer_update_init(void) {
    T0CON0bits.EN = 0;
    
    T0CON1bits.CS = 0b010; // Fosc/4 -> 16MHz
    T0CON1bits.CKPS = 0b1110; // divide by 16384 -> 9765.25 Hz (1.024 ms)
    
    PIE3bits.TMR0IE = 1;
}

void timer_update_set_time(uint8_t time) {
    const uint8_t interrupt_state = GIE;
    GIE = 0;

    PIR3bits.TMR0IF = 0;
    T0CON0bits.EN = 0;
    if (time != 0) {
        TMR0L = 0;
        TMR0H = time;
        T0CON0bits.EN = 1;
    }

    GIE = interrupt_state;
}

void timer_update_set_callback(void (* interrupt_handler)(void)){
    interrupt_set_handler_TMR0(interrupt_handler);
}
