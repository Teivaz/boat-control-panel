#include "button.h"
#include "comm.h"
#include "i2c_fake.h"
#include "input.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"
#include "test_support.h"

#include <xc.h>

/*
 * The per-button trigger state machine.
 *
 * Each of the seven buttons runs one of three behaviours, chosen by the main
 * board at runtime:
 *
 *   CHANGE   fire on every edge, press and release alike
 *   HOLD     fire once, t ms after the press, and not again until released
 *   RELEASE  fire on release, but only if the button was held at least t ms
 *
 * The awkward cases are what these tests are for: a release that arrives
 * before the timer, a mode changed while a timer is armed, a stray second
 * press, and t = 0 collapsing the waiting state away entirely.  Each of those
 * either fires an event the user did not ask for or swallows one they did, and
 * on this panel an event is a circuit switching.
 *
 * Events are observed where they actually leave the board: as button_changed
 * frames handed to the I2C driver.
 */

/* input.c samples the pins inverted (pressed = pulled low), so the byte the
 * IOC handler publishes has a 1 where a button is down. */
void IOC_ISR(void);

/* Physical pin per logical button, from input.c. */
static const uint8_t button_pin[BUTTON_COUNT] = {7, 6, 0, 1, 2, 3, 4};

static TaskController ctrl;

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    i2c_fake_reset();
    comm_interface_init();
    task_controller_init(&ctrl);
    PORTA = 0xFF; /* pull-ups: nothing pressed */
    input_init(&ctrl);
    comm_init(&ctrl);
    button_init(&ctrl);
    return NULL;
}

/* Press or release a button and let the edge propagate to the FSM.  input.c
 * defers dispatch to the main loop, so one poll is needed before the button
 * module has seen it. */
static void press(uint8_t id, uint8_t down) {
    if (down) {
        PORTA &= (uint8_t)~(1u << button_pin[id]);
    } else {
        PORTA |= (uint8_t)(1u << button_pin[id]);
    }
    IOC_ISR();
    test_advance_ms(&ctrl, 1);
}

static void set_mode(uint8_t id, CommButtonMode mode, uint16_t ms) {
    button_set_trigger(id, comm_button_trigger_make(mode, ms));
}

/*
 * Let a queued event reach the bus.
 *
 * comm_send_button_event only enqueues; comm.c's retry task drains the queue
 * on the next poll.  An event raised from an *edge* goes out in the same poll,
 * because deferred callbacks run before periodic tasks — but one raised by a
 * button *timer* is itself a periodic task, and the retry task occupies an
 * earlier slot, so it goes out one tick later.  A couple of milliseconds
 * covers both without pinning the test to slot ordering.
 */
static void settle(void) {
    test_advance_ms(&ctrl, 2);
}

/* Outbound button_changed frames, decoded. */
static uint8_t events(void) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < i2c_fake_tx_count(); i++) {
        if (i2c_fake_tx(i)->tx[0] == COMM_BUTTON_CHANGED) {
            n++;
        }
    }
    return n;
}

static CommButtonChanged last_event(void) {
    CommButtonChanged ev = {0};
    for (uint8_t i = i2c_fake_tx_count(); i > 0; i--) {
        const I2cFakeTx* tx = i2c_fake_tx((uint8_t)(i - 1u));
        if (tx->tx[0] == COMM_BUTTON_CHANGED) {
            comm_parse_button_changed((uint8_t*)tx->tx + 1, &ev);
            return ev;
        }
    }
    munit_error("no button_changed was sent");
    return ev;
}

/* ── Configuration ────────────────────────────────────────────────────── */

static MunitResult test_unconfigured_button_is_silent(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Mode UNKNOWN is the power-on state, before the main board has told the
     * panel what each button does.  Pressing must do nothing rather than
     * guess. */
    press(0, 1);
    press(0, 0);
    test_advance_ms(&ctrl, 100);
    assert_uint8(events(), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_trigger_round_trips(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint8_t id = 0; id < BUTTON_COUNT; id++) {
        set_mode(id, COMM_BUTTON_MODE_HOLD, 500);
        CommTriggerConfig got = button_get_trigger(id);
        assert_uint8(got.mode, ==, COMM_BUTTON_MODE_HOLD);
        assert_uint16(comm_button_trigger_time_ms(got), ==, 500);
    }
    /* Out-of-range ids are ignored rather than corrupting a neighbour. */
    set_mode(BUTTON_COUNT, COMM_BUTTON_MODE_CHANGE, 10);
    assert_uint8(button_get_trigger(0).mode, ==, COMM_BUTTON_MODE_HOLD);
    return MUNIT_OK;
}

/* ── CHANGE ───────────────────────────────────────────────────────────── */

static MunitResult test_change_fires_on_both_edges(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(2, COMM_BUTTON_MODE_CHANGE, 0);

    press(2, 1);
    assert_uint8(events(), ==, 1);
    CommButtonChanged ev = last_event();
    assert_uint8(ev.button_id, ==, 2);
    assert_uint8(ev.pressed, ==, 1);
    assert_uint8(ev.mode, ==, COMM_BUTTON_MODE_CHANGE);
    assert_uint8(ev.device_address, ==, comm_address());

    press(2, 0);
    assert_uint8(events(), ==, 2);
    assert_uint8(last_event().pressed, ==, 0);
    return MUNIT_OK;
}

/* CHANGE ignores the time field — there is no waiting state to arm. */
static MunitResult test_change_ignores_the_time(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(2, COMM_BUTTON_MODE_CHANGE, 5000);
    press(2, 1);
    assert_uint8(events(), ==, 1);
    return MUNIT_OK;
}

/* ── HOLD ─────────────────────────────────────────────────────────────── */

static MunitResult test_hold_fires_after_the_delay(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(1, COMM_BUTTON_MODE_HOLD, 500);

    press(1, 1);
    assert_uint8(events(), ==, 0);
    test_advance_ms(&ctrl, 498); /* still short of the 500 ms deadline */
    assert_uint8(events(), ==, 0);
    test_advance_ms(&ctrl, 2);
    settle();
    assert_uint8(events(), ==, 1);

    CommButtonChanged ev = last_event();
    assert_uint8(ev.button_id, ==, 1);
    assert_uint8(ev.pressed, ==, 1); /* a hold reports as a press */
    assert_uint8(ev.mode, ==, COMM_BUTTON_MODE_HOLD);
    return MUNIT_OK;
}

/* Releasing early cancels: the user changed their mind before the hold
 * completed, and nothing should happen. */
static MunitResult test_hold_released_early_fires_nothing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(1, COMM_BUTTON_MODE_HOLD, 500);

    press(1, 1);
    test_advance_ms(&ctrl, 200);
    press(1, 0);
    test_advance_ms(&ctrl, 1000);
    assert_uint8(events(), ==, 0);

    /* And the timer slot is released, not leaked — there are only sixteen. */
    assert_uint8(test_task_active(&ctrl, TASK_BUTTON_1), ==, 0);
    return MUNIT_OK;
}

/* Once fired, holding longer must not fire again. */
static MunitResult test_hold_fires_once_per_press(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(1, COMM_BUTTON_MODE_HOLD, 100);

    press(1, 1);
    test_advance_ms(&ctrl, 2000);
    settle();
    assert_uint8(events(), ==, 1);

    press(1, 0);
    test_advance_ms(&ctrl, 500);
    assert_uint8(events(), ==, 1); /* release is not an event in HOLD mode */
    return MUNIT_OK;
}

/* t = 0 means "immediately" — the waiting state collapses away. */
static MunitResult test_hold_with_zero_time_fires_on_press(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(1, COMM_BUTTON_MODE_HOLD, 0);
    press(1, 1);
    assert_uint8(events(), ==, 1);
    return MUNIT_OK;
}

/* A second press with no intervening release is a stray edge — noise, or a
 * dropped release — and must not re-arm. */
static MunitResult test_hold_ignores_a_stray_second_press(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(1, COMM_BUTTON_MODE_HOLD, 100);

    press(1, 1);
    test_advance_ms(&ctrl, 50);
    press(1, 1); /* stray */
    test_advance_ms(&ctrl, 60);
    settle();
    assert_uint8(events(), ==, 1); /* the original timer, once */
    return MUNIT_OK;
}

/* ── RELEASE ──────────────────────────────────────────────────────────── */

static MunitResult test_release_fires_on_release_after_the_delay(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(3, COMM_BUTTON_MODE_RELEASE, 300);

    press(3, 1);
    test_advance_ms(&ctrl, 400);
    assert_uint8(events(), ==, 0); /* nothing yet — it fires on release */

    press(3, 0);
    assert_uint8(events(), ==, 1);
    CommButtonChanged ev = last_event();
    assert_uint8(ev.button_id, ==, 3);
    assert_uint8(ev.pressed, ==, 0);
    assert_uint8(ev.mode, ==, COMM_BUTTON_MODE_RELEASE);
    return MUNIT_OK;
}

static MunitResult test_release_too_soon_fires_nothing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(3, COMM_BUTTON_MODE_RELEASE, 300);

    press(3, 1);
    test_advance_ms(&ctrl, 100);
    press(3, 0);
    test_advance_ms(&ctrl, 1000);
    assert_uint8(events(), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_release_with_zero_time_fires_on_any_release(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(3, COMM_BUTTON_MODE_RELEASE, 0);
    press(3, 1);
    assert_uint8(events(), ==, 0);
    press(3, 0);
    assert_uint8(events(), ==, 1);
    return MUNIT_OK;
}

/* ── Reconfiguration mid-press ────────────────────────────────────────── */

/*
 * The main board can retrigger a button at any moment, including while a
 * timer is counting.  A mode change has to abandon the in-flight state
 * outright — carrying a HOLD timer into RELEASE mode would fire the wrong
 * event on the next edge.
 */
static MunitResult test_mode_change_cancels_a_pending_timer(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(4, COMM_BUTTON_MODE_HOLD, 500);
    press(4, 1);
    test_advance_ms(&ctrl, 100);

    set_mode(4, COMM_BUTTON_MODE_RELEASE, 500);
    test_advance_ms(&ctrl, 1000);
    assert_uint8(events(), ==, 0);
    assert_uint8(test_task_active(&ctrl, TASK_BUTTON_4), ==, 0);

    /* And the button works normally afterwards. */
    press(4, 0);
    press(4, 1);
    test_advance_ms(&ctrl, 600);
    press(4, 0);
    settle();
    assert_uint8(events(), ==, 1);
    return MUNIT_OK;
}

/* Changing only the time re-arms with the new value rather than leaving the
 * old deadline in place. */
static MunitResult test_time_change_rearms_the_timer(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(5, COMM_BUTTON_MODE_HOLD, 5000);
    press(5, 1);
    test_advance_ms(&ctrl, 100);

    set_mode(5, COMM_BUTTON_MODE_HOLD, 200);
    test_advance_ms(&ctrl, 198);
    assert_uint8(events(), ==, 0);
    test_advance_ms(&ctrl, 2);
    settle();
    assert_uint8(events(), ==, 1);
    return MUNIT_OK;
}

/* Shortening the time to zero while waiting collapses to an immediate fire,
 * the same as it would have on the original press. */
static MunitResult test_time_change_to_zero_fires_immediately(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(5, COMM_BUTTON_MODE_HOLD, 5000);
    press(5, 1);
    test_advance_ms(&ctrl, 100);
    assert_uint8(events(), ==, 0);

    set_mode(5, COMM_BUTTON_MODE_HOLD, 0);
    settle();
    assert_uint8(events(), ==, 1);
    return MUNIT_OK;
}

/* ── Independence ─────────────────────────────────────────────────────── */

static MunitResult test_buttons_do_not_interfere(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(0, COMM_BUTTON_MODE_CHANGE, 0);
    set_mode(1, COMM_BUTTON_MODE_HOLD, 300);
    set_mode(2, COMM_BUTTON_MODE_RELEASE, 300);

    press(0, 1);
    press(1, 1);
    press(2, 1);
    assert_uint8(events(), ==, 1); /* only the CHANGE button so far */

    test_advance_ms(&ctrl, 350);
    settle();
    assert_uint8(events(), ==, 2); /* HOLD fired */

    press(2, 0);
    assert_uint8(events(), ==, 3); /* RELEASE fired */
    assert_uint8(last_event().button_id, ==, 2);
    return MUNIT_OK;
}

/* Two buttons changing on the same edge — the user hitting both at once —
 * must both be dispatched, not just the lowest-numbered one. */
static MunitResult test_simultaneous_edges_are_both_dispatched(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_mode(0, COMM_BUTTON_MODE_CHANGE, 0);
    set_mode(6, COMM_BUTTON_MODE_CHANGE, 0);

    PORTA &= (uint8_t)~((1u << button_pin[0]) | (1u << button_pin[6]));
    IOC_ISR();
    test_advance_ms(&ctrl, 1);

    assert_uint8(events(), ==, 2);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/unconfigured_button_is_silent", test_unconfigured_button_is_silent),
    T("/trigger_round_trips", test_trigger_round_trips),
    T("/change_fires_on_both_edges", test_change_fires_on_both_edges),
    T("/change_ignores_the_time", test_change_ignores_the_time),
    T("/hold_fires_after_the_delay", test_hold_fires_after_the_delay),
    T("/hold_released_early_fires_nothing", test_hold_released_early_fires_nothing),
    T("/hold_fires_once_per_press", test_hold_fires_once_per_press),
    T("/hold_with_zero_time_fires_on_press", test_hold_with_zero_time_fires_on_press),
    T("/hold_ignores_a_stray_second_press", test_hold_ignores_a_stray_second_press),
    T("/release_fires_on_release_after_the_delay", test_release_fires_on_release_after_the_delay),
    T("/release_too_soon_fires_nothing", test_release_too_soon_fires_nothing),
    T("/release_with_zero_time_fires_on_any_release", test_release_with_zero_time_fires_on_any_release),
    T("/mode_change_cancels_a_pending_timer", test_mode_change_cancels_a_pending_timer),
    T("/time_change_rearms_the_timer", test_time_change_rearms_the_timer),
    T("/time_change_to_zero_fires_immediately", test_time_change_to_zero_fires_immediately),
    T("/buttons_do_not_interfere", test_buttons_do_not_interfere),
    T("/simultaneous_edges_are_both_dispatched", test_simultaneous_edges_are_both_dispatched),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b2_button_suite(void) {
    MunitSuite s = {"/board2/button", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
