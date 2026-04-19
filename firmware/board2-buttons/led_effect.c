#include "led_effect.h"

#include "rgbled.h"
#include "task_ids.h"

/* 50 Hz animation tick: flashing at ~3 Hz (phase bit 3 toggles every 160 ms),
 * pulsating at ~0.4 Hz (triangular ramp over 128 ticks = 2.56 s). */
#define LED_EFFECT_TICK_MS 20

static TaskController *ctrl;
static CommButtonOutputEffect effects[LED_EFFECT_COUNT];
static RGBLedData frame[LED_EFFECT_COUNT];
static uint8_t phase;

static void led_effect_task(TaskId id, void *ctx);
static uint8_t level_for_mode(uint8_t mode);
static RGBLedData color_for_value(uint8_t color, uint8_t level);

void led_effect_init(TaskController *c) {
    ctrl = c;
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        effects[i].raw = 0;
        frame[i].red = frame[i].green = frame[i].blue = 0;
    }
    phase = 0;
    task_controller_add(ctrl, TASK_LED_EFFECT, LED_EFFECT_TICK_MS,
                        led_effect_task, 0);
}

void led_effect_set(uint8_t led_id, CommButtonOutputEffect eff) {
    if (led_id < LED_EFFECT_COUNT)
        effects[led_id] = eff;
}

CommButtonOutputEffect led_effect_get(uint8_t led_id) {
    CommButtonOutputEffect e = {0};
    if (led_id < LED_EFFECT_COUNT)
        e = effects[led_id];
    return e;
}

static uint8_t level_for_mode(uint8_t mode) {
    switch (mode) {
        case COMM_EFFECT_MODE_DISABLED:
            return 0;
        case COMM_EFFECT_MODE_ENABLED:
            return 0xFF;
        case COMM_EFFECT_MODE_FLASHING:
            return (phase & 0x08) ? 0x00 : 0xFF;
        case COMM_EFFECT_MODE_PULSATING: {
            uint8_t p = phase & 0x3F; /* 0..63 */
            return (p < 32) ? (uint8_t) (p << 3) : (uint8_t) ((63 - p) << 3);
        }
    }
    return 0;
}

static RGBLedData color_for_value(uint8_t color, uint8_t level) {
    RGBLedData d = {0};
    switch (color) {
        case COMM_EFFECT_COLOR_WHITE:
            d.red = level;
            d.green = level;
            d.blue = level;
            break;
        case COMM_EFFECT_COLOR_RED:
            d.red = level;
            break;
        case COMM_EFFECT_COLOR_GREEN:
            d.green = level;
            break;
        case COMM_EFFECT_COLOR_BLUE:
            d.blue = level;
            break;
    }
    return d;
}

static void led_effect_task(TaskId id, void *ctx) {
    (void) id;
    (void) ctx;
    phase++;
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        uint8_t lvl = level_for_mode(effects[i].mode);
        frame[i] = color_for_value(effects[i].color, lvl);
    }
    rgbled_set(frame, LED_EFFECT_COUNT);
}
