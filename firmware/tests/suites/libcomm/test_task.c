#include "test_support.h"

#include "task.h"

#include <string.h>

/*
 * The cooperative scheduler every board's main loop is built on.
 *
 * The behaviours worth pinning down are the ones modules quietly depend on:
 * that a callback can remove or re-pace itself mid-dispatch (button.c's
 * one-shot timers, config.c's flush task), that the interval reload reads the
 * *current* interval, and that the ISR-to-main deferred queue drops rather
 * than corrupts when it overflows (input.c relies on that being self-healing).
 */

#define ID_A 1
#define ID_B 2
#define ID_C 3

typedef struct {
    TaskController ctrl;
    unsigned fired[8];
    TaskId last_id;
    void* last_ctx;
} Fixture;

static Fixture* g_fx; /* callbacks have no other way back to the fixture */

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    test_reset_hardware();
    Fixture* fx = munit_new(Fixture);
    memset(fx, 0, sizeof(*fx));
    task_controller_init(&fx->ctrl);
    g_fx = fx;
    return fx;
}

static void tear_down(void* fixture) {
    g_fx = NULL;
    free(fixture);
}

static void count_cb(TaskId id, void* ctx) {
    g_fx->fired[id]++;
    g_fx->last_id = id;
    g_fx->last_ctx = ctx;
}

/* ── Registration ─────────────────────────────────────────────────────── */

static MunitResult test_add_validates(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    assert_int8(task_controller_add(&fx->ctrl, TASK_INVALID_ID, 10, count_cb, NULL), ==, -4);
    assert_int8(task_controller_add(&fx->ctrl, ID_A, 0, count_cb, NULL), ==, -3);
    assert_int8(task_controller_add(&fx->ctrl, ID_A, TASK_MAX_MS + 1, count_cb, NULL), ==, -3);
    assert_int8(task_controller_add(&fx->ctrl, ID_A, TASK_MIN_MS, count_cb, NULL), ==, 0);
    assert_int8(task_controller_add(&fx->ctrl, ID_A, 10, count_cb, NULL), ==, -2); /* duplicate id */
    return MUNIT_OK;
}

static MunitResult test_table_fills_and_reports_full(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        assert_int8(task_controller_add(&fx->ctrl, (TaskId)(i + 1u), 10, count_cb, NULL), ==, 0);
    }
    assert_int8(task_controller_add(&fx->ctrl, TASK_MAX_COUNT + 1, 10, count_cb, NULL), ==, -1);

    /* Freeing one slot must make room again — the table is reusable, not
     * a one-way allocation. */
    assert_int8(task_controller_remove(&fx->ctrl, 1), ==, 0);
    assert_int8(task_controller_add(&fx->ctrl, TASK_MAX_COUNT + 1, 10, count_cb, NULL), ==, 0);
    return MUNIT_OK;
}

static MunitResult test_remove_unknown(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    assert_int8(task_controller_remove(&fx->ctrl, ID_A), ==, -2);
    assert_int8(task_controller_set_interval(&fx->ctrl, ID_A, 10), ==, -2);
    return MUNIT_OK;
}

/* ── Timing ───────────────────────────────────────────────────────────── */

static MunitResult test_fires_on_interval(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    task_controller_add(&fx->ctrl, ID_A, 10, count_cb, NULL);

    test_advance_ms(&fx->ctrl, 9);
    assert_uint(fx->fired[ID_A], ==, 0);
    test_advance_ms(&fx->ctrl, 1);
    assert_uint(fx->fired[ID_A], ==, 1);

    /* And keeps firing on the same cadence, not drifting or stopping. */
    test_advance_ms(&fx->ctrl, 30);
    assert_uint(fx->fired[ID_A], ==, 4);
    return MUNIT_OK;
}

static MunitResult test_context_is_passed_through(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    int marker = 0;
    task_controller_add(&fx->ctrl, ID_A, 1, count_cb, &marker);
    test_advance_ms(&fx->ctrl, 1);
    assert_uint8(fx->last_id, ==, ID_A);
    assert_ptr_equal(fx->last_ctx, &marker);
    return MUNIT_OK;
}

static MunitResult test_independent_intervals(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    task_controller_add(&fx->ctrl, ID_A, 3, count_cb, NULL);
    task_controller_add(&fx->ctrl, ID_B, 5, count_cb, NULL);
    test_advance_ms(&fx->ctrl, 15);
    assert_uint(fx->fired[ID_A], ==, 5);
    assert_uint(fx->fired[ID_B], ==, 3);
    return MUNIT_OK;
}

/* The tick never runs a callback — that is what bounds ISR latency no matter
 * how many tasks land on the same millisecond. */
static MunitResult test_tick_does_not_dispatch(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    task_controller_add(&fx->ctrl, ID_A, 1, count_cb, NULL);
    for (int i = 0; i < 5; i++) {
        task_controller_tick(&fx->ctrl);
    }
    assert_uint(fx->fired[ID_A], ==, 0);

    /* Five elapsed intervals collapse into one dispatch: pending is a flag,
     * not a counter, so a slow main loop drops repeats instead of building a
     * backlog it can never work off. */
    task_controller_poll(&fx->ctrl);
    assert_uint(fx->fired[ID_A], ==, 1);
    return MUNIT_OK;
}

/* ── Self-modification from inside a callback ─────────────────────────── */

static void remove_self_cb(TaskId id, void* ctx) {
    Fixture* fx = ctx;
    fx->fired[id]++;
    task_controller_remove(&fx->ctrl, id);
}

static MunitResult test_callback_can_remove_itself(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    task_controller_add(&fx->ctrl, ID_A, 2, remove_self_cb, fx);
    test_advance_ms(&fx->ctrl, 20);
    assert_uint(fx->fired[ID_A], ==, 1);
    assert_uint8(test_task_active(&fx->ctrl, ID_A), ==, 0);
    /* The slot is genuinely released, not just skipped. */
    assert_uint8(test_active_task_count(&fx->ctrl), ==, 0);
    return MUNIT_OK;
}

static void repace_cb(TaskId id, void* ctx) {
    Fixture* fx = ctx;
    fx->fired[id]++;
    if (fx->fired[id] == 1) {
        task_controller_set_interval(&fx->ctrl, id, 100);
    }
}

/* poll() reloads remaining_ms from the interval *after* the callback returns,
 * which is what lets a task re-pace itself.  led_effect and the poll tasks in
 * board3's controller both depend on this. */
static MunitResult test_callback_can_repace_itself(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    task_controller_add(&fx->ctrl, ID_A, 5, repace_cb, fx);

    test_advance_ms(&fx->ctrl, 5);
    assert_uint(fx->fired[ID_A], ==, 1);

    /* Old cadence would have fired 19 more times by now. */
    test_advance_ms(&fx->ctrl, 99);
    assert_uint(fx->fired[ID_A], ==, 1);
    test_advance_ms(&fx->ctrl, 1);
    assert_uint(fx->fired[ID_A], ==, 2);
    return MUNIT_OK;
}

static void replace_self_cb(TaskId id, void* ctx) {
    Fixture* fx = ctx;
    fx->fired[id]++;
    task_controller_remove(&fx->ctrl, id);
    task_controller_add(&fx->ctrl, ID_C, 4, count_cb, NULL);
}

/* Removing yourself and putting a different task in your place mid-dispatch
 * must not resurrect the removed one.  poll() rechecks the slot id before
 * reloading, which is the guard being tested. */
static MunitResult test_callback_can_replace_itself(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    task_controller_add(&fx->ctrl, ID_A, 2, replace_self_cb, fx);
    test_advance_ms(&fx->ctrl, 2);
    assert_uint(fx->fired[ID_A], ==, 1);
    assert_uint8(test_task_active(&fx->ctrl, ID_A), ==, 0);
    assert_uint8(test_task_active(&fx->ctrl, ID_C), ==, 1);

    test_advance_ms(&fx->ctrl, 8);
    assert_uint(fx->fired[ID_A], ==, 1); /* stayed dead */
    assert_uint(fx->fired[ID_C], ==, 2);
    return MUNIT_OK;
}

/* ── Deferred (ISR -> main) queue ─────────────────────────────────────── */

static unsigned g_deferred_runs;
static void* g_deferred_ctx;

static void deferred_cb(void* ctx) {
    g_deferred_runs++;
    g_deferred_ctx = ctx;
}

static MunitResult test_deferred_runs_on_next_poll(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    g_deferred_runs = 0;
    int marker = 0;

    assert_int8(run_in_main_loop(&fx->ctrl, deferred_cb, &marker), ==, 0);
    assert_uint(g_deferred_runs, ==, 0); /* not from the caller's context */

    task_controller_poll(&fx->ctrl);
    assert_uint(g_deferred_runs, ==, 1);
    assert_ptr_equal(g_deferred_ctx, &marker);

    /* One-shot: a second poll must not re-run it. */
    task_controller_poll(&fx->ctrl);
    assert_uint(g_deferred_runs, ==, 1);
    return MUNIT_OK;
}

/* Capacity is SIZE-1, not SIZE: the ring distinguishes full from empty by
 * leaving one slot open. */
static MunitResult test_deferred_queue_drops_when_full(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    g_deferred_runs = 0;

    for (uint8_t i = 0; i < TASK_DEFERRED_QUEUE_SIZE - 1u; i++) {
        assert_int8(run_in_main_loop(&fx->ctrl, deferred_cb, NULL), ==, 0);
    }
    assert_int8(run_in_main_loop(&fx->ctrl, deferred_cb, NULL), ==, -1);

    task_controller_poll(&fx->ctrl);
    assert_uint(g_deferred_runs, ==, TASK_DEFERRED_QUEUE_SIZE - 1u);

    /* Draining restores capacity — the drop is transient, not terminal. */
    assert_int8(run_in_main_loop(&fx->ctrl, deferred_cb, NULL), ==, 0);
    return MUNIT_OK;
}

static void self_requeue_cb(void* ctx) {
    Fixture* fx = ctx;
    g_deferred_runs++;
    if (g_deferred_runs < 10) {
        run_in_main_loop(&fx->ctrl, self_requeue_cb, fx);
    }
}

/* A callback that re-defers itself must not spin the current poll forever:
 * the drain loop snapshots the tail up front.  Without that, an ISR-fed
 * producer could starve the periodic tasks indefinitely. */
static MunitResult test_deferred_requeue_waits_for_next_poll(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    g_deferred_runs = 0;
    run_in_main_loop(&fx->ctrl, self_requeue_cb, fx);

    task_controller_poll(&fx->ctrl);
    assert_uint(g_deferred_runs, ==, 1);
    task_controller_poll(&fx->ctrl);
    assert_uint(g_deferred_runs, ==, 2);
    return MUNIT_OK;
}

/* Deferred work is drained before periodic tasks in the same poll, so an
 * edge captured by an ISR is visible to the tasks that run after it. */
static MunitResult test_deferred_drains_before_tasks(const MunitParameter p[], void* f) {
    (void)p;
    Fixture* fx = f;
    g_deferred_runs = 0;

    task_controller_add(&fx->ctrl, ID_A, 1, count_cb, NULL);
    task_controller_tick(&fx->ctrl);
    run_in_main_loop(&fx->ctrl, deferred_cb, NULL);

    task_controller_poll(&fx->ctrl);
    assert_uint(g_deferred_runs, ==, 1);
    assert_uint(fx->fired[ID_A], ==, 1);
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, tear_down, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/add_validates", test_add_validates),
    T("/table_fills_and_reports_full", test_table_fills_and_reports_full),
    T("/remove_unknown", test_remove_unknown),
    T("/fires_on_interval", test_fires_on_interval),
    T("/context_is_passed_through", test_context_is_passed_through),
    T("/independent_intervals", test_independent_intervals),
    T("/tick_does_not_dispatch", test_tick_does_not_dispatch),
    T("/callback_can_remove_itself", test_callback_can_remove_itself),
    T("/callback_can_repace_itself", test_callback_can_repace_itself),
    T("/callback_can_replace_itself", test_callback_can_replace_itself),
    T("/deferred_runs_on_next_poll", test_deferred_runs_on_next_poll),
    T("/deferred_queue_drops_when_full", test_deferred_queue_drops_when_full),
    T("/deferred_requeue_waits_for_next_poll", test_deferred_requeue_waits_for_next_poll),
    T("/deferred_drains_before_tasks", test_deferred_drains_before_tasks),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite task_suite(void) {
    MunitSuite s = {"/task", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
