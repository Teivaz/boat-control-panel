#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include "task.h"

/* Protocol dispatcher. Wires i2c + libcomm to the application modules
 * (button, led_effect, config, input), and registers a periodic task that
 * drains the outbound button_changed retry queue. */
void comm_init(TaskController *ctrl);

/* Enqueue a button_changed (0x02) event for the main board. Safe to call
 * from any context (IOC ISR edge, timer task, I2C ISR reconfigure).
 * The actual transmit runs in main context; bus collisions / NACKs stay
 * queued and retry on the next tick. Drops on overflow — the main board's
 * polled button_state_read is the fallback. */
void comm_send_button_changed(uint8_t prev_state, uint8_t current_state);

#endif /* COMM_H */
