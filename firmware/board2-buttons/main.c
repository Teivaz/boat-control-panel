#include "rgbled.h"
#include "input.h"
#include "interrupt.h"
#include "i2c.h"
#include "task.h"
#include "task_ids.h"
#include "button.h"
#include "libcomm.h"

#include <xc.h>

#define _XTAL_FREQ 64000000UL

static TaskController ctrl;

static void update_task(TaskId id, void *ctx);
static void tick_isr   (void);
static void tick_init  (void);

void init(void)
{
    rgbled_init();
    input_init();
    i2c_init();

    task_controller_init(&ctrl);
    button_init(&ctrl);
    task_controller_add(&ctrl, TASK_UPDATE, 10, update_task, 0);
    tick_init();

    // Interrupts should be enabled last
    interrupt_init();
}

void send_button_event(uint8_t button_id)
{
    CommMessage msg;
    uint8_t cur  = input_state_current().integer;
    uint8_t prev = cur ^ (uint8_t)(1u << button_id);
    uint8_t len  = comm_build_button_changed(&msg, prev, cur);
    // TODO: transmit msg[0..len] to COMM_ADDRESS_MAIN once master-mode I2C is wired up
    (void)msg;
    (void)len;
}

uint8_t curr = 0;
uint8_t step = 1;

uint8_t button_state = 0;
uint8_t mode = 0;

uint8_t i2c_data;

static void update_task(TaskId id, void *ctx) {
    (void)id;
    (void)ctx;
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

/* TMR0 in 8-bit mode clocked from Fosc/4 with /16384 prescaler gives
 * 16 MHz / 16384 ≈ 976.56 Hz (~1.024 ms per count). Match value 1 in
 * TMR0H yields a ~1 ms period interrupt. */
static void tick_isr(void) {
    task_controller_tick(&ctrl);
}

static void tick_init(void) {
    T0CON0bits.EN   = 0;
    T0CON1bits.CS   = 0b010;
    T0CON1bits.CKPS = 0b1110;
    interrupt_set_handler_TMR0(tick_isr);
    PIE3bits.TMR0IE = 1;

    const uint8_t gie_save = GIE;
    GIE = 0;
    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 1;
    T0CON0bits.EN = 1;
    GIE = gie_save;
}

void main(void)
{
    init();
    while (1)
    {
        task_controller_poll(&ctrl);
    }
}
