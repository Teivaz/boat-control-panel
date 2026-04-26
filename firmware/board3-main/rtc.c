#include "rtc.h"

#include "i2c.h"
#include "libcomm.h"

/* Single in-flight DS3231 transaction. Both read and write callers are
 * driven from the controller polling tasks (250 ms cadence) and the menu
 * UI; concurrency is bounded to one. */

typedef enum {
    RTC_OP_NONE = 0,
    RTC_OP_READ,
    RTC_OP_WRITE,
} RtcOp;

static struct {
    RtcOp op;
    uint8_t attempt; /* 0 = first, 1 = post-bus-recover retry */
    /* Per-op state */
    uint8_t read_reg; /* RTC register pointer (0x00) for the write phase */
    uint8_t read_buf[7];
    uint8_t write_buf[4];
    uint8_t write_hour;
    uint8_t write_minute;
    union {
        RtcReadCompletion read;
        RtcWriteCompletion write;
    } cb;
    void* ctx;
} state;

static void start_read(void);
static void start_write(void);
static void on_read_done(I2cResult r, uint8_t* rx, uint8_t rx_len, void* ctx);
static void on_write_done(I2cResult r, uint8_t* rx, uint8_t rx_len, void* ctx);
static inline uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}
static inline uint8_t bin_to_bcd(uint8_t bin) {
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

void rtc_read(RtcReadCompletion cb, void* ctx) {
    if (state.op != RTC_OP_NONE) {
        if (cb) {
            cb(0, 0, ctx);
        }
        return;
    }
    state.op = RTC_OP_READ;
    state.attempt = 0;
    state.cb.read = cb;
    state.ctx = ctx;
    start_read();
}

void rtc_write_time(uint8_t hour, uint8_t minute, RtcWriteCompletion cb, void* ctx) {
    if (hour > 23 || minute > 59 || state.op != RTC_OP_NONE) {
        if (cb) {
            cb(0, ctx);
        }
        return;
    }
    state.op = RTC_OP_WRITE;
    state.attempt = 0;
    state.cb.write = cb;
    state.ctx = ctx;
    state.write_hour = hour;
    state.write_minute = minute;
    start_write();
}

static void start_read(void) {
    state.read_reg = 0x00;
    if (i2c_submit(COMM_ADDRESS_RTC, &state.read_reg, 1, state.read_buf, sizeof(state.read_buf), on_read_done, 0) !=
        I2C_RESULT_OK) {
        on_read_done(I2C_RESULT_QUEUE_FULL, 0, 0, 0);
    }
}

static void start_write(void) {
    /* Single transaction: register pointer (0x00) + sec/min/hour BCD.
     * Hour bit 6 cleared keeps the chip in 24-hour mode. */
    state.write_buf[0] = 0x00;
    state.write_buf[1] = bin_to_bcd(0);
    state.write_buf[2] = bin_to_bcd(state.write_minute);
    state.write_buf[3] = (uint8_t)(bin_to_bcd(state.write_hour) & 0x3F);
    if (i2c_submit(COMM_ADDRESS_RTC, state.write_buf, sizeof(state.write_buf), 0, 0, on_write_done, 0) !=
        I2C_RESULT_OK) {
        on_write_done(I2C_RESULT_QUEUE_FULL, 0, 0, 0);
    }
}

static void on_read_done(I2cResult r, uint8_t* rx, uint8_t rx_len, void* ctx) {
    (void)rx;
    (void)rx_len;
    (void)ctx;
    if (r != I2C_RESULT_OK) {
        if (state.attempt == 0) {
            /* DS3231 can be left holding SDA low if a transaction is
             * aborted mid-byte — clock it free and retry once. */
            state.attempt = 1;
            i2c_bus_recover();
            start_read();
            return;
        }
        RtcReadCompletion cb = state.cb.read;
        void* user_ctx = state.ctx;
        state.op = RTC_OP_NONE;
        if (cb) {
            cb(0, 0, user_ctx);
        }
        return;
    }

    RtcTime t;
    t.second = bcd_to_bin(state.read_buf[0] & 0x7F);
    t.minute = bcd_to_bin(state.read_buf[1] & 0x7F);
    /* Hours register: bit 6 selects 12h mode. Assume 24h (POR default);
     * mask the mode bits. */
    t.hour = bcd_to_bin(state.read_buf[2] & 0x3F);
    t.day = (uint8_t)(state.read_buf[3] & 0x07);
    t.date = bcd_to_bin(state.read_buf[4] & 0x3F);
    /* Month register: bit 7 is the century flag; mask it out. */
    t.month = bcd_to_bin(state.read_buf[5] & 0x1F);
    t.year = (uint16_t)(2000 + bcd_to_bin(state.read_buf[6]));

    RtcReadCompletion cb = state.cb.read;
    void* user_ctx = state.ctx;
    state.op = RTC_OP_NONE;
    if (cb) {
        cb(1, &t, user_ctx);
    }
}

static void on_write_done(I2cResult r, uint8_t* rx, uint8_t rx_len, void* ctx) {
    (void)rx;
    (void)rx_len;
    (void)ctx;
    if (r != I2C_RESULT_OK) {
        if (state.attempt == 0) {
            state.attempt = 1;
            i2c_bus_recover();
            start_write();
            return;
        }
        RtcWriteCompletion cb = state.cb.write;
        void* user_ctx = state.ctx;
        state.op = RTC_OP_NONE;
        if (cb) {
            cb(0, user_ctx);
        }
        return;
    }
    RtcWriteCompletion cb = state.cb.write;
    void* user_ctx = state.ctx;
    state.op = RTC_OP_NONE;
    if (cb) {
        cb(1, user_ctx);
    }
}
