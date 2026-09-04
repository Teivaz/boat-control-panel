#include "adc.h"
#include "comm.h"
#include "config.h"
#include "controller.h"
#include "i2c_fake.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "relay_mon.h"
#include "relay_out.h"
#include "sensors.h"
#include "task_ids.h"
#include "test_support.h"

#include "hw_sim.h"

#include <string.h>
#include <xc.h>

/*
 * The switching board as a whole: what it does with commands off the bus, and
 * what it puts back on the bus when the physical state changes.
 *
 * These run against the real protocol dispatcher and a fake I2C driver, so a
 * frame arriving here travels the same path it does on the boat — CRC check,
 * dispatch, board callback, hardware — and the reply is inspected as bytes
 * rather than as a function argument.
 */

#define OFF_LEVEL_MODE 0x05
#define MAGIC_LO 0x5A
#define MAGIC_HI 0xA9

#define BIT_MAIN 0x0001
#define BIT_ANCHOR 0x0040

static TaskController ctrl;

static void boot(void) {
    test_reset_hardware();
    i2c_fake_reset();
    sr_sim_attach();
    relay_out_init();
    relay_mon_init();
    mux_sim_attach(0);
    comm_interface_init();
    task_controller_init(&ctrl);
    config_init(&ctrl);
    adc_init(&ctrl);
    comm_init();
    sensors_init(&ctrl);
    controller_init(&ctrl);
}

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    boot();
    return NULL;
}

/* Deliver a framed write from another master and let the main loop dispatch it. */
static void deliver(uint8_t id, const uint8_t* payload, uint8_t len) {
    uint8_t frame[16];
    uint8_t n = i2c_fake_frame(frame, id, payload, len);
    i2c_fake_deliver_write(frame, n);
    i2c_poll();
}

/* Deliver the write phase of a read and return the staged reply payload
 * (without its trailing CRC), checking the CRC on the way through. */
static uint8_t read_request(uint8_t id, const uint8_t* payload, uint8_t plen, uint8_t* out) {
    uint8_t frame[16];
    uint8_t n = i2c_fake_frame(frame, id, payload, plen);
    i2c_fake_deliver_read_request(frame, n);

    uint8_t len = 0;
    const uint8_t* reply = i2c_fake_client_tx(&len);
    if (!reply || len < 2) {
        munit_errorf("read 0x%02X staged no reply", id);
    }
    if (reply[len - 1u] != test_crc8(reply, (uint8_t)(len - 1u))) {
        munit_errorf("read 0x%02X staged a reply with a bad CRC", id);
    }
    memcpy(out, reply, (size_t)(len - 1u));
    return (uint8_t)(len - 1u);
}

/* ── Boot state ───────────────────────────────────────────────────────── */

/* Every relay must be off at boot regardless of what the shift register was
 * holding — on a boat the alternative is switching a circuit nobody asked
 * for, at power-on, unattended. */
static MunitResult test_boots_with_everything_off(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint16(controller_relay_target(), ==, 0);
    assert_uint16(sr_latched(), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_level_mode_survives_a_reboot(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* The installer picks American senders; that choice must outlive a power
     * cycle without being re-sent by the main board. */
    const uint8_t mode = (COMM_METER_MODE_240_33 << 2) | COMM_METER_MODE_240_33;
    deliver(COMM_LEVEL_MODE, &mode, 1);
    test_advance_ms(&ctrl, TASK_MIN_MS);
    assert_uint8(pic_eeprom_get(OFF_LEVEL_MODE), ==, mode);

    /* Reboot with the EEPROM intact. */
    pic_mock_reset();
    i2c_fake_reset();
    task_controller_init(&ctrl);
    config_init(&ctrl);
    controller_init(&ctrl);
    assert_uint8(controller_level_mode(), ==, mode);
    return MUNIT_OK;
}

/* A corrupt cell reading as the erase pattern must land on the documented
 * default rather than on whatever 0x0F happens to mean. */
static MunitResult test_corrupt_level_mode_falls_back(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    pic_mock_reset();
    pic_eeprom_put(0x00, MAGIC_LO);
    pic_eeprom_put(0x01, MAGIC_HI);
    pic_eeprom_put(OFF_LEVEL_MODE, 0xFF);

    task_controller_init(&ctrl);
    config_init(&ctrl);
    controller_init(&ctrl);

    const uint8_t expect = (COMM_METER_MODE_0_190 << 2) | COMM_METER_MODE_0_190;
    assert_uint8(controller_level_mode(), ==, expect);
    return MUNIT_OK;
}

/* ── Inbound commands ─────────────────────────────────────────────────── */

static MunitResult test_relay_state_write_drives_the_coils(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint16_t want = BIT_MAIN | BIT_ANCHOR;
    const uint8_t payload[2] = {(uint8_t)want, (uint8_t)(want >> 8)};
    deliver(COMM_RELAY_STATE, payload, 2);

    assert_uint16(controller_relay_target(), ==, want);
    /* main -> SR pin 1, anchor light -> SR pin 6 */
    assert_uint16(sr_latched(), ==, (uint16_t)((1u << 1) | (1u << 6)));
    return MUNIT_OK;
}

static MunitResult test_config_write_is_persisted(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t payload[2] = {CONFIG_ADDR_WATER_CAL, 88};
    deliver(COMM_CONFIG, payload, 2);
    test_advance_ms(&ctrl, TASK_MIN_MS);
    assert_uint8(config_read_byte(CONFIG_ADDR_WATER_CAL), ==, 88);
    return MUNIT_OK;
}

static MunitResult test_reset_command_reboots(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint(pic_reset_count, ==, 0);
    deliver(COMM_RESET, NULL, 0);
    assert_uint(pic_reset_count, ==, 1);
    return MUNIT_OK;
}

/* ── Read requests ────────────────────────────────────────────────────── */

static MunitResult test_relay_state_read_returns_the_target(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint16_t want = 0x1234;
    const uint8_t set[2] = {(uint8_t)want, (uint8_t)(want >> 8)};
    deliver(COMM_RELAY_STATE, set, 2);

    uint8_t reply[8];
    assert_uint8(read_request(COMM_RELAY_STATE_READ, NULL, 0, reply), ==, 2);
    assert_uint16((uint16_t)(reply[0] | (reply[1] << 8)), ==, want);
    return MUNIT_OK;
}

/*
 * channel_state is not relay_state: it is the voltage actually observed
 * downstream of each fuse.  A blown fuse shows as a commanded relay whose
 * channel reads zero, and that difference is the whole reason both reads
 * exist.
 */
static MunitResult test_channel_state_read_reports_observed_voltage(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint16_t commanded = BIT_MAIN | BIT_ANCHOR;
    const uint8_t set[2] = {(uint8_t)commanded, (uint8_t)(commanded >> 8)};
    deliver(COMM_RELAY_STATE, set, 2);

    /* The anchor light's fuse is blown: commanded, but no voltage downstream. */
    mux_sim_set(BIT_MAIN);
    test_advance_ms(&ctrl, 50); /* one monitor tick */

    uint8_t reply[8];
    assert_uint8(read_request(COMM_CHANNEL_STATE_READ, NULL, 0, reply), ==, 2);
    assert_uint16((uint16_t)(reply[0] | (reply[1] << 8)), ==, BIT_MAIN);
    assert_uint16(controller_relay_target(), ==, commanded); /* still commanded */
    return MUNIT_OK;
}

static MunitResult test_sensors_read_reports_three_bits(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t reply[8];
    assert_uint8(read_request(COMM_SENSORS_READ, NULL, 0, reply), ==, 1);
    assert_uint8(reply[0] & 0xF8u, ==, 0); /* nothing outside bits 0..2 */
    return MUNIT_OK;
}

static MunitResult test_level_mode_read_returns_what_was_set(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t mode = (COMM_METER_MODE_CALIBRATION << 2) | COMM_METER_MODE_240_33;
    deliver(COMM_LEVEL_MODE, &mode, 1);

    uint8_t reply[8];
    assert_uint8(read_request(COMM_LEVEL_MODE_READ, NULL, 0, reply), ==, 1);
    assert_uint8(reply[0] & 0x0Fu, ==, mode);
    return MUNIT_OK;
}

static MunitResult test_config_read_returns_the_addressed_byte(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t reply[8];
    const uint8_t addr = COMM_CONFIG_DEVICE_ID;
    assert_uint8(read_request(COMM_CONFIG_READ, &addr, 1, reply), ==, 1);
    assert_uint8(reply[0], ==, COMM_ADDRESS_SWITCHING);
    return MUNIT_OK;
}

static MunitResult test_battery_and_levels_reads_are_two_bytes(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t reply[8];
    assert_uint8(read_request(COMM_BATTERY_READ, NULL, 0, reply), ==, 2);
    assert_uint8(read_request(COMM_LEVELS_READ, NULL, 0, reply), ==, 2);
    return MUNIT_OK;
}

/* ── Outbound pushes ──────────────────────────────────────────────────── */

/*
 * The main board learns about a blown fuse or a tripped bilge float because
 * the switching board pushes, not because it polls fast enough.  A change
 * that is never pushed is a change the panel never shows.
 */
static MunitResult test_channel_change_is_pushed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    mux_sim_set(BIT_MAIN);
    test_advance_ms(&ctrl, 60); /* monitor tick, then the retry task */

    const uint8_t i = i2c_fake_find(COMM_CHANNEL_CHANGED);
    if (i == 0xFF) {
        munit_error("no channel_changed was pushed");
    }
    const I2cFakeTx* tx = i2c_fake_tx(i);
    assert_uint8(tx->addr, ==, COMM_ADDRESS_MAIN);
    assert_uint8(tx->tx_len, ==, 9); /* 1 id + 7 payload + 1 crc */

    CommChannelChanged ev;
    comm_parse_channel_changed((uint8_t*)tx->tx + 1, &ev);
    assert_uint8(ev.device_address, ==, COMM_ADDRESS_SWITCHING);
    assert_uint16(ev.prev_channels, ==, 0);
    assert_uint16(ev.current_channels, ==, BIT_MAIN);
    return MUNIT_OK;
}

/* One push per change, not one per monitor tick. */
static MunitResult test_steady_state_is_quiet(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    mux_sim_set(BIT_MAIN);
    test_advance_ms(&ctrl, 60);
    const uint8_t after_first = i2c_fake_tx_count();

    test_advance_ms(&ctrl, 500); /* ten more monitor ticks, nothing changed */
    assert_uint8(i2c_fake_tx_count(), ==, after_first);
    return MUNIT_OK;
}

/*
 * If the bus is busy the push has to be held, not dropped.  The main board
 * would otherwise be left believing a stale channel mask indefinitely — there
 * is no periodic re-push to correct it.
 */
static MunitResult test_failed_push_is_retried(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_submit_result(I2C_RESULT_QUEUE_FULL);
    mux_sim_set(BIT_MAIN);
    test_advance_ms(&ctrl, 60);
    assert_uint8(i2c_fake_tx_count(), ==, 0);

    i2c_fake_set_submit_result(I2C_RESULT_OK);
    test_advance_ms(&ctrl, 10);
    assert_uint8(i2c_fake_find(COMM_CHANNEL_CHANGED), !=, 0xFF);
    return MUNIT_OK;
}

/* The pushed "previous" value is the last one the main board was actually
 * told about, so a change that happens while a push is blocked is not lost in
 * the gap. */
static MunitResult test_push_reports_the_last_acknowledged_state(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    mux_sim_set(BIT_MAIN);
    test_advance_ms(&ctrl, 60);

    mux_sim_set(BIT_MAIN | BIT_ANCHOR);
    test_advance_ms(&ctrl, 60);

    /* The second push, not the first: prev must be what the main board was
     * last told, so the two messages chain without a gap. */
    const I2cFakeTx* tx = i2c_fake_last_tx();
    assert_uint8(tx->tx[0], ==, COMM_CHANNEL_CHANGED);
    CommChannelChanged ev;
    comm_parse_channel_changed((uint8_t*)tx->tx + 1, &ev);
    assert_uint16(ev.prev_channels, ==, BIT_MAIN);
    assert_uint16(ev.current_channels, ==, BIT_MAIN | BIT_ANCHOR);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/boots_with_everything_off", test_boots_with_everything_off),
    T("/level_mode_survives_a_reboot", test_level_mode_survives_a_reboot),
    T("/corrupt_level_mode_falls_back", test_corrupt_level_mode_falls_back),
    T("/relay_state_write_drives_the_coils", test_relay_state_write_drives_the_coils),
    T("/config_write_is_persisted", test_config_write_is_persisted),
    T("/reset_command_reboots", test_reset_command_reboots),
    T("/relay_state_read_returns_the_target", test_relay_state_read_returns_the_target),
    T("/channel_state_read_reports_observed_voltage", test_channel_state_read_reports_observed_voltage),
    T("/sensors_read_reports_three_bits", test_sensors_read_reports_three_bits),
    T("/level_mode_read_returns_what_was_set", test_level_mode_read_returns_what_was_set),
    T("/config_read_returns_the_addressed_byte", test_config_read_returns_the_addressed_byte),
    T("/battery_and_levels_reads_are_two_bytes", test_battery_and_levels_reads_are_two_bytes),
    T("/channel_change_is_pushed", test_channel_change_is_pushed),
    T("/steady_state_is_quiet", test_steady_state_is_quiet),
    T("/failed_push_is_retried", test_failed_push_is_retried),
    T("/push_reports_the_last_acknowledged_state", test_push_reports_the_last_acknowledged_state),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b1_controller_suite(void) {
    MunitSuite s = {"/board1/controller", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
