#include "task.h"
#include "libcomm.h"   /* INTERRUPT_PUSH / INTERRUPT_POP */

static Task *find_slot(TaskController *c, TaskId id) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        if (c->tasks[i].id == id) return &c->tasks[i];
    }
    return 0;
}

void task_controller_init(TaskController *c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        c->tasks[i].id      = TASK_INVALID_ID;
        c->tasks[i].pending = 0;
    }
}

/* add/remove/set_interval are ISR-callable: they mutate shared state that
 * `tick` also reads. The guards keep those writes atomic relative to both
 * the tick ISR and any preempting producer on the same controller. */

int8_t task_controller_add(TaskController *c, TaskId id,
                           uint16_t interval_ms,
                           TaskCallback cb, void *context) {
    if (id == TASK_INVALID_ID)                                 return -4;
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS) return -3;

    INTERRUPT_PUSH;
    int8_t ret = 0;
    if (find_slot(c, id) != 0) {
        ret = -2;
    } else {
        Task *slot = find_slot(c, TASK_INVALID_ID);
        if (slot == 0) {
            ret = -1;
        } else {
            slot->interval_ms  = interval_ms;
            slot->remaining_ms = interval_ms;
            slot->callback     = cb;
            slot->context      = context;
            slot->pending      = 0;
            slot->id           = id;   /* publish last — tick skips slots until id is set */
        }
    }
    INTERRUPT_POP;
    return ret;
}

int8_t task_controller_remove(TaskController *c, TaskId id) {
    INTERRUPT_PUSH;
    Task *slot = find_slot(c, id);
    int8_t ret = slot ? 0 : -2;
    if (slot) slot->id = TASK_INVALID_ID;
    INTERRUPT_POP;
    return ret;
}

int8_t task_controller_set_interval(TaskController *c, TaskId id,
                                    uint16_t interval_ms) {
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS) return -3;
    INTERRUPT_PUSH;
    Task *slot = find_slot(c, id);
    int8_t ret = slot ? 0 : -2;
    if (slot) slot->interval_ms = interval_ms;
    INTERRUPT_POP;
    return ret;
}

void task_controller_tick(TaskController *c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task *t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID) continue;
        if (t->pending)               continue;
        if (--t->remaining_ms == 0)   t->pending = 1;
    }
}

void task_controller_poll(TaskController *c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task *t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID || !t->pending) continue;

        TaskId       id  = t->id;
        TaskCallback cb  = t->callback;
        void        *ctx = t->context;
        cb(id, ctx);

        if (t->id == id && t->pending) {
            t->remaining_ms = t->interval_ms;
            t->pending      = 0;
        }
    }
}
