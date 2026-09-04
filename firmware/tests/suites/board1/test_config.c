#include "config.h"
#include "libcomm.h"
#include "task_ids.h"
#include "test_support.h"

#include <xc.h>

/*
 * Persistent configuration.
 *
 * The interesting parts are not the reads and writes but the two properties
 * that keep the EEPROM alive and the I2C ISR fast: writes are enqueued from
 * ISR context and programmed from the main loop, and repeated writes to one
 * address collapse into a single cell program.  Data EEPROM on this part is
 * rated for ~100k cycles, and the main board polls the switching board
 * continuously, so a write path that programmed on every message would wear a
 * cell out in weeks.
 */

/* Internal EEPROM layout, from config.c's own documentation. */
#define OFF_MAGIC_LO 0x00
#define OFF_MAGIC_HI 0x01
#define OFF_WATER 0x02
#define OFF_FUEL 0x03
#define OFF_BATT 0x04
#define OFF_LEVEL_MODE 0x05

#define MAGIC_LO 0x5A
#define MAGIC_HI 0xA9

static TaskController ctrl;

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    task_controller_init(&ctrl);
    return NULL;
}

/* Run the flush task to completion. */
static void flush(void) {
    test_advance_ms(&ctrl, TASK_MIN_MS);
}

/* ── First boot ───────────────────────────────────────────────────────── */

static MunitResult test_virgin_device_seeds_defaults(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* An unprogrammed cell reads as 0xFF, which is how config_init tells a
     * virgin device from one that has been configured. */
    config_init(&ctrl);

    assert_uint8(pic_eeprom_get(OFF_MAGIC_LO), ==, MAGIC_LO);
    assert_uint8(pic_eeprom_get(OFF_MAGIC_HI), ==, MAGIC_HI);
    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 100); /* 100 Ω reads as 100 Ω */
    assert_uint8(pic_eeprom_get(OFF_FUEL), ==, 100);
    assert_uint8(pic_eeprom_get(OFF_BATT), ==, 120);  /* 12000 mV in 100 mV units */
    assert_uint8(pic_eeprom_get(OFF_LEVEL_MODE), ==, 0x05); /* both meters European */
    return MUNIT_OK;
}

/*
 * The magic word is written *after* the defaults, so a reset partway through
 * seeding leaves the header absent and the next boot starts over.  Writing it
 * first would leave a half-configured device that looks configured.
 */
static MunitResult test_magic_is_written_last(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Make every program fail: the defaults never land, and neither should the
     * magic — so the next boot with a working EEPROM still seeds. */
    pic_eeprom_write_fails = 1;
    config_init(&ctrl);
    assert_uint8(pic_eeprom_get(OFF_MAGIC_LO), ==, 0xFF);

    pic_eeprom_write_fails = 0;
    config_init(&ctrl);
    assert_uint8(pic_eeprom_get(OFF_MAGIC_LO), ==, MAGIC_LO);
    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 100);
    return MUNIT_OK;
}

static MunitResult test_configured_device_is_left_alone(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    pic_eeprom_put(OFF_MAGIC_LO, MAGIC_LO);
    pic_eeprom_put(OFF_MAGIC_HI, MAGIC_HI);
    pic_eeprom_put(OFF_WATER, 77);

    config_init(&ctrl);
    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 77);
    assert_uint8(config_read_byte(CONFIG_ADDR_WATER_CAL), ==, 77);
    return MUNIT_OK;
}

/* A layout change bumps the magic, and a device carrying the old one must be
 * re-seeded rather than reading the new fields out of the old layout. */
static MunitResult test_stale_magic_reseeds(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    pic_eeprom_put(OFF_MAGIC_LO, MAGIC_LO);
    pic_eeprom_put(OFF_MAGIC_HI, 0xA8); /* the previous layout's magic */
    pic_eeprom_put(OFF_WATER, 77);

    config_init(&ctrl);
    assert_uint8(pic_eeprom_get(OFF_MAGIC_HI), ==, MAGIC_HI);
    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 100);
    return MUNIT_OK;
}

/* ── Address space ────────────────────────────────────────────────────── */

static MunitResult test_universal_addresses(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);
    /* Every device answers these three, and the device id must match the
     * address it actually listens on — that is how the main board confirms it
     * is talking to the board it thinks it is. */
    assert_uint8(config_read_byte(COMM_CONFIG_DEVICE_ID), ==, comm_address());
    assert_uint8(config_read_byte(COMM_CONFIG_DEVICE_ID), ==, COMM_ADDRESS_SWITCHING);
    assert_uint8(config_read_byte(COMM_CONFIG_HW_REVISION), ==, 0x01);
    assert_uint8(config_read_byte(COMM_CONFIG_SW_REVISION), ==, 0x01);
    return MUNIT_OK;
}

static MunitResult test_unmapped_addresses(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);
    const unsigned before = pic_eeprom_writes;

    /* Reads of unmapped addresses report the erase pattern... */
    assert_uint8(config_read_byte(0x7F), ==, 0xFF);
    assert_uint8(config_read_byte(0x20), ==, 0xFF);

    /* ...and writes to them are dropped rather than landing on some
     * neighbouring cell. */
    config_write_byte(0x7F, 0x11);
    flush();
    assert_uint(pic_eeprom_writes, ==, before);

    /* The universal read-only fields are not writable either. */
    config_write_byte(COMM_CONFIG_HW_REVISION, 0x99);
    flush();
    assert_uint8(config_read_byte(COMM_CONFIG_HW_REVISION), ==, 0x01);
    return MUNIT_OK;
}

static MunitResult test_each_mapped_address_round_trips(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);

    const struct {
        uint8_t proto;
        unsigned offset;
    } map[] = {
        {CONFIG_ADDR_WATER_CAL, OFF_WATER},
        {CONFIG_ADDR_FUEL_CAL, OFF_FUEL},
        {CONFIG_ADDR_BATTERY_CAL, OFF_BATT},
        {CONFIG_ADDR_LEVEL_MODE, OFF_LEVEL_MODE},
    };

    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        const uint8_t value = (uint8_t)(0x40u + i);
        config_write_byte(map[i].proto, value);
        flush();
        if (pic_eeprom_get(map[i].offset) != value) {
            munit_errorf("protocol address 0x%02X did not land at EEPROM offset 0x%02X",
                         map[i].proto, (unsigned)map[i].offset);
        }
        assert_uint8(config_read_byte(map[i].proto), ==, value);
    }
    return MUNIT_OK;
}

/* ── Deferred write queue ─────────────────────────────────────────────── */

/*
 * config_write_byte is called from the I2C ISR, where a ~4 ms EEPROM program
 * would stall the bus.  It must enqueue and return, with the program happening
 * later in main context.
 */
static MunitResult test_write_is_deferred_to_main_context(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);
    const unsigned before = pic_eeprom_writes;

    config_write_byte(CONFIG_ADDR_WATER_CAL, 55);
    assert_uint(pic_eeprom_writes, ==, before); /* nothing programmed yet */

    /* But the new value is already visible to readers — the queue is searched
     * ahead of the cell, so a read-after-write from the same transaction sees
     * what was just written. */
    assert_uint8(config_read_byte(CONFIG_ADDR_WATER_CAL), ==, 55);

    flush();
    assert_uint(pic_eeprom_writes, >, before);
    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 55);
    return MUNIT_OK;
}

/* Repeated writes to one address collapse to a single program.  Without this
 * the main board's polling would burn a cell's endurance in short order. */
static MunitResult test_repeated_writes_coalesce(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);
    const unsigned before = pic_eeprom_writes;

    for (uint8_t i = 0; i < 20; i++) {
        config_write_byte(CONFIG_ADDR_FUEL_CAL, i);
    }
    flush();

    assert_uint8(pic_eeprom_get(OFF_FUEL), ==, 19); /* the last value wins */
    assert_uint(pic_eeprom_writes - before, ==, 1);
    return MUNIT_OK;
}

/* Writing the value a cell already holds must not program it at all. */
static MunitResult test_idempotent_write_does_not_program(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);
    config_write_byte(CONFIG_ADDR_BATTERY_CAL, 130);
    flush();

    const unsigned before = pic_eeprom_writes;
    config_write_byte(CONFIG_ADDR_BATTERY_CAL, 130);
    flush();
    assert_uint(pic_eeprom_writes, ==, before);
    return MUNIT_OK;
}

/* Distinct addresses queued together must all reach their own cells. */
static MunitResult test_queue_drains_every_entry(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);

    config_write_byte(CONFIG_ADDR_WATER_CAL, 11);
    config_write_byte(CONFIG_ADDR_FUEL_CAL, 22);
    config_write_byte(CONFIG_ADDR_BATTERY_CAL, 33);
    flush();

    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 11);
    assert_uint8(pic_eeprom_get(OFF_FUEL), ==, 22);
    assert_uint8(pic_eeprom_get(OFF_BATT), ==, 33);
    return MUNIT_OK;
}

/* The flush task removes itself once the queue empties, and a later write
 * brings it back — otherwise it would poll forever for nothing, or worse,
 * never run again. */
static MunitResult test_flush_task_comes_and_goes(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);
    assert_uint8(test_task_active(&ctrl, TASK_CONFIG_FLUSH), ==, 0);

    config_write_byte(CONFIG_ADDR_WATER_CAL, 5);
    assert_uint8(test_task_active(&ctrl, TASK_CONFIG_FLUSH), ==, 1);

    flush();
    assert_uint8(test_task_active(&ctrl, TASK_CONFIG_FLUSH), ==, 0);

    config_write_byte(CONFIG_ADDR_WATER_CAL, 6);
    assert_uint8(test_task_active(&ctrl, TASK_CONFIG_FLUSH), ==, 1);
    flush();
    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 6);
    return MUNIT_OK;
}

/* The queue holds three entries (power-of-two ring, one slot reserved).
 * Overflow drops silently, which is safe because config writes are
 * idempotent and the host can retry — but the entries that did fit must
 * still land. */
static MunitResult test_queue_overflow_drops_without_corrupting(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);

    config_write_byte(CONFIG_ADDR_WATER_CAL, 1);
    config_write_byte(CONFIG_ADDR_FUEL_CAL, 2);
    config_write_byte(CONFIG_ADDR_BATTERY_CAL, 3);
    config_write_byte(CONFIG_ADDR_LEVEL_MODE, 4); /* beyond capacity */
    flush();

    assert_uint8(pic_eeprom_get(OFF_WATER), ==, 1);
    assert_uint8(pic_eeprom_get(OFF_FUEL), ==, 2);
    assert_uint8(pic_eeprom_get(OFF_BATT), ==, 3);

    /* And the dropped write can simply be reissued. */
    config_write_byte(CONFIG_ADDR_LEVEL_MODE, 4);
    flush();
    assert_uint8(pic_eeprom_get(OFF_LEVEL_MODE), ==, 4);
    return MUNIT_OK;
}

/* ── 16-bit helper ────────────────────────────────────────────────────── */

static MunitResult test_read_word_is_little_endian(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_init(&ctrl);
    config_write_byte(CONFIG_ADDR_WATER_CAL, 0x34); /* 0x10 */
    config_write_byte(CONFIG_ADDR_FUEL_CAL, 0x12);  /* 0x11 */
    flush();
    assert_uint16(config_read_word(CONFIG_ADDR_WATER_CAL), ==, 0x1234);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/virgin_device_seeds_defaults", test_virgin_device_seeds_defaults),
    T("/magic_is_written_last", test_magic_is_written_last),
    T("/configured_device_is_left_alone", test_configured_device_is_left_alone),
    T("/stale_magic_reseeds", test_stale_magic_reseeds),
    T("/universal_addresses", test_universal_addresses),
    T("/unmapped_addresses", test_unmapped_addresses),
    T("/each_mapped_address_round_trips", test_each_mapped_address_round_trips),
    T("/write_is_deferred_to_main_context", test_write_is_deferred_to_main_context),
    T("/repeated_writes_coalesce", test_repeated_writes_coalesce),
    T("/idempotent_write_does_not_program", test_idempotent_write_does_not_program),
    T("/queue_drains_every_entry", test_queue_drains_every_entry),
    T("/flush_task_comes_and_goes", test_flush_task_comes_and_goes),
    T("/queue_overflow_drops_without_corrupting", test_queue_overflow_drops_without_corrupting),
    T("/read_word_is_little_endian", test_read_word_is_little_endian),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b1_config_suite(void) {
    MunitSuite s = {"/board1/config", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
