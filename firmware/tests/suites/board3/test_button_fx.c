#include "button_fx.h"
#include "controller.h"
#include "i2c_fake.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"
#include "test_support.h"

#include <string.h>

/*
 * Per-button LED feedback on the two button panels.
 *
 * When the user presses a button the main board cannot switch anything
 * itself — it asks the switching board and waits for the channel state to
 * come back changed.  That round trip is what these three states are for:
 *
 *   idle     the channel is where it should be; steady on or off
 *   pending  we asked, we are waiting; pulsating white
 *   error    a second went by and the channel never moved; flashing red
 *
 * The error state is the point of the whole module: a blown fuse or a stuck
 * relay would otherwise look identical to a working switch, because the
 * button lights up either way.
 *
 * Effects are only transmitted when they change — the button boards animate
 * locally — so the tests watch the outbound button_effect frames rather than
 * any internal state.
 */

#define TICK_MS 10u
#define TIMEOUT_MS 1000u

static TaskController ctrl;

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    i2c_fake_reset();
    comm_interface_init();
    task_controller_init(&ctrl);
    button_fx_init(&ctrl);
    return NULL;
}

/* Run the refresh task and let any write complete, so the module is not left
 * blocked on an in-flight transaction. */
static void tick(unsigned frames) {
    for (unsigned i = 0; i < frames; i++) {
        test_advance_ms(&ctrl, TICK_MS);
        i2c_poll(); /* delivers the button_effect completion */
    }
}

/* The most recent effect sent to one side, or NULL if none was. */
static uint8_t last_effect(uint8_t addr, CommButtonEffect* out) {
    for (uint8_t i = i2c_fake_tx_count(); i > 0; i--) {
        const I2cFakeTx* tx = i2c_fake_tx((uint8_t)(i - 1u));
        if (tx->addr == addr && tx->tx[0] == COMM_BUTTON_EFFECT) {
            comm_parse_button_effect((uint8_t*)tx->tx + 1, out);
            return 1;
        }
    }
    return 0;
}

static CommButtonOutputEffect effect_for(uint8_t addr, uint8_t index) {
    CommButtonEffect eff;
    CommButtonOutputEffect out = {0};
    if (!last_effect(addr, &eff)) {
        munit_errorf("no button_effect was ever sent to 0x%02X", addr);
    }
    comm_button_effect_get(&eff, index, &out);
    return out;
}

static uint8_t sends_to(uint8_t addr) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < i2c_fake_tx_count(); i++) {
        const I2cFakeTx* tx = i2c_fake_tx(i);
        if (tx->addr == addr && tx->tx[0] == COMM_BUTTON_EFFECT) {
            n++;
        }
    }
    return n;
}

/* ── Quiescence ───────────────────────────────────────────────────────── */

/*
 * With nothing pressed, the rendered effect is all-off — which is also the
 * initial "last transmitted" value, so the module must stay silent.  A frame
 * every 10 ms per side would otherwise saturate the bus with no-ops.
 */
static MunitResult test_idle_panel_sends_nothing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    tick(50);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_L), ==, 0);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_R), ==, 0);
    return MUNIT_OK;
}

/* ── Pending ──────────────────────────────────────────────────────────── */

static MunitResult test_press_shows_pending(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT);
    tick(1);

    CommButtonOutputEffect e = effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2);
    assert_uint8(e.mode, ==, COMM_EFFECT_MODE_PULSATING);
    assert_uint8(e.color, ==, COMM_EFFECT_COLOR_WHITE);
    return MUNIT_OK;
}

/* Confirmation from the switching board resolves the wait into a steady
 * "on" — the user sees the button settle. */
static MunitResult test_confirmation_settles_to_on(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT);
    tick(1);

    button_fx_on_channel_state(CHANNEL_AUTOPILOT);
    tick(1);

    CommButtonOutputEffect e = effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2);
    assert_uint8(e.mode, ==, COMM_EFFECT_MODE_ENABLED);
    assert_uint8(e.color, ==, COMM_EFFECT_COLOR_WHITE);
    return MUNIT_OK;
}

/*
 * Nothing came back within the deadline: the circuit did not do what it was
 * told.  Flashing red is the only indication the operator gets that the
 * switch worked but the load did not.
 */
static MunitResult test_unconfirmed_press_becomes_an_error(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT);

    tick(TIMEOUT_MS / TICK_MS - 1u);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2).mode, ==, COMM_EFFECT_MODE_PULSATING);

    tick(3);
    CommButtonOutputEffect e = effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2);
    assert_uint8(e.mode, ==, COMM_EFFECT_MODE_FLASHING);
    assert_uint8(e.color, ==, COMM_EFFECT_COLOR_RED);
    return MUNIT_OK;
}

/* A late confirmation still clears the error — a fuse that was reseated, or a
 * relay that eventually caught. */
static MunitResult test_late_confirmation_clears_the_error(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT);
    tick(TIMEOUT_MS / TICK_MS + 2u);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2).mode, ==, COMM_EFFECT_MODE_FLASHING);

    button_fx_on_channel_state(CHANNEL_AUTOPILOT);
    tick(1);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2).mode, ==, COMM_EFFECT_MODE_ENABLED);
    return MUNIT_OK;
}

/* button_fx_clear drops the slot back to plain rendering, whatever it was
 * doing — the escape hatch the controller uses on power-down. */
static MunitResult test_clear_abandons_pending(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_R0, CHANNEL_WATER_PUMP, CHANNEL_WATER_PUMP);
    tick(1);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_R, 0).mode, ==, COMM_EFFECT_MODE_PULSATING);

    button_fx_clear(BUTTON_R0);
    tick(TIMEOUT_MS / TICK_MS + 2u);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_R, 0).mode, ==, COMM_EFFECT_MODE_DISABLED);
    return MUNIT_OK;
}

/*
 * Switching a channel *off* resolves immediately rather than pending.  The
 * user has no feedback to wait for — the light going out is the confirmation
 * — and pulsating white on the way down would read as a fault.
 */
static MunitResult test_switching_off_does_not_pend(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT);
    button_fx_on_channel_state(CHANNEL_AUTOPILOT);
    tick(1);

    button_fx_set(BUTTON_L2, 0, CHANNEL_AUTOPILOT);
    tick(1);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2).mode, ==, COMM_EFFECT_MODE_DISABLED);

    /* And it stays settled — no deadline is running. */
    tick(TIMEOUT_MS / TICK_MS + 2u);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2).mode, ==, COMM_EFFECT_MODE_DISABLED);
    return MUNIT_OK;
}

/* Re-pressing with the same expectation must not restart the deadline —
 * otherwise a user tapping repeatedly would never see the error. */
static MunitResult test_repeat_press_does_not_restart_the_deadline(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT);
    tick(TIMEOUT_MS / TICK_MS / 2u);

    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT); /* same intent */
    tick(TIMEOUT_MS / TICK_MS / 2u + 3u);

    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2).mode, ==, COMM_EFFECT_MODE_FLASHING);
    return MUNIT_OK;
}

/*
 * A channel that drops out on its own — a fuse blowing while the circuit is
 * running — must raise the error without anybody pressing anything.
 */
static MunitResult test_unexpected_channel_loss_raises_an_error(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L2, CHANNEL_AUTOPILOT, CHANNEL_AUTOPILOT);
    button_fx_on_channel_state(CHANNEL_AUTOPILOT);
    tick(1);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2).mode, ==, COMM_EFFECT_MODE_ENABLED);

    button_fx_on_channel_state(0); /* the channel went away */
    tick(1);
    CommButtonOutputEffect e = effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 2);
    assert_uint8(e.mode, ==, COMM_EFFECT_MODE_FLASHING);
    assert_uint8(e.color, ==, COMM_EFFECT_COLOR_RED);
    return MUNIT_OK;
}

/* ── Side independence ────────────────────────────────────────────────── */

/* Bit 3 of the button index selects the panel.  A slot on one side must
 * never render on the other. */
static MunitResult test_sides_are_independent(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set(BUTTON_L1, CHANNEL_INSTRUMENTS, CHANNEL_INSTRUMENTS);
    button_fx_set(BUTTON_R3, CHANNEL_CABIN_LIGHTS, CHANNEL_CABIN_LIGHTS);
    tick(1);

    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 1).mode, ==, COMM_EFFECT_MODE_PULSATING);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_L, 3).mode, ==, COMM_EFFECT_MODE_DISABLED);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_R, 3).mode, ==, COMM_EFFECT_MODE_PULSATING);
    assert_uint8(effect_for(COMM_ADDRESS_BUTTON_BOARD_R, 1).mode, ==, COMM_EFFECT_MODE_DISABLED);
    return MUNIT_OK;
}

/* Out-of-range indices are ignored rather than writing past the slot table. */
static MunitResult test_out_of_range_index_is_ignored(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    button_fx_set((ButtonIndex)BUTTON_COUNT, 0xFFFF, 0xFFFF);
    button_fx_clear((ButtonIndex)(BUTTON_COUNT + 5));
    tick(2);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_L), ==, 0);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_R), ==, 0);
    return MUNIT_OK;
}

/* ── Bus discipline ───────────────────────────────────────────────────── */

/*
 * One write per side may be outstanding.  Without that, a pulsating slot
 * whose neighbour keeps changing could queue a new frame every 10 ms and
 * crowd out the relay commands that actually switch things.
 */
static MunitResult test_only_one_write_per_side_is_in_flight(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_autocomplete_writes(0); /* nothing ever completes */

    button_fx_set(BUTTON_L1, CHANNEL_INSTRUMENTS, CHANNEL_INSTRUMENTS);
    test_advance_ms(&ctrl, TICK_MS);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_L), ==, 1);

    /* Keep changing the picture; the module must not pile on. */
    for (int i = 0; i < 20; i++) {
        button_fx_set(BUTTON_L1, (uint16_t)(i & 1 ? CHANNEL_INSTRUMENTS : 0), CHANNEL_INSTRUMENTS);
        test_advance_ms(&ctrl, TICK_MS);
    }
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_L), ==, 1);
    return MUNIT_OK;
}

/* A failed write must not be treated as delivered, or the module would stop
 * retransmitting and the panel would sit on a stale picture. */
static MunitResult test_failed_write_is_retransmitted(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_autocomplete_writes(0);

    button_fx_set(BUTTON_L1, CHANNEL_INSTRUMENTS, CHANNEL_INSTRUMENTS);
    test_advance_ms(&ctrl, TICK_MS);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_L), ==, 1);

    i2c_fake_respond(0, I2C_RESULT_NACK, NULL, 0);
    i2c_poll();

    test_advance_ms(&ctrl, TICK_MS);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_L), ==, 2);
    return MUNIT_OK;
}

/* A stuck side must not hold up the other one. */
static MunitResult test_a_stuck_side_does_not_block_the_other(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    i2c_fake_set_autocomplete_writes(0);

    button_fx_set(BUTTON_L1, CHANNEL_INSTRUMENTS, CHANNEL_INSTRUMENTS);
    test_advance_ms(&ctrl, TICK_MS);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_L), ==, 1);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_R), ==, 0);

    button_fx_set(BUTTON_R1, CHANNEL_FRIDGE, CHANNEL_FRIDGE);
    test_advance_ms(&ctrl, TICK_MS);
    assert_uint8(sends_to(COMM_ADDRESS_BUTTON_BOARD_R), ==, 1);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/idle_panel_sends_nothing", test_idle_panel_sends_nothing),
    T("/press_shows_pending", test_press_shows_pending),
    T("/confirmation_settles_to_on", test_confirmation_settles_to_on),
    T("/unconfirmed_press_becomes_an_error", test_unconfirmed_press_becomes_an_error),
    T("/late_confirmation_clears_the_error", test_late_confirmation_clears_the_error),
    T("/clear_abandons_pending", test_clear_abandons_pending),
    T("/switching_off_does_not_pend", test_switching_off_does_not_pend),
    T("/repeat_press_does_not_restart_the_deadline", test_repeat_press_does_not_restart_the_deadline),
    T("/unexpected_channel_loss_raises_an_error", test_unexpected_channel_loss_raises_an_error),
    T("/sides_are_independent", test_sides_are_independent),
    T("/out_of_range_index_is_ignored", test_out_of_range_index_is_ignored),
    T("/only_one_write_per_side_is_in_flight", test_only_one_write_per_side_is_in_flight),
    T("/failed_write_is_retransmitted", test_failed_write_is_retransmitted),
    T("/a_stuck_side_does_not_block_the_other", test_a_stuck_side_does_not_block_the_other),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b3_button_fx_suite(void) {
    MunitSuite s = {"/board3/button_fx", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
