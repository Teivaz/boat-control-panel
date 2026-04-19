#include "comm.h"
#include "i2c.h"
#include "libcomm.h"
#include "button.h"
#include "led_effect.h"
#include "config.h"
#include "input.h"
#include "task.h"
#include "task_ids.h"

#include <xc.h>

/* Outbound retry queue for button_changed. Enqueue is ISR-safe and happens
 * on every edge/timer fire; actual transmission runs in main context on a
 * periodic task. Size 8 covers bursty multi-button transitions. Drops on
 * overflow — the main board can still resync via button_state_read. */
#define RETRY_QUEUE_SIZE   8
#define RETRY_QUEUE_MASK   (RETRY_QUEUE_SIZE - 1)
#define RETRY_TICK_MS      TASK_MIN_MS   /* drain as soon as main context polls */

typedef struct {
    uint8_t prev;
    uint8_t curr;
} ButtonChange;

static volatile ButtonChange queue[RETRY_QUEUE_SIZE];
static volatile uint8_t      q_head;   /* consumer (retry_task) owns this */
static volatile uint8_t      q_tail;   /* producer (any caller of _send_*) owns this */

static void    on_rx     (const uint8_t *data, uint8_t len);
static uint8_t on_read   (const uint8_t *request, uint8_t request_len,
                          uint8_t *response, uint8_t response_max);
static void    retry_task(TaskId id, void *ctx);
static void    apply_button_effect(const CommButtonEffect *eff);

void comm_init(TaskController *ctrl) {
    q_head = q_tail = 0;
    i2c_set_rx_handler  (on_rx);
    i2c_set_read_handler(on_read);
    task_controller_add(ctrl, TASK_COMM_RETRY, RETRY_TICK_MS, retry_task, 0);
}

/* Callable from any context. INTERRUPT_PUSH/POP covers the tail bump against
 * preempting producers; the consumer reads `head` independently. */
void comm_send_button_changed(uint8_t prev_state, uint8_t current_state) {
    INTERRUPT_PUSH;
    uint8_t next = (uint8_t)((q_tail + 1) & RETRY_QUEUE_MASK);
    if (next != q_head) {
        queue[q_tail].prev = prev_state;
        queue[q_tail].curr = current_state;
        q_tail = next;
    }
    INTERRUPT_POP;
}

/* Drain oldest-first. Stop on the first non-OK transmit so the failed entry
 * stays queued for the next tick; transient collisions clear within a few ms
 * on a contested bus. i2c_transmit runs with only the I2C IRQ group masked,
 * so the scheduler tick keeps advancing while a transfer is in flight. */
static void retry_task(TaskId id, void *ctx) {
    (void)id; (void)ctx;
    while (q_head != q_tail) {
        CommMessage msg;
        uint8_t len = comm_build_button_changed(&msg,
                                                queue[q_head].prev,
                                                queue[q_head].curr);
        if (i2c_transmit(COMM_ADDRESS_MAIN,
                         (const uint8_t *)&msg, len) != I2C_RESULT_OK) break;
        q_head = (uint8_t)((q_head + 1) & RETRY_QUEUE_MASK);
    }
}

/* ============================================================================
 * Inbound dispatch (ISR context)
 *
 * Handlers must stay short — any work beyond a few microseconds stretches
 * the I2C clock. config_write_byte is non-blocking (deferred to the flush
 * task); button_set_trigger is an in-RAM operation.
 * ============================================================================ */

static void on_rx(const uint8_t *data, uint8_t len) {
    if (len == 0) return;
    switch (data[0]) {
        case COMM_BUTTON_EFFECT:
            if (len == 1 + sizeof(CommButtonEffect)) {
                CommButtonEffect eff;
                comm_parse_button_effect(&data[1], &eff);
                apply_button_effect(&eff);
            }
            break;

        case COMM_BUTTON_TRIGGER:
            if (len == 1 + sizeof(CommButtonTrigger)) {
                CommButtonTrigger trg;
                comm_parse_button_trigger_write(&data[1], &trg);
                button_set_trigger(trg.button_id, trg.config);
            }
            break;

        case COMM_CONFIG:
            if (len == 1 + sizeof(CommConfig)) {
                CommConfig cfg;
                comm_parse_config_write(&data[1], &cfg);
                config_write_byte(cfg.address, cfg.value);
            }
            break;

        case COMM_RESET:
            RESET();                    /* software reset; does not return */
            break;

        default:
            break;
    }
}

static uint8_t on_read(const uint8_t *request, uint8_t request_len,
                       uint8_t *response, uint8_t response_max) {
    if (request_len == 0 || response_max == 0) return 0;

    switch (request[0]) {
        case COMM_BUTTON_STATE_READ:
            response[0] = input_state_current().integer;
            return 1;

        case COMM_BUTTON_TRIGGER_READ:
            if (request_len >= 2) {
                CommTriggerConfig cfg = button_get_trigger(request[1] & 0x07);
                response[0] = *(uint8_t *)&cfg;
                return 1;
            }
            break;

        case COMM_CONFIG_READ:
            if (request_len >= 2) {
                response[0] = config_read_byte(request[1]);
                return 1;
            }
            break;

        default:
            break;
    }
    return 0;
}

static void apply_button_effect(const CommButtonEffect *eff) {
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        CommButtonOutputEffect out;
        if (comm_button_effect_get(eff, i, &out) == 0) {
            led_effect_set(i, out);
        }
    }
}
