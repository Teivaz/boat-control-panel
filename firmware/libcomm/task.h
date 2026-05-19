#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX_COUNT 16
#define TASK_INVALID_ID 0xFF
#define TASK_MIN_MS 1
#define TASK_MAX_MS 15000

/* Capacity of the ISR → main-loop deferred queue. Bursts beyond this are
 * dropped — callers that require lossless delivery must dedupe on their
 * side (set a "scheduled" flag cleared by the callback). */
#define TASK_DEFERRED_QUEUE_SIZE 4

typedef uint8_t TaskId;
typedef void (*TaskCallback)(TaskId id, void* context);
typedef void (*MainLoopCallback)(void* context);

typedef struct {
    TaskId id;       /* TASK_INVALID_ID = free slot */
    uint8_t pending; /* set by tick, cleared by poll */
    uint16_t remaining_ms;
    uint16_t interval_ms;
    TaskCallback callback;
    void* context;
} Task;

typedef struct {
    MainLoopCallback cb;
    void* context;
} DeferredEntry;

typedef struct {
    Task tasks[TASK_MAX_COUNT];
    /* One-shot deferred queue; producer = ISR, consumer = poll. */
    volatile DeferredEntry deferred[TASK_DEFERRED_QUEUE_SIZE];
    volatile uint8_t deferred_head; /* consumer owns */
    volatile uint8_t deferred_tail; /* producer owns */
} TaskController;

/* Return codes:
 *   0  ok
 *  -1  table full
 *  -2  id already in use (add) / not found (remove, set_interval)
 *  -3  interval out of 1..15000 range
 *  -4  id == TASK_INVALID_ID
 */

/* Call once at boot, before interrupts are enabled. Resets the task table
 * and the deferred queue with no synchronisation — safe only because no
 * ISR can run yet. Main-loop / init context only. */
void task_controller_init(TaskController* c);

/* Mutators for the task table. Internally guarded by INTERRUPT_PUSH/POP so
 * they are safe in any context: main loop, inside a task callback, or
 * from an ISR (including a higher-priority ISR that pre-empts the TMR0
 * tick ISR). In practice every caller in this codebase is in main-loop
 * context — set_interval is routinely called from inside a task callback
 * to re-pace itself. */
int8_t task_controller_add(TaskController* c, TaskId id, uint16_t interval_ms, TaskCallback cb, void* context);
int8_t task_controller_remove(TaskController* c, TaskId id);
int8_t task_controller_set_interval(TaskController* c, TaskId id, uint16_t interval_ms);

/* Call from the board's 1 ms timer ISR (TMR0). Walks the task table
 * decrementing each active task's remaining_ms by 1 and marking pending
 * when it reaches zero. Never runs a user callback — those fire later
 * from task_controller_poll — so the ISR's run time stays bounded and
 * independent of how many tasks happen to fire on the same tick.
 * ISR context only. */
void task_controller_tick(TaskController* c);

/* Drain whatever the ISR side has queued, then dispatch pending periodic
 * tasks. Main-loop context only. Each callback runs to completion before
 * the next is dispatched; heavy callbacks block subsequent ones, so keep
 * them short or hand work off via run_in_main_loop. */
void task_controller_poll(TaskController* c);

/* Enqueue cb(context) for one-shot execution on the next
 * task_controller_poll. The intended caller is an interrupt handler that
 * wants to push non-trivial work out of ISR context; calling from the
 * main loop also works (it'll dispatch later in the same poll cycle).
 * Returns 0 on success, -1 if the deferred queue is full — callers that
 * require lossless delivery must dedupe with a flag the callback clears.
 * ISR-safe. */
int8_t run_in_main_loop(TaskController* c, MainLoopCallback cb, void* context);

#endif /* TASK_H */
