#ifndef RGBLED_H
#define RGBLED_H

#include <xc.h>

typedef union {
    struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };
    uint8_t raw[3];
} RGBLedData;

void rgbled_init(void);
void rgbled_set(RGBLedData* rgb, uint8_t count);

#endif /* RGBLED_H */
