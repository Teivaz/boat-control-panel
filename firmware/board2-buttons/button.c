#include "button.h"

#include "input.h"
#include "task_ids.h"

/* Per-button FSM states.
 *
 *   mode=CHANGE  → always IDLE. edge → fire.
 *   mode=HOLD    → IDLE -press→ HOLD_WAIT -timer→ HOLD_FIRED -release→ IDLE
 *                           (time_ms=0 on press: skip HOLD_WAIT, go straight
 *                            to HOLD_FIRED and fire)
 *   mode=RELEASE → IDLE -press→ REL_WAIT -timer→ REL_READY -release→ IDLE
 * (fire) -release from REL_WAIT→ IDLE (no event) (time_ms=0 on press: skip
 * REL_WAIT, go to REL_READY)
 */
typedef enum {
    FSM_IDLE,
    FSM_HOLD_WAIT,
    FSM_HOLD_FIRED,
    FSM_REL_WAIT,
    FSM_REL_READY,
} ButtonFsm;

typedef struct {
    TaskId task_id;
    CommButtonMode mode;
    uint16_t time_ms;
    ButtonFsm fsm;
} ButtonState;

static TaskController* ctrl = 0;
static ButtonState buttons[BUTTON_COUNT];

static const TaskId button_task_ids[BUTTON_COUNT] = {
    TASK_BUTTON_0, TASK_BUTTON_1, TASK_BUTTON_2, TASK_BUTTON_3, TASK_BUTTON_4, TASK_BUTTON_5, TASK_BUTTON_6,
};

static void button_timer_cb(TaskId id, void* context);
static void on_input_changed(uint8_t prev, uint8_t curr);
static void dispatch_edge(ButtonState* b, uint8_t pressed);
static void dispatch_timer(ButtonState* b);
static void arm_timer(ButtonState* b, uint16_t ms);
static void cancel_timer(ButtonState* b);
static uint8_t button_index(const ButtonState* b) {
    return (uint8_t)(b - buttons);
}

void button_init(TaskController* c) {
    ctrl = c;
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].task_id = button_task_ids[i];
        buttons[i].mode = COMM_BUTTON_MODE_UNKNOWN;
        buttons[i].time_ms = 0;
        buttons[i].fsm = FSM_IDLE;
    }
    input_set_change_handler(on_input_changed);
}

void button_set_trigger(uint8_t button_id, CommTriggerConfig cfg) {
    if (button_id >= BUTTON_COUNT) {
        return;
    }
    ButtonState* b = &buttons[button_id];
    const uint16_t new_time = comm_button_trigger_time_ms(cfg);
    const uint8_t mode_diff = (cfg.mode != b->mode);
    const uint8_t time_diff = (new_time != b->time_ms);

    if (mode_diff) {
        cancel_timer(b);
        b->fsm = FSM_IDLE;
    }
    b->mode = cfg.mode;
    b->time_ms = new_time;

    /* time-only change while a timer is armed: re-arm with the new value,
     * collapsing through the zero case as on the original press. */
    if (!mode_diff && time_diff && (b->fsm == FSM_HOLD_WAIT || b->fsm == FSM_REL_WAIT)) {
        cancel_timer(b);
        if (new_time == 0) {
            if (b->fsm == FSM_HOLD_WAIT) {
                send_button_event(button_index(b));
                b->fsm = FSM_HOLD_FIRED;
            } else {
                b->fsm = FSM_REL_READY;
            }
        } else {
            arm_timer(b, new_time);
        }
    }
}

CommTriggerConfig button_get_trigger(uint8_t button_id) {
    CommTriggerConfig cfg = {0};
    if (button_id < BUTTON_COUNT) {
        cfg = comm_button_trigger_make(buttons[button_id].mode, buttons[button_id].time_ms);
    }
    return cfg;
}

/* Runs in the IOC ISR context. Timeout-driven FSM transitions are the
 * only work that still flows through the task scheduler. */
static void on_input_changed(uint8_t prev, uint8_t curr) {
    // TODO: Simplify this interrupt. Remove call dispatch_edge, instead add a function to run on main loop and put it there
    uint8_t changed = prev ^ curr;
    for (uint8_t i = 0; i < BUTTON_COUNT && changed; i++, changed >>= 1) {
        if (changed & 1) {
            dispatch_edge(&buttons[i], (curr >> i) & 1);
        }
    }
}

static void arm_timer(ButtonState* b, uint16_t ms) {
    task_controller_add(ctrl, b->task_id, ms, button_timer_cb, b);
}

static void cancel_timer(ButtonState* b) {
    task_controller_remove(ctrl, b->task_id);
}

static void button_timer_cb(TaskId id, void* context) {
    task_controller_remove(ctrl, id); /* one-shot */
    dispatch_timer((ButtonState*)context);
}

static void dispatch_edge(ButtonState* b, uint8_t pressed) {
    const uint8_t i = button_index(b);

    switch (b->mode) {
        case COMM_BUTTON_MODE_CHANGE:
            send_button_event(i);
            break;

        case COMM_BUTTON_MODE_HOLD:
            if (pressed) {
                if (b->fsm != FSM_IDLE) {
                    break; /* stray edge */
                }
                if (b->time_ms == 0) {
                    send_button_event(i);
                    b->fsm = FSM_HOLD_FIRED;
                } else {
                    arm_timer(b, b->time_ms);
                    b->fsm = FSM_HOLD_WAIT;
                }
            } else {
                if (b->fsm == FSM_HOLD_WAIT) {
                    cancel_timer(b);
                }
                b->fsm = FSM_IDLE;
            }
            break;

        case COMM_BUTTON_MODE_RELEASE:
            if (pressed) {
                if (b->fsm != FSM_IDLE) {
                    break;
                }
                if (b->time_ms == 0) {
                    b->fsm = FSM_REL_READY;
                } else {
                    arm_timer(b, b->time_ms);
                    b->fsm = FSM_REL_WAIT;
                }
            } else {
                if (b->fsm == FSM_REL_WAIT) {
                    cancel_timer(b);
                    b->fsm = FSM_IDLE; /* released too early */
                } else if (b->fsm == FSM_REL_READY) {
                    send_button_event(i);
                    b->fsm = FSM_IDLE;
                }
            }
            break;

        default:
            b->fsm = FSM_IDLE;
            break;
    }
}

static void dispatch_timer(ButtonState* b) {
    switch (b->fsm) {
        case FSM_HOLD_WAIT:
            send_button_event(button_index(b));
            b->fsm = FSM_HOLD_FIRED;
            break;
        case FSM_REL_WAIT:
            b->fsm = FSM_REL_READY;
            break;
        default:
            /* stale fire — ignore */
            break;
    }
}
