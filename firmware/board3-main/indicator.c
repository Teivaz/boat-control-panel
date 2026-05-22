#include "indicator.h"

#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "rgbled.h"
#include "task_ids.h"

/* Five RGB LEDs on RB5 mirror the five nav-light slots in NAV_LIGHT_*
 * bit order:
 *
 *   0 anchoring, 1 tricolor, 2 steaming, 3 bow, 4 stern
 *
 * The controller owns visibility (indicator_set_active(on)); while
 * active, the periodic refresh task pulls the current state from
 * config_mode + controller_nav_* and ships a per-LED RGB triple via
 * rgbled_set. Phase advances even across stalls so the red-flash and
 * white-pulsate animations stay smooth.
 *
 * All public entry points run in main-loop context, and refresh_task is
 * a normal cooperative-scheduler task — no INTERRUPT guards are required
 * inside this file. The peak rendering intensity comes from the
 * EEPROM-backed config (CONFIG_ADDR_INDICATOR_BRIGHTNESS) and is re-read
 * every refresh so brightness changes take effect within one frame. */

#define LED_COUNT 5
#define REFRESH_MS 50u

/* Red flash square-wave: ~2 Hz, 50% duty. */
#define FLASH_HALF_PERIOD_MS 250u
/* White pulsate triangle wave: ~1 s end-to-end, 50% rise + 50% fall. */
#define PULSATE_PERIOD_MS 1000u
/* Phase counter wraps at the longest animation period — LCM(500, 1000)
 * is just the pulsate period since 2 * FLASH_HALF_PERIOD_MS divides it. */
#define PHASE_WRAP_MS PULSATE_PERIOD_MS

/* Palette at unit (peak) intensity, named per application context so
 * each rendering decision points back to a single audit-able constant
 * — change the colour for "config cursor on" by editing one line and
 * every render path picks it up. Multiple constants may resolve to the
 * same RGB triple (e.g. blue for both halves of the config slot); the
 * separate names document intent and let the colours diverge later
 * without a code search. attenuate() scales each value to runtime
 * brightness before writing to the LEDs. */

/* Universal "no light" — shared across every mode that needs an off pixel. */
static const RGBLedData k_off                  = {.red = 0x00, .green = 0x00, .blue = 0x00};

/* ERROR mode — all five LEDs flash this colour at ~2 Hz. */
static const RGBLedData k_error                = {.red = 0xFF, .green = 0x00, .blue = 0x00};

/* CONFIG mode. "Active" = the cursor slot; "inactive" = the other four.
 * "on" / "off" = the nav-enable bit for that slot. */
static const RGBLedData k_config_active_on     = {.red = 0x00, .green = 0xFF, .blue = 0x00};
static const RGBLedData k_config_active_off    = {.red = 0xFF, .green = 0xFF, .blue = 0x00};
static const RGBLedData k_config_inactive_on   = {.red = 0x00, .green = 0x00, .blue = 0xFF};
static const RGBLedData k_config_inactive_off  = {.red = 0xFF, .green = 0x00, .blue = 0x00};

/* ON mode — per-LED state, picked in priority order errored > pending
 * > enabled by render_on. */
static const RGBLedData k_on_enabled           = {.red = 0xFF, .green = 0xFF, .blue = 0xFF};
static const RGBLedData k_on_pending           = {.red = 0xFF, .green = 0xFF, .blue = 0xFF};
static const RGBLedData k_on_errored           = {.red = 0xFF, .green = 0x00, .blue = 0x00};

static uint8_t g_active;
static RGBLedData g_leds[LED_COUNT];
static uint16_t g_phase_ms;

static uint8_t max_brightness(void);
static uint8_t dim_brightness(void);
static uint8_t pulsate_level(void);
static uint8_t flash_high(void);
static RGBLedData attenuate(RGBLedData color, uint8_t brightness);
static void render_dark(void);
static void render_error(void);
static void render_config(uint8_t cursor);
static void render_on(NavLights enabled, NavLights pending, NavLights errored);
static void refresh_task(TaskId id, void* ctx);

void indicator_init(TaskController* ctrl) {
    g_active = 0;
    g_phase_ms = 0;
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        g_leds[i] = k_off;
    }
    /* Ship one dark frame so the WS2812 chain is in a known state before
     * the first activation. */
    rgbled_set(g_leds, LED_COUNT);
    task_controller_add(ctrl, TASK_NAV_LIGHTS, REFRESH_MS, refresh_task, 0);
}

void indicator_set_active(uint8_t on) {
    on = on ? 1u : 0u;
    if (on == g_active) {
        return;
    }
    g_active = on;
    if (!on) {
        /* WS2812 has no power-save: latch a dark frame immediately so the
         * ring goes visibly dark instead of holding the last animation
         * frame indefinitely. */
        render_dark();
        rgbled_set(g_leds, LED_COUNT);
    }
}

static uint8_t max_brightness(void) {
    /* Read once per refresh — config_read_byte serves from an in-RAM
     * shadow so this is just a couple of branches plus a byte fetch.
     * Re-reading per frame lets a runtime change (config_write_byte at
     * CONFIG_ADDR_INDICATOR_BRIGHTNESS) take effect immediately without
     * needing the indicator to subscribe to a change callback. */
    return config_get_indicator_brightness();
}

/* Secondary intensity used for "disabled / inactive" hints in the
 * config editor. Derived from max_brightness so changing the peak keeps
 * the contrast ratio consistent. */
static uint8_t dim_brightness(void) {
    return (uint8_t)(max_brightness() / 8u);
}

/* Triangle-wave brightness over PULSATE_PERIOD_MS: 0 → peak across the
 * first half, peak → 0 across the second. Cheaper than a sine table
 * and reads as a smooth "breathing" effect against the panel. */
static uint8_t pulsate_level(void) {
    uint8_t peak = max_brightness();
    uint16_t half = PULSATE_PERIOD_MS / 2u;
    uint16_t p = g_phase_ms;
    if (p < half) {
        return (uint8_t)(((uint16_t)peak * p) / half);
    }
    return (uint8_t)(((uint16_t)peak * (PULSATE_PERIOD_MS - p)) / half);
}

/* Square wave: 1 while (phase / half_period) is even, 0 while odd. */
static uint8_t flash_high(void) {
    return (uint8_t)((((g_phase_ms / FLASH_HALF_PERIOD_MS) & 1u) == 0u) ? 1u : 0u);
}

/* Scale every channel of `color` by `brightness / 255`. The `+ 0x80`
 * before the right-shift implements round-to-nearest, so a pure primary
 * (e.g. k_red with red = 0xFF) attenuated to peak P returns exactly P
 * in the active channel — no off-by-one drift across the palette. */
static RGBLedData attenuate(RGBLedData color, uint8_t brightness) {
    RGBLedData out;
    out.red   = (uint8_t)((((uint16_t)color.red   * brightness) + 0x80u) >> 8);
    out.green = (uint8_t)((((uint16_t)color.green * brightness) + 0x80u) >> 8);
    out.blue  = (uint8_t)((((uint16_t)color.blue  * brightness) + 0x80u) >> 8);
    return out;
}

static void render_dark(void) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        g_leds[i] = k_off;
    }
}

static void render_error(void) {
    RGBLedData c = attenuate(k_error, flash_high() ? max_brightness() : 0u);
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        g_leds[i] = c;
    }
}

/* Config editor: cursor LED renders green (slot enabled) or dim red
 * (slot disabled); every other LED renders blue with bright/dim
 * indicating enabled/disabled. Reads the working mask (live in-RAM
 * edit buffer in config_mode, not the EEPROM-backed value) so each
 * toggle is reflected immediately — the working mask only gets
 * persisted on config-mode exit. cursor=0xFF (no nav-screen cursor)
 * renders all five LEDs as non-cursor — useful when the operator is in
 * config mode but on a different sub-screen. */
static void render_config(uint8_t cursor) {
    uint8_t enabled = config_mode_nav_working_mask();
    uint8_t bright = max_brightness();
    uint8_t dim = dim_brightness();
    /* Pre-attenuate once per frame — these four colours apply to every
     * non-cursor / cursor combination, so computing them up front keeps
     * the per-LED loop a single struct assignment. */
    RGBLedData active_on    = attenuate(k_config_active_on,    bright);
    RGBLedData active_off   = attenuate(k_config_active_off,   dim);
    RGBLedData inactive_on  = attenuate(k_config_inactive_on,  bright);
    RGBLedData inactive_off = attenuate(k_config_inactive_off, dim);
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t on = (enabled & (uint8_t)(1u << i)) != 0u;
        if (i == cursor) {
            g_leds[i] = on ? active_on : active_off;
        } else {
            g_leds[i] = on ? inactive_on : inactive_off;
        }
    }
}

static void render_on(NavLights enabled, NavLights pending, NavLights errored) {
    uint8_t bright = max_brightness();
    /* Pre-attenuate the three per-state colours once per frame so the
     * per-LED loop is just a priority cascade of struct assignments.
     * `flash_high() ? bright : 0` does the on/off square-wave directly
     * via attenuate — when "off", the result is k_off. */
    RGBLedData enabled_c = attenuate(k_on_enabled, bright);
    RGBLedData pending_c = attenuate(k_on_pending, pulsate_level());
    RGBLedData errored_c = attenuate(k_on_errored, flash_high() ? bright : 0u);
    uint8_t e = enabled.raw;
    uint8_t p = pending.raw;
    uint8_t r = errored.raw;
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        if (r & bit) {
            g_leds[i] = errored_c;
        } else if (p & bit) {
            g_leds[i] = pending_c;
        } else if (e & bit) {
            g_leds[i] = enabled_c;
        } else {
            g_leds[i] = k_off;
        }
    }
}

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    if (!g_active) {
        return;
    }

    g_phase_ms = (uint16_t)(g_phase_ms + REFRESH_MS);
    if (g_phase_ms >= PHASE_WRAP_MS) {
        g_phase_ms = 0;
    }

    /* Priority: config-mode editing wins over normal rendering, and
     * within normal rendering a config-error (mode unrealisable) flashes
     * the whole ring red. Otherwise ship per-LED enabled / pending /
     * errored. */
    if (config_mode_active()) {
        render_config(config_mode_nav_cursor());
    } else if (controller_nav_config_error()) {
        render_error();
    } else {
        render_on(controller_nav_enabled(), controller_nav_pending(), controller_nav_errored());
    }
    rgbled_set(g_leds, LED_COUNT);
}
