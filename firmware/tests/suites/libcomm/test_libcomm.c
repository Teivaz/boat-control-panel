#include "test_support.h"

#include "libcomm.h"

#include <string.h>

/*
 * The transport-agnostic protocol library: builders, parsers and the two
 * packed-field codecs.
 *
 * This binary is compiled with DEVICE_TYPE_MAIN, so comm_address() is the
 * main board's and the builders that stamp their own address stamp that one.
 */

#define SELF COMM_ADDRESS_MAIN

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    return NULL;
}

/* ── Identity ─────────────────────────────────────────────────────────── */

static MunitResult test_address(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint8(comm_address(), ==, COMM_ADDRESS_MAIN);
    return MUNIT_OK;
}

/* ── Wire layout ──────────────────────────────────────────────────────── */

/*
 * The envelope is written straight onto the wire, so any padding the host
 * compiler inserts between `id` and a uint16_t payload member would shift
 * every subsequent byte.  The struct carries __attribute__((packed)) for
 * exactly this reason; assert it took effect rather than trusting it.
 */
static MunitResult test_envelope_is_packed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_size(sizeof(CommMessage), ==, 9); /* 1 id + 7 payload + 1 crc */
    assert_size(sizeof(CommTriggerConfig), ==, 1);
    assert_size(sizeof(CommButtonOutputEffect), ==, 1);
    assert_size(sizeof(CommButtonEffect), ==, 4);
    assert_size(sizeof(CommChannelChanged), ==, 7);
    assert_size(sizeof(CommLevelMode), ==, 1);

    CommMessage m;
    memset(&m, 0, sizeof(m));
    m.relay_state.relays = 0x1234;
    /* Little-endian, low byte immediately after the id. */
    assert_uint8(((uint8_t*)&m)[1], ==, 0x34);
    assert_uint8(((uint8_t*)&m)[2], ==, 0x12);
    return MUNIT_OK;
}

/* ── Builders ─────────────────────────────────────────────────────────── */

static MunitResult test_build_write_messages(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommMessage m;
    uint8_t n;

    n = comm_build_reset(&m);
    assert_frame((uint8_t*)&m, n, COMM_RESET);

    n = comm_build_config(&m, 0x11, 0x42);
    assert_frame((uint8_t*)&m, n, COMM_CONFIG, 0x11, 0x42);

    n = comm_build_relay_state(&m, 0xBEEF);
    assert_frame((uint8_t*)&m, n, COMM_RELAY_STATE, 0xEF, 0xBE);

    n = comm_build_level_mode(&m, COMM_METER_MODE_240_33, COMM_METER_MODE_0_190);
    /* mode_0 in bits [1:0], mode_1 in bits [3:2].  Only the low nibble is
     * defined: CommLevelMode is four bits wide and the builder writes the
     * bitfields without clearing the byte, so bits [7:4] carry whatever the
     * caller's CommMessage happened to hold.  Harmless — the CRC covers it and
     * the parser masks it off — but it means this frame is not byte-stable, so
     * assert the defined bits rather than the whole byte. */
    assert_uint8(n, ==, 3);
    assert_uint8(((uint8_t*)&m)[0], ==, COMM_LEVEL_MODE);
    assert_uint8(((uint8_t*)&m)[1] & 0x0F, ==, (uint8_t)(2u | (1u << 2)));
    assert_uint8(((uint8_t*)&m)[2], ==, test_crc8((uint8_t*)&m, 2));

    n = comm_build_button_trigger(&m, 5, comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 500));
    assert_uint8(n, ==, 4);
    assert_uint8(((uint8_t*)&m)[1], ==, 5);

    n = comm_build_test_echo(&m, SELF, 0x7E);
    assert_frame((uint8_t*)&m, n, COMM_TEST_ECHO, SELF, 0x7E);

    n = comm_build_test_echo_response(&m, SELF, 0x7E);
    assert_frame((uint8_t*)&m, n, COMM_TEST_ECHO_RESPONSE, SELF, 0x7E);
    return MUNIT_OK;
}

/* Every read command must round-trip through comm_can_parse: the receiving
 * side validates the write phase with it before honouring the request. */
static MunitResult test_build_read_requests(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommMessage m;
    uint8_t n;

    struct {
        const char* name;
        uint8_t (*build)(CommMessage*);
        uint8_t id;
    } id_only[] = {
        {"button_state_read", comm_build_button_state_read, COMM_BUTTON_STATE_READ},
        {"relay_state_read", comm_build_relay_state_read, COMM_RELAY_STATE_READ},
        {"channel_state_read", comm_build_channel_state_read, COMM_CHANNEL_STATE_READ},
        {"battery_read", comm_build_battery_read, COMM_BATTERY_READ},
        {"levels_read", comm_build_levels_read, COMM_LEVELS_READ},
        {"level_mode_read", comm_build_level_mode_read, COMM_LEVEL_MODE_READ},
        {"sensors_read", comm_build_sensors_read, COMM_SENSORS_READ},
    };

    for (size_t i = 0; i < sizeof(id_only) / sizeof(id_only[0]); i++) {
        memset(&m, 0xAA, sizeof(m));
        n = id_only[i].build(&m);
        if (n != 2) {
            munit_errorf("%s built %u bytes, expected 2", id_only[i].name, n);
        }
        assert_frame((uint8_t*)&m, n, id_only[i].id);
        if (!comm_can_parse((uint8_t*)&m, n)) {
            munit_errorf("%s does not survive comm_can_parse", id_only[i].name);
        }
    }

    n = comm_build_config_read(&m, 0x13);
    assert_frame((uint8_t*)&m, n, COMM_CONFIG_READ, 0x13);
    assert_uint8(comm_can_parse((uint8_t*)&m, n), ==, 1);

    n = comm_build_button_trigger_read(&m, 3);
    assert_frame((uint8_t*)&m, n, COMM_BUTTON_TRIGGER_READ, 3);
    assert_uint8(comm_can_parse((uint8_t*)&m, n), ==, 1);

    /* The value must land in the first payload slot and survive the CRC
     * write.  It used to be written to raw[1] — one byte past the payload —
     * and then clobbered by the CRC, so the frame carried stack garbage where
     * the value should be.  Reusing `m` here is deliberate: it leaves a
     * recognisable value behind for that bug to expose. */
    n = comm_build_test_read(&m, 0x5C);
    assert_frame((uint8_t*)&m, n, COMM_TEST_READ, 0x5C);
    assert_uint8(comm_can_parse((uint8_t*)&m, n), ==, 1);

    uint8_t echoed = 0;
    comm_parse_test_read_request((uint8_t*)&m + 1, &echoed);
    assert_uint8(echoed, ==, 0x5C);
    return MUNIT_OK;
}

/*
 * libcomm.c hard-codes the CRC of every id-only command as a #define, with a
 * comment explaining how to regenerate them by hand.  Hand-maintained
 * constants derived from another function are exactly the thing that rots
 * silently: change an id and the frame still builds, still has two bytes, and
 * is rejected by every peer on the bus.  Recompute them here.
 */
static MunitResult test_precomputed_crcs_are_current(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t ids[] = {
        COMM_RESET,              COMM_BUTTON_STATE_READ, COMM_RELAY_STATE_READ,
        COMM_CHANNEL_STATE_READ, COMM_BATTERY_READ,      COMM_LEVELS_READ,
        COMM_LEVEL_MODE_READ,    COMM_SENSORS_READ,
    };
    uint8_t (*const builders[])(CommMessage*) = {
        comm_build_reset,              comm_build_button_state_read, comm_build_relay_state_read,
        comm_build_channel_state_read, comm_build_battery_read,      comm_build_levels_read,
        comm_build_level_mode_read,    comm_build_sensors_read,
    };

    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        CommMessage m;
        memset(&m, 0, sizeof(m));
        uint8_t n = builders[i](&m);
        uint8_t id = ((uint8_t*)&m)[0];
        uint8_t want = test_crc8((uint8_t*)&m, 1);
        if (id != ids[i]) {
            munit_errorf("builder %zu wrote id 0x%02X, expected 0x%02X", i, id, ids[i]);
        }
        if (((uint8_t*)&m)[n - 1u] != want) {
            munit_errorf("precomputed CRC for id 0x%02X is 0x%02X, should be 0x%02X",
                         id, ((uint8_t*)&m)[n - 1u], want);
        }
    }
    return MUNIT_OK;
}

/* ── Frame validation ─────────────────────────────────────────────────── */

static MunitResult test_can_parse_accepts_valid(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommMessage m;
    uint8_t n = comm_build_channel_changed(&m, 0x0001, 0x8001, 0x02, 0x06);
    assert_uint8(n, ==, 9);
    assert_uint8(comm_can_parse((uint8_t*)&m, n), ==, 1);
    return MUNIT_OK;
}

static MunitResult test_can_parse_rejects(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommMessage m;
    uint8_t n = comm_build_relay_state(&m, 0x00FF);
    uint8_t* b = (uint8_t*)&m;

    assert_uint8(comm_can_parse(b, n), ==, 1);

    /* Too short to be a frame at all. */
    assert_uint8(comm_can_parse(b, 0), ==, 0);
    assert_uint8(comm_can_parse(b, 1), ==, 0);

    /* Right CRC, wrong length — a truncated or over-long frame must not pass
     * just because its trailing byte happens to check out. */
    assert_uint8(comm_can_parse(b, (uint8_t)(n - 1u)), ==, 0);
    assert_uint8(comm_can_parse(b, (uint8_t)(n + 1u)), ==, 0);

    /* Unknown command id. */
    uint8_t unknown[4] = {0x77, 0x00, 0x00, 0x00};
    unknown[3] = test_crc8(unknown, 3);
    assert_uint8(comm_can_parse(unknown, 4), ==, 0);

    /* Every single-bit corruption of a valid frame must be rejected. */
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            uint8_t copy[16];
            memcpy(copy, b, n);
            copy[i] ^= (uint8_t)(1u << bit);
            if (comm_can_parse(copy, n)) {
                munit_errorf("corrupting bit %u of byte %u still parses", bit, i);
            }
        }
    }
    return MUNIT_OK;
}

/* ── Round trips ──────────────────────────────────────────────────────── */

static MunitResult test_roundtrip_relay_state(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommMessage m;
    comm_build_relay_state(&m, 0xA55A);
    CommRelayState out;
    comm_parse_relay_state_write((uint8_t*)&m + 1, &out);
    assert_uint16(out.relays, ==, 0xA55A);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_channel_changed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommMessage m;
    comm_build_channel_changed(&m, 0x1234, 0xFEDC, 0x03, 0x05);

    CommChannelChanged out;
    comm_parse_channel_changed((uint8_t*)&m + 1, &out);
    assert_uint8(out.device_address, ==, SELF);
    assert_uint16(out.prev_channels, ==, 0x1234);
    assert_uint16(out.current_channels, ==, 0xFEDC);
    assert_uint8(out.prev_sensors, ==, 0x03);
    assert_uint8(out.current_sensors, ==, 0x05);
    return MUNIT_OK;
}

static MunitResult test_roundtrip_button_changed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint8_t id = 0; id < 8; id++) {
        for (uint8_t pressed = 0; pressed < 2; pressed++) {
            CommMessage m;
            comm_build_button_changed(&m, id, pressed, COMM_BUTTON_MODE_RELEASE);
            CommButtonChanged out;
            comm_parse_button_changed((uint8_t*)&m + 1, &out);
            assert_uint8(out.device_address, ==, SELF);
            assert_uint8(out.button_id, ==, id);
            assert_uint8(out.pressed, ==, pressed);
            assert_uint8(out.mode, ==, COMM_BUTTON_MODE_RELEASE);
        }
    }
    return MUNIT_OK;
}

static MunitResult test_roundtrip_level_mode(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint8_t a = 0; a < 3; a++) {
        for (uint8_t b = 0; b < 3; b++) {
            CommMessage m;
            comm_build_level_mode(&m, (CommMeterMode)a, (CommMeterMode)b);
            CommLevelMode out;
            comm_parse_level_mode_write((uint8_t*)&m + 1, &out);
            assert_uint8(out.mode_0, ==, a);
            assert_uint8(out.mode_1, ==, b);
        }
    }
    return MUNIT_OK;
}

static MunitResult test_roundtrip_responses(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Responses carry the payload alone — no id byte — followed by a CRC. */
    uint8_t battery[] = {0x34, 0x12};
    CommBattery bat;
    comm_parse_battery_response(battery, &bat);
    assert_uint16(bat.voltage, ==, 0x1234);

    uint8_t levels[] = {42, 99};
    CommLevels lv;
    comm_parse_levels_response(levels, &lv);
    assert_uint8(lv.level_0, ==, 42);
    assert_uint8(lv.level_1, ==, 99);

    uint8_t channels[] = {0xCD, 0xAB};
    CommChannelState cs;
    comm_parse_channel_state_response(channels, &cs);
    assert_uint16(cs.channels, ==, 0xABCD);

    uint8_t sensors[] = {0x07};
    CommSensors sn;
    comm_parse_sensors_response(sensors, &sn);
    assert_uint8(sn.sensors, ==, 0x07);
    return MUNIT_OK;
}

/* ── Trigger time codec (MMEETTTT) ────────────────────────────────────── */

static MunitResult test_trigger_exact_values(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    struct {
        uint16_t ms;
        uint8_t mantissa, exponent;
    } cases[] = {
        {0, 0, 0},        {1, 1, 0},        {15, 15, 0},
        {20, 2, 1},       {150, 15, 1},     {200, 2, 2},
        {1500, 15, 2},    {2000, 2, 3},     {15000, 15, 3},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        CommTriggerConfig c = comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, cases[i].ms);
        if (c.time_mantissa != cases[i].mantissa || c.time_exponent != cases[i].exponent) {
            munit_errorf("%u ms encoded as %ux10^%u, expected %ux10^%u", cases[i].ms,
                         c.time_mantissa, c.time_exponent, cases[i].mantissa, cases[i].exponent);
        }
        assert_uint16(comm_button_trigger_time_ms(c), ==, cases[i].ms);
        assert_uint8(c.mode, ==, COMM_BUTTON_MODE_HOLD);
    }
    return MUNIT_OK;
}

/* The header promises "the smallest EE that fits (finest available
 * resolution)", which is what keeps a 150 ms hold from being rounded to
 * 100 ms. */
static MunitResult test_trigger_picks_finest_resolution(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint16_t ms = 0; ms <= 15; ms++) {
        assert_uint8(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, ms).time_exponent, ==, 0);
    }
    assert_uint8(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 16).time_exponent, ==, 1);
    assert_uint8(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 150).time_exponent, ==, 1);
    assert_uint8(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 151).time_exponent, ==, 2);
    assert_uint8(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1500).time_exponent, ==, 2);
    assert_uint8(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1501).time_exponent, ==, 3);
    return MUNIT_OK;
}

static MunitResult test_trigger_clamps_and_never_overshoots(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint16(comm_button_trigger_time_ms(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 20000)), ==, 15000);
    assert_uint16(comm_button_trigger_time_ms(comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 0xFFFF)), ==, 15000);

    /* Rounding is always downward: a button must not fire later than asked.
     * Sweep the whole representable range rather than sampling. */
    for (uint32_t ms = 0; ms <= 15000; ms++) {
        CommTriggerConfig c = comm_button_trigger_make(COMM_BUTTON_MODE_CHANGE, (uint16_t)ms);
        uint16_t decoded = comm_button_trigger_time_ms(c);
        if (decoded > ms) {
            munit_errorf("%u ms encoded to %u ms — longer than requested", (unsigned)ms, decoded);
        }
    }
    return MUNIT_OK;
}

/* Anything that survives an encode must decode to itself. */
static MunitResult test_trigger_decode_encode_is_stable(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (unsigned raw = 0; raw < 256; raw++) {
        CommTriggerConfig c;
        *(uint8_t*)&c = (uint8_t)raw;
        uint16_t ms = comm_button_trigger_time_ms(c);
        CommTriggerConfig again = comm_button_trigger_make((CommButtonMode)c.mode, ms);
        if (comm_button_trigger_time_ms(again) != ms) {
            munit_errorf("raw 0x%02X decodes to %u ms but re-encodes to %u ms",
                         raw, ms, comm_button_trigger_time_ms(again));
        }
        assert_uint8(again.mode, ==, c.mode);
    }
    return MUNIT_OK;
}

/* ── button_effect nibble packing ─────────────────────────────────────── */

/*
 * Four bytes carry eight outputs, high output first, odd output in the upper
 * nibble.  Getting this wrong swaps which button lights up, which is invisible
 * in a CRC check and obvious only on the physical panel.
 */
static MunitResult test_effect_packing_layout(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommButtonEffect e;
    comm_button_effect_init(&e);

    CommButtonOutputEffect v;
    v.raw = 0;
    v.color = COMM_EFFECT_COLOR_BLUE;
    v.mode = COMM_EFFECT_MODE_FLASHING;
    const uint8_t nibble = (uint8_t)((COMM_EFFECT_COLOR_BLUE << 2) | COMM_EFFECT_MODE_FLASHING);

    assert_int8(comm_button_effect_set(&e, 0, v), ==, 0);
    assert_uint8(e.outputs_10, ==, nibble);         /* output 0 -> low nibble of the last byte */
    assert_uint8(e.outputs_76, ==, 0);

    comm_button_effect_init(&e);
    assert_int8(comm_button_effect_set(&e, 7, v), ==, 0);
    assert_uint8(e.outputs_76, ==, (uint8_t)(nibble << 4)); /* output 7 -> high nibble of the first */
    return MUNIT_OK;
}

static MunitResult test_effect_set_get_all_outputs(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommButtonEffect e;
    comm_button_effect_init(&e);

    /* Give every output a distinct value and read them all back — catches a
     * set that clobbers its neighbour's nibble. */
    for (uint8_t i = 0; i < 8; i++) {
        CommButtonOutputEffect v;
        v.raw = 0;
        v.color = (uint8_t)(i & 0x03);
        v.mode = (uint8_t)((i >> 1) & 0x03);
        assert_int8(comm_button_effect_set(&e, i, v), ==, 0);
    }
    for (uint8_t i = 0; i < 8; i++) {
        CommButtonOutputEffect got;
        assert_int8(comm_button_effect_get(&e, i, &got), ==, 0);
        assert_uint8(got.color, ==, (uint8_t)(i & 0x03));
        assert_uint8(got.mode, ==, (uint8_t)((i >> 1) & 0x03));
    }
    return MUNIT_OK;
}

static MunitResult test_effect_rejects_bad_args(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommButtonEffect e;
    comm_button_effect_init(&e);
    CommButtonOutputEffect v = {0};

    assert_int8(comm_button_effect_set(&e, 8, v), ==, -1);
    assert_int8(comm_button_effect_set(NULL, 0, v), ==, -1);
    assert_int8(comm_button_effect_get(&e, 8, &v), ==, -1);
    assert_int8(comm_button_effect_get(&e, 0, NULL), ==, -1);
    return MUNIT_OK;
}

static MunitResult test_effect_survives_the_wire(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommButtonEffect e;
    comm_button_effect_init(&e);
    for (uint8_t i = 0; i < 8; i++) {
        CommButtonOutputEffect v;
        v.raw = (uint8_t)(i | 0x08); /* upper bit is not part of the nibble */
        comm_button_effect_set(&e, i, v);
    }

    CommMessage m;
    uint8_t n = comm_build_button_effect(&m, &e);
    assert_uint8(n, ==, 6);
    assert_uint8(comm_can_parse((uint8_t*)&m, n), ==, 1);

    CommButtonEffect out;
    comm_parse_button_effect((uint8_t*)&m + 1, &out);
    for (uint8_t i = 0; i < 8; i++) {
        CommButtonOutputEffect a, b;
        comm_button_effect_get(&e, i, &a);
        comm_button_effect_get(&out, i, &b);
        assert_uint8(b.raw, ==, a.raw);
    }
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/address", test_address),
    T("/envelope_is_packed", test_envelope_is_packed),
    T("/build_write_messages", test_build_write_messages),
    T("/build_read_requests", test_build_read_requests),
    T("/precomputed_crcs_are_current", test_precomputed_crcs_are_current),
    T("/can_parse_accepts_valid", test_can_parse_accepts_valid),
    T("/can_parse_rejects", test_can_parse_rejects),
    T("/roundtrip_relay_state", test_roundtrip_relay_state),
    T("/roundtrip_channel_changed", test_roundtrip_channel_changed),
    T("/roundtrip_button_changed", test_roundtrip_button_changed),
    T("/roundtrip_level_mode", test_roundtrip_level_mode),
    T("/roundtrip_responses", test_roundtrip_responses),
    T("/trigger_exact_values", test_trigger_exact_values),
    T("/trigger_picks_finest_resolution", test_trigger_picks_finest_resolution),
    T("/trigger_clamps_and_never_overshoots", test_trigger_clamps_and_never_overshoots),
    T("/trigger_decode_encode_is_stable", test_trigger_decode_encode_is_stable),
    T("/effect_packing_layout", test_effect_packing_layout),
    T("/effect_set_get_all_outputs", test_effect_set_get_all_outputs),
    T("/effect_rejects_bad_args", test_effect_rejects_bad_args),
    T("/effect_survives_the_wire", test_effect_survives_the_wire),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite libcomm_suite(void) {
    MunitSuite s = {"/libcomm", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
