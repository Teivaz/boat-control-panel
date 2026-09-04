#include "test_support.h"

#include "crc.h"

#include <string.h>

/*
 * CRC-8, poly 0x07, init 0xFF, MSB-first, no reflection, no final XOR.
 *
 * The point of these tests is not that the table is transcribed correctly
 * (a typo there would break every message equally and be caught by any
 * round-trip test).  It is the property crc.h says the init value was chosen
 * for: that an all-zero frame does not validate.
 */

/*
 * Pins the parameter set.  Note that crc.h calls this "CRC-8/ROHC", but ROHC
 * is a *reflected* algorithm whose check value is 0xD0; this implementation
 * shifts MSB-first and checks 0xFB.  The code is self-consistent — both ends
 * of the bus run this same routine — so nothing is broken, but the catalogue
 * name in the header is wrong and anyone writing a third-party tool against
 * it (a bridge script, a bus analyser) would compute the wrong byte.  Asserting
 * the real value here is what stops that being rediscovered the hard way.
 */
static MunitResult test_known_vector(const MunitParameter p[], void* fixture) {
    (void)p;
    (void)fixture;
    uint8_t data[] = "123456789";
    assert_uint8(comm_crc8(data, 9), ==, 0xFB);
    return MUNIT_OK;
}

static MunitResult test_empty_is_init(const MunitParameter p[], void* fixture) {
    (void)p;
    (void)fixture;
    uint8_t data[] = {0};
    assert_uint8(comm_crc8(data, 0), ==, 0xFF);
    return MUNIT_OK;
}

/*
 * The reason for init=0xFF, spelled out in crc.h: with init=0x00 the CRC of
 * {0x00, 0x00} is 0x00, so a bus delivering three zero bytes would present a
 * "valid" all-zero response.  Every fixed-size response on this protocol is
 * 2 or 3 bytes, so that failure would be indistinguishable from real data.
 */
static MunitResult test_all_zero_frame_is_rejected(const MunitParameter p[], void* fixture) {
    (void)p;
    (void)fixture;
    for (uint8_t len = 1; len <= 8; len++) {
        uint8_t zeros[8] = {0};
        assert_uint8(comm_crc8(zeros, len), !=, 0x00);
    }
    return MUNIT_OK;
}

/* A single flipped bit anywhere in a short frame must change the CRC — this
 * is the whole reason the byte is on the wire. */
static MunitResult test_single_bit_flips_detected(const MunitParameter p[], void* fixture) {
    (void)p;
    (void)fixture;
    uint8_t base[] = {0x05, 0x34, 0x12};
    const uint8_t good = comm_crc8(base, sizeof(base));

    for (size_t byte = 0; byte < sizeof(base); byte++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            uint8_t corrupt[sizeof(base)];
            memcpy(corrupt, base, sizeof(base));
            corrupt[byte] ^= (uint8_t)(1u << bit);
            if (comm_crc8(corrupt, sizeof(corrupt)) == good) {
                munit_errorf("bit %u of byte %zu is invisible to the CRC", bit, byte);
            }
        }
    }
    return MUNIT_OK;
}

/* Length is part of the message: truncating a frame must not still validate. */
static MunitResult test_length_matters(const MunitParameter p[], void* fixture) {
    (void)p;
    (void)fixture;
    uint8_t data[] = {0x06, 0x00, 0x00, 0x00};
    assert_uint8(comm_crc8(data, 4), !=, comm_crc8(data, 3));
    assert_uint8(comm_crc8(data, 3), !=, comm_crc8(data, 2));
    return MUNIT_OK;
}

/* The exported table is what libcomm.c's unrolled finaliser indexes directly,
 * so it has to agree with the function byte for byte. */
static MunitResult test_table_matches_function(const MunitParameter p[], void* fixture) {
    (void)p;
    (void)fixture;
    for (unsigned b = 0; b < 256; b++) {
        uint8_t byte = (uint8_t)b;
        uint8_t via_fn = comm_crc8(&byte, 1);
        uint8_t via_table = crc8_table[0xFFu ^ byte];
        assert_uint8(via_fn, ==, via_table);
    }
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/known_vector", test_known_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/empty_is_init", test_empty_is_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/all_zero_frame_is_rejected", test_all_zero_frame_is_rejected, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/single_bit_flips_detected", test_single_bit_flips_detected, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/length_matters", test_length_matters, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/table_matches_function", test_table_matches_function, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite crc_suite(void) {
    MunitSuite s = {"/crc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
