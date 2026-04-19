#include "display_text.h"

#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
#include "task_ids.h"

#include <stdint.h>

/* ============================================================================
 * 5x8 column-major ASCII font — just the glyphs the UI actually renders.
 *
 * Each entry is 5 bytes, one byte per column, bit 0 = top pixel. A 1-px
 * vertical gap is added between characters at render time so cell pitch is
 * 6 px.
 * ============================================================================
 */

typedef struct {
    char ch;
    uint8_t cols[5];
} Glyph;

#define FONT_W 5
#define CHAR_W 6
#define FONT_H 8

static const Glyph font[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}}, {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {'%', {0x62, 0x64, 0x08, 0x13, 0x23}}, {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}}, {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}}, {'2', {0x62, 0x51, 0x49, 0x49, 0x46}},
    {'3', {0x22, 0x41, 0x49, 0x49, 0x36}}, {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}}, {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}}, {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}}, {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}}, {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}}, {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}}, {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}}, {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}}, {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}}, {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}}, {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}}, {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
};

static const uint8_t* glyph_cols(char c) {
    for (uint8_t i = 0; i < sizeof(font) / sizeof(font[0]); i++) {
        if (font[i].ch == c) {
            return font[i].cols;
        }
    }
    return font[0].cols;
}

/* ============================================================================
 * SSD1322 layout
 *
 * Panel is 256x64 @ 4 bpp. The "column" address unit on this controller is
 * 4 pixels (= 2 bytes of RAM); a 256-wide panel therefore uses 64 column
 * steps. On the common 3.12" module the visible region starts at column
 * 0x1C (datasheet default) — may need tuning per board.
 * ============================================================================
 */

#define SSD1322_COL_CMD 0x15
#define SSD1322_ROW_CMD 0x75
#define SSD1322_WRITE_CMD 0x5C
#define SSD1322_COL_START 0x1C
#define SSD1322_COL_END 0x5B

#define DISPLAY_ROW_BYTES 128u /* 256 pixels / 2 pixels per byte */
#define DISPLAY_TEXT_COLUMNS (DISPLAY_ROW_BYTES * 2u)

#define REFRESH_MS 250u

#define LINE_COUNT 3
#define LINE_TOP_0 8
#define LINE_TOP_1 28
#define LINE_TOP_2 48

#define MAX_LINE_CHARS 20u

static const uint8_t line_tops[LINE_COUNT] = {LINE_TOP_0, LINE_TOP_1, LINE_TOP_2};

static void render_row(const char* text, uint8_t row);
static void blit_line(uint8_t line_idx, const char* text);
static void blit_all(const char* const lines[LINE_COUNT]);
static void format_levels(char* out, const char* label, uint8_t level_raw);
static void format_battery(char* out, uint16_t mv);
static void format_nav_slot(char* out, uint8_t slot, uint8_t enabled, uint8_t cursor);
static uint8_t append_str(char* out, uint8_t pos, const char* s);
static uint8_t append_u8(char* out, uint8_t pos, uint16_t value, uint8_t min_digits);
static void refresh_task(TaskId id, void* ctx);

void display_text_init(TaskController* ctrl) {
    task_controller_add(ctrl, TASK_DISPLAY_TEXT, REFRESH_MS, refresh_task, 0);
}

/* ---------------------------------------------------------------------------
 * Rendering
 * ---------------------------------------------------------------------------
 */

static void blit_line(uint8_t line_idx, const char* text) {
    uint8_t top = line_tops[line_idx];

    uint8_t col_args[2] = {SSD1322_COL_START, SSD1322_COL_END};
    uint8_t row_args[2] = {top, (uint8_t)(top + FONT_H - 1u)};

    display_send_cmd(SSD1322_COL_CMD);
    display_send_data(col_args, 2);
    display_send_cmd(SSD1322_ROW_CMD);
    display_send_data(row_args, 2);
    display_send_cmd(SSD1322_WRITE_CMD);

    for (uint8_t row = 0; row < FONT_H; row++) {
        render_row(text, row);
    }
}

static void blit_all(const char* const lines[LINE_COUNT]) {
    for (uint8_t i = 0; i < LINE_COUNT; i++) {
        blit_line(i, lines[i]);
    }
}

/* Emit one display row (128 bytes / 256 pixels) for the given text string.
 * Characters past the text length render as blank, padding the remainder of
 * the row to the controller's expected column range. */
static void render_row(const char* text, uint8_t row) {
    uint16_t text_len = 0;
    while (text[text_len] != '\0' && text_len < MAX_LINE_CHARS) {
        text_len++;
    }
    uint16_t text_px = (uint16_t)text_len * CHAR_W;

    uint8_t accum = 0;
    for (uint16_t x = 0; x < DISPLAY_TEXT_COLUMNS; x++) {
        uint8_t on = 0;
        if (x < text_px) {
            uint16_t char_idx = x / CHAR_W;
            uint8_t col_in_char = (uint8_t)(x - char_idx * CHAR_W);
            if (col_in_char < FONT_W) {
                const uint8_t* g = glyph_cols(text[char_idx]);
                on = (uint8_t)((g[col_in_char] >> row) & 1u);
            }
        }
        uint8_t nibble = on ? 0x0F : 0x00;
        if ((x & 1u) == 0u) {
            accum = (uint8_t)(nibble << 4); /* left pixel -> high nibble */
        } else {
            accum |= nibble; /* right pixel -> low nibble */
            display_send_data(&accum, 1);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Formatting helpers — zero-alloc string assembly for the text UI
 * ---------------------------------------------------------------------------
 */

static uint8_t append_str(char* out, uint8_t pos, const char* s) {
    while (*s != '\0') {
        out[pos++] = *s++;
    }
    return pos;
}

static uint8_t append_u8(char* out, uint8_t pos, uint16_t value, uint8_t min_digits) {
    char buf[5];
    uint8_t n = 0;
    if (value == 0) {
        buf[n++] = '0';
    } else {
        while (value > 0 && n < sizeof(buf)) {
            buf[n++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
    }
    while (n < min_digits) {
        buf[n++] = '0';
    }
    while (n > 0) {
        out[pos++] = buf[--n];
    }
    return pos;
}

static void format_levels(char* out, const char* label, uint8_t level_raw) {
    /* level_raw is ADC output 0..255 scaled into a rough 0..100% read-out. */
    uint16_t percent = (uint16_t)(((uint16_t)level_raw * 100u + 127u) / 255u);
    if (percent > 100u) {
        percent = 100u;
    }
    uint8_t pos = 0;
    pos = append_str(out, pos, label);
    out[pos++] = ' ';
    pos = append_u8(out, pos, percent, 1);
    out[pos++] = '%';
    out[pos] = '\0';
}

static void format_battery(char* out, uint16_t mv) {
    /* Show as XX.YV with one decimal place, clamped at 99.9V. */
    uint16_t tenths = (uint16_t)((mv + 50u) / 100u);
    if (tenths > 999u) {
        tenths = 999u;
    }
    uint16_t whole = tenths / 10u;
    uint16_t frac = tenths % 10u;
    uint8_t pos = 0;
    pos = append_str(out, pos, "BATT ");
    pos = append_u8(out, pos, whole, 1);
    out[pos++] = '.';
    pos = append_u8(out, pos, frac, 1);
    out[pos++] = 'V';
    out[pos] = '\0';
}

static void format_nav_slot(char* out, uint8_t slot, uint8_t enabled, uint8_t cursor) {
    static const char* names[] = {"ANCHOR", "TRICOL", "STEAM", "BOW", "STERN"};
    uint8_t pos = 0;
    out[pos++] = (slot == cursor) ? '>' : ' ';
    pos = append_str(out, pos, names[slot]);
    out[pos++] = ' ';
    pos = append_str(out, pos, (enabled & (uint8_t)(1u << slot)) ? "ON" : "OFF");
    out[pos] = '\0';
}

/* ---------------------------------------------------------------------------
 * Periodic refresh
 * ---------------------------------------------------------------------------
 */

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    static char buf0[MAX_LINE_CHARS + 1];
    static char buf1[MAX_LINE_CHARS + 1];
    static char buf2[MAX_LINE_CHARS + 1];
    const char* lines[LINE_COUNT] = {buf0, buf1, buf2};

    if (config_mode_active()) {
        uint8_t enabled = config_get_nav_enabled_mask();
        uint8_t cursor = config_mode_cursor();
        /* Show the cursor row and one slot on each side so the user can
         * see the neighbourhood while editing. */
        uint8_t top = (cursor == 0) ? 0 : (uint8_t)(cursor - 1u);
        if (top > 2u) {
            top = 2u;
        }
        format_nav_slot(buf0, top, enabled, cursor);
        format_nav_slot(buf1, (uint8_t)(top + 1u), enabled, cursor);
        format_nav_slot(buf2, (uint8_t)(top + 2u), enabled, cursor);
    } else {
        format_levels(buf0, "WATER", controller_level(0));
        format_levels(buf1, "FUEL", controller_level(1));
        format_battery(buf2, controller_battery_mv());
    }

    blit_all(lines);
}
