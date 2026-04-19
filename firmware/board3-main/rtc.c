#include "rtc.h"
#include "i2c.h"
#include "libcomm.h"

static inline uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

uint8_t rtc_read(RtcTime *out) {
    uint8_t reg = 0x00;
    uint8_t buf[7];
    if (i2c_receive(COMM_ADDRESS_RTC, &reg, 1, buf, sizeof(buf)) != I2C_RESULT_OK) {
        return 0;
    }
    out->second = bcd_to_bin(buf[0] & 0x7F);
    out->minute = bcd_to_bin(buf[1] & 0x7F);
    /* Hours register: bit 6 selects 12h mode. Assume the chip is in 24h
     * mode (default after POR); mask the mode bits. */
    out->hour   = bcd_to_bin(buf[2] & 0x3F);
    out->day    = (uint8_t)(buf[3] & 0x07);
    out->date   = bcd_to_bin(buf[4] & 0x3F);
    /* Month register: bit 7 is the century flag; mask it out. */
    out->month  = bcd_to_bin(buf[5] & 0x1F);
    out->year   = (uint16_t)(2000 + bcd_to_bin(buf[6]));
    return 1;
}
