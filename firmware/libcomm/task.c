#include "task.h"

#include "libcomm.h" /* INTERRUPT_PUSH / INTERRUPT_POP */

#include <xc.h>

/* Linear scan of the task table. Safe to run unguarded against tick (tick
 * never writes id), but every external caller that mutates anything calls
 * find_slot under INTERRUPT_PUSH so the result is stable for the rest of
 * the critical section. Any context. */
static Task* find_slot(TaskController* c, TaskId id) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        if (c->tasks[i].id == id) {
            return &c->tasks[i];
        }
    }
    return 0;
}

/* Main-loop / init context only. No INTERRUPT_PUSH because the caller is
 * expected to run this before interrupts are enabled — there is no ISR
 * that could observe a half-initialised controller. */
void task_controller_init(TaskController* c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        c->tasks[i].id = TASK_INVALID_ID;
        c->tasks[i].pending = 0;
    }
    c->deferred_head = 0;
    c->deferred_tail = 0;
}

/* add / remove / set_interval mutate shared state that the TMR0 tick ISR
 * also touches; INTERRUPT_PUSH/POP keeps the writes atomic both against
 * that ISR and against a higher-priority ISR that might pre-empt one of
 * these calls. The "publish id last" pattern in add is belt-and-braces for
 * the latter case — with interrupts masked here it's redundant, but it
 * keeps the slot in a consistent state if anyone ever calls add without
 * the guard (e.g. cross-controller wiring). */

int8_t task_controller_add(TaskController* c, TaskId id, uint16_t interval_ms, TaskCallback cb, void* context) {
    if (id == TASK_INVALID_ID) {
        return -4;
    }
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS) {
        return -3;
    }

    INTERRUPT_PUSH;
    int8_t ret = 0;
    if (find_slot(c, id) != 0) {
        ret = -2;
    } else {
        Task* slot = find_slot(c, TASK_INVALID_ID);
        if (slot == 0) {
            ret = -1;
        } else {
            slot->interval_ms = interval_ms;
            slot->remaining_ms = interval_ms;
            slot->callback = cb;
            slot->context = context;
            slot->pending = 0;
            slot->id = id; /* publish last — tick skips slots until id is set */
        }
    }
    INTERRUPT_POP;
    return ret;
}

int8_t task_controller_remove(TaskController* c, TaskId id) {
    INTERRUPT_PUSH;
    Task* slot = find_slot(c, id);
    int8_t ret = slot ? 0 : -2;
    if (slot) {
        slot->id = TASK_INVALID_ID;
    }
    INTERRUPT_POP;
    return ret;
}

int8_t task_controller_set_interval(TaskController* c, TaskId id, uint16_t interval_ms) {
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS) {
        return -3;
    }
    INTERRUPT_PUSH;
    Task* slot = find_slot(c, id);
    int8_t ret = slot ? 0 : -2;
    if (slot) {
        slot->interval_ms = interval_ms;
    }
    INTERRUPT_POP;
    return ret;
}

/* TMR0 ISR context (1 ms tick). Decrements every active task's remaining_ms
 * by 1 and flips pending when it reaches zero. Already-pending slots are
 * skipped so remaining_ms freezes at 0 from the moment tick marks the
 * task until poll dispatches it — the next reload happens in poll, not
 * here. No callbacks run from this path, keeping ISR latency bounded. */
void task_controller_tick(TaskController* c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task* t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID) {
            continue;
        }
        if (t->pending) {
            continue;
        }
        if (--t->remaining_ms == 0) {
            t->pending = 1;
        }
    }
}

/* ISR-safe enqueue onto the single-producer / single-consumer ring used by
 * task_controller_poll. INTERRUPT_PUSH/POP keeps the tail bump atomic if
 * a higher-priority ISR pre-empts mid-call. Returns -1 when the ring is
 * full (head catches tail+1) so the caller can decide whether to retry,
 * coalesce, or set a sticky "scheduled" flag. */
int8_t run_in_main_loop(TaskController* c, MainLoopCallback cb, void* context) {
    INTERRUPT_PUSH;
    uint8_t next = (uint8_t)((c->deferred_tail + 1) & (TASK_DEFERRED_QUEUE_SIZE - 1));
    int8_t ret;
    if (next == c->deferred_head) {
        ret = -1;
    } else {
        c->deferred[c->deferred_tail].cb = cb;
        c->deferred[c->deferred_tail].context = context;
        c->deferred_tail = next;
        ret = 0;
    }
    INTERRUPT_POP;
    return ret;
}

/* Main-loop context only. Two phases:
 *   1. Drain the deferred queue up to its snapshot tail. Entries added by
 *      run_in_main_loop *during* dispatch wait for the next poll, which
 *      bounds work per call and prevents unbounded recursion when a
 *      callback re-defers itself.
 *   2. Walk the task table once; for each pending slot, snapshot id and
 *      callback then invoke. After the callback returns, reload
 *      remaining_ms and clear pending only if the slot is still the same
 *      task — letting the callback safely remove or replace itself.
 *      Reading interval_ms after the callback means a callback can
 *      adjust its own cadence (task_controller_set_interval) and the new
 *      value takes effect on the next fire. */
void task_controller_poll(TaskController* c) {
    uint8_t tail = c->deferred_tail;
    while (c->deferred_head != tail) {
        MainLoopCallback cb = c->deferred[c->deferred_head].cb;
        void* ctx = c->deferred[c->deferred_head].context;
        c->deferred_head = (uint8_t)((c->deferred_head + 1) & (TASK_DEFERRED_QUEUE_SIZE - 1));
        if (cb) {
            cb(ctx);
        }
    }

    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task* t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID || !t->pending) {
            continue;
        }

        TaskId id = t->id;
        TaskCallback cb = t->callback;
        void* ctx = t->context;
        cb(id, ctx);

        if (t->id == id && t->pending) {
            t->remaining_ms = t->interval_ms;
            t->pending = 0;
        }
    }
}
