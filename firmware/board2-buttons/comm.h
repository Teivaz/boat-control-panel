#ifndef COMM_H
#define COMM_H

#include <stdint.h>

/* Protocol dispatcher. Wires i2c + libcomm to the application modules
 * (button, led_effect, config, input) and emits outbound events. */
void comm_init(void);

/* Send button_changed (0x02) to the main board. Called from main context
 * when a button trigger fires. Bus collisions / NACKs are logged via the
 * return code dropped on the floor — the main board's polled
 * button_state_read provides a fallback path. */
void comm_send_button_changed(uint8_t prev_state, uint8_t current_state);

#endif /* COMM_H */
