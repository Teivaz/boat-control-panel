#include "display_text.h"

#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
#include "i2c_log.h"
#include "rtc.h"
#include "task_ids.h"
#include "u8g2.h"

#include <stdint.h>

/* Normal mode: 2 columns × 3 rows. Left column carries "LABEL NN%" text;
 * right column carries a framed horizontal progress bar reflecting the
 * same percentage. Rows are separated by full-width divider lines. Row
 * baselines (16/36/56) are the same as before so the unifont 8x16 cells
 * sit centred in each 21 px row strip. */
#define REFRESH_MS 250u
#define LINE_BASELINE_0 16
#define LINE_BASELINE_1 36
#define LINE_BASELINE_2 56
#define MAX_LINE_CHARS 20u

#define DISPLAY_WIDTH 256

/* Progress-bar geometry — derived pixel-for-pixel from Display-v4.png.
 * The bar lives entirely in the right half (x=128..253) inside a 1 px
 * gap to the display's right border at x=255. Each row's bar is 18 px
 * tall (one row of padding at top and bottom of the row strip). */
#define BAR_LEFT_X 128
#define BAR_WIDTH 126
#define BAR_HEIGHT 18

/* Tick notches drawn at the bottom of each bar using XOR colour, so they
 * read as dark notches inside the filled portion and as light marks
 * against the empty portion. Side ticks are 1 px wide × 3 rows; the
 * centre tick is 2 px wide × 5 rows, extending 2 rows higher than the
 * sides. dx values are offsets from BAR_LEFT_X. */
#define SIDE_TICK_HEIGHT 3
#define CENTER_TICK_HEIGHT 5
#define CENTER_TICK_DX 59  /* left pixel of the 2-wide centre tick */

/* Text column lives in the left half. Vertical separator sits at x=126,
 * leaving a 1 px gap (x=127) before the bar starts at x=128 — that gap
 * stops the bar fill from visually touching the separator. Text is
 * right-aligned to TEXT_RIGHT_X so values line up across all rows. */
#define TEXT_LEFT_X 2
#define TEXT_RIGHT_X 125
#define VSEP_X 126

static const uint8_t baselines[3] = {LINE_BASELINE_0, LINE_BASELINE_1, LINE_BASELINE_2};

static void refresh_task(TaskId id, void* ctx);
static void render_normal(u8g2_t* g);
static void render_config_menu(u8g2_t* g);
static void render_config_nav(u8g2_t* g);
static void render_config_time(u8g2_t* g);
static void render_config_offset(u8g2_t* g);
static void format_row_text(char* out, const char* label, uint8_t value);
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

/* Compose just the value portion of a row — caller positions it
 * separately (right-aligned). Returns the character length written
 * (without the NUL). */
static uint8_t format_percent(char* out, uint8_t value) {
    uint8_t pos = append_u8(out, 0, value, 1);
    out[pos++] = '%';
    out[pos] = '\0';
    return pos;
}

static uint8_t format_voltage(char* out, uint16_t mv) {
    /* Show as XX.YV with one decimal place, clamped at 99.9V. */
    uint16_t tenths = (uint16_t)((mv + 50u) / 100u);
    if (tenths > 999u) {
        tenths = 999u;
    }
    uint16_t whole = tenths / 10u;
    uint16_t frac = tenths % 10u;
    uint8_t pos = 0;
    pos = append_u8(out, pos, whole, 1);
    out[pos++] = '.';
    pos = append_u8(out, pos, frac, 1);
    out[pos++] = 'V';
    out[pos] = '\0';
    return pos;
}

/* 12 V lead-acid SoC from battery voltage. Linear between 11.8 V (0 %) and
 * 12.7 V (100 %) — close enough to the resting-voltage curve for gauge use.
 * Sags under load, so values shown at high current will underread. */
static uint8_t battery_soc_pct(uint16_t mv) {
    if (mv >= 12700u) {
        return 100;
    }
    if (mv <= 11800u) {
        return 0;
    }
    /* (mv - 11800) * 100 / 900 simplifies to (mv - 11800) / 9. */
    return (uint8_t)((mv - 11800u) / 9u);
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
    static const char* const labels[3] = {"WATER", "FUEL", "BATTERY"};
    /* Per-row bar top y (each bar is BAR_HEIGHT=18 rows tall, padded by 1
     * row from the divider above and below). */
    static const uint8_t bar_top[3] = {2, 23, 44};
    /* Per-row text baseline. Battery row sits one pixel below the
     * geometrically-centred baseline to balance its taller descenders
     * against the divider. */
    static const uint8_t text_baseline[3] = {16, 36, 57};
    /* Side-tick offsets from BAR_LEFT_X (8 ticks, 4 on each side of the
     * 2-wide centre tick at CENTER_TICK_DX..+1). 13 px spacing matches the
     * mock-up exactly. */
    static const uint8_t side_tick_dx[8] = {7, 20, 33, 46, 73, 86, 99, 112};

    char vbuf[8];

    /* Outer frame: top / bottom / left / right of the panel + the two
     * horizontal dividers between rows + the vertical separator between
     * the text and bar columns. */
    u8g2_DrawHLine(g, 0, 0, DISPLAY_WIDTH);
    u8g2_DrawHLine(g, 0, 63, DISPLAY_WIDTH);
    u8g2_DrawVLine(g, 0, 0, 64);
    u8g2_DrawVLine(g, DISPLAY_WIDTH - 1, 0, 64);
    u8g2_DrawHLine(g, 0, 21, DISPLAY_WIDTH);
    u8g2_DrawHLine(g, 0, 42, DISPLAY_WIDTH);
    u8g2_DrawVLine(g, VSEP_X, 0, 64);

    for (uint8_t i = 0; i < 3; i++) {
        /* Bar value is always a percentage (0..100). For water/fuel it is
         * the level meter reading; for the battery row it is the derived
         * SoC. Values >100 from the switching board are the over-range
         * sentinel — render "--- ERROR ---" in the bar column instead. */
        uint8_t bar_value;
        if (i == 0) {
            bar_value = controller_level(0);
        } else if (i == 1) {
            bar_value = controller_level(1);
        } else {
            bar_value = battery_soc_pct(controller_battery_mv());
        }

        /* Label always at the left edge of the text column. */
        u8g2_DrawStr(g, TEXT_LEFT_X, text_baseline[i], labels[i]);

        /* Right-aligned value (skipped in the error case so the bar-side
         * "--- ERROR ---" is unambiguous). For battery, voltage is more
         * actionable than SoC — the bar carries the SoC visually. */
        if (bar_value <= 100u) {
            if (i == 2) {
                (void)format_voltage(vbuf, controller_battery_mv());
            } else {
                (void)format_percent(vbuf, bar_value);
            }
            uint8_t vw = (uint8_t)u8g2_GetStrWidth(g, vbuf);
            u8g2_DrawStr(g, (uint8_t)(TEXT_RIGHT_X - vw), text_baseline[i], vbuf);
        }

        if (bar_value > 100u) {
            /* "--- ERROR ---" centred in the bar column. The dashes pad
             * the word to occupy more of the bar so the error reads from
             * a distance. */
            const char* err = "--- ERROR ---";
            uint8_t err_w = (uint8_t)u8g2_GetStrWidth(g, err);
            uint8_t err_x = (uint8_t)(BAR_LEFT_X + (BAR_WIDTH - err_w) / 2u);
            u8g2_DrawStr(g, err_x, text_baseline[i], err);
            continue;
        }

        /* Left-to-right fill at the row's bar y. Computed in 16-bit math so
         * a 100 % bar lands exactly on BAR_WIDTH and a 1 % bar gives a
         * single visible pixel column. */
        uint8_t fill_w = (uint8_t)(((uint16_t)BAR_WIDTH * bar_value) / 100u);
        if (fill_w > 0) {
            u8g2_DrawBox(g, BAR_LEFT_X, bar_top[i], fill_w, BAR_HEIGHT);
        }

        /* Tick notches at the bottom of the bar — XOR-drawn so a single
         * loop renders them correctly in both the filled and empty
         * regions. Centre tick anchored at the same bottom edge as the
         * side ticks but extends 2 rows higher. */
        uint8_t side_y = (uint8_t)(bar_top[i] + BAR_HEIGHT - SIDE_TICK_HEIGHT);
        uint8_t center_y = (uint8_t)(bar_top[i] + BAR_HEIGHT - CENTER_TICK_HEIGHT);
        u8g2_SetDrawColor(g, 2);
        for (uint8_t t = 0; t < 8; t++) {
            u8g2_DrawVLine(g, (uint8_t)(BAR_LEFT_X + side_tick_dx[t]), side_y, SIDE_TICK_HEIGHT);
        }
        u8g2_DrawVLine(g, (uint8_t)(BAR_LEFT_X + CENTER_TICK_DX), center_y, CENTER_TICK_HEIGHT);
        u8g2_DrawVLine(g, (uint8_t)(BAR_LEFT_X + CENTER_TICK_DX + 1), center_y, CENTER_TICK_HEIGHT);
        u8g2_SetDrawColor(g, 1);
    }
}

static void render_config_menu(u8g2_t* g) {
    static const char* items[CONFIG_MENU_COUNT] = {
        "NAV LIGHTS",
        "SET TIME",
        "WATER OFS",
        "FUEL OFS",
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
            case CONFIG_SCREEN_MENU:
                render_config_menu(g);
                break;
            case CONFIG_SCREEN_NAV:
                render_config_nav(g);
                break;
            case CONFIG_SCREEN_TIME:
                render_config_time(g);
                break;
            case CONFIG_SCREEN_OFFSET:
                render_config_offset(g);
                break;
            default:
                break;
        }
        /* Diagnostic I2C event log on the right half. */
        i2c_log_render(g);
    } else if (controller_power_on()) {
        render_normal(g);
    }
    /* Disabled normal mode: buffer already cleared, ship it blank. */

    u8g2_SendBuffer(g);

    /* Take the panel out of power-save *after* the first frame has been
     * shipped — the SSD1322 powers up with whatever was last in its DDRAM,
     * and enabling it before SendBuffer lets the user see that noise for
     * one tick. One-shot flag avoids the redundant command on every
     * refresh. */
    static uint8_t panel_enabled = 0;
    if (!panel_enabled) {
        display_enable();
        panel_enabled = 1;
    }
}
