#ifndef ADC_H
#define ADC_H

#include "task.h"

#include <stdint.h>

/* Continuous three-channel ADC sweep. Each conversion is a hardware 16-sample
 * burst-average; the AD completion interrupt caches the result and starts the
 * next channel, so the read accessors always return the most recently sampled
 * value without blocking. The level-meter channels additionally feed a
 * first-order running-average filter whose time constant spans several
 * seconds, absorbing wave-induced float oscillation. */
void adc_init(void);

uint16_t adc_read_battery_mv(void);       /* 12 V rail after /10 divider */
uint8_t adc_read_level_fresh_water(void); /* 8-bit smoothed float reading */
uint8_t adc_read_level_fuel(void);        /* 8-bit smoothed float reading */

#endif /* ADC_H */
