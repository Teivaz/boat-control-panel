#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
#include "display_text.h"
#include "i2c_fake.h"
#include "i2c_log.h"
#include "indicator.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"
#include "test_support.h"
#include "u8g2.h"

#include <string.h>
#include <xc.h>

/*
 * The text UI on the 256x64 OLED.
 *
 * u8g2 is vendored and portable, so it is linked and run for real here: the
 * tests render into the actual framebuffer and read the pixels back.  That
 * makes the assertions about *what appears on screen* rather than about which
 * drawing calls were made — a renderer that draws the right primitives in the
 * wrong place still fails.
 *
 * The panel is a full-buffer setup (u8g2_Setup_ssd1322_nhd_256x64_f), so the
 * buffer is the whole screen and pixels can be addressed directly.
 */

#define W 256
#define H 64
#define REFRESH_MS 50u

static TaskController ctrl;

static void boot(void) {
    test_reset_hardware();
    PORTAbits.RA7 = 1; /* config switch open */
    i2c_fake_reset();
    comm_interface_init();
    task_controller_init(&ctrl);
    config_init(&ctrl);
    display_init();
    display_text_init(&ctrl);
    indicator_init(&ctrl);
    config_mode_init(&ctrl);
    controller_init(&ctrl);
}

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    boot();
    return NULL;
}

/* One pixel out of the u8g2 page buffer.  Rows are packed eight to a byte,
 * so the tile row is y/8 and the bit is y%8. */
static uint8_t pixel(unsigned x, unsigned y) {
    const uint8_t* buf = u8g2_GetBufferPtr(display_u8g2());
    return (uint8_t)((buf[((y / 8u) * W) + x] >> (y % 8u)) & 1u);
}

/* Counts over an inclusive x range.  `unsigned` rather than uint8_t: the
 * right edge is x = 255, and a uint8_t loop counter can never pass it. */
static unsigned lit_pixels(unsigned x0, unsigned x1) {
    unsigned n = 0;
    for (unsigned y = 0; y < H; y++) {
        for (unsigned x = x0; x <= x1; x++) {
            n += pixel(x, y);
        }
    }
    return n;
}

static unsigned buffer_hash(void) {
    const uint8_t* buf = u8g2_GetBufferPtr(display_u8g2());
    unsigned h = 2166136261u;
    for (unsigned i = 0; i < (unsigned)(W * H / 8); i++) {
        h = (h ^ buf[i]) * 16777619u;
    }
    return h;
}

static void frame(void) {
    test_advance_ms(&ctrl, REFRESH_MS);
}

/*
 * Power the panel on the way the user does — the left-hand power button.
 *
 * The renderer paints the normal screen only when the controller reports
 * power on, so display_text_set_active(1) alone leaves a blank screen: the
 * panel is awake and showing nothing.  Going through the button keeps the two
 * pieces of state in the same relationship they have in the product.
 */
static void power_on(void) {
    uint8_t payload[2] = {COMM_ADDRESS_BUTTON_BOARD_L,
                          (uint8_t)(0u | (1u << 3) | (COMM_BUTTON_MODE_CHANGE << 4))};
    uint8_t f[8];
    uint8_t n = i2c_fake_frame(f, COMM_BUTTON_CHANGED, payload, 2);
    i2c_fake_deliver_write(f, n);
    i2c_poll();
}

/* ── Panel wiring ─────────────────────────────────────────────────────── */

static MunitResult test_init_configures_the_parallel_bus(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* PORTC is the 8-bit data bus, all digital outputs; RB0/RB3/RB4 are
     * D/!C, !RST and the write strobe. */
    assert_uint8(TRISC, ==, 0x00);
    assert_uint8(ANSELC, ==, 0x00);
    assert_uint8(TRISBbits.TRISB0, ==, 0);
    assert_uint8(TRISBbits.TRISB3, ==, 0);
    assert_uint8(TRISBbits.TRISB4, ==, 0);
    /* !WR idles high — the SSD1322 latches on its rising edge. */
    assert_uint8(LATBbits.LATB4, ==, 1);
    return MUNIT_OK;
}

/* ── Power-save gating ────────────────────────────────────────────────── */

/*
 * The panel comes up in power-save with a blank buffer already shipped, so
 * the first thing the user ever sees is the intended UI rather than the
 * SSD1322's power-on RAM contents.
 */
static MunitResult test_starts_blank_and_inactive(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint(lit_pixels(0, W - 1u), ==, 0);

    /* And stays blank: the refresh task early-returns while inactive, so
     * nothing is painted into a panel nobody can see. */
    for (int i = 0; i < 10; i++) {
        frame();
    }
    assert_uint(lit_pixels(0, W - 1u), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_activation_paints_a_frame(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    frame();
    assert_uint(lit_pixels(0, W - 1u), >, 0);
    return MUNIT_OK;
}

/* Deactivating stops painting but leaves the buffer alone — the next
 * activation shows the last frame until the following refresh. */
static MunitResult test_deactivation_freezes_the_buffer(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    frame();
    const unsigned painted = buffer_hash();
    assert_uint(lit_pixels(0, W - 1u), >, 0);

    display_text_set_active(0);
    CommBattery b = {.voltage = 9000};
    comm_on_battery_read_response(I2C_RESULT_OK, &b);
    for (int i = 0; i < 5; i++) {
        frame();
    }
    assert_uint(buffer_hash(), ==, painted);
    return MUNIT_OK;
}

/* ── Normal screen ────────────────────────────────────────────────────── */

/*
 * Three rows, each a label and a bar.  Both halves must carry something: a
 * layout regression that pushes the bars off the right edge, or the text off
 * the left, leaves a screen that still looks plausible in a screenshot.
 */
static MunitResult test_normal_screen_uses_both_halves(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommLevels lv = {.level_0 = 50, .level_1 = 50};
    comm_on_levels_read_response(I2C_RESULT_OK, &lv);
    CommBattery b = {.voltage = 12600};
    comm_on_battery_read_response(I2C_RESULT_OK, &b);

    power_on();
    frame();

    assert_uint(lit_pixels(0, 125), >, 0);      /* text column */
    assert_uint(lit_pixels(128, W - 1u), >, 0); /* bar column  */
    return MUNIT_OK;
}

/* A changed reading must change the picture — a display that renders once and
 * then never updates is the classic silent failure here. */
static MunitResult test_frame_follows_the_readings(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();

    CommLevels empty = {.level_0 = 0, .level_1 = 0};
    comm_on_levels_read_response(I2C_RESULT_OK, &empty);
    frame();
    const unsigned at_empty = buffer_hash();

    CommLevels full = {.level_0 = 100, .level_1 = 100};
    comm_on_levels_read_response(I2C_RESULT_OK, &full);
    frame();
    assert_uint(buffer_hash(), !=, at_empty);
    return MUNIT_OK;
}

/* A fuller tank must light more of its bar than an emptier one. */
static MunitResult test_bar_length_tracks_the_level(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();

    CommLevels low = {.level_0 = 10, .level_1 = 10};
    comm_on_levels_read_response(I2C_RESULT_OK, &low);
    frame();
    const unsigned at_low = lit_pixels(128, W - 1u);

    CommLevels high = {.level_0 = 90, .level_1 = 90};
    comm_on_levels_read_response(I2C_RESULT_OK, &high);
    frame();
    assert_uint(lit_pixels(128, W - 1u), >, at_low);
    return MUNIT_OK;
}

/* ── Config screen ────────────────────────────────────────────────────── */

static MunitResult test_config_mode_shows_a_different_screen(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    frame();
    const unsigned normal = buffer_hash();

    PORTAbits.RA7 = 0; /* config switch closed */
    test_advance_ms(&ctrl, 200);
    frame();
    assert_uint8(config_mode_active(), ==, 1);
    assert_uint(buffer_hash(), !=, normal);
    return MUNIT_OK;
}

/* Moving the cursor has to be visible, or the menu is unusable. */
static MunitResult test_menu_cursor_is_visible(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    PORTAbits.RA7 = 0;
    test_advance_ms(&ctrl, 200);
    frame();
    const unsigned first = buffer_hash();

    config_mode_on_action(MENU_CONTROL_NEXT);
    frame();
    assert_uint(buffer_hash(), !=, first);
    return MUNIT_OK;
}

/* ── I2C log overlay ──────────────────────────────────────────────────── */

/*
 * The bus monitor is a debug overlay on the right half of the config screen.
 * It must stay there: spilling into the left half would cover the menu it is
 * meant to sit beside.
 */
static MunitResult test_log_overlay_stays_on_the_right(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_log_init();

    /* Put some traffic in the driver's ring. */
    uint8_t reply[2] = {0x12, 0x34};
    i2c_set_client_tx(reply, 2);

    u8g2_t* g = display_u8g2();
    u8g2_ClearBuffer(g);
    i2c_log_render(g);

    assert_uint(lit_pixels(0, 129), ==, 0);      /* left half untouched */
    assert_uint(lit_pixels(130, W - 1u), >, 0);  /* something was drawn  */
    return MUNIT_OK;
}

static MunitResult test_log_overlay_is_empty_without_traffic(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_log_init();
    u8g2_t* g = display_u8g2();
    u8g2_ClearBuffer(g);
    i2c_log_render(g);
    assert_uint(lit_pixels(0, W - 1u), ==, 0);
    return MUNIT_OK;
}

/* Newest entry at the top: when the bus is misbehaving, the line that matters
 * is the last one. */
static MunitResult test_log_overlay_grows_with_traffic(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_log_init();
    u8g2_t* g = display_u8g2();

    uint8_t reply[2] = {0x12, 0x34};
    i2c_set_client_tx(reply, 2);
    u8g2_ClearBuffer(g);
    i2c_log_render(g);
    const unsigned one_line = lit_pixels(130, W - 1u);

    for (int i = 0; i < 4; i++) {
        i2c_set_client_tx(reply, 2);
    }
    u8g2_ClearBuffer(g);
    i2c_log_render(g);
    assert_uint(lit_pixels(130, W - 1u), >, one_line);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/init_configures_the_parallel_bus", test_init_configures_the_parallel_bus),
    T("/starts_blank_and_inactive", test_starts_blank_and_inactive),
    T("/activation_paints_a_frame", test_activation_paints_a_frame),
    T("/deactivation_freezes_the_buffer", test_deactivation_freezes_the_buffer),
    T("/normal_screen_uses_both_halves", test_normal_screen_uses_both_halves),
    T("/frame_follows_the_readings", test_frame_follows_the_readings),
    T("/bar_length_tracks_the_level", test_bar_length_tracks_the_level),
    T("/config_mode_shows_a_different_screen", test_config_mode_shows_a_different_screen),
    T("/menu_cursor_is_visible", test_menu_cursor_is_visible),
    T("/log_overlay_stays_on_the_right", test_log_overlay_stays_on_the_right),
    T("/log_overlay_is_empty_without_traffic", test_log_overlay_is_empty_without_traffic),
    T("/log_overlay_grows_with_traffic", test_log_overlay_grows_with_traffic),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b3_display_suite(void) {
    MunitSuite s = {"/board3/display", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
