#include "i2c_log.h"

#include "i2c.h"
#include "libcomm.h"
#include "u8g2.h"

#include <stdint.h>
#include <xc.h>

/* Capacity must be a power of two so we can mask-wrap the head index.
 * 8 entries gives some headroom beyond the 6 lines we render. */
#define LOG_CAPACITY  8u
#define LOG_MASK      (LOG_CAPACITY - 1u)
#define LOG_DATA_MAX  6u  /* enough for the largest typed payload we emit */

/* Right-half layout on the 256x64 panel.  luRS10 cap height ~10 px, so we
 * pack 6 lines with 10 px spacing (baselines at 10, 20, 30, 40, 50, 60). */
#define LOG_X            130u
#define LOG_LINE_HEIGHT  10u
#define LOG_MAX_LINES    6u
#define LOG_TEXT_MAX     24u  /* label + up to 8 hex bytes with spaces */

typedef struct {
    uint8_t kind;  /* I2cLogKind */
    uint8_t addr;
    uint8_t len;
    uint8_t data[LOG_DATA_MAX];
} LogEntry;

/* Single-writer (ISR via on_event) / single-reader (main via i2c_log_render
 * with IRQ lockout), so plain (non-volatile) storage suffices. */
static LogEntry g_log[LOG_CAPACITY];
static uint8_t g_head = 0;   /* index of oldest entry */
static uint8_t g_count = 0;  /* entries currently stored */

static void append_entry(I2cLogKind kind, uint8_t addr, const uint8_t* data, uint8_t len) {
    uint8_t slot;
    if (g_count < LOG_CAPACITY) {
        slot = (uint8_t)((g_head + g_count) & LOG_MASK);
        g_count++;
    } else {
        slot = g_head;
        g_head = (uint8_t)((g_head + 1u) & LOG_MASK);
    }
    g_log[slot].kind = (uint8_t)kind;
    g_log[slot].addr = addr;
    uint8_t n = (len > LOG_DATA_MAX) ? LOG_DATA_MAX : len;
    g_log[slot].len = n;
    for (uint8_t i = 0; i < n; i++) {
        g_log[slot].data[i] = data[i];
    }
}

static void on_event(I2cLogKind kind, uint8_t addr, const uint8_t* data, uint8_t len) {
    append_entry(kind, addr, data, len);
}

void i2c_log_init(void) {
    i2c_set_logger(on_event);
}

static const char* kind_label(uint8_t k) {
    switch (k) {
        case I2C_LOG_CR: return "CR";
        case I2C_LOG_WA: return "WA";
        case I2C_LOG_WN: return "WN";
        case I2C_LOG_RA: return "RA";
        case I2C_LOG_RN: return "RN";
        default:         return "??";
    }
}

static char nibble_hex(uint8_t n) {
    return (char)((n < 10u) ? ('0' + n) : ('A' + (n - 10u)));
}

static uint8_t append_hex(char* out, uint8_t pos, uint8_t v) {
    out[pos++] = nibble_hex((uint8_t)((v >> 4) & 0x0F));
    out[pos++] = nibble_hex((uint8_t)(v & 0x0F));
    return pos;
}

static void format_entry(char* out, const LogEntry* e) {
    const char* label = kind_label(e->kind);
    uint8_t pos = 0;
    out[pos++] = label[0];
    out[pos++] = label[1];
    /* CR carries sender/id in its payload; host ops prefix the target addr. */
    if (e->kind != I2C_LOG_CR) {
        out[pos++] = ' ';
        pos = append_hex(out, pos, e->addr);
    }
    for (uint8_t j = 0; j < e->len && pos + 3u <= LOG_TEXT_MAX; j++) {
        out[pos++] = ' ';
        pos = append_hex(out, pos, e->data[j]);
    }
    out[pos] = '\0';
}

void i2c_log_render(u8g2_t* g) {
    /* Snapshot the ring buffer under IRQ lockout so an ISR append can't
     * interleave mid-copy. */
    LogEntry snapshot[LOG_CAPACITY];
    uint8_t count;
    uint8_t head;
    INTERRUPT_PUSH;
    count = g_count;
    head = g_head;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = (uint8_t)((head + i) & LOG_MASK);
        snapshot[i] = g_log[idx];  /* struct copy */
    }
    INTERRUPT_POP;

    u8g2_SetFont(g, u8g2_font_luRS10_tr);

    uint8_t shown = (count < LOG_MAX_LINES) ? count : LOG_MAX_LINES;
    char buf[LOG_TEXT_MAX + 1];
    for (uint8_t i = 0; i < shown; i++) {
        /* Newest at the top: snapshot[count-1] is newest, snapshot[0] oldest. */
        const LogEntry* e = &snapshot[count - 1u - i];
        format_entry(buf, e);
        uint8_t baseline = (uint8_t)(LOG_LINE_HEIGHT * (i + 1u));
        u8g2_DrawStr(g, LOG_X, baseline, buf);
    }

    /* Leave font restored for subsequent draws. */
    u8g2_SetFont(g, u8g2_font_unifont_tr);
}
