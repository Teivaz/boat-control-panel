#ifndef BUTTON_H
#define BUTTON_H

#include "libcomm.h"
#include "task.h"

#include <stdint.h>

#define BUTTON_COUNT 7

void button_init(TaskController* ctrl);

/* Both ISR-callable — invoked from the I2C on_rx / on_read handlers. In-RAM
 * state update; button_set_trigger may emit a button_changed (enabled /
 * disabled) via the comm queue, which is itself ISR-safe. */
void button_set_trigger(uint8_t button_id, CommTriggerConfig cfg);
CommTriggerConfig button_get_trigger(uint8_t button_id);

#endif /* BUTTON_H */
