#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include "task.h"

/* Three on/off inputs (bilge / shore / AC) debounced by a periodic sampler.
 * `sensors_state()` returns a packed bitfield:
 *   bit 0  bilge float triggered   (RC1)
 *   bit 1  shore power present     (RC2)
 *   bit 2  AC power present        (RC5)
 * The remaining bits are always zero. The optional change hook fires in
 * main context whenever the debounced state transitions. */
typedef void (*SensorsChangeHandler)(uint8_t prev, uint8_t curr);

void    sensors_init              (TaskController *ctrl);
uint8_t sensors_state             (void);
void    sensors_set_change_handler(SensorsChangeHandler handler);

#endif /* SENSORS_H */
