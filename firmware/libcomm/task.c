#include "task.h"

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

int8_t task_controller_add(TaskController *c, TaskId id,
                           uint16_t interval_ms,
                           TaskCallback cb, void *context) {
    if (id == TASK_INVALID_ID)                                return -4;
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS) return -3;
    if (find_slot(c, id) != 0)                                return -2;

    Task *slot = find_slot(c, TASK_INVALID_ID);
    if (slot == 0) return -1;

    slot->interval_ms  = interval_ms;
    slot->remaining_ms = interval_ms;
    slot->callback     = cb;
    slot->context      = context;
    slot->pending      = 0;
    slot->id           = id;   /* publish last — tick skips slots until id is set */
    return 0;
}

int8_t task_controller_remove(TaskController *c, TaskId id) {
    Task *slot = find_slot(c, id);
    if (slot == 0) return -2;
    slot->id = TASK_INVALID_ID;
    return 0;
}

int8_t task_controller_set_interval(TaskController *c, TaskId id,
                                    uint16_t interval_ms) {
    if (interval_ms < TASK_MIN_MS || interval_ms > TASK_MAX_MS) return -3;
    Task *slot = find_slot(c, id);
    if (slot == 0) return -2;
    slot->interval_ms = interval_ms;
    return 0;
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
