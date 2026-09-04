#include "config.h"
#include "nav_lights.h"
#include "test_support.h"

/*
 * Resolving a requested navigation-light mode against the lights actually
 * fitted to the boat.
 *
 * This is the one piece of the panel with a legal dimension: showing the wrong
 * navigation lights under way misrepresents the vessel to everyone around it.
 * Each mode has a primary pattern and one permitted fallback, and if neither
 * can be lit the resolver must say so rather than light something close.
 *
 * The function is pure, so the whole input space — four modes across all 32
 * combinations of fitted lights — is checked exhaustively.
 */

static NavLights lights(uint8_t raw) {
    NavLights n;
    n.raw = raw;
    return n;
}

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    return NULL;
}

/* ── Off ──────────────────────────────────────────────────────────────── */

static MunitResult test_off_lights_nothing(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Off is always realisable, whatever is fitted. */
    for (uint8_t avail = 0; avail <= NAV_LIGHT_ALL; avail++) {
        NavResolution r = nav_lights_resolve(NAV_LIGHTS_MODE_OFF, lights(avail));
        if (r.lights_mask.raw != 0 || r.error != 0) {
            munit_errorf("off resolved to 0x%02X (error %u) with 0x%02X available",
                         r.lights_mask.raw, r.error, avail);
        }
    }
    return MUNIT_OK;
}

/* ── Primary patterns ─────────────────────────────────────────────────── */

static MunitResult test_primary_patterns_win_when_available(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    NavResolution r;

    r = nav_lights_resolve(NAV_LIGHTS_MODE_ANCHORING, lights(NAV_LIGHT_ALL));
    assert_uint8(r.error, ==, 0);
    assert_uint8(r.lights_mask.raw, ==, NAV_LIGHT_ANCHORING);

    r = nav_lights_resolve(NAV_LIGHTS_MODE_STEAMING, lights(NAV_LIGHT_ALL));
    assert_uint8(r.error, ==, 0);
    assert_uint8(r.lights_mask.raw, ==, NAV_LIGHT_STERN | NAV_LIGHT_BOW | NAV_LIGHT_STEAMING);

    /* Running under sail: the tricolour at the masthead replaces the
     * separate bow and stern lights. */
    r = nav_lights_resolve(NAV_LIGHTS_MODE_RUNNING, lights(NAV_LIGHT_ALL));
    assert_uint8(r.error, ==, 0);
    assert_uint8(r.lights_mask.raw, ==, NAV_LIGHT_TRICOLOR);
    return MUNIT_OK;
}

/* ── Fallbacks ────────────────────────────────────────────────────────── */

/*
 * A boat without a tricolour still has to be able to run: the fallback is the
 * separate bow and stern pair, which is the same signature seen from outside.
 */
static MunitResult test_running_falls_back_to_bow_and_stern(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    NavResolution r = nav_lights_resolve(NAV_LIGHTS_MODE_RUNNING,
                                         lights(NAV_LIGHT_BOW | NAV_LIGHT_STERN | NAV_LIGHT_ANCHORING));
    assert_uint8(r.error, ==, 0);
    assert_uint8(r.lights_mask.raw, ==, NAV_LIGHT_BOW | NAV_LIGHT_STERN);
    return MUNIT_OK;
}

static MunitResult test_anchoring_falls_back(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    NavResolution r = nav_lights_resolve(NAV_LIGHTS_MODE_ANCHORING,
                                         lights(NAV_LIGHT_STEAMING | NAV_LIGHT_STERN));
    assert_uint8(r.error, ==, 0);
    assert_uint8(r.lights_mask.raw, ==, NAV_LIGHT_STEAMING | NAV_LIGHT_STERN);
    return MUNIT_OK;
}

static MunitResult test_steaming_falls_back(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    NavResolution r = nav_lights_resolve(NAV_LIGHTS_MODE_STEAMING,
                                         lights(NAV_LIGHT_BOW | NAV_LIGHT_ANCHORING));
    assert_uint8(r.error, ==, 0);
    assert_uint8(r.lights_mask.raw, ==, NAV_LIGHT_BOW | NAV_LIGHT_ANCHORING);
    return MUNIT_OK;
}

/* ── Unrealisable ─────────────────────────────────────────────────────── */

/*
 * With nothing fitted, every mode but off must report an error.  Silently
 * resolving to "no lights" would leave the panel showing a nav mode as
 * selected while the boat runs dark.
 */
static MunitResult test_nothing_fitted_is_an_error(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const NavLightsMode modes[] = {NAV_LIGHTS_MODE_ANCHORING, NAV_LIGHTS_MODE_STEAMING, NAV_LIGHTS_MODE_RUNNING};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        NavResolution r = nav_lights_resolve(modes[i], lights(0));
        assert_uint8(r.error, ==, 1);
        assert_uint8(r.lights_mask.raw, ==, 0);
    }
    return MUNIT_OK;
}

static MunitResult test_partial_pattern_is_not_used(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Steaming needs stern + bow + steaming, or bow + anchoring.  With only
     * the steaming light fitted, neither is complete — and lighting the
     * steaming light alone would signal a power-driven vessel with no
     * sidelights, which is a different vessel entirely. */
    NavResolution r = nav_lights_resolve(NAV_LIGHTS_MODE_STEAMING, lights(NAV_LIGHT_STEAMING));
    assert_uint8(r.error, ==, 1);
    assert_uint8(r.lights_mask.raw, ==, 0);
    return MUNIT_OK;
}

/* ── Exhaustive properties ────────────────────────────────────────────── */

/*
 * Whatever comes out must be a subset of what is fitted, and a resolution
 * that reports an error must light nothing.  Sweep every mode against every
 * possible set of fitted lights.
 */
static MunitResult test_result_is_always_a_subset_of_available(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint8_t mode = NAV_LIGHTS_MODE_OFF; mode <= NAV_LIGHTS_MODE_RUNNING; mode++) {
        for (uint8_t avail = 0; avail <= NAV_LIGHT_ALL; avail++) {
            NavResolution r = nav_lights_resolve((NavLightsMode)mode, lights(avail));
            if (r.lights_mask.raw & (uint8_t)~avail) {
                munit_errorf("mode %u with 0x%02X fitted lit 0x%02X — includes a light that is not there",
                             mode, avail, r.lights_mask.raw);
            }
            if (r.error && r.lights_mask.raw != 0) {
                munit_errorf("mode %u with 0x%02X fitted reported an error but still lit 0x%02X",
                             mode, avail, r.lights_mask.raw);
            }
            if (!r.error && mode != NAV_LIGHTS_MODE_OFF && r.lights_mask.raw == 0) {
                munit_errorf("mode %u with 0x%02X fitted lit nothing without reporting an error", mode, avail);
            }
        }
    }
    return MUNIT_OK;
}

/* Adding a light can never take away a working resolution. */
static MunitResult test_more_lights_never_hurts(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    for (uint8_t mode = NAV_LIGHTS_MODE_ANCHORING; mode <= NAV_LIGHTS_MODE_RUNNING; mode++) {
        for (uint8_t avail = 0; avail <= NAV_LIGHT_ALL; avail++) {
            if (nav_lights_resolve((NavLightsMode)mode, lights(avail)).error) {
                continue;
            }
            for (uint8_t extra = 0; extra < 5; extra++) {
                const uint8_t more = (uint8_t)(avail | (1u << extra));
                if (nav_lights_resolve((NavLightsMode)mode, lights(more)).error) {
                    munit_errorf("mode %u works with 0x%02X but fails with 0x%02X", mode, avail, more);
                }
            }
        }
    }
    return MUNIT_OK;
}

/* Resolution depends only on the inputs — there is no hidden state to carry
 * a previous answer into the next one. */
static MunitResult test_resolution_is_pure(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    NavResolution a = nav_lights_resolve(NAV_LIGHTS_MODE_RUNNING, lights(NAV_LIGHT_ALL));
    nav_lights_resolve(NAV_LIGHTS_MODE_STEAMING, lights(0));
    nav_lights_resolve(NAV_LIGHTS_MODE_ANCHORING, lights(NAV_LIGHT_ANCHORING));
    NavResolution b = nav_lights_resolve(NAV_LIGHTS_MODE_RUNNING, lights(NAV_LIGHT_ALL));
    assert_uint8(a.lights_mask.raw, ==, b.lights_mask.raw);
    assert_uint8(a.error, ==, b.error);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/off_lights_nothing", test_off_lights_nothing),
    T("/primary_patterns_win_when_available", test_primary_patterns_win_when_available),
    T("/running_falls_back_to_bow_and_stern", test_running_falls_back_to_bow_and_stern),
    T("/anchoring_falls_back", test_anchoring_falls_back),
    T("/steaming_falls_back", test_steaming_falls_back),
    T("/nothing_fitted_is_an_error", test_nothing_fitted_is_an_error),
    T("/partial_pattern_is_not_used", test_partial_pattern_is_not_used),
    T("/result_is_always_a_subset_of_available", test_result_is_always_a_subset_of_available),
    T("/more_lights_never_hurts", test_more_lights_never_hurts),
    T("/resolution_is_pure", test_resolution_is_pure),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite b3_nav_suite(void) {
    MunitSuite s = {"/board3/nav_lights", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
