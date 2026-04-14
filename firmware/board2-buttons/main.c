#include "rgbled.h"
#include "input.h"
#include "interrupt.h"
#include "timer_update.h"
#include "libcomm/i2c.h"

#include <xc.h>

#define _XTAL_FREQ 64000000UL

void on_update(void);

void init(void)
{
    rgbled_init();
    input_init();
    timer_update_init();
    i2c_init();
    
    // Interrupts should be enabled last
    interrupt_init();
}

uint8_t curr = 0;
uint8_t step = 1;

uint8_t button_state = 0;
uint8_t mode = 0;

uint8_t i2c_data;

void on_update(void) {
    curr += step;
    RGBLedData d[7] = {0};
    uint8_t next_button_state = input_state_current().integer;
    if (button_state != next_button_state)
    {
        uint8_t mask = button_state ^ next_button_state;
        uint8_t released = next_button_state & mask;
        mode ^= released;
        button_state = next_button_state;
    }
    d[0].red = i2c_data;
    for (uint8_t i = 0; i < 7; i++)
    {
        if (mode & (1 << i))
        {
            if (i == 0)
            {
//                d[i].red = 0xff - curr;
            }
            if (i == 1)
            {
                d[i].green = 0xff - curr;
            }
            if (i == 2)
            {
                d[i].blue = 0xff - curr;
            }
            if (i == 3)
            {
                d[i].red = (0xff - curr) >> 2;
                d[i].blue = (0xff - curr) >> 2;
            }
            if (i == 4)
            {
                d[i].red = (0xff - curr) >> 2;
                d[i].green = (0xff - curr) >> 2;
            }
            if (i == 5)
            {
                d[i].blue = (0xff - curr) >> 2;
                d[i].green = (0xff - curr) >> 2;
            }
            if (i == 6)
            {
                d[i].red = (0xff - curr) >> 2;
                d[i].blue = (0xff - curr) >> 2;
                d[i].green = (0xff - curr) >> 2;
            }
        }
    }
    rgbled_set(d, 7);
}


void main(void)
{
    init();
    timer_update_set_callback(&on_update);
    timer_update_set_time(10); // 10 ms -> 100 Hz 
    while (1)
    {
//        i2c_update();
        __nop();
    }
}
