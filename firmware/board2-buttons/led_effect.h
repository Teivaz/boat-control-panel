#ifndef LED_EFFECT_H
#define LED_EFFECT_H

#include "libcomm.h"
#include "task.h"

#include <stdint.h>

#define LED_EFFECT_COUNT 7

void led_effect_init(TaskController *ctrl);

/* Per-LED effect — color + mode from the button_effect nibble. Takes effect
 * on the next animation tick. */
void led_effect_set(uint8_t led_id, CommButtonOutputEffect eff);
CommButtonOutputEffect led_effect_get(uint8_t led_id);

#endif /* LED_EFFECT_H */
