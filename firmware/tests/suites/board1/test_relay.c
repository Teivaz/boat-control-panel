#include "hw_sim.h"
#include "relay_mon.h"
#include "relay_out.h"
#include "test_support.h"

#include <xc.h>

/*
 * The two bit-mapping tables at the hardware boundary.
 *
 * Both are hand-built lookup tables recovered by probing the assembled board,
 * and both are silent when wrong: a swapped pair still latches a valid word,
 * still reads back cleanly, and simply switches the wrong circuit.  Nothing
 * upstream can detect it.  These tests are the only place the mapping is
 * checked, so they check it exhaustively — one bit at a time, in both
 * directions.
 */

/* The wire-bit -> shift-register-pin map from board1-switching/relay_out.c's
 * documentation, written out independently so a typo in the driver's table
 * shows up as a mismatch rather than being copied into the expectation. */
static const uint8_t expect_wire_to_sr[16] = {
    1,  /* 0  main             */
    0,  /* 1  instruments      */
    2,  /* 2  autopilot        */
    3,  /* 3  bow_light        */
    4,  /* 4  stern_light      */
    5,  /* 5  steaming_light   */
    6,  /* 6  anchor_light     */
    8,  /* 7  tricolor_light   */
    11, /* 8  inverter         */
    9,  /* 9  fresh_water_pump */
    10, /* 10 fridge           */
    14, /* 11 deck_lights      */
    7,  /* 12 cabin_lights     */
    15, /* 13 usb              */
    12, /* 14 aux1             */
    13, /* 15 aux2             */
};

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    return NULL;
}

/* ── relay_out ────────────────────────────────────────────────────────── */

static MunitResult test_init_clears_all_relays(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sr_sim_attach();
    relay_out_init();

    /* The board must come up with every coil de-energised, whatever the shift
     * register powered on holding. */
    assert_uint16(sr_latched(), ==, 0);
    assert_uint(sr_latch_count(), >, 0);

    /* Pins configured as digital outputs. */
    assert_uint8(TRISAbits.TRISA0, ==, 0);
    assert_uint8(TRISAbits.TRISA1, ==, 0);
    assert_uint8(TRISAbits.TRISA2, ==, 0);
    assert_uint8(ANSELAbits.ANSELA0, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_each_wire_bit_reaches_its_own_pin(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sr_sim_attach();
    relay_out_init();

    for (uint8_t wire = 0; wire < 16; wire++) {
        relay_out_write((uint16_t)(1u << wire));
        const uint16_t got = sr_latched();
        const uint16_t want = (uint16_t)(1u << expect_wire_to_sr[wire]);
        if (got != want) {
            munit_errorf("wire bit %u latched 0x%04X, expected 0x%04X (SR pin %u)",
                         wire, got, want, expect_wire_to_sr[wire]);
        }
    }
    return MUNIT_OK;
}

/* The map has to be a permutation — two wire bits driving one pin would make
 * one relay uncontrollable and another follow the wrong switch. */
static MunitResult test_mapping_is_a_permutation(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sr_sim_attach();
    relay_out_init();

    uint16_t seen = 0;
    for (uint8_t wire = 0; wire < 16; wire++) {
        relay_out_write((uint16_t)(1u << wire));
        const uint16_t got = sr_latched();
        /* Exactly one pin, and one no other wire bit has claimed. */
        if (got == 0 || (got & (uint16_t)(got - 1u)) != 0) {
            munit_errorf("wire bit %u drove %u pins, expected exactly 1", wire, __builtin_popcount(got));
        }
        if (seen & got) {
            munit_errorf("wire bit %u drives a pin already used", wire);
        }
        seen |= got;
    }
    assert_uint16(seen, ==, 0xFFFF);
    return MUNIT_OK;
}

static MunitResult test_all_on_and_all_off(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sr_sim_attach();
    relay_out_init();

    relay_out_write(0xFFFF);
    assert_uint16(sr_latched(), ==, 0xFFFF);
    relay_out_write(0x0000);
    assert_uint16(sr_latched(), ==, 0x0000);
    return MUNIT_OK;
}

static MunitResult test_combination_is_the_union_of_its_bits(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sr_sim_attach();
    relay_out_init();

    /* main + anchor light + fridge */
    const uint16_t wire = (1u << 0) | (1u << 6) | (1u << 10);
    const uint16_t want = (uint16_t)((1u << expect_wire_to_sr[0]) | (1u << expect_wire_to_sr[6]) |
                                     (1u << expect_wire_to_sr[10]));
    relay_out_write(wire);
    assert_uint16(sr_latched(), ==, want);
    return MUNIT_OK;
}

/* Data must be stable when the latch pulse arrives, i.e. the whole word is
 * shifted before it is presented — the outputs never show a partial word. */
static MunitResult test_one_latch_per_write(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sr_sim_attach();
    relay_out_init();
    const unsigned before = sr_latch_count();

    relay_out_write(0x00FF);
    assert_uint(sr_latch_count() - before, ==, 1);
    return MUNIT_OK;
}

/* ── relay_mon ────────────────────────────────────────────────────────── */

static MunitResult test_mon_reads_each_channel_back(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    relay_mon_init();
    mux_sim_attach(0);

    for (uint8_t wire = 0; wire < 16; wire++) {
        mux_sim_set((uint16_t)(1u << wire));
        const uint16_t got = relay_mon_read();
        if (got != (uint16_t)(1u << wire)) {
            munit_errorf("channel %u read back as 0x%04X", wire, got);
        }
    }
    return MUNIT_OK;
}

static MunitResult test_mon_reads_combinations(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    relay_mon_init();
    mux_sim_attach(0);

    const uint16_t patterns[] = {0x0000, 0xFFFF, 0xAAAA, 0x5555, 0x0F0F, 0x8001};
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
        mux_sim_set(patterns[i]);
        assert_uint16(relay_mon_read(), ==, patterns[i]);
    }
    return MUNIT_OK;
}

static MunitResult test_mon_configures_pins(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    relay_mon_init();
    /* Select lines out, commons in, all digital. */
    assert_uint8(TRISBbits.TRISB3, ==, 0);
    assert_uint8(TRISBbits.TRISB4, ==, 0);
    assert_uint8(TRISBbits.TRISB5, ==, 0);
    assert_uint8(TRISAbits.TRISA3, ==, 1);
    assert_uint8(TRISAbits.TRISA4, ==, 1);
    assert_uint8(ANSELAbits.ANSELA3, ==, 0);
    assert_uint8(ANSELAbits.ANSELA4, ==, 0);
    return MUNIT_OK;
}

/*
 * The output map and the monitor map are inverses of each other in wire
 * space: command a set of channels, feed the physical state back through the
 * muxes, and the observed mask must equal what was commanded.  A board where
 * the two tables disagree reports the wrong channel as blown.
 */
static MunitResult test_output_and_monitor_agree(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sr_sim_attach();
    relay_out_init();
    relay_mon_init();
    mux_sim_attach(0);

    for (uint8_t wire = 0; wire < 16; wire++) {
        const uint16_t commanded = (uint16_t)(1u << wire);
        relay_out_write(commanded);

        /* Translate the latched SR word back into wire space using the same
         * table the output side was checked against, then present it to the
         * muxes as the observed physical state. */
        const uint16_t latched = sr_latched();
        uint16_t observed = 0;
        for (uint8_t w = 0; w < 16; w++) {
            if (latched & (uint16_t)(1u << expect_wire_to_sr[w])) {
                observed |= (uint16_t)(1u << w);
            }
        }
        mux_sim_set(observed);

        if (relay_mon_read() != commanded) {
            munit_errorf("commanded 0x%04X but monitored 0x%04X", commanded, relay_mon_read());
        }
    }
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/init_clears_all_relays", test_init_clears_all_relays),
    T("/each_wire_bit_reaches_its_own_pin", test_each_wire_bit_reaches_its_own_pin),
    T("/mapping_is_a_permutation", test_mapping_is_a_permutation),
    T("/all_on_and_all_off", test_all_on_and_all_off),
    T("/combination_is_the_union_of_its_bits", test_combination_is_the_union_of_its_bits),
    T("/one_latch_per_write", test_one_latch_per_write),
    T("/mon_reads_each_channel_back", test_mon_reads_each_channel_back),
    T("/mon_reads_combinations", test_mon_reads_combinations),
    T("/mon_configures_pins", test_mon_configures_pins),
    T("/output_and_monitor_agree", test_output_and_monitor_agree),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b1_relay_suite(void) {
    MunitSuite s = {"/board1/relay", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
