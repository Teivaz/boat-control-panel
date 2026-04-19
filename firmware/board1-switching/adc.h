#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/* Polled ADC helpers. Each call triggers a 16-sample burst on the selected
 * channel (~30 µs @ Fosc/64) and returns the averaged result. Main context
 * only; uses the FVR module at 2.048 V as the positive reference. */
void adc_init(void);

uint16_t adc_read_battery_mv(void);       // 12 V rail after /10 divider
uint8_t adc_read_level_fresh_water(void); // raw 8-bit (ADRES >> 4)
uint8_t adc_read_level_fuel(void);        // raw 8-bit (ADRES >> 4)

#endif /* ADC_H */
