#include "sensors.h"
#include "task_ids.h"
#include "test_support.h"

#include <xc.h>

/*
 * The three on/off inputs (bilge float, shore power, AC present).
 *
 * These come off a boat's harness — long runs, relays, contactor arcs — so
 * the debounce is not a nicety.  An undebounced bilge float would push a
 * channel_changed onto the bus on every bounce, and the whole point of the
 * design is that an edge only counts once it has held for 50 ms.
 *
 * The state machine is split across two contexts: the IOC interrupt arms a
 * window on every edge, and a 5 ms task counts it down.  Both halves are
 * driven directly here.
 */

void IOC_ISR(void); /* defined in sensors.c; the interrupt attribute is erased */

#define BIT_BILGE 0x01 /* RC1 */
#define BIT_SHORE 0x02 /* RC2 */
#define BIT_AC 0x04    /* RC5 */

#define POLL_MS 5u
#define THRESHOLD_MS 50u

static TaskController ctrl;
static uint8_t hook_calls;
static uint8_t hook_prev;
static uint8_t hook_curr;

static void on_change(uint8_t prev, uint8_t curr) {
    hook_calls++;
    hook_prev = prev;
    hook_curr = curr;
}

/* Drive the physical pins.  RC1/RC2 map to bits 0/1 and RC5 to bit 2. */
static void set_pins(uint8_t state) {
    PORTCbits.RC1 = (state & BIT_BILGE) ? 1 : 0;
    PORTCbits.RC2 = (state & BIT_SHORE) ? 1 : 0;
    PORTCbits.RC5 = (state & BIT_AC) ? 1 : 0;
}

/* Change the pins and let the hardware notice, as an edge would. */
static void drive(uint8_t state) {
    set_pins(state);
    IOC_ISR();
}

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    task_controller_init(&ctrl);
    hook_calls = 0;
    hook_prev = hook_curr = 0xFF;
    set_pins(0);
    sensors_init(&ctrl);
    sensors_set_change_handler(on_change);
    return NULL;
}

/* ── Setup ────────────────────────────────────────────────────────────── */

static MunitResult test_init_samples_current_state(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Whatever the pins read at boot is the starting truth — the panel must
     * not report a spurious transition on the first poll. */
    test_reset_hardware();
    task_controller_init(&ctrl);
    set_pins(BIT_SHORE | BIT_AC);
    sensors_init(&ctrl);
    sensors_set_change_handler(on_change);

    assert_uint8(sensors_state(), ==, BIT_SHORE | BIT_AC);
    test_advance_ms(&ctrl, 200);
    assert_uint8(hook_calls, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_init_configures_pins(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint8(TRISCbits.TRISC1, ==, 1);
    assert_uint8(TRISCbits.TRISC2, ==, 1);
    assert_uint8(TRISCbits.TRISC5, ==, 1);
    assert_uint8(ANSELCbits.ANSELC1, ==, 0);
    assert_uint8(ANSELCbits.ANSELC2, ==, 0);
    assert_uint8(ANSELCbits.ANSELC5, ==, 0);
    /* Both edges on each of the three pins, and nothing else. */
    assert_uint8(IOCCP, ==, BIT_BILGE << 1 | BIT_SHORE << 1 | 0x20);
    assert_uint8(IOCCN, ==, IOCCP);
    assert_uint8(PIE0bits.IOCIE, ==, 1);
    return MUNIT_OK;
}

/* ── Debounce ─────────────────────────────────────────────────────────── */

static MunitResult test_change_commits_after_threshold(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    drive(BIT_BILGE);

    /* Not yet: the window is still open. */
    test_advance_ms(&ctrl, THRESHOLD_MS - POLL_MS);
    assert_uint8(sensors_state(), ==, 0);
    assert_uint8(hook_calls, ==, 0);

    test_advance_ms(&ctrl, POLL_MS);
    assert_uint8(sensors_state(), ==, BIT_BILGE);
    assert_uint8(hook_calls, ==, 1);
    assert_uint8(hook_prev, ==, 0);
    assert_uint8(hook_curr, ==, BIT_BILGE);
    return MUNIT_OK;
}

/* A pulse shorter than the window is exactly what a bouncing float switch
 * produces, and it must leave no trace. */
static MunitResult test_short_pulse_is_ignored(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    drive(BIT_SHORE);
    test_advance_ms(&ctrl, THRESHOLD_MS / 2u);
    drive(0);
    test_advance_ms(&ctrl, 200);

    assert_uint8(sensors_state(), ==, 0);
    assert_uint8(hook_calls, ==, 0);
    return MUNIT_OK;
}

/* Contact bounce: many edges in quick succession, settling on the new value.
 * The window restarts on each change, so the commit lands 50 ms after the
 * *last* bounce, not the first. */
static MunitResult test_bouncing_contact_commits_once(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (int i = 0; i < 6; i++) {
        drive(BIT_AC);
        test_advance_ms(&ctrl, POLL_MS);
        drive(0);
        test_advance_ms(&ctrl, POLL_MS);
    }
    drive(BIT_AC);
    test_advance_ms(&ctrl, THRESHOLD_MS);

    assert_uint8(sensors_state(), ==, BIT_AC);
    assert_uint8(hook_calls, ==, 1);
    return MUNIT_OK;
}

/* A glitch mid-window that returns to the committed value cancels the
 * pending change outright rather than committing it late. */
static MunitResult test_revert_during_window_cancels(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    drive(BIT_BILGE);
    test_advance_ms(&ctrl, THRESHOLD_MS - (2u * POLL_MS));
    drive(0); /* back to where we started */
    test_advance_ms(&ctrl, 200);

    assert_uint8(sensors_state(), ==, 0);
    assert_uint8(hook_calls, ==, 0);
    return MUNIT_OK;
}

/* Each input debounces on its own timer; a bouncing bilge float must not
 * delay a clean shore-power transition. */
static MunitResult test_inputs_are_independent(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    drive(BIT_SHORE);
    test_advance_ms(&ctrl, THRESHOLD_MS / 2u);

    /* Bilge starts bouncing halfway through shore's window. */
    drive(BIT_SHORE | BIT_BILGE);
    test_advance_ms(&ctrl, POLL_MS);
    drive(BIT_SHORE);

    test_advance_ms(&ctrl, THRESHOLD_MS);
    assert_uint8(sensors_state() & BIT_SHORE, ==, BIT_SHORE);
    assert_uint8(sensors_state() & BIT_BILGE, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_all_three_can_be_set_and_cleared(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t all = BIT_BILGE | BIT_SHORE | BIT_AC;

    drive(all);
    test_advance_ms(&ctrl, THRESHOLD_MS);
    assert_uint8(sensors_state(), ==, all);

    drive(0);
    test_advance_ms(&ctrl, THRESHOLD_MS);
    assert_uint8(sensors_state(), ==, 0);

    /* Nothing outside the three defined bits ever appears. */
    assert_uint8(sensors_state() & (uint8_t)~all, ==, 0);
    return MUNIT_OK;
}

/* The hook is optional — a board that never registers one must still
 * debounce. */
static MunitResult test_works_without_a_handler(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sensors_set_change_handler(0);
    drive(BIT_AC);
    test_advance_ms(&ctrl, THRESHOLD_MS);
    assert_uint8(sensors_state(), ==, BIT_AC);
    return MUNIT_OK;
}

/* Without an edge to arm it the poll task must do nothing at all — the
 * debounce is edge-triggered, not a free-running sampler. */
static MunitResult test_polling_alone_changes_nothing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    set_pins(BIT_BILGE); /* pin moved but no interrupt was raised */
    test_advance_ms(&ctrl, 500);
    assert_uint8(sensors_state(), ==, 0);
    assert_uint8(hook_calls, ==, 0);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/init_samples_current_state", test_init_samples_current_state),
    T("/init_configures_pins", test_init_configures_pins),
    T("/change_commits_after_threshold", test_change_commits_after_threshold),
    T("/short_pulse_is_ignored", test_short_pulse_is_ignored),
    T("/bouncing_contact_commits_once", test_bouncing_contact_commits_once),
    T("/revert_during_window_cancels", test_revert_during_window_cancels),
    T("/inputs_are_independent", test_inputs_are_independent),
    T("/all_three_can_be_set_and_cleared", test_all_three_can_be_set_and_cleared),
    T("/works_without_a_handler", test_works_without_a_handler),
    T("/polling_alone_changes_nothing", test_polling_alone_changes_nothing),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b1_sensors_suite(void) {
    MunitSuite s = {"/board1/sensors", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
