#include "adopter_stubs.h"
#include "i2c_fake.h"
#include "test_support.h"

#include <string.h>

/*
 * libcomm_interface.c — the protocol dispatcher that sits between the I2C
 * driver and a board.
 *
 * These tests drive it from both ends over the fake driver: frames are pushed
 * in as if another master had written them, and outbound comm_send_* calls are
 * read back off the fake's submission log.  What is being checked is the
 * routing and the failure behaviour, not the framing (test_libcomm covers
 * that) — in particular the two rules that keep a corrupt bus from reaching
 * the board: a frame that fails comm_can_parse reaches nobody, and a failed
 * read hands the board a NULL payload rather than stale bytes.
 */

#define SELF COMM_ADDRESS_MAIN
#define PEER COMM_ADDRESS_SWITCHING

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    i2c_fake_reset();
    adopter_reset();
    comm_interface_init();
    return NULL;
}

/* Push a well-formed frame in through the client-write path and drain. */
static void deliver(uint8_t id, const uint8_t* payload, uint8_t len) {
    uint8_t frame[16];
    uint8_t n = i2c_fake_frame(frame, id, payload, len);
    i2c_fake_deliver_write(frame, n);
    i2c_poll();
}

/* ── Inbound writes ───────────────────────────────────────────────────── */

static MunitResult test_routes_relay_state(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t payload[] = {0x0F, 0xF0};
    deliver(COMM_RELAY_STATE, payload, 2);

    const CallbackRecord* r = adopter_find(CB_RELAY_STATE_RECEIVED);
    assert_not_null((void*)r);
    assert_uint16(r->u16[0], ==, 0xF00F);
    return MUNIT_OK;
}

static MunitResult test_routes_channel_changed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t payload[] = {PEER, 0x01, 0x00, 0x03, 0x80, 0x01, 0x05};
    deliver(COMM_CHANNEL_CHANGED, payload, 7);

    const CallbackRecord* r = adopter_find(CB_CHANNEL_CHANGED_RECEIVED);
    assert_not_null((void*)r);
    assert_uint8(r->addr, ==, PEER);
    assert_uint16(r->u16[0], ==, 0x0001);
    assert_uint16(r->u16[1], ==, 0x8003);
    assert_uint8(r->u8[0], ==, 0x01);
    assert_uint8(r->u8[1], ==, 0x05);
    return MUNIT_OK;
}

static MunitResult test_routes_reset_and_config(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    deliver(COMM_RESET, NULL, 0);
    assert_not_null((void*)adopter_find(CB_RESET));

    uint8_t cfg[] = {0x12, 0x77};
    deliver(COMM_CONFIG, cfg, 2);
    const CallbackRecord* r = adopter_find(CB_CONFIG_RECEIVED);
    assert_not_null((void*)r);
    assert_uint8(r->u8[0], ==, 0x12);
    assert_uint8(r->u8[1], ==, 0x77);
    return MUNIT_OK;
}

/*
 * A frame the CRC rejects must not reach any callback.  This is the boundary
 * that stops bus noise from being interpreted as, say, a relay command.
 */
static MunitResult test_corrupt_frame_reaches_nobody(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t payload[] = {0xFF, 0xFF};
    uint8_t frame[8];
    uint8_t n = i2c_fake_frame(frame, COMM_RELAY_STATE, payload, 2);

    frame[n - 1u] ^= 0x01; /* break the CRC */
    i2c_fake_deliver_write(frame, n);
    i2c_poll();
    assert_uint8(adopter_log_count, ==, 0);

    /* Truncated frame, valid CRC over what remains. */
    frame[2] = test_crc8(frame, 2);
    i2c_fake_deliver_write(frame, 3);
    i2c_poll();
    assert_uint8(adopter_log_count, ==, 0);

    /* Unknown command id. */
    n = i2c_fake_frame(frame, 0x7F, payload, 2);
    i2c_fake_deliver_write(frame, n);
    i2c_poll();
    assert_uint8(adopter_log_count, ==, 0);
    return MUNIT_OK;
}

/* test_echo is answered by the dispatcher itself — no board callback exists
 * for it, and the reply must carry *this* device's address so the requester
 * learns who answered. */
static MunitResult test_echo_is_answered_by_the_dispatcher(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t payload[] = {PEER, 0x5A};
    deliver(COMM_TEST_ECHO, payload, 2);

    assert_uint8(adopter_log_count, ==, 0); /* no board involvement */
    const I2cFakeTx* tx = i2c_fake_last_tx();
    assert_not_null((void*)tx);
    assert_uint8(tx->addr, ==, PEER);
    assert_frame(tx->tx, tx->tx_len, COMM_TEST_ECHO_RESPONSE, SELF, 0x5A);
    return MUNIT_OK;
}

/* ── Inbound read requests (synchronous path) ─────────────────────────── */

static MunitResult test_read_request_stages_reply_synchronously(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    adopter_response[0] = 0x11;
    adopter_response[1] = 0x22;
    adopter_response_len = 2;

    uint8_t frame[8];
    uint8_t n = i2c_fake_frame(frame, COMM_BATTERY_READ, NULL, 0);
    uint8_t staged = i2c_fake_deliver_read_request(frame, n);

    /* Staged before returning — on the device the read-phase address arrives
     * microseconds later, long before the main loop runs again. */
    assert_uint8(staged, ==, 3); /* 2 payload + CRC */
    assert_not_null((void*)adopter_find(CB_BATTERY_REQUESTED));

    uint8_t len = 0;
    const uint8_t* reply = i2c_fake_client_tx(&len);
    assert_not_null((void*)reply);
    assert_uint8(len, ==, 3);
    assert_uint8(reply[0], ==, 0x11);
    assert_uint8(reply[1], ==, 0x22);
    assert_uint8(reply[2], ==, test_crc8(reply, 2));
    return MUNIT_OK;
}

static MunitResult test_read_request_carries_its_argument(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    adopter_response[0] = 0x99;
    adopter_response_len = 1;

    uint8_t addr = 0x2A;
    uint8_t frame[8];
    uint8_t n = i2c_fake_frame(frame, COMM_CONFIG_READ, &addr, 1);
    i2c_fake_deliver_read_request(frame, n);

    const CallbackRecord* r = adopter_find(CB_CONFIG_REQUESTED);
    assert_not_null((void*)r);
    assert_uint8(r->u8[0], ==, 0x2A);
    return MUNIT_OK;
}

/*
 * If the board declines to respond, nothing must be staged.  On the wire the
 * driver then NACKs the read-phase address, which is a clean failure the
 * master can retry — as opposed to clocking out whatever the previous
 * transaction left in the buffer.
 */
static MunitResult test_declined_read_stages_nothing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* First a successful reply, so there is something stale to leak. */
    adopter_response[0] = 0xAB;
    adopter_response_len = 1;
    uint8_t frame[8];
    uint8_t n = i2c_fake_frame(frame, COMM_SENSORS_READ, NULL, 0);
    assert_uint8(i2c_fake_deliver_read_request(frame, n), ==, 2);

    adopter_response_len = 0; /* board has nothing to say this time */
    assert_uint8(i2c_fake_deliver_read_request(frame, n), ==, 0);

    uint8_t len = 0;
    assert_null((void*)i2c_fake_client_tx(&len));
    assert_uint8(len, ==, 0);
    return MUNIT_OK;
}

/* A corrupt read request must not stage a reply either — otherwise a noisy
 * bus could make the device answer a request nobody made. */
static MunitResult test_corrupt_read_request_stages_nothing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    adopter_response[0] = 0xAB;
    adopter_response_len = 1;

    uint8_t frame[8];
    uint8_t n = i2c_fake_frame(frame, COMM_BATTERY_READ, NULL, 0);
    frame[n - 1u] ^= 0xFF;
    assert_uint8(i2c_fake_deliver_read_request(frame, n), ==, 0);
    assert_uint8(adopter_log_count, ==, 0);
    return MUNIT_OK;
}

/* test_read is self-serviced: the read phase echoes the write-phase value,
 * with no board callback in the path.  It is the one command that proves the
 * whole write-then-read round trip without involving board state. */
static MunitResult test_test_read_echoes_itself(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t value = 0x6C;
    uint8_t frame[8];
    uint8_t n = i2c_fake_frame(frame, COMM_TEST_READ, &value, 1);
    assert_uint8(i2c_fake_deliver_read_request(frame, n), ==, 2);
    assert_uint8(adopter_log_count, ==, 0);

    uint8_t len = 0;
    const uint8_t* reply = i2c_fake_client_tx(&len);
    assert_uint8(len, ==, 2);
    assert_uint8(reply[0], ==, 0x6C);
    assert_uint8(reply[1], ==, test_crc8(reply, 1));
    return MUNIT_OK;
}

/* ── Outbound writes ──────────────────────────────────────────────────── */

static MunitResult test_send_builds_correct_frames(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_int(comm_send_relay_state(0x0102), ==, I2C_RESULT_OK);
    const I2cFakeTx* tx = i2c_fake_last_tx();
    assert_uint8(tx->addr, ==, COMM_ADDRESS_SWITCHING);
    assert_uint8(tx->rx_len, ==, 0);
    assert_frame(tx->tx, tx->tx_len, COMM_RELAY_STATE, 0x02, 0x01);

    assert_int(comm_send_config(PEER, 0x10, 0x64), ==, I2C_RESULT_OK);
    tx = i2c_fake_last_tx();
    assert_uint8(tx->addr, ==, PEER);
    assert_frame(tx->tx, tx->tx_len, COMM_CONFIG, 0x10, 0x64);

    assert_int(comm_send_reset(PEER), ==, I2C_RESULT_OK);
    tx = i2c_fake_last_tx();
    assert_frame(tx->tx, tx->tx_len, COMM_RESET);
    return MUNIT_OK;
}

static MunitResult test_send_reports_queue_full(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_submit_result(I2C_RESULT_QUEUE_FULL);
    assert_int(comm_send_relay_state(0x0001), ==, I2C_RESULT_QUEUE_FULL);
    assert_uint8(i2c_fake_tx_count(), ==, 0);

    /* Retryable, not fatal: the same call succeeds once the queue drains. */
    i2c_fake_set_submit_result(I2C_RESULT_OK);
    assert_int(comm_send_relay_state(0x0001), ==, I2C_RESULT_OK);
    return MUNIT_OK;
}

/* Completions are reconstructed from the queued TX bytes, so what the board
 * sees is what actually went on the wire. */
static MunitResult test_write_completion_reports_transmitted_args(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    comm_send_config(PEER, 0x33, 0x44);
    i2c_poll();

    const CallbackRecord* r = adopter_find(CB_CONFIG_COMPLETION);
    assert_not_null((void*)r);
    assert_int(r->result, ==, I2C_RESULT_OK);
    assert_uint8(r->addr, ==, PEER);
    assert_uint8(r->u8[0], ==, 0x33);
    assert_uint8(r->u8[1], ==, 0x44);
    return MUNIT_OK;
}

static MunitResult test_write_completion_reports_failure(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_autocomplete_writes(0);
    comm_send_relay_state(0x00FF);
    i2c_fake_respond(0, I2C_RESULT_NACK, NULL, 0);
    i2c_poll();

    const CallbackRecord* r = adopter_find(CB_RELAY_STATE_COMPLETION);
    assert_not_null((void*)r);
    assert_int(r->result, ==, I2C_RESULT_NACK);
    assert_uint16(r->u16[0], ==, 0x00FF);
    return MUNIT_OK;
}

/* ── Outbound reads ───────────────────────────────────────────────────── */

static MunitResult test_read_request_shape(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_int(comm_send_battery_read(), ==, I2C_RESULT_OK);
    const I2cFakeTx* tx = i2c_fake_last_tx();
    assert_uint8(tx->addr, ==, COMM_ADDRESS_SWITCHING);
    assert_uint8(tx->rx_len, ==, 3); /* 2-byte payload + CRC */
    assert_frame(tx->tx, tx->tx_len, COMM_BATTERY_READ);

    assert_int(comm_send_level_mode_read(), ==, I2C_RESULT_OK);
    tx = i2c_fake_last_tx();
    assert_uint8(tx->rx_len, ==, 2); /* 1-byte payload + CRC */
    return MUNIT_OK;
}

static MunitResult test_read_response_is_parsed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    comm_send_battery_read();
    uint8_t payload[] = {0x9C, 0x30}; /* 12444 mV, little-endian */
    i2c_fake_respond_to(COMM_BATTERY_READ, payload, 2);
    i2c_poll();

    const CallbackRecord* r = adopter_find(CB_BATTERY_RESPONSE);
    assert_not_null((void*)r);
    assert_int(r->result, ==, I2C_RESULT_OK);
    assert_uint8(r->had_payload, ==, 1);
    assert_uint16(r->u16[0], ==, 12444);
    return MUNIT_OK;
}

/*
 * Every failure mode must reach the board as (non-OK result, NULL payload).
 * A board that only checks the pointer, or only checks the result, is then
 * still safe.
 */
static MunitResult test_read_failures_yield_null_payload(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;

    /* 1. Bus-level failure. */
    comm_send_levels_read();
    i2c_fake_respond(i2c_fake_find(COMM_LEVELS_READ), I2C_RESULT_TIMEOUT, NULL, 0);
    i2c_poll();
    const CallbackRecord* r = adopter_find(CB_LEVELS_RESPONSE);
    assert_not_null((void*)r);
    assert_int(r->result, ==, I2C_RESULT_TIMEOUT);
    assert_uint8(r->had_payload, ==, 0);

    /* 2. Bus said OK but the CRC does not check out. */
    adopter_reset();
    comm_send_levels_read();
    uint8_t bad[] = {11, 22, 0x00};
    bad[2] = (uint8_t)(test_crc8(bad, 2) ^ 0xFF);
    i2c_fake_respond(i2c_fake_find(COMM_LEVELS_READ), I2C_RESULT_OK, bad, 3);
    i2c_poll();
    r = adopter_find(CB_LEVELS_RESPONSE);
    assert_not_null((void*)r);
    assert_int(r->result, ==, I2C_RESULT_BAD_CRC);
    assert_uint8(r->had_payload, ==, 0);

    /* 3. Right CRC, wrong length — a short read whose trailing byte happens
     *    to validate must still be refused. */
    adopter_reset();
    comm_send_levels_read();
    uint8_t shortr[] = {11, 0x00};
    shortr[1] = test_crc8(shortr, 1);
    i2c_fake_respond(i2c_fake_find(COMM_LEVELS_READ), I2C_RESULT_OK, shortr, 2);
    i2c_poll();
    r = adopter_find(CB_LEVELS_RESPONSE);
    assert_not_null((void*)r);
    assert_int(r->result, ==, I2C_RESULT_BAD_CRC);
    assert_uint8(r->had_payload, ==, 0);

    /* 4. Nothing came back at all. */
    adopter_reset();
    comm_send_levels_read();
    i2c_fake_respond(i2c_fake_find(COMM_LEVELS_READ), I2C_RESULT_OK, NULL, 0);
    i2c_poll();
    r = adopter_find(CB_LEVELS_RESPONSE);
    assert_not_null((void*)r);
    assert_uint8(r->had_payload, ==, 0);
    return MUNIT_OK;
}

/* The per-address reads must report which device answered, since the same
 * callback serves both button boards. */
static MunitResult test_addressed_read_reports_its_peer(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    comm_send_button_state_read(COMM_ADDRESS_BUTTON_BOARD_R);
    uint8_t payload[] = {0x2A};
    i2c_fake_respond_to(COMM_BUTTON_STATE_READ, payload, 1);
    i2c_poll();

    const CallbackRecord* r = adopter_find(CB_BUTTON_STATE_RESPONSE);
    assert_not_null((void*)r);
    assert_uint8(r->addr, ==, COMM_ADDRESS_BUTTON_BOARD_R);
    assert_uint8(r->u8[0], ==, 0x2A);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/routes_relay_state", test_routes_relay_state),
    T("/routes_channel_changed", test_routes_channel_changed),
    T("/routes_reset_and_config", test_routes_reset_and_config),
    T("/corrupt_frame_reaches_nobody", test_corrupt_frame_reaches_nobody),
    T("/echo_is_answered_by_the_dispatcher", test_echo_is_answered_by_the_dispatcher),
    T("/read_request_stages_reply_synchronously", test_read_request_stages_reply_synchronously),
    T("/read_request_carries_its_argument", test_read_request_carries_its_argument),
    T("/declined_read_stages_nothing", test_declined_read_stages_nothing),
    T("/corrupt_read_request_stages_nothing", test_corrupt_read_request_stages_nothing),
    T("/test_read_echoes_itself", test_test_read_echoes_itself),
    T("/send_builds_correct_frames", test_send_builds_correct_frames),
    T("/send_reports_queue_full", test_send_reports_queue_full),
    T("/write_completion_reports_transmitted_args", test_write_completion_reports_transmitted_args),
    T("/write_completion_reports_failure", test_write_completion_reports_failure),
    T("/read_request_shape", test_read_request_shape),
    T("/read_response_is_parsed", test_read_response_is_parsed),
    T("/read_failures_yield_null_payload", test_read_failures_yield_null_payload),
    T("/addressed_read_reports_its_peer", test_addressed_read_reports_its_peer),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite interface_suite(void) {
    MunitSuite s = {"/interface", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
