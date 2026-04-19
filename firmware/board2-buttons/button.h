#ifndef BUTTON_H
#define BUTTON_H

#include "libcomm.h"
#include "task.h"

#include <stdint.h>

#define BUTTON_COUNT 7

void button_init(TaskController *ctrl);

void button_set_trigger(uint8_t button_id, CommTriggerConfig cfg);
CommTriggerConfig button_get_trigger(uint8_t button_id);

/* Provided by the application (main.c). Invoked when a button's
 * configured trigger mode fires. */
void send_button_event(uint8_t button_id);

#endif /* BUTTON_H */
