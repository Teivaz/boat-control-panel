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

/* Async completion: ok==1 means *time is populated. Runs in main context. */
typedef void (*RtcReadCompletion)(uint8_t ok, const RtcTime* time, void* ctx);
typedef void (*RtcWriteCompletion)(uint8_t ok, void* ctx);

/* Read the 7 time registers at 0x00. The driver retries once with bus
 * recovery on transient failure before reporting ok=0 to the callback. */
void rtc_read(RtcReadCompletion cb, void* ctx);

/* Update seconds (zeroed), minutes and hours (24h mode). Date / day /
 * month / year are left untouched. Bus-recovery retry on failure. */
void rtc_write_time(uint8_t hour, uint8_t minute, RtcWriteCompletion cb, void* ctx);

#endif /* RTC_H */
