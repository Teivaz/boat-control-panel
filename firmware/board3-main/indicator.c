#include "indicator.h"

#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "rgbled.h"
#include "task_ids.h"

/* LED layout (index -> physical meaning) mirrors the NAV_LIGHT_* bit order
 * and the RELAY_NAV_* relay indices, so a single bit position selects the
 * relay, the nav-mask entry, and the corresponding pixel simultaneously. */
#define LED_COUNT 5
#define REFRESH_MS 50u
#define ERROR_HALF_PERIOD_MS 250u /* -> ~2 Hz flash */

#define ON_BRIGHTNESS 0x20
#define ERROR_BRIGHTNESS 0x40
#define DIM_BRIGHTNESS 0x04

static RGBLedData leds[LED_COUNT];
static uint16_t phase_ms;

static void render_error(void);
static void render_normal(void);
static void render_config(void);
static void refresh_task(TaskId id, void* ctx);

void indicator_init(TaskController* ctrl) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        leds[i].red = 0;
        leds[i].green = 0;
        leds[i].blue = 0;
    }
    phase_ms = 0;
    task_controller_add(ctrl, TASK_NAV_LIGHTS, REFRESH_MS, refresh_task, 0);
}

static void render_error(void) {
    uint8_t on = (phase_ms < ERROR_HALF_PERIOD_MS);
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        leds[i].red = on ? ERROR_BRIGHTNESS : 0;
        leds[i].green = 0;
        leds[i].blue = 0;
    }
}

static void render_normal(void) {
    uint16_t phys = controller_relay_physical();
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t active = (phys & (uint16_t)(1u << i)) != 0;
        leds[i].red = active ? ON_BRIGHTNESS : 0;
        leds[i].green = active ? ON_BRIGHTNESS : 0;
        leds[i].blue = active ? ON_BRIGHTNESS : 0;
    }
}

/* In the nav-edit screen the pixels reflect the working enabled-mask and
 * highlight the selected slot. On other config screens (menu / time) the
 * indicators dim out so the user's attention stays on the OLED. */
static void render_config(void) {
    if (config_mode_screen() != CONFIG_SCREEN_NAV) {
        for (uint8_t i = 0; i < LED_COUNT; i++) {
            leds[i].red = 0;
            leds[i].green = 0;
            leds[i].blue = DIM_BRIGHTNESS;
        }
        return;
    }
    uint8_t enabled = config_mode_nav_working_mask();
    uint8_t cursor = config_mode_nav_cursor();
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t on = (enabled & (uint8_t)(1u << i)) != 0;
        if (i == cursor) {
            leds[i].red = on ? 0 : DIM_BRIGHTNESS*2;
            leds[i].green = on ? ON_BRIGHTNESS : DIM_BRIGHTNESS;
            leds[i].blue = 0;
        } else {
            leds[i].red = 0;
            leds[i].green = 0;
            leds[i].blue = on ? ON_BRIGHTNESS : DIM_BRIGHTNESS;
        }
    }
}

static void refresh_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;

    phase_ms += REFRESH_MS;
    if (phase_ms >= 2u * ERROR_HALF_PERIOD_MS) {
        phase_ms = 0;
    }

    if (config_mode_active()) {
        render_config();
    } else if (controller_nav_error()) {
        render_error();
    } else {
        render_normal();
    }
    rgbled_set(leds, LED_COUNT);
}
