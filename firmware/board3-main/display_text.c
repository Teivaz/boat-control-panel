#include "display_text.h"

#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
#include "rtc.h"
#include "task_ids.h"
#include "u8g2.h"

#include <stdint.h>

/* Three text lines on the 256x64 panel. The unifont glyph cell is 8x16 px;
 * we baseline-align at y = 16, 36, 56 to spread the three rows evenly. */
#define REFRESH_MS 250u
#define LINE_BASELINE_0 16
#define LINE_BASELINE_1 36
#define LINE_BASELINE_2 56
#define MAX_LINE_CHARS 20u

#define DISPLAY_WIDTH 256
#define TIME_BASELINE 10 /* luRS10 cap height ~10 px, sits above row 0 */

static const uint8_t baselines[3] = {LINE_BASELINE_0, LINE_BASELINE_1, LINE_BASELINE_2};

static void refresh_task(TaskId id, void* ctx);
static void render_normal(u8g2_t* g);
static void render_config_menu(u8g2_t* g);
static void render_config_nav(u8g2_t* g);
static void render_config_time(u8g2_t* g);
static void render_config_offset(u8g2_t* g);
static void format_levels(char* out, const char* label, uint8_t level_raw);
static void format_battery(char* out, uint16_t mv);
static void format_nav_slot(char* out, uint8_t slot, uint8_t enabled, uint8_t cursor);
static void format_hhmm(char* out, uint8_t hour, uint8_t minute);
static uint8_t append_str(char* out, uint8_t pos, const char* s);
static uint8_t append_u8(char* out, uint8_t pos, uint16_t value, uint8_t min_digits);

void display_text_init(TaskController* ctrl) {
    task_controller_add(ctrl, TASK_DISPLAY_TEXT, REFRESH_MS, refresh_task, 0);
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

static void format_hhmm(char* out, uint8_t hour, uint8_t minute) {
    out[0] = (char)('0' + (hour / 10u));
    out[1] = (char)('0' + (hour % 10u));
    out[2] = ':';
    out[3] = (char)('0' + (minute / 10u));
    out[4] = (char)('0' + (minute % 10u));
    out[5] = '\0';
}

/* ---------------------------------------------------------------------------
 * Per-screen rendering
 * ---------------------------------------------------------------------------
 */

static void render_normal(u8g2_t* g) {
    char buf[3][MAX_LINE_CHARS + 1];
    format_levels(buf[0], "WATER", controller_level(0));
    format_levels(buf[1], "FUEL", controller_level(1));
    format_battery(buf[2], controller_battery_mv());
    for (uint8_t i = 0; i < 3; i++) {
        u8g2_DrawStr(g, 0, baselines[i], buf[i]);
    }
    /* Time top-right in the small font — only if the RTC shadow is valid. */
    RtcTime t;
    if (controller_time(&t)) {
        char tbuf[6];
        format_hhmm(tbuf, t.hour, t.minute);
        u8g2_SetFont(g, u8g2_font_luRS10_tr);
        uint8_t w = (uint8_t)u8g2_GetStrWidth(g, tbuf);
        u8g2_DrawStr(g, DISPLAY_WIDTH - w, TIME_BASELINE, tbuf);
        u8g2_SetFont(g, u8g2_font_unifont_tr);
    }
}

static void render_config_menu(u8g2_t* g) {
    static const char* items[CONFIG_MENU_COUNT] = {
        "NAV LIGHTS", "SET TIME", "WATER OFS", "FUEL OFS",
    };
    uint8_t cursor = config_mode_menu_cursor();
    /* Scroll vertically when the cursor would fall off the 3-line window. */
    uint8_t top = (cursor < 3u) ? 0u : (uint8_t)(cursor - 2u);
    if ((uint8_t)(top + 3u) > CONFIG_MENU_COUNT) {
        top = (uint8_t)(CONFIG_MENU_COUNT - 3u);
    }
    char line[MAX_LINE_CHARS + 1];
    for (uint8_t i = 0; i < 3 && (uint8_t)(top + i) < CONFIG_MENU_COUNT; i++) {
        uint8_t item = (uint8_t)(top + i);
        uint8_t pos = 0;
        line[pos++] = (item == cursor) ? '>' : ' ';
        line[pos++] = ' ';
        pos = append_str(line, pos, items[item]);
        line[pos] = '\0';
        u8g2_DrawStr(g, 0, baselines[i], line);
    }
}

static void render_config_nav(u8g2_t* g) {
    uint8_t enabled = config_mode_nav_working_mask();
    uint8_t cursor = config_mode_nav_cursor();
    uint8_t top = (cursor == 0) ? 0 : (uint8_t)(cursor - 1u);
    if (top > 2u) {
        top = 2u;
    }
    char buf[MAX_LINE_CHARS + 1];
    for (uint8_t i = 0; i < 3; i++) {
        format_nav_slot(buf, (uint8_t)(top + i), enabled, cursor);
        u8g2_DrawStr(g, 0, baselines[i], buf);
    }
}

static void render_config_time(u8g2_t* g) {
    char title[] = "SET TIME";
    u8g2_DrawStr(g, 0, baselines[0], title);
    char tbuf[6];
    format_hhmm(tbuf, config_mode_time_hour(), config_mode_time_minute());
    u8g2_DrawStr(g, 0, baselines[1], tbuf);
    /* Underline the field being edited. The unifont cell is 8 px wide, so
     * hours occupy x=0..15 and minutes x=24..39 with the colon in between. */
    uint8_t field = config_mode_time_field();
    uint8_t x = (field == 0) ? 0 : 24;
    u8g2_DrawHLine(g, x, baselines[1] + 2, 16);
}

static void render_config_offset(u8g2_t* g) {
    const char* title = (config_mode_offset_target() == 0) ? "WATER OFS" : "FUEL OFS";
    u8g2_DrawStr(g, 0, baselines[0], title);
    char vbuf[4];
    /* Always render as 3 zero-padded digits so the field width is stable. */
    uint8_t v = config_mode_offset_value();
    vbuf[0] = (char)('0' + (v / 100u));
    vbuf[1] = (char)('0' + ((v / 10u) % 10u));
    vbuf[2] = (char)('0' + (v % 10u));
    vbuf[3] = '\0';
    u8g2_DrawStr(g, 0, baselines[1], vbuf);
    u8g2_DrawHLine(g, 0, baselines[1] + 2, 24);
}

/* ---------------------------------------------------------------------------
 * Periodic refresh — assemble the active screen and ship via the u8g2 full
 * buffer in one transaction.
 * ---------------------------------------------------------------------------
 */

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    u8g2_t* g = display_u8g2();
    u8g2_ClearBuffer(g);

    if (config_mode_active()) {
        switch (config_mode_screen()) {
            case CONFIG_SCREEN_MENU:   render_config_menu(g); break;
            case CONFIG_SCREEN_NAV:    render_config_nav(g); break;
            case CONFIG_SCREEN_TIME:   render_config_time(g); break;
            case CONFIG_SCREEN_OFFSET: render_config_offset(g); break;
            default: break;
        }
    } else {
        render_normal(g);
    }

    u8g2_SendBuffer(g);
}
