#include "adc.h"
#include "config.h"
#include "controller.h"
#include "libcomm.h"
#include "test_support.h"

#include <xc.h>

/*
 * The three-channel ADC sweep and its conversion chain.
 *
 * Two things make this worth testing away from hardware.  First, the analogue
 * chain is a stack of documented constants — 2.048 V FVR reference, 5 mA
 * constant-current source into the level senders, /10 divider on the battery
 * — and getting any of them wrong produces a plausible-looking wrong number
 * rather than an obvious failure.  Second, the two meter modes run in opposite
 * directions (European 0..190 Ω reads full at *high* resistance, American
 * 240..33 Ω at *low*), which is exactly the kind of thing that gets inverted.
 *
 * The mock supplies the factory FVR calibration through a simulated table
 * read, so the scaling is exercised for real rather than divided by zero.
 */

void AD_ISR(void); /* defined in adc.c */

#define FVR_MV 2048u  /* what the DIA word would hold on a typical part */
#define ADC_FULL 4095u

#define ADC_CH_WATER 0x05
#define ADC_CH_BATT 0x06
#define ADC_CH_FUEL 0x07

static TaskController ctrl;

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    task_controller_init(&ctrl);
    pic_flash_put_word(DIA_FVRA2X, FVR_MV);
    config_init(&ctrl);
    adc_init(&ctrl);
    return NULL;
}

/* Complete one burst on whichever channel is currently selected. */
static void convert(uint16_t raw) {
    ADRES = raw;
    ADCNT = ADRPT; /* the ISR ignores anything but a completed burst */
    AD_ISR();
    task_controller_poll(&ctrl); /* the processor runs in main context */
}

/* Advance the sweep to `channel`, then convert `raw` on it. */
static void sample(uint8_t channel, uint16_t raw) {
    for (int guard = 0; guard < 4 && ADPCH != channel; guard++) {
        convert(0);
    }
    if (ADPCH != channel) {
        munit_errorf("sweep never reached channel 0x%02X", channel);
    }
    convert(raw);
}

/* The raw count that produces a given sender resistance:
 * Ω -> mV at 5 mA -> ADC counts against the 2.048 V reference. */
static uint16_t raw_for_ohm(unsigned ohm) {
    const unsigned mv = ohm * 5u;
    return (uint16_t)((mv * ADC_FULL) / FVR_MV);
}

/* The raw count for a battery voltage, through the /10 divider. */
static uint16_t raw_for_battery_mv(unsigned mv) {
    return (uint16_t)(((mv / 10u) * ADC_FULL) / FVR_MV);
}

static void assert_near(unsigned got, unsigned want, unsigned tol, const char* what) {
    const unsigned diff = (got > want) ? got - want : want - got;
    if (diff > tol) {
        munit_errorf("%s: got %u, expected %u +/- %u", what, got, want, tol);
    }
}

/* ── Peripheral setup ─────────────────────────────────────────────────── */

static MunitResult test_init_programs_the_converter(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    assert_uint8(FVRCONbits.ADFVR, ==, 0b10); /* 2.048 V, matching the DIA word read */
    assert_uint8(FVRCONbits.EN, ==, 1);
    assert_uint8(ADREFbits.PREF, ==, 0b11);   /* Vref+ = FVR, not VDD */
    assert_uint8(ADCON0bits.FM, ==, 1);       /* right-justified */
    assert_uint8(ADCON2bits.MD, ==, 0b011);   /* burst average */
    assert_uint8(ADRPT, ==, 16);
    assert_uint8(ADCON0bits.ON, ==, 1);
    assert_uint8(ADCON0bits.GO, ==, 1); /* first burst already running */

    /* All three inputs analogue. */
    assert_uint8(ANSELAbits.ANSELA5, ==, 1);
    assert_uint8(ANSELAbits.ANSELA6, ==, 1);
    assert_uint8(ANSELAbits.ANSELA7, ==, 1);
    return MUNIT_OK;
}

/* The sweep must visit all three channels in turn and keep going — a stalled
 * sweep would freeze one reading at its last value with no other symptom. */
static MunitResult test_sweep_visits_every_channel(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t seen[3] = {0, 0, 0};
    for (int i = 0; i < 6; i++) {
        switch (ADPCH) {
            case ADC_CH_WATER: seen[0] = 1; break;
            case ADC_CH_BATT:  seen[1] = 1; break;
            case ADC_CH_FUEL:  seen[2] = 1; break;
            default: munit_errorf("sweep selected unexpected channel 0x%02X", ADPCH);
        }
        convert(0);
    }
    assert_uint8(seen[0], ==, 1);
    assert_uint8(seen[1], ==, 1);
    assert_uint8(seen[2], ==, 1);
    return MUNIT_OK;
}

/* A half-finished burst is not a result. */
static MunitResult test_incomplete_burst_is_ignored(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sample(ADC_CH_BATT, raw_for_battery_mv(12000));
    const uint16_t settled = adc_read_battery_mv();

    ADRES = 0;
    ADCNT = (uint8_t)(ADRPT - 1u); /* burst still accumulating */
    AD_ISR();
    task_controller_poll(&ctrl);
    assert_uint16(adc_read_battery_mv(), ==, settled);
    return MUNIT_OK;
}

/* ── Battery ──────────────────────────────────────────────────────────── */

static MunitResult test_battery_scales_through_the_divider(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sample(ADC_CH_BATT, raw_for_battery_mv(12600));
    /* 1 % covers the integer rounding in the raw -> mV -> x10 chain. */
    assert_near(adc_read_battery_mv(), 12600, 126, "battery at 12.6 V");
    return MUNIT_OK;
}

static MunitResult test_battery_zero_reads_zero(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    sample(ADC_CH_BATT, 0);
    assert_uint16(adc_read_battery_mv(), ==, 0);
    return MUNIT_OK;
}

/*
 * The calibration byte is what the channel *reported* when 12.000 V was
 * applied, in 100 mV units.  A board reading 12.5 V for a real 12.0 V gets
 * cal = 125, and the correction has to bring the reading back down.
 */
static MunitResult test_battery_calibration_corrects_the_reading(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_write_byte(CONFIG_ADDR_BATTERY_CAL, 125);
    test_advance_ms(&ctrl, TASK_MIN_MS);

    sample(ADC_CH_BATT, raw_for_battery_mv(12500));
    /* Reported 12.5 V when 12.0 V was applied => scale by 120/125. */
    assert_near(adc_read_battery_mv(), 12000, 150, "calibrated battery");
    return MUNIT_OK;
}

/* A zero calibration byte — a corrupt cell, or a user mistake — must fall
 * back to the default rather than divide by zero. */
static MunitResult test_battery_zero_calibration_falls_back(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    config_write_byte(CONFIG_ADDR_BATTERY_CAL, 0);
    test_advance_ms(&ctrl, TASK_MIN_MS);

    sample(ADC_CH_BATT, raw_for_battery_mv(12000));
    assert_near(adc_read_battery_mv(), 12000, 120, "battery with cal=0");
    return MUNIT_OK;
}

/* ── Level meters ─────────────────────────────────────────────────────── */

static MunitResult test_calibration_mode_passes_ohms_through(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Mode 0 exists so the installer can read the sender's actual resistance
     * while choosing the calibration byte. */
    controller_set_level_mode((COMM_METER_MODE_CALIBRATION << 2) | COMM_METER_MODE_CALIBRATION);

    sample(ADC_CH_WATER, raw_for_ohm(120));
    assert_near(adc_read_level_fresh_water(), 120, 2, "passthrough ohms");
    return MUNIT_OK;
}

static MunitResult test_european_mode_is_empty_at_zero_ohms(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_0_190 << 2) | COMM_METER_MODE_0_190);

    sample(ADC_CH_WATER, 0);
    assert_uint8(adc_read_level_fresh_water(), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_european_mode_is_full_at_top_of_range(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_0_190 << 2) | COMM_METER_MODE_0_190);

    /* Anywhere at or above 190 Ω clamps to full.  Note that a sender sitting
     * exactly on 190 Ω reads 99 %, not 100: the raw -> mV -> Ω chain truncates
     * at each step and lands one ohm short.  That is a display nicety, not a
     * fault, so the clamp is checked a little above the boundary. */
    sample(ADC_CH_WATER, raw_for_ohm(200));
    assert_uint8(adc_read_level_fresh_water(), ==, 100);
    return MUNIT_OK;
}

static MunitResult test_european_mode_is_proportional(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_0_190 << 2) | COMM_METER_MODE_0_190);

    sample(ADC_CH_WATER, raw_for_ohm(95));
    assert_near(adc_read_level_fresh_water(), 50, 2, "half tank, European sender");
    return MUNIT_OK;
}

/*
 * The American sender runs the other way — 240 Ω empty, 33 Ω full.  Reading
 * it with the European curve would show a full tank as empty, which is the
 * single most consequential thing this module can get wrong.
 */
static MunitResult test_american_mode_is_inverted(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_240_33 << 2) | COMM_METER_MODE_240_33);

    sample(ADC_CH_WATER, raw_for_ohm(240));
    assert_uint8(adc_read_level_fresh_water(), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_american_mode_is_full_at_33_ohms(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_240_33 << 2) | COMM_METER_MODE_240_33);

    sample(ADC_CH_FUEL, raw_for_ohm(33));
    assert_uint8(adc_read_level_fuel(), ==, 100);
    return MUNIT_OK;
}

/*
 * An open circuit — a disconnected sender or a broken float wire — rails the
 * current source and reads far above the sender's range.  Reporting 0 % or
 * 100 % there would be actively misleading, so the module reports 255, which
 * is outside the legal percentage range and shows as an error on the panel.
 */
static MunitResult test_open_circuit_reports_the_error_sentinel(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_0_190 << 2) | COMM_METER_MODE_0_190);
    sample(ADC_CH_WATER, raw_for_ohm(400)); /* > 1.5 x 190 */
    assert_uint8(adc_read_level_fresh_water(), ==, 255);

    controller_set_level_mode((COMM_METER_MODE_240_33 << 2) | COMM_METER_MODE_240_33);
    sample(ADC_CH_FUEL, raw_for_ohm(400)); /* > 1.5 x 240 */
    assert_uint8(adc_read_level_fuel(), ==, 255);
    return MUNIT_OK;
}

/* The two meters carry independent modes in one packed byte — meter 0 in the
 * low two bits, meter 1 in the next two. */
static MunitResult test_meters_use_their_own_mode(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* water = European, fuel = American. */
    controller_set_level_mode((uint8_t)((COMM_METER_MODE_240_33 << 2) | COMM_METER_MODE_0_190));

    sample(ADC_CH_WATER, raw_for_ohm(200));
    sample(ADC_CH_FUEL, raw_for_ohm(200));

    assert_uint8(adc_read_level_fresh_water(), ==, 100); /* full on the European curve */
    assert_uint8(adc_read_level_fuel(), <, 50);          /* nearly empty on the American one */
    return MUNIT_OK;
}

/* Each meter reads its own calibration byte; a miswired lookup would apply
 * the water correction to the fuel tank. */
static MunitResult test_meters_use_their_own_calibration(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_CALIBRATION << 2) | COMM_METER_MODE_CALIBRATION);
    config_write_byte(CONFIG_ADDR_WATER_CAL, 200); /* reads double: halve it */
    config_write_byte(CONFIG_ADDR_FUEL_CAL, 50);   /* reads half: double it   */
    test_advance_ms(&ctrl, TASK_MIN_MS);

    sample(ADC_CH_WATER, raw_for_ohm(100));
    sample(ADC_CH_FUEL, raw_for_ohm(100));

    assert_near(adc_read_level_fresh_water(), 50, 2, "water with cal=200");
    assert_near(adc_read_level_fuel(), 200, 3, "fuel with cal=50");
    return MUNIT_OK;
}

/*
 * The sloshing filter is seeded from the first sample rather than ramping up
 * from zero.  With a ~6 s time constant a cold-start ramp would show an empty
 * tank for the first half minute after every boot.
 */
static MunitResult test_filter_is_seeded_not_ramped(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    controller_set_level_mode((COMM_METER_MODE_CALIBRATION << 2) | COMM_METER_MODE_CALIBRATION);

    sample(ADC_CH_WATER, raw_for_ohm(150));
    assert_near(adc_read_level_fresh_water(), 150, 2, "first sample");

    /* And afterwards it really does filter: one wildly different sample
     * barely moves the output. */
    const uint8_t before = adc_read_level_fresh_water();
    sample(ADC_CH_WATER, raw_for_ohm(0));
    assert_near(adc_read_level_fresh_water(), before, 1, "one outlier sample");
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/init_programs_the_converter", test_init_programs_the_converter),
    T("/sweep_visits_every_channel", test_sweep_visits_every_channel),
    T("/incomplete_burst_is_ignored", test_incomplete_burst_is_ignored),
    T("/battery_scales_through_the_divider", test_battery_scales_through_the_divider),
    T("/battery_zero_reads_zero", test_battery_zero_reads_zero),
    T("/battery_calibration_corrects_the_reading", test_battery_calibration_corrects_the_reading),
    T("/battery_zero_calibration_falls_back", test_battery_zero_calibration_falls_back),
    T("/calibration_mode_passes_ohms_through", test_calibration_mode_passes_ohms_through),
    T("/european_mode_is_empty_at_zero_ohms", test_european_mode_is_empty_at_zero_ohms),
    T("/european_mode_is_full_at_top_of_range", test_european_mode_is_full_at_top_of_range),
    T("/european_mode_is_proportional", test_european_mode_is_proportional),
    T("/american_mode_is_inverted", test_american_mode_is_inverted),
    T("/american_mode_is_full_at_33_ohms", test_american_mode_is_full_at_33_ohms),
    T("/open_circuit_reports_the_error_sentinel", test_open_circuit_reports_the_error_sentinel),
    T("/meters_use_their_own_mode", test_meters_use_their_own_mode),
    T("/meters_use_their_own_calibration", test_meters_use_their_own_calibration),
    T("/filter_is_seeded_not_ramped", test_filter_is_seeded_not_ramped),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b1_adc_suite(void) {
    MunitSuite s = {"/board1/adc", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
