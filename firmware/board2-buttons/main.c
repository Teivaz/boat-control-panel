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

uint8_t curr = 0;
uint8_t step = 1;

uint8_t button_state = 0;
uint8_t mode = 0;

uint8_t i2c_data;
RGBLedData led_data[7] = {0};

static void update_task(TaskId id, void *ctx) {
    (void)id;
    (void)ctx;
    curr += step;
    uint8_t next_button_state = input_state_current().integer;
    if (button_state != next_button_state)
    {
        uint8_t mask = button_state ^ next_button_state;
        uint8_t released = next_button_state & mask;
        mode ^= released;
        button_state = next_button_state;
    }
    rgbled_set(led_data, 7);
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

    led_data[button_id].red = !led_data[button_id].red * 10;
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

    while (1)
    {
        task_controller_poll(&ctrl);
    }
}
