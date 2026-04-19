#ifndef RTC_H
#define RTC_H

#include <stdint.h>

/* DS3231 time snapshot. Fields are binary (BCD is decoded by the driver). */
typedef struct {
    uint8_t second; /* 0-59 */
    uint8_t minute; /* 0-59 */
    uint8_t hour;   /* 0-23 */
    uint8_t day;    /* 1-7  */
    uint8_t date;   /* 1-31 */
    uint8_t month;  /* 1-12 */
    uint16_t year;  /* 2000-2099 */
} RtcTime;

/* Blocking read of the 7 time registers at address 0x00. Returns 1 on
 * success, 0 on any I2C failure (caller should retry on the next tick). */
uint8_t rtc_read(RtcTime* out);

#endif /* RTC_H */
