#include "i2c_log.h"

#include "i2c.h"
#include "libcomm.h"
#include "u8g2.h"

#include <stdint.h>
#include <xc.h>

/* Right-half layout on the 256x64 panel.  u8g2_font_5x7_tr is a fixed 5x7
 * pixel cell, so 8 px line spacing gives 8 visible lines in the 64 px
 * height (baselines at 7, 15, 23, 31, 39, 47, 55, 63). */
#define LOG_X            130u
#define LOG_LINE_HEIGHT  8u
#define LOG_MAX_LINES    8u
#define LOG_TEXT_MAX     24u  /* label + up to 8 hex bytes with spaces */

void i2c_log_init(void) {
    /* Driver owns the ring buffer; nothing to register here.  Retained as a
     * stable symbol in case boot wiring wants a hook in the future. */
}

#if I2C_LOG_VISIBLE
static uint8_t crc_valid(const I2cLogEntry* e) {
    /* CR/CT frames store payload + trailing CRC; anything <2 bytes can't be
     * framed, so fail closed. */
    if (e->len < 2u) {
        return 0;
    }
    return (uint8_t)(comm_crc8(e->data, (uint8_t)(e->len - 1u)) == e->data[e->len - 1u]);
}

static const char* kind_label(const I2cLogEntry* e) {
    switch (e->kind) {
        case I2C_LOG_CR: return crc_valid(e) ? "CR+" : "CR!";
        case I2C_LOG_CT: return crc_valid(e) ? "CT+" : "CT!";
        case I2C_LOG_WA: return "W+";
        case I2C_LOG_WN: return "W-";
        /* Distinguish:
         *   R+  bus-level ACK on all bytes AND response CRC matches
         *   R!  bus-level ACK on all bytes BUT response CRC mismatch
         *       (slave responded with wrong content — often "stale
         *       g_client_tx leaked through" or "torn shadow read")
         *   R-  bus-level NACK / abort (address NACK, BTO, collision) */
        case I2C_LOG_RA: return crc_valid(e) ? "R+" : "R!";
        case I2C_LOG_RN: return "R-";
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

static void format_entry(char* out, const I2cLogEntry* e) {
    uint8_t pos = 0;
    /* Per-transaction id (hex, wraps at 0xFF) — WA and RA for a write-
     * then-read share an id, so the eye can pair them visually. */
    pos = append_hex(out, pos, e->req_id);
    out[pos++] = ' ';
    const char* label = kind_label(e);
    while (*label && pos < LOG_TEXT_MAX) {
        out[pos++] = *label++;
    }
    /* CR / CT don't carry a separate peer address (CR embeds the sender in
     * its payload, CT uses addr=0); host ops prefix the target addr. */
    if (e->kind != I2C_LOG_CR && e->kind != I2C_LOG_CT) {
        out[pos++] = ' ';
        pos = append_hex(out, pos, e->addr);
    }
    for (uint8_t j = 0; j < e->len && pos + 3u <= LOG_TEXT_MAX; j++) {
        out[pos++] = ' ';
        pos = append_hex(out, pos, e->data[j]);
    }
    out[pos] = '\0';
}
#endif /* I2C_LOG_VISIBLE */

void i2c_log_render(u8g2_t* g) {
#if I2C_LOG_VISIBLE
    I2cLogEntry snapshot[LOG_MAX_LINES];
    uint8_t count = i2c_log_snapshot(snapshot, LOG_MAX_LINES);

    u8g2_SetFont(g, u8g2_font_5x7_tr);

    char buf[LOG_TEXT_MAX + 1];
    for (uint8_t i = 0; i < count; i++) {
        /* snapshot[0] is newest — render at the top. */
        format_entry(buf, &snapshot[i]);
        uint8_t baseline = (uint8_t)(LOG_LINE_HEIGHT * (i + 1u) - 1u);
        u8g2_DrawStr(g, LOG_X, baseline, buf);
    }

    /* Leave font restored for subsequent draws. */
    u8g2_SetFont(g, u8g2_font_unifont_tr);
#else
    (void)g;
#endif
}
