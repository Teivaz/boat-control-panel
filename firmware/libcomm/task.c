#include "task.h"

#include "libcomm.h" /* INTERRUPT_PUSH / INTERRUPT_POP */

static Task *find_slot(TaskController *c, TaskId id) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        if (c->tasks[i].id == id)
            return &c->tasks[i];
    }
    return 0;
}

void task_controller_init(TaskController *c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        c->tasks[i].id = TASK_INVALID_ID;
        c->tasks[i].pending = 0;
    }
    c->deferred_head = 0;
    c->deferred_tail = 0;
}

/* add/remove/set_interval are ISR-callable: they mutate shared state that
 * `tick` also reads. The guards keep those writes atomic relative to both
 * the tick ISR and any preempting producer on the same controller. */

int8_t task_controller_add(TaskController *c, TaskId id, uint16_t interval_ms,
                           TaskCallback cb, void *context) {
    if (id == TASK_INVALID_ID)
        return -4;
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS)
        return -3;

    INTERRUPT_PUSH;
    int8_t ret = 0;
    if (find_slot(c, id) != 0) {
        ret = -2;
    } else {
        Task *slot = find_slot(c, TASK_INVALID_ID);
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

int8_t task_controller_remove(TaskController *c, TaskId id) {
    INTERRUPT_PUSH;
    Task *slot = find_slot(c, id);
    int8_t ret = slot ? 0 : -2;
    if (slot)
        slot->id = TASK_INVALID_ID;
    INTERRUPT_POP;
    return ret;
}

int8_t task_controller_set_interval(TaskController *c, TaskId id,
                                    uint16_t interval_ms) {
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS)
        return -3;
    INTERRUPT_PUSH;
    Task *slot = find_slot(c, id);
    int8_t ret = slot ? 0 : -2;
    if (slot)
        slot->interval_ms = interval_ms;
    INTERRUPT_POP;
    return ret;
}

/* ISR-callable. */
void task_controller_tick(TaskController *c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task *t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID)
            continue;
        if (t->pending)
            continue;
        if (--t->remaining_ms == 0)
            t->pending = 1;
    }
}

/* ISR-callable — INTERRUPT_PUSH/POP keeps the tail bump atomic against a
 * preempting producer on the same controller. Callbacks enqueued during a
 * poll iteration are picked up on the next poll. */
int8_t run_in_main_loop(TaskController *c, MainLoopCallback cb, void *context) {
    INTERRUPT_PUSH;
    uint8_t next = (uint8_t) ((c->deferred_tail + 1) & (TASK_DEFERRED_QUEUE_SIZE - 1));
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

/* Drain deferred entries queued by run_in_main_loop first, then dispatch any
 * pending periodic tasks. Snapshot the tail once so entries queued *during*
 * dispatch wait for the next poll — bounds work per call and prevents
 * unbounded recursion when a callback defers itself. */
void task_controller_poll(TaskController *c) {
    uint8_t tail = c->deferred_tail;
    while (c->deferred_head != tail) {
        MainLoopCallback cb = c->deferred[c->deferred_head].cb;
        void *ctx = c->deferred[c->deferred_head].context;
        c->deferred_head = (uint8_t) ((c->deferred_head + 1) & (TASK_DEFERRED_QUEUE_SIZE - 1));
        if (cb)
            cb(ctx);
    }

    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task *t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID || !t->pending)
            continue;

        TaskId id = t->id;
        TaskCallback cb = t->callback;
        void *ctx = t->context;
        cb(id, ctx);

        if (t->id == id && t->pending) {
            t->remaining_ms = t->interval_ms;
            t->pending = 0;
        }
    }
}
