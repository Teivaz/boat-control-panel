#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX_COUNT 16
#define TASK_INVALID_ID 0xFF
#define TASK_MIN_MS 1
#define TASK_MAX_MS 15000

typedef uint8_t TaskId;
typedef void (*TaskCallback)(TaskId id, void *context);

typedef struct {
    TaskId id;       /* TASK_INVALID_ID = free slot */
    uint8_t pending; /* set by tick, cleared by poll */
    uint16_t remaining_ms;
    uint16_t interval_ms;
    TaskCallback callback;
    void *context;
} Task;

typedef struct {
    Task tasks[TASK_MAX_COUNT];
} TaskController;

/* Return codes:
 *   0  ok
 *  -1  table full
 *  -2  id already in use (add) / not found (remove, set_interval)
 *  -3  interval out of 1..15000 range
 *  -4  id == TASK_INVALID_ID
 */

void task_controller_init(TaskController *c);
int8_t task_controller_add(TaskController *c, TaskId id, uint16_t interval_ms,
                           TaskCallback cb, void *context);
int8_t task_controller_remove(TaskController *c, TaskId id);
int8_t task_controller_set_interval(TaskController *c, TaskId id,
                                    uint16_t interval_ms);

/* Call from the board's 1 ms timer ISR. Never invokes callbacks. */
void task_controller_tick(TaskController *c);

/* Call from the main loop. Dispatches any pending callbacks. */
void task_controller_poll(TaskController *c);

#endif /* TASK_H */
