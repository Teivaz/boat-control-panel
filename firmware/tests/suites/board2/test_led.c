#include "config.h"
#include "led_effect.h"
#include "libcomm.h"
#include "rgbled.h"
#include "task_ids.h"
#include "test_support.h"

#include <string.h>
#include <xc.h>

/*
 * The per-button RGB chain: the WS2812 symbol encoder and the animation that
 * feeds it.
 *
 * The encoder turns each intensity bit into a 4-bit SPI symbol — 0b0001 for a
 * zero, 0b0111 for a one — so the 3.2 MHz shift register produces WS2812
 * timing with no dedicated peripheral.  Getting a symbol wrong does not fail
 * loudly; it produces LEDs that latch the wrong colour or flicker, which is
 * only visible on an assembled panel.
 *
 * rgbled.c keeps its frame buffer file-static with no accessor, so the test
 * recovers it the way the DMA does: through the source address the driver
 * programmed into the channel.  On the device that register is 24 bits; here
 * it is pointer-width (see uint24_t in pic_mock.h), so what the driver wrote
 * is what comes back.
 */

#define LED_COUNT 7
#define BYTES_PER_LED (3u * 4u) /* 3 colour bytes, 4 SPI bytes each */

static TaskController ctrl;

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    task_controller_init(&ctrl);
    config_init(&ctrl);
    rgbled_init();
    return NULL;
}

static const uint8_t* frame_buffer(void) {
    DMASELECT = 0; /* the LED chain uses DMA1 */
    return (const uint8_t*)(uintptr_t)DMAnSSA;
}

static uint16_t frame_len(void) {
    DMASELECT = 0;
    return (uint16_t)DMAnSSZ;
}

/*
 * Peak brightness is 0xFE, not 0xFF: config_get_led_brightness treats 0xFF as
 * the EEPROM erase pattern and substitutes the default, so the top settable
 * value is one below.  Worth stating explicitly — a "set it to maximum" write
 * of 0xFF quietly turns the panel *down*.
 */
#define BRIGHT_MAX 0xFEu

/* What the animation writes for a channel at `level` scaled by `bright`,
 * matching the rounded scaling in led_effect.c. */
static uint8_t scaled(uint8_t level, uint8_t bright) {
    return (uint8_t)((((uint16_t)level * bright) + 0x80u) >> 8);
}

/* The four SPI bytes rgbled.c should produce for one intensity byte. */
static void expected_symbols(uint8_t value, uint8_t out[4]) {
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t byte = 0;
        for (uint8_t n = 0; n < 2; n++) {
            const uint8_t bit = (value & 0x80u) ? 1u : 0u;
            value = (uint8_t)(value << 1);
            const uint8_t symbol = bit ? 0b0111u : 0b0001u;
            byte |= (uint8_t)(symbol << (n * 4u));
        }
        out[i] = byte;
    }
}

/* ── Encoder ──────────────────────────────────────────────────────────── */

static MunitResult test_spi_is_configured_for_ws2812_timing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* 3.2 Mbps: 64 MHz / (2 * (9 + 1)).  Each 4-bit symbol is then 1.25 us,
     * the WS2812 bit period. */
    assert_uint8(SPI1BAUD, ==, 9);
    assert_uint8(SPI1CLK, ==, 0x00);      /* Fosc */
    assert_uint8(SPI1CON0bits.MST, ==, 1);
    assert_uint8(SPI1CON0bits.LSBF, ==, 1); /* symbols are packed low-nibble first */
    assert_uint8(SPI1CON2bits.TXR, ==, 1);  /* transmit only */
    assert_uint8(SPI1CON2bits.RXR, ==, 0);
    assert_uint8(SPI1CON0bits.EN, ==, 1);
    assert_uint8(RC2PPS, ==, 0x32);         /* RC2 -> SPI1 SDO */
    return MUNIT_OK;
}

static MunitResult test_dma_is_wired_to_the_spi_transmitter(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    DMASELECT = 0;
    assert_uint8(DMAnSIRQ, ==, 0x19);       /* SPI1TX */
    assert_uint8(DMAnCON1bits.SMODE, ==, 0b01); /* source walks the buffer */
    assert_uint8(DMAnCON1bits.DMODE, ==, 0b00); /* destination is one register */
    assert_uint8(DMAnCON1bits.SSTP, ==, 1);     /* stop when the frame is out */
    assert_uint8(DMAnCON0bits.EN, ==, 1);
    assert_uint8(PRLOCKbits.PRLOCKED, ==, 1);   /* mandatory for DMA operation */
    return MUNIT_OK;
}

static MunitResult test_one_led_encodes_to_twelve_bytes(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    RGBLedData led = {.red = 0xFF, .green = 0x00, .blue = 0xAA};
    rgbled_set(&led, 1);

    assert_uint16(frame_len(), ==, BYTES_PER_LED);
    assert_uint16(SPI1TCNT, ==, BYTES_PER_LED);

    const uint8_t* buf = frame_buffer();
    uint8_t want[4];

    /* Byte order on the wire is whatever the struct declares — red, green,
     * blue — and each byte becomes four symbols. */
    expected_symbols(0xFF, want);
    assert_memory_equal(4, buf + 0, want);
    expected_symbols(0x00, want);
    assert_memory_equal(4, buf + 4, want);
    expected_symbols(0xAA, want);
    assert_memory_equal(4, buf + 8, want);
    return MUNIT_OK;
}

/* Every bit pattern must round-trip through the encoder — a symbol table with
 * one wrong entry only shows up on particular intensities. */
static MunitResult test_every_intensity_encodes_correctly(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (unsigned v = 0; v < 256; v++) {
        RGBLedData led = {.red = (uint8_t)v, .green = (uint8_t)v, .blue = (uint8_t)v};
        rgbled_set(&led, 1);

        uint8_t want[4];
        expected_symbols((uint8_t)v, want);
        const uint8_t* buf = frame_buffer();
        for (uint8_t byte = 0; byte < 3; byte++) {
            if (memcmp(buf + byte * 4u, want, 4) != 0) {
                munit_errorf("intensity 0x%02X encoded wrongly in colour byte %u", v, byte);
            }
        }
    }
    return MUNIT_OK;
}

/* Only zero and one symbols may appear — anything else is a timing violation
 * the LED would latch as noise. */
static MunitResult test_only_legal_symbols_are_emitted(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    RGBLedData leds[LED_COUNT];
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        leds[i].red = (uint8_t)(i * 37u);
        leds[i].green = (uint8_t)(255u - i * 11u);
        leds[i].blue = (uint8_t)(i * 91u);
    }
    rgbled_set(leds, LED_COUNT);

    const uint8_t* buf = frame_buffer();
    for (uint16_t i = 0; i < frame_len(); i++) {
        for (uint8_t nib = 0; nib < 2; nib++) {
            const uint8_t symbol = (uint8_t)((buf[i] >> (nib * 4u)) & 0x0Fu);
            if (symbol != 0b0001u && symbol != 0b0111u) {
                munit_errorf("byte %u nibble %u is 0b%04u, not a WS2812 symbol", i, nib,
                             (unsigned)(((symbol >> 3) & 1) * 1000 + ((symbol >> 2) & 1) * 100 +
                                        ((symbol >> 1) & 1) * 10 + (symbol & 1)));
            }
        }
    }
    return MUNIT_OK;
}

static MunitResult test_full_chain_length(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    RGBLedData leds[LED_COUNT] = {{{0}}};
    rgbled_set(leds, LED_COUNT);
    assert_uint16(frame_len(), ==, LED_COUNT * BYTES_PER_LED);
    return MUNIT_OK;
}

/*
 * A frame submitted while the previous one is still going out would corrupt
 * the one on the wire, and a WS2812 chain has no way to recover a torn frame
 * short of the next full refresh.  The driver checks XIP and drops the update.
 */
static MunitResult test_update_while_busy_is_dropped(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    RGBLedData one = {.red = 1, .green = 2, .blue = 3};
    rgbled_set(&one, 1);
    const uint16_t before = frame_len();

    DMASELECT = 0;
    DMAnCON0bits.XIP = 1; /* a transfer is in flight */
    RGBLedData many[LED_COUNT] = {{{0}}};
    rgbled_set(many, LED_COUNT);
    assert_uint16(frame_len(), ==, before); /* untouched */

    DMASELECT = 0;
    DMAnCON0bits.XIP = 0;
    rgbled_set(many, LED_COUNT);
    assert_uint16(frame_len(), ==, LED_COUNT * BYTES_PER_LED);
    return MUNIT_OK;
}

/* More LEDs than the buffer holds must be truncated, not overrun. */
static MunitResult test_oversized_chain_is_clamped(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    RGBLedData leds[LED_COUNT + 5];
    memset(leds, 0x5A, sizeof(leds));
    rgbled_set(leds, LED_COUNT + 5);
    assert_uint16(frame_len(), <=, LED_COUNT * BYTES_PER_LED);
    return MUNIT_OK;
}

/* ── Animation ────────────────────────────────────────────────────────── */

static MunitResult test_effects_default_to_off(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    led_effect_init(&ctrl);
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        assert_uint8(led_effect_get(i).raw, ==, 0);
    }
    return MUNIT_OK;
}

static MunitResult test_effect_round_trips(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    led_effect_init(&ctrl);
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        CommButtonOutputEffect e = {0};
        e.color = (uint8_t)(i & 3u);
        e.mode = COMM_EFFECT_MODE_FLASHING;
        led_effect_set(i, e);
    }
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        assert_uint8(led_effect_get(i).color, ==, (uint8_t)(i & 3u));
        assert_uint8(led_effect_get(i).mode, ==, COMM_EFFECT_MODE_FLASHING);
    }
    /* Out-of-range ids are ignored both ways. */
    CommButtonOutputEffect e = {0};
    e.color = COMM_EFFECT_COLOR_RED;
    led_effect_set(LED_EFFECT_COUNT, e);
    assert_uint8(led_effect_get(LED_EFFECT_COUNT).raw, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_animation_ships_a_full_frame(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    led_effect_init(&ctrl);
    assert_uint8(test_task_active(&ctrl, TASK_LED_EFFECT), ==, 1);

    test_advance_ms(&ctrl, 20); /* one animation tick */
    assert_uint16(frame_len(), ==, LED_EFFECT_COUNT * BYTES_PER_LED);
    return MUNIT_OK;
}

/* A solid colour must be the same on every frame, and it must be the colour
 * that was asked for. */
static MunitResult test_enabled_effect_is_steady(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    led_effect_init(&ctrl);
    config_write_byte(CONFIG_ADDR_LED_BRIGHTNESS, BRIGHT_MAX);
    test_advance_ms(&ctrl, TASK_MIN_MS);

    CommButtonOutputEffect e = {0};
    e.color = COMM_EFFECT_COLOR_GREEN;
    e.mode = COMM_EFFECT_MODE_ENABLED;
    led_effect_set(0, e);

    test_advance_ms(&ctrl, 20);
    uint8_t first[BYTES_PER_LED];
    memcpy(first, frame_buffer(), sizeof(first));

    test_advance_ms(&ctrl, 200); /* ten more frames */
    assert_memory_equal(BYTES_PER_LED, frame_buffer(), first);

    /* Green at full brightness: only the middle colour byte is lit. */
    uint8_t want_off[4], want_on[4];
    expected_symbols(0x00, want_off);
    expected_symbols(scaled(0xFF, BRIGHT_MAX), want_on);
    assert_memory_equal(4, first + 0, want_off); /* red   */
    assert_memory_equal(4, first + 4, want_on);  /* green */
    assert_memory_equal(4, first + 8, want_off); /* blue  */
    return MUNIT_OK;
}

/* Flashing has to actually change between frames, and come back — a "flashing"
 * LED that never changes reads as solid on, which on this panel means an
 * acknowledged command rather than a pending one. */
static MunitResult test_flashing_effect_alternates(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    led_effect_init(&ctrl);
    config_write_byte(CONFIG_ADDR_LED_BRIGHTNESS, BRIGHT_MAX);
    test_advance_ms(&ctrl, TASK_MIN_MS);

    CommButtonOutputEffect e = {0};
    e.color = COMM_EFFECT_COLOR_RED;
    e.mode = COMM_EFFECT_MODE_FLASHING;
    led_effect_set(0, e);

    uint8_t lit = 0, dark = 0;
    uint8_t on[4], off[4];
    expected_symbols(scaled(0xFF, BRIGHT_MAX), on);
    expected_symbols(0x00, off);

    /* The phase bit toggles every 8 ticks, so 32 frames covers two cycles. */
    for (int frame = 0; frame < 32; frame++) {
        test_advance_ms(&ctrl, 20);
        if (memcmp(frame_buffer(), on, 4) == 0) {
            lit = 1;
        } else if (memcmp(frame_buffer(), off, 4) == 0) {
            dark = 1;
        }
    }
    assert_uint8(lit, ==, 1);
    assert_uint8(dark, ==, 1);
    return MUNIT_OK;
}

static MunitResult test_disabled_effect_stays_dark(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    led_effect_init(&ctrl);
    CommButtonOutputEffect e = {0};
    e.color = COMM_EFFECT_COLOR_WHITE;
    e.mode = COMM_EFFECT_MODE_DISABLED;
    led_effect_set(3, e);

    uint8_t off[4];
    expected_symbols(0x00, off);
    for (int frame = 0; frame < 16; frame++) {
        test_advance_ms(&ctrl, 20);
        assert_memory_equal(4, frame_buffer() + 3u * BYTES_PER_LED, off);
    }
    return MUNIT_OK;
}

/*
 * Brightness scales every channel by the same factor, so the hue is unchanged
 * and only the intensity moves.  It is re-read on every frame, which is what
 * lets a config write take effect without restarting the animation.
 */
static MunitResult test_brightness_scales_without_shifting_hue(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    led_effect_init(&ctrl);
    CommButtonOutputEffect e = {0};
    e.color = COMM_EFFECT_COLOR_WHITE;
    e.mode = COMM_EFFECT_MODE_ENABLED;
    led_effect_set(0, e);

    config_write_byte(CONFIG_ADDR_LED_BRIGHTNESS, 0x80);
    test_advance_ms(&ctrl, TASK_MIN_MS);
    test_advance_ms(&ctrl, 20);

    uint8_t half[4];
    expected_symbols(0x80, half); /* (0xFF * 0x80 + 0x80) >> 8 == 0x80 */
    for (uint8_t byte = 0; byte < 3; byte++) {
        assert_memory_equal(4, frame_buffer() + byte * 4u, half);
    }

    /* Turning it down again takes effect on the next frame. */
    config_write_byte(CONFIG_ADDR_LED_BRIGHTNESS, 0x00);
    test_advance_ms(&ctrl, TASK_MIN_MS);
    test_advance_ms(&ctrl, 20);
    uint8_t dark[4];
    expected_symbols(0x00, dark);
    assert_memory_equal(4, frame_buffer(), dark);
    return MUNIT_OK;
}

/* A virgin or corrupted brightness cell reads as 0xFF, which taken literally
 * would drive the whole panel at full power.  The default is used instead. */
static MunitResult test_unset_brightness_uses_the_default(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* config_init seeds the cell; force it back to the erase pattern. */
    pic_eeprom_erase();
    assert_uint8(config_get_led_brightness(), !=, 0xFF);
    assert_uint8(config_get_led_brightness(), ==, 0x10);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/spi_is_configured_for_ws2812_timing", test_spi_is_configured_for_ws2812_timing),
    T("/dma_is_wired_to_the_spi_transmitter", test_dma_is_wired_to_the_spi_transmitter),
    T("/one_led_encodes_to_twelve_bytes", test_one_led_encodes_to_twelve_bytes),
    T("/every_intensity_encodes_correctly", test_every_intensity_encodes_correctly),
    T("/only_legal_symbols_are_emitted", test_only_legal_symbols_are_emitted),
    T("/full_chain_length", test_full_chain_length),
    T("/update_while_busy_is_dropped", test_update_while_busy_is_dropped),
    T("/oversized_chain_is_clamped", test_oversized_chain_is_clamped),
    T("/effects_default_to_off", test_effects_default_to_off),
    T("/effect_round_trips", test_effect_round_trips),
    T("/animation_ships_a_full_frame", test_animation_ships_a_full_frame),
    T("/enabled_effect_is_steady", test_enabled_effect_is_steady),
    T("/flashing_effect_alternates", test_flashing_effect_alternates),
    T("/disabled_effect_stays_dark", test_disabled_effect_stays_dark),
    T("/brightness_scales_without_shifting_hue", test_brightness_scales_without_shifting_hue),
    T("/unset_brightness_uses_the_default", test_unset_brightness_uses_the_default),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b2_led_suite(void) {
    MunitSuite s = {"/board2/led", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
