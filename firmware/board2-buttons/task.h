#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX_COUNT   16
#define TASK_INVALID_ID  0xFF
#define TASK_MIN_MS      1
#define TASK_MAX_MS      15000

typedef uint8_t TaskId;
typedef void (*TaskCallback)(TaskId id, void *context);

typedef struct {
    TaskId        id;             /* TASK_INVALID_ID = free slot */
    uint8_t       pending;        /* set by tick ISR, cleared by poll */
    uint16_t      remaining_ms;
    uint16_t      interval_ms;
    TaskCallback  callback;
    void         *context;
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

void   task_controller_init        (TaskController *c);
int8_t task_controller_add         (TaskController *c, TaskId id,
                                    uint16_t interval_ms,
                                    TaskCallback cb, void *context);
int8_t task_controller_remove      (TaskController *c, TaskId id);
int8_t task_controller_set_interval(TaskController *c, TaskId id,
                                    uint16_t interval_ms);

/* Configures TMR0 as the 1 ms tick source, installs the ISR, and starts
 * the timer. Call after task_controller_init and any initial add()s. */
void   task_controller_start       (TaskController *c);

/* Call from the main loop. Dispatches any pending callbacks. */
void   task_controller_poll        (TaskController *c);

/* Example:
 *
 *   static TaskController ctrl;
 *   enum { TASK_BLINK_L, TASK_BLINK_R, TASK_ONE_SHOT };
 *
 *   static void blink(TaskId id, void *ctx) {
 *       (void)id;
 *       led_toggle((LedId)(uintptr_t)ctx);
 *   }
 *
 *   static void one_shot(TaskId id, void *ctx) {
 *       (void)ctx;
 *       do_work();
 *       task_controller_remove(&ctrl, id);         // safe from callback
 *   }
 *
 *   static void ramp(TaskId id, void *ctx) {
 *       uint16_t *period = ctx;
 *       step_brightness();
 *       if (*period > 20) {
 *           *period -= 5;
 *           task_controller_set_interval(&ctrl, id, *period);  // takes
 *       }                                                      // effect next fire
 *   }
 *
 *   int main(void) {
 *       task_controller_init(&ctrl);
 *       task_controller_add(&ctrl, TASK_BLINK_L, 500, blink, (void *)LED_L);
 *       task_controller_add(&ctrl, TASK_ONE_SHOT, 2000, one_shot, 0);
 *       task_controller_start(&ctrl);
 *       interrupt_init();
 *       while (1) {
 *           task_controller_poll(&ctrl);
 *       }
 *   }
 */

#endif /* TASK_H */
