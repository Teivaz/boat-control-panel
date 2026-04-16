#include "task.h"
#include "interrupt.h"

#include <xc.h>

static TaskController *active_ctrl = 0;

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
    slot->interval_ms = interval_ms;   /* applied at next reload */
    return 0;
}

static void tick(TaskController *c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task *t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID) continue;
        if (t->pending)               continue;   /* waiting for poll */
        if (--t->remaining_ms == 0)   t->pending = 1;
    }
}

static void on_tmr0(void) {
    if (active_ctrl) tick(active_ctrl);
}

/* TMR0 in 8-bit mode clocked from Fosc/4 with /16384 prescaler gives
 * 16 MHz / 16384 ≈ 976.56 Hz (~1.024 ms per count). Match value of 1 in
 * TMR0H yields a ~1 ms period interrupt. */
void task_controller_start(TaskController *c) {
    active_ctrl = c;

    T0CON0bits.EN   = 0;
    T0CON1bits.CS   = 0b010;     /* Fosc/4 -> 16 MHz */
    T0CON1bits.CKPS = 0b1110;    /* /16384 -> ~1.024 ms per count */
    interrupt_set_handler_TMR0(on_tmr0);
    PIE3bits.TMR0IE = 1;

    const uint8_t gie_save = GIE;
    GIE = 0;
    PIR3bits.TMR0IF = 0;
    TMR0L = 0;
    TMR0H = 1;                    /* match at 1 -> ~1 ms period */
    T0CON0bits.EN = 1;
    GIE = gie_save;
}

void task_controller_poll(TaskController *c) {
    for (uint8_t i = 0; i < TASK_MAX_COUNT; i++) {
        Task *t = &c->tasks[i];
        if (t->id == TASK_INVALID_ID || !t->pending) continue;

        TaskId       id  = t->id;
        TaskCallback cb  = t->callback;
        void        *ctx = t->context;
        cb(id, ctx);

        /* Reload only if still the same task (callback may have removed
         * it, or removed and re-added at this slot — in which case
         * pending was reset to 0 by add). */
        if (t->id == id && t->pending) {
            t->remaining_ms = t->interval_ms;   /* interval read AFTER callback */
            t->pending      = 0;
        }
    }
}
