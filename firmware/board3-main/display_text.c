#include "display_text.h"

#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
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

static const uint8_t baselines[3] = {LINE_BASELINE_0, LINE_BASELINE_1, LINE_BASELINE_2};

static void refresh_task(TaskId id, void* ctx);
static void format_levels(char* out, const char* label, uint8_t level_raw);
static void format_battery(char* out, uint16_t mv);
static void format_nav_slot(char* out, uint8_t slot, uint8_t enabled, uint8_t cursor);
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

/* ---------------------------------------------------------------------------
 * Periodic refresh — assemble the three lines and ship via the u8g2 full
 * buffer in one transaction.
 * ---------------------------------------------------------------------------
 */

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    char buf[3][MAX_LINE_CHARS + 1];

    if (config_mode_active()) {
        uint8_t enabled = config_get_nav_enabled_mask();
        uint8_t cursor = config_mode_cursor();
        uint8_t top = (cursor == 0) ? 0 : (uint8_t)(cursor - 1u);
        if (top > 2u) {
            top = 2u;
        }
        format_nav_slot(buf[0], top, enabled, cursor);
        format_nav_slot(buf[1], (uint8_t)(top + 1u), enabled, cursor);
        format_nav_slot(buf[2], (uint8_t)(top + 2u), enabled, cursor);
    } else {
        format_levels(buf[0], "WATER", controller_level(0));
        format_levels(buf[1], "FUEL", controller_level(1));
        format_battery(buf[2], controller_battery_mv());
    }

    u8g2_t* g = display_u8g2();
    u8g2_ClearBuffer(g);
    for (uint8_t i = 0; i < 3; i++) {
        u8g2_DrawStr(g, 0, baselines[i], buf[i]);
    }
    u8g2_SendBuffer(g);
}
