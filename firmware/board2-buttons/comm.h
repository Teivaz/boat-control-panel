#ifndef COMM_H
#define COMM_H

#include "libcomm.h"
#include "task.h"

#include <stdint.h>

void comm_init(TaskController* ctrl);
void comm_send_button_event(uint8_t button_id, uint8_t pressed, CommButtonMode mode);

#endif /* COMM_H */
