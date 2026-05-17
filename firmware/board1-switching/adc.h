#ifndef ADC_H
#define ADC_H

#include "task.h"

#include <stdint.h>

/* Continuous three-channel ADC sweep. Each conversion is a hardware 16-sample
 * burst-average; the AD-completion ISR caches the raw count and schedules a
 * per-channel processor onto the main loop via run_in_main_loop. The
 * processors convert raw → mV (via DIA cal), normalise against the per-channel
 * (minimum, nominal) calibration in EEPROM, apply offset trim, and (for the
 * level meters) map the result to a 0..100 percent via the configured
 * CommMeterMode. Read accessors return the most-recent processed value. */
void adc_init(TaskController* ctrl);

uint16_t adc_read_battery_mv(void);       /* 12 V rail, real mV */
uint8_t adc_read_level_fresh_water(void); /* 0..100 percent */
uint8_t adc_read_level_fuel(void);        /* 0..100 percent */

#endif /* ADC_H */
