#include "button.h"
#include "comm.h"
#include "config.h"
#include "i2c_board.h"
#include "i2c_fake.h"
#include "input.h"
#include "led_effect.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"
#include "test_support.h"

#include <string.h>
#include <xc.h>

/*
 * The button board as a peer on the bus: its identity, its inputs, its
 * persisted configuration, and the traffic it exchanges with the main board.
 */

void IOC_ISR(void);

#define OFF_MAGIC_LO 0x00
#define OFF_MAGIC_HI 0x01
#define OFF_BUTTONS 0x02
#define OFF_EFFECTS (OFF_BUTTONS + BUTTON_COUNT)
#define OFF_BRIGHT (OFF_EFFECTS + 4)
#define MAGIC_LO 0x5A
#define MAGIC_HI 0xA5

static const uint8_t button_pin[BUTTON_COUNT] = {7, 6, 0, 1, 2, 3, 4};

static TaskController ctrl;

static void boot(void) {
    test_reset_hardware();
    i2c_fake_reset();
    comm_interface_init();
    task_controller_init(&ctrl);
    PORTA = 0xFF;
    i2c_pins_init();
    input_init(&ctrl);
    config_init(&ctrl);
    led_effect_init(&ctrl);
    comm_init(&ctrl);
    button_init(&ctrl);
}

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    boot();
    return NULL;
}

static void deliver(uint8_t id, const uint8_t* payload, uint8_t len) {
    uint8_t frame[16];
    uint8_t n = i2c_fake_frame(frame, id, payload, len);
    i2c_fake_deliver_write(frame, n);
    i2c_poll();
}

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

/* ── Identity ─────────────────────────────────────────────────────────── */

/*
 * Both button boards run the same binary; which one this is comes from a
 * strap on RB0.  If the strap were misread, two boards would answer the same
 * address and the bus would collide on every poll.
 */
static MunitResult test_address_comes_from_the_strap(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    PORTBbits.RB0 = 0;
    assert_uint8(comm_address(), ==, COMM_ADDRESS_BUTTON_BOARD_L);
    PORTBbits.RB0 = 1;
    assert_uint8(comm_address(), ==, COMM_ADDRESS_BUTTON_BOARD_R);
    return MUNIT_OK;
}

static MunitResult test_strap_pin_is_an_input_with_a_pullup(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Left is the strapped-low variant, so an unstrapped board must float
     * high rather than be ambiguous. */
    assert_uint8(TRISBbits.TRISB0, ==, 1);
    assert_uint8(WPUBbits.WPUB0, ==, 1);
    assert_uint8(ANSELBbits.ANSELB0, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_i2c_pins_are_open_drain(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* An I2C line driven push-pull would fight the other master on the bus. */
    assert_uint8(ODCONCbits.ODCC3, ==, 1);
    assert_uint8(ODCONCbits.ODCC4, ==, 1);
    assert_uint8(TRISCbits.TRISC3, ==, 0);
    assert_uint8(TRISCbits.TRISC4, ==, 0);
    assert_uint8(I2C1SCLPPS, ==, 0x13);
    assert_uint8(I2C1SDAPPS, ==, 0x14);
    assert_uint8(RC3PPS, ==, 0x37);
    assert_uint8(RC4PPS, ==, 0x38);
    return MUNIT_OK;
}

/* ── Inputs ───────────────────────────────────────────────────────────── */

/* Buttons pull to ground, so the sampled byte is the inverse of the port. */
static MunitResult test_input_is_active_low(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    PORTA = 0xFF;
    IOC_ISR();
    assert_uint8(input_state_current().integer, ==, 0x00);

    PORTA = 0x00;
    IOC_ISR();
    assert_uint8(input_state_current().integer, ==, 0xFF);
    return MUNIT_OK;
}

/* Each logical button must map to its own physical pin — a transposed pair
 * makes two buttons swap functions. */
static MunitResult test_each_button_maps_to_its_pin(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint8_t id = 0; id < BUTTON_COUNT; id++) {
        PORTA = (uint8_t)~(1u << button_pin[id]);
        IOC_ISR();
        assert_uint8(input_state_current().integer, ==, (uint8_t)(1u << id));
    }
    return MUNIT_OK;
}

static uint8_t probe_calls;
static uint8_t probe_prev;
static uint8_t probe_curr;

static void probe(uint8_t prev, uint8_t curr) {
    probe_calls++;
    probe_prev = prev;
    probe_curr = curr;
}

/*
 * The interrupt captures the pins and hands dispatch to the main loop, so a
 * slow listener cannot stall it.  The listener is also given both the old and
 * the new byte, which is what lets button.c work out which pins moved.
 */
static MunitResult test_dispatch_is_deferred_to_main_context(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    probe_calls = 0;
    input_set_change_handler(probe);

    PORTA = (uint8_t)~(1u << button_pin[0]);
    IOC_ISR();
    assert_uint8(input_state_current().integer, ==, 0x01); /* visible at once */
    assert_uint8(probe_calls, ==, 0);                      /* but not dispatched */

    test_advance_ms(&ctrl, 1);
    assert_uint8(probe_calls, ==, 1);
    assert_uint8(probe_prev, ==, 0x00);
    assert_uint8(probe_curr, ==, 0x01);

    /* An interrupt that changes nothing must not produce a dispatch — the
     * queue is lossy, so spurious entries would crowd out real edges. */
    IOC_ISR();
    test_advance_ms(&ctrl, 1);
    assert_uint8(probe_calls, ==, 1);

    input_set_change_handler(0);
    return MUNIT_OK;
}

/* ── Persisted configuration ──────────────────────────────────────────── */

static MunitResult test_virgin_device_seeds_defaults(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint8(pic_eeprom_get(OFF_MAGIC_LO), ==, MAGIC_LO);
    assert_uint8(pic_eeprom_get(OFF_MAGIC_HI), ==, MAGIC_HI);
    /* Half intensity, matching the main board's indicator default so a
     * freshly flashed panel is uniform. */
    assert_uint8(pic_eeprom_get(OFF_BRIGHT), ==, 0x10);
    assert_uint8(config_get_led_brightness(), ==, 0x10);
    return MUNIT_OK;
}

static MunitResult test_button_triggers_persist(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Flushed one at a time: the deferred-write ring holds three entries, so
     * queueing all seven at once would silently drop the tail.  That is by
     * design (writes are idempotent and the host retries), but it means a
     * caller cannot treat config_write_byte as a bulk operation. */
    for (uint8_t id = 0; id < BUTTON_COUNT; id++) {
        config_set_button(id, comm_button_trigger_make(COMM_BUTTON_MODE_RELEASE, (uint16_t)(id * 100u)));
        test_advance_ms(&ctrl, 2);
    }

    for (uint8_t id = 0; id < BUTTON_COUNT; id++) {
        CommTriggerConfig cfg = config_get_button(id);
        assert_uint8(cfg.mode, ==, COMM_BUTTON_MODE_RELEASE);
        assert_uint16(comm_button_trigger_time_ms(cfg), ==, (uint16_t)(id * 100u));
        /* And each landed in its own cell. */
        assert_uint8(pic_eeprom_get(OFF_BUTTONS + id), ==, *(uint8_t*)&cfg);
    }
    return MUNIT_OK;
}

/*
 * Effects are stored two per byte in the same packed layout the wire uses.
 * Writing one must not disturb its neighbour in the shared byte.
 */
static MunitResult test_effects_share_bytes_without_clobbering(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint8_t id = 0; id < LED_EFFECT_COUNT; id++) {
        CommButtonOutputEffect e = {0};
        e.color = (uint8_t)(id & 3u);
        e.mode = (uint8_t)((id >> 1) & 3u);
        config_set_effect(id, e);
        test_advance_ms(&ctrl, 2);
    }
    for (uint8_t id = 0; id < LED_EFFECT_COUNT; id++) {
        CommButtonOutputEffect got = config_get_effect(id);
        assert_uint8(got.color, ==, (uint8_t)(id & 3u));
        assert_uint8(got.mode, ==, (uint8_t)((id >> 1) & 3u));
    }
    return MUNIT_OK;
}

static MunitResult test_unmapped_addresses_are_inert(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint8(config_read_byte(0x50), ==, 0xFF);
    const unsigned before = pic_eeprom_writes;
    config_write_byte(0x50, 0x11);
    test_advance_ms(&ctrl, 10);
    assert_uint(pic_eeprom_writes, ==, before);
    return MUNIT_OK;
}

/* ── Inbound commands ─────────────────────────────────────────────────── */

static MunitResult test_button_trigger_command_configures_a_button(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommTriggerConfig cfg = comm_button_trigger_make(COMM_BUTTON_MODE_HOLD, 1000);
    const uint8_t payload[2] = {4, *(uint8_t*)&cfg};
    deliver(COMM_BUTTON_TRIGGER, payload, 2);

    CommTriggerConfig got = button_get_trigger(4);
    assert_uint8(got.mode, ==, COMM_BUTTON_MODE_HOLD);
    assert_uint16(comm_button_trigger_time_ms(got), ==, 1000);
    return MUNIT_OK;
}

static MunitResult test_button_effect_command_reaches_the_leds(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommButtonEffect eff;
    comm_button_effect_init(&eff);
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        CommButtonOutputEffect e = {0};
        e.color = COMM_EFFECT_COLOR_BLUE;
        e.mode = COMM_EFFECT_MODE_PULSATING;
        comm_button_effect_set(&eff, i, e);
    }
    deliver(COMM_BUTTON_EFFECT, (const uint8_t*)&eff, 4);

    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        assert_uint8(led_effect_get(i).color, ==, COMM_EFFECT_COLOR_BLUE);
        assert_uint8(led_effect_get(i).mode, ==, COMM_EFFECT_MODE_PULSATING);
    }
    return MUNIT_OK;
}

static MunitResult test_config_command_is_persisted(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t payload[2] = {CONFIG_ADDR_LED_BRIGHTNESS, 0x40};
    deliver(COMM_CONFIG, payload, 2);
    test_advance_ms(&ctrl, 10);
    assert_uint8(config_get_led_brightness(), ==, 0x40);
    assert_uint8(pic_eeprom_get(OFF_BRIGHT), ==, 0x40);
    return MUNIT_OK;
}

static MunitResult test_reset_command_reboots(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    deliver(COMM_RESET, NULL, 0);
    assert_uint(pic_reset_count, ==, 1);
    return MUNIT_OK;
}

/* ── Read requests ────────────────────────────────────────────────────── */

static MunitResult test_button_state_read_returns_the_live_pins(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    PORTA = (uint8_t)~((1u << button_pin[0]) | (1u << button_pin[3]));
    IOC_ISR();

    uint8_t reply[8];
    assert_uint8(read_request(COMM_BUTTON_STATE_READ, NULL, 0, reply), ==, 1);
    assert_uint8(reply[0], ==, (uint8_t)((1u << 0) | (1u << 3)));
    return MUNIT_OK;
}

static MunitResult test_button_trigger_read_returns_the_configured_trigger(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommTriggerConfig cfg = comm_button_trigger_make(COMM_BUTTON_MODE_CHANGE, 0);
    button_set_trigger(2, cfg);

    const uint8_t arg = 2;
    uint8_t reply[8];
    assert_uint8(read_request(COMM_BUTTON_TRIGGER_READ, &arg, 1, reply), ==, 1);
    assert_uint8(reply[0], ==, *(uint8_t*)&cfg);
    return MUNIT_OK;
}

static MunitResult test_config_read_returns_the_device_id(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t arg = COMM_CONFIG_DEVICE_ID;
    uint8_t reply[8];
    assert_uint8(read_request(COMM_CONFIG_READ, &arg, 1, reply), ==, 1);
    assert_uint8(reply[0], ==, comm_address());
    return MUNIT_OK;
}

/* ── Outbound queue ───────────────────────────────────────────────────── */

/*
 * Button events are queued and drained by a task, so a press that lands while
 * the bus is busy is delayed rather than lost.  A dropped press means a switch
 * the user pressed and nothing happened — the failure mode most likely to be
 * blamed on the hardware.
 */
static MunitResult test_events_are_held_until_the_bus_frees_up(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_submit_result(I2C_RESULT_QUEUE_FULL);

    comm_send_button_event(1, 1, COMM_BUTTON_MODE_CHANGE);
    comm_send_button_event(2, 1, COMM_BUTTON_MODE_CHANGE);
    test_advance_ms(&ctrl, 10);
    assert_uint8(i2c_fake_tx_count(), ==, 0);

    i2c_fake_set_submit_result(I2C_RESULT_OK);
    test_advance_ms(&ctrl, 2);
    assert_uint8(i2c_fake_tx_count(), ==, 2);

    /* Order preserved — press and release must not be transposed. */
    CommButtonChanged first;
    comm_parse_button_changed((uint8_t*)i2c_fake_tx(0)->tx + 1, &first);
    assert_uint8(first.button_id, ==, 1);
    return MUNIT_OK;
}

/* The queue is bounded; beyond it events are dropped rather than overwriting
 * ones already waiting. */
static MunitResult test_event_queue_is_bounded(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_submit_result(I2C_RESULT_QUEUE_FULL);
    for (uint8_t i = 0; i < 20; i++) {
        comm_send_button_event((uint8_t)(i & 7u), 1, COMM_BUTTON_MODE_CHANGE);
    }
    i2c_fake_set_submit_result(I2C_RESULT_OK);
    test_advance_ms(&ctrl, 5);

    assert_uint8(i2c_fake_tx_count(), <=, 7); /* ring of 8, one slot reserved */
    assert_uint8(i2c_fake_tx_count(), >, 0);
    /* The oldest survived, which is what keeps the sequence coherent. */
    CommButtonChanged first;
    comm_parse_button_changed((uint8_t*)i2c_fake_tx(0)->tx + 1, &first);
    assert_uint8(first.button_id, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_event_is_addressed_to_the_main_board(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    comm_send_button_event(6, 0, COMM_BUTTON_MODE_RELEASE);
    test_advance_ms(&ctrl, 2);

    const I2cFakeTx* tx = i2c_fake_last_tx();
    assert_uint8(tx->addr, ==, COMM_ADDRESS_MAIN);
    assert_uint8(tx->rx_len, ==, 0);

    CommButtonChanged ev;
    comm_parse_button_changed((uint8_t*)tx->tx + 1, &ev);
    assert_uint8(ev.device_address, ==, comm_address());
    assert_uint8(ev.button_id, ==, 6);
    assert_uint8(ev.pressed, ==, 0);
    assert_uint8(ev.mode, ==, COMM_BUTTON_MODE_RELEASE);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/address_comes_from_the_strap", test_address_comes_from_the_strap),
    T("/strap_pin_is_an_input_with_a_pullup", test_strap_pin_is_an_input_with_a_pullup),
    T("/i2c_pins_are_open_drain", test_i2c_pins_are_open_drain),
    T("/input_is_active_low", test_input_is_active_low),
    T("/each_button_maps_to_its_pin", test_each_button_maps_to_its_pin),
    T("/dispatch_is_deferred_to_main_context", test_dispatch_is_deferred_to_main_context),
    T("/virgin_device_seeds_defaults", test_virgin_device_seeds_defaults),
    T("/button_triggers_persist", test_button_triggers_persist),
    T("/effects_share_bytes_without_clobbering", test_effects_share_bytes_without_clobbering),
    T("/unmapped_addresses_are_inert", test_unmapped_addresses_are_inert),
    T("/button_trigger_command_configures_a_button", test_button_trigger_command_configures_a_button),
    T("/button_effect_command_reaches_the_leds", test_button_effect_command_reaches_the_leds),
    T("/config_command_is_persisted", test_config_command_is_persisted),
    T("/reset_command_reboots", test_reset_command_reboots),
    T("/button_state_read_returns_the_live_pins", test_button_state_read_returns_the_live_pins),
    T("/button_trigger_read_returns_the_configured_trigger", test_button_trigger_read_returns_the_configured_trigger),
    T("/config_read_returns_the_device_id", test_config_read_returns_the_device_id),
    T("/events_are_held_until_the_bus_frees_up", test_events_are_held_until_the_bus_frees_up),
    T("/event_queue_is_bounded", test_event_queue_is_bounded),
    T("/event_is_addressed_to_the_main_board", test_event_is_addressed_to_the_main_board),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b2_comm_suite(void) {
    MunitSuite s = {"/board2/comm", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
