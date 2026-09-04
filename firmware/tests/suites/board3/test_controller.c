#include "button_fx.h"
#include "comm.h"
#include "config.h"
#include "config_mode.h"
#include "controller.h"
#include "display.h"
#include "display_text.h"
#include "i2c_fake.h"
#include "indicator.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "nav_lights.h"
#include "task_ids.h"
#include "test_support.h"

#include <string.h>
#include <xc.h>

/*
 * The main board's control logic: what a button press on either panel turns
 * into on the bus, and how the panel's picture of the boat is kept in step
 * with what the switching board reports.
 *
 * The main board owns no relays.  It sends relay_state and waits, so every
 * behaviour here is observable as an outbound frame — which is what these
 * tests inspect.
 */

/* controller.c's retry task re-paces itself from interval_for_task(). */
#define RETRY_MS 2100u

static TaskController ctrl;

static void boot(void) {
    test_reset_hardware();
    /* RA7 is the config-mode switch, pulled up and closed to ground when the
     * operator wants the menu.  A zeroed port reads as *closed*, so it has to
     * be released before config_mode_init samples it — otherwise every test
     * runs in the menu, where relay presses are deliberately swallowed. */
    PORTAbits.RA7 = 1;
    i2c_fake_reset();
    comm_interface_init();
    task_controller_init(&ctrl);
    config_init(&ctrl);
    display_init();
    display_text_init(&ctrl);
    indicator_init(&ctrl);
    config_mode_init(&ctrl);
    button_fx_init(&ctrl);
    controller_init(&ctrl);
    comm_init();
}

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    boot();
    return NULL;
}

/* Deliver a button_changed as the named panel would send it. */
static void press(uint8_t panel_addr, uint8_t button_id) {
    CommMessage msg;
    /* comm_build_button_changed stamps *our* address, so the frame is built
     * by hand with the sender the main board should see. */
    uint8_t payload[2];
    payload[0] = panel_addr;
    payload[1] = (uint8_t)(button_id | (1u << 3) | (COMM_BUTTON_MODE_CHANGE << 4));
    uint8_t frame[8];
    uint8_t n = i2c_fake_frame(frame, COMM_BUTTON_CHANGED, payload, 2);
    (void)msg;
    i2c_fake_deliver_write(frame, n);
    i2c_poll();
}

/* Report the switching board's observed channel state. */
static void observe(uint16_t channels) {
    uint8_t payload[7] = {COMM_ADDRESS_SWITCHING, 0, 0, (uint8_t)channels, (uint8_t)(channels >> 8), 0, 0};
    uint8_t frame[16];
    uint8_t n = i2c_fake_frame(frame, COMM_CHANNEL_CHANGED, payload, 7);
    i2c_fake_deliver_write(frame, n);
    i2c_poll();
}

/* Run the retry task and let the write complete, then report the relay mask
 * the switching board was last told to apply.  0xFFFF means "never told". */
static uint16_t sync_relays(void) {
    test_advance_ms(&ctrl, RETRY_MS);
    for (int i = 0; i < 4; i++) {
        i2c_poll();
    }
    for (uint8_t i = i2c_fake_tx_count(); i > 0; i--) {
        const I2cFakeTx* tx = i2c_fake_tx((uint8_t)(i - 1u));
        if (tx->tx[0] == COMM_RELAY_STATE) {
            CommRelayState st;
            comm_parse_relay_state_write((uint8_t*)tx->tx + 1, &st);
            return st.relays;
        }
    }
    return 0xFFFF;
}

static uint8_t relay_writes(void) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < i2c_fake_tx_count(); i++) {
        if (i2c_fake_tx(i)->tx[0] == COMM_RELAY_STATE) {
            n++;
        }
    }
    return n;
}

/* Bring the panel up and get past the initial main-on sync. */
static void power_on(void) {
    press(COMM_ADDRESS_BUTTON_BOARD_L, 0);
    sync_relays();
    observe(CHANNEL_MAIN);
}

/* ── Power ────────────────────────────────────────────────────────────── */

static MunitResult test_boots_off(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint8(controller_power_on(), ==, 0);
    assert_uint8(relay_writes(), ==, 0);
    return MUNIT_OK;
}

/*
 * Waking up brings only the master rail.  Restoring whatever was on before
 * would mean a boat that starts its own bilge pump or nav lights when someone
 * touches the panel.
 */
static MunitResult test_power_on_raises_only_main(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    press(COMM_ADDRESS_BUTTON_BOARD_L, 0);
    assert_uint8(controller_power_on(), ==, 1);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN);
    return MUNIT_OK;
}

/* The display and the indicator ring follow the power state — the panel goes
 * dark when it is off. */
static MunitResult test_power_drives_the_ui(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    press(COMM_ADDRESS_BUTTON_BOARD_L, 0);
    assert_uint8(controller_power_on(), ==, 1);
    press(COMM_ADDRESS_BUTTON_BOARD_L, 0);
    assert_uint8(controller_power_on(), ==, 0);
    return MUNIT_OK;
}

/*
 * Turning the panel off should drop every channel with it.
 *
 * It does not: clear_channels() resets the per-button feedback slots but
 * never clears g_relay_target, so the retry task sees target == last-sent and
 * stays quiet.  Every circuit the user had switched on stays energised after
 * they turn the panel off — the fridge, the cabin lights, the inverter.
 *
 * Marked TODO so the suite passes while it stands: munit reports a TODO that
 * *starts* passing as an error, so fixing clear_channels will show up here as
 * "remove the TODO" rather than being missed.
 */
static MunitResult test_power_off_drops_every_channel(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    press(COMM_ADDRESS_BUTTON_BOARD_R, 1); /* fridge on */
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_FRIDGE);

    press(COMM_ADDRESS_BUTTON_BOARD_L, 0); /* panel off */
    assert_uint8(controller_power_on(), ==, 0);
    assert_uint16(sync_relays(), ==, 0);
    return MUNIT_OK;
}

/* ── Relay buttons ────────────────────────────────────────────────────── */

static MunitResult test_each_button_toggles_its_own_channel(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();

    const struct {
        uint8_t addr;
        uint8_t id;
        uint16_t channel;
    } map[] = {
        {COMM_ADDRESS_BUTTON_BOARD_L, 1, CHANNEL_INSTRUMENTS},
        {COMM_ADDRESS_BUTTON_BOARD_L, 2, CHANNEL_AUTOPILOT},
        {COMM_ADDRESS_BUTTON_BOARD_L, 6, CHANNEL_INVERTER},
        {COMM_ADDRESS_BUTTON_BOARD_R, 0, CHANNEL_WATER_PUMP},
        {COMM_ADDRESS_BUTTON_BOARD_R, 1, CHANNEL_FRIDGE},
        {COMM_ADDRESS_BUTTON_BOARD_R, 2, CHANNEL_DECK_LIGHTS},
        {COMM_ADDRESS_BUTTON_BOARD_R, 3, CHANNEL_CABIN_LIGHTS},
        {COMM_ADDRESS_BUTTON_BOARD_R, 4, CHANNEL_USB},
        {COMM_ADDRESS_BUTTON_BOARD_R, 5, CHANNEL_AUX_1},
        {COMM_ADDRESS_BUTTON_BOARD_R, 6, CHANNEL_AUX_2},
    };

    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        press(map[i].addr, map[i].id);
        const uint16_t on = sync_relays();
        if (on != (uint16_t)(CHANNEL_MAIN | map[i].channel)) {
            munit_errorf("panel 0x%02X button %u produced 0x%04X, expected 0x%04X",
                         map[i].addr, map[i].id, on, (unsigned)(CHANNEL_MAIN | map[i].channel));
        }
        press(map[i].addr, map[i].id); /* and back off again */
        assert_uint16(sync_relays(), ==, CHANNEL_MAIN);
    }
    return MUNIT_OK;
}

/* The same button id on the two panels must mean different things — the side
 * lives in bit 3 of the index and comes from the sender's address. */
static MunitResult test_panel_address_selects_the_side(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    press(COMM_ADDRESS_BUTTON_BOARD_L, 1);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_INSTRUMENTS);
    press(COMM_ADDRESS_BUTTON_BOARD_R, 1);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_INSTRUMENTS | CHANNEL_FRIDGE);
    return MUNIT_OK;
}

/* An event from a device that is not a button panel must be ignored, not
 * mapped onto whatever button id it happens to carry. */
static MunitResult test_unknown_sender_is_ignored(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    const uint8_t before = relay_writes();
    press(COMM_ADDRESS_SWITCHING, 1);
    sync_relays();
    assert_uint8(relay_writes(), ==, before);
    return MUNIT_OK;
}

/* ── Nav lights ───────────────────────────────────────────────────────── */

static MunitResult test_nav_mode_lights_the_right_channels(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();

    /* Running: the tricolour, since every light is fitted by default. */
    press(COMM_ADDRESS_BUTTON_BOARD_L, 4);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_NAV_TRICOLOR);
    assert_uint8(controller_nav_config_error(), ==, 0);

    /* Steaming replaces it rather than adding to it — the two must never be
     * lit together. */
    press(COMM_ADDRESS_BUTTON_BOARD_L, 3);
    assert_uint16(sync_relays(), ==,
                  CHANNEL_MAIN | CHANNEL_NAV_STERN | CHANNEL_NAV_BOW | CHANNEL_NAV_STEAMING);

    /* Anchoring likewise. */
    press(COMM_ADDRESS_BUTTON_BOARD_L, 5);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_NAV_ANCHORING);
    return MUNIT_OK;
}

/* Pressing the mode you are already in turns the nav lights off. */
static MunitResult test_repressing_a_nav_mode_turns_it_off(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    press(COMM_ADDRESS_BUTTON_BOARD_L, 5);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_NAV_ANCHORING);
    press(COMM_ADDRESS_BUTTON_BOARD_L, 5);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN);
    return MUNIT_OK;
}

/* Changing nav mode must not disturb the house circuits. */
static MunitResult test_nav_mode_leaves_other_channels_alone(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    press(COMM_ADDRESS_BUTTON_BOARD_R, 1); /* fridge */
    press(COMM_ADDRESS_BUTTON_BOARD_L, 4); /* running */
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_FRIDGE | CHANNEL_NAV_TRICOLOR);

    press(COMM_ADDRESS_BUTTON_BOARD_L, 4); /* nav off */
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_FRIDGE);
    return MUNIT_OK;
}

/*
 * With the fitted-lights mask cut down so a mode cannot be realised, the
 * controller has to raise the config error rather than light a partial
 * pattern.
 */
static MunitResult test_unrealisable_nav_mode_raises_a_config_error(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Only the steaming light is fitted — neither running pattern works. */
    config_write_byte(CONFIG_ADDR_NAV_ENABLED_MASK, NAV_LIGHT_STEAMING);
    test_advance_ms(&ctrl, 5);
    power_on();

    press(COMM_ADDRESS_BUTTON_BOARD_L, 4); /* running */
    assert_uint8(controller_nav_config_error(), ==, 1);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN); /* nothing extra lit */
    return MUNIT_OK;
}

/* And it clears again when the mode is abandoned. */
static MunitResult test_config_error_clears_on_nav_off(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_write_byte(CONFIG_ADDR_NAV_ENABLED_MASK, NAV_LIGHT_STEAMING);
    test_advance_ms(&ctrl, 5);
    power_on();

    press(COMM_ADDRESS_BUTTON_BOARD_L, 4);
    assert_uint8(controller_nav_config_error(), ==, 1);
    press(COMM_ADDRESS_BUTTON_BOARD_L, 4);
    assert_uint8(controller_nav_config_error(), ==, 0);
    return MUNIT_OK;
}

/* ── Bus discipline ───────────────────────────────────────────────────── */

/* Nothing to say, nothing sent. */
static MunitResult test_no_write_when_nothing_changed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    const uint8_t before = relay_writes();
    for (int i = 0; i < 5; i++) {
        sync_relays();
    }
    assert_uint8(relay_writes(), ==, before);
    return MUNIT_OK;
}

/*
 * Several presses while a write is in flight collapse into one write of the
 * final state — a latest-wins slot rather than a queue.  Queueing would make
 * the relays step through intermediate combinations the user never chose.
 */
static MunitResult test_rapid_presses_collapse_to_the_final_state(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    i2c_fake_set_autocomplete_writes(0);

    press(COMM_ADDRESS_BUTTON_BOARD_R, 1);
    test_advance_ms(&ctrl, RETRY_MS);
    const uint8_t after_first = relay_writes();

    press(COMM_ADDRESS_BUTTON_BOARD_R, 2);
    press(COMM_ADDRESS_BUTTON_BOARD_R, 3);
    test_advance_ms(&ctrl, RETRY_MS * 2u);
    assert_uint8(relay_writes(), ==, after_first); /* still just the one */

    /* Let it complete; the next attempt carries everything. */
    i2c_fake_set_autocomplete_writes(1);
    const uint8_t idx = i2c_fake_find(COMM_RELAY_STATE);
    i2c_fake_respond(idx, I2C_RESULT_OK, NULL, 0);
    i2c_poll();
    assert_uint16(sync_relays(), ==,
                  CHANNEL_MAIN | CHANNEL_FRIDGE | CHANNEL_DECK_LIGHTS | CHANNEL_CABIN_LIGHTS);
    return MUNIT_OK;
}

/* A write that fails must be retried — the switching board would otherwise
 * never learn about the change. */
static MunitResult test_failed_write_is_retried(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    i2c_fake_set_autocomplete_writes(0);

    press(COMM_ADDRESS_BUTTON_BOARD_R, 1);
    test_advance_ms(&ctrl, RETRY_MS);
    const uint8_t idx = i2c_fake_find(COMM_RELAY_STATE);
    assert_uint8(idx, !=, 0xFF);
    i2c_fake_respond(idx, I2C_RESULT_TIMEOUT, NULL, 0);
    i2c_poll();

    i2c_fake_set_autocomplete_writes(1);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_FRIDGE);
    return MUNIT_OK;
}

/* ── Observations from the switching board ────────────────────────────── */

/* channel_changed is the main board's only picture of the boat; the sensor
 * byte rides along with it. */
static MunitResult test_channel_changed_updates_the_shadow(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();

    uint8_t payload[7] = {COMM_ADDRESS_SWITCHING, 0, 0,
                          (uint8_t)(CHANNEL_MAIN | CHANNEL_FRIDGE),
                          (uint8_t)((CHANNEL_MAIN | CHANNEL_FRIDGE) >> 8), 0, 0x05};
    uint8_t frame[16];
    uint8_t n = i2c_fake_frame(frame, COMM_CHANNEL_CHANGED, payload, 7);
    i2c_fake_deliver_write(frame, n);
    i2c_poll();

    assert_uint8(controller_sensors(), ==, 0x05);
    return MUNIT_OK;
}

/* Read responses feed the display's shadows; a failed read must leave the
 * last good value rather than blanking the gauge. */
static MunitResult test_failed_read_leaves_the_last_value(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    CommBattery good = {.voltage = 12800};
    comm_on_battery_read_response(I2C_RESULT_OK, &good);
    assert_uint16(controller_battery_mv(), ==, 12800);

    comm_on_battery_read_response(I2C_RESULT_TIMEOUT, 0);
    assert_uint16(controller_battery_mv(), ==, 12800);

    CommLevels lv = {.level_0 = 40, .level_1 = 70};
    comm_on_levels_read_response(I2C_RESULT_OK, &lv);
    assert_uint8(controller_level(0), ==, 40);
    assert_uint8(controller_level(1), ==, 70);
    comm_on_levels_read_response(I2C_RESULT_BAD_CRC, 0);
    assert_uint8(controller_level(0), ==, 40);
    return MUNIT_OK;
}

/* ── Config mode ──────────────────────────────────────────────────────── */

/*
 * While the config switch is closed the four left buttons drive the menu and
 * everything else is swallowed.  A misclick must not switch a circuit while
 * the operator is editing calibration values.
 */
static MunitResult test_config_mode_swallows_relay_presses(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();

    PORTAbits.RA7 = 0;          /* switch closed */
    test_advance_ms(&ctrl, 200); /* debounce */
    assert_uint8(config_mode_active(), ==, 1);

    const uint8_t before = relay_writes();
    press(COMM_ADDRESS_BUTTON_BOARD_R, 1);
    press(COMM_ADDRESS_BUTTON_BOARD_R, 4);
    sync_relays();
    assert_uint8(relay_writes(), ==, before);

    PORTAbits.RA7 = 1;
    test_advance_ms(&ctrl, 200);
    assert_uint8(config_mode_active(), ==, 0);

    /* And normal service resumes. */
    press(COMM_ADDRESS_BUTTON_BOARD_R, 1);
    assert_uint16(sync_relays(), ==, CHANNEL_MAIN | CHANNEL_FRIDGE);
    return MUNIT_OK;
}

/* The menu buttons move the cursor rather than doing nothing. */
static MunitResult test_config_mode_buttons_drive_the_menu(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    power_on();
    PORTAbits.RA7 = 0;
    test_advance_ms(&ctrl, 200);

    const uint8_t start = config_mode_menu_cursor();
    press(COMM_ADDRESS_BUTTON_BOARD_L, 1); /* up / next */
    assert_uint8(config_mode_menu_cursor(), !=, start);
    press(COMM_ADDRESS_BUTTON_BOARD_L, 2); /* down / prev */
    assert_uint8(config_mode_menu_cursor(), ==, start);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}
#define TODO(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_TODO, NULL}

static MunitTest tests[] = {
    T("/boots_off", test_boots_off),
    T("/power_on_raises_only_main", test_power_on_raises_only_main),
    T("/power_drives_the_ui", test_power_drives_the_ui),
    TODO("/power_off_drops_every_channel", test_power_off_drops_every_channel),
    T("/each_button_toggles_its_own_channel", test_each_button_toggles_its_own_channel),
    T("/panel_address_selects_the_side", test_panel_address_selects_the_side),
    T("/unknown_sender_is_ignored", test_unknown_sender_is_ignored),
    T("/nav_mode_lights_the_right_channels", test_nav_mode_lights_the_right_channels),
    T("/repressing_a_nav_mode_turns_it_off", test_repressing_a_nav_mode_turns_it_off),
    T("/nav_mode_leaves_other_channels_alone", test_nav_mode_leaves_other_channels_alone),
    T("/unrealisable_nav_mode_raises_a_config_error", test_unrealisable_nav_mode_raises_a_config_error),
    T("/config_error_clears_on_nav_off", test_config_error_clears_on_nav_off),
    T("/no_write_when_nothing_changed", test_no_write_when_nothing_changed),
    T("/rapid_presses_collapse_to_the_final_state", test_rapid_presses_collapse_to_the_final_state),
    T("/failed_write_is_retried", test_failed_write_is_retried),
    T("/channel_changed_updates_the_shadow", test_channel_changed_updates_the_shadow),
    T("/failed_read_leaves_the_last_value", test_failed_read_leaves_the_last_value),
    T("/config_mode_swallows_relay_presses", test_config_mode_swallows_relay_presses),
    T("/config_mode_buttons_drive_the_menu", test_config_mode_buttons_drive_the_menu),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b3_controller_suite(void) {
    MunitSuite s = {"/board3/controller", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
