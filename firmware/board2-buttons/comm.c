#include "comm.h"

#include "button.h"
#include "config.h"
#include "input.h"
#include "led_effect.h"
#include "libcomm.h"
#include "libcomm_interface.h"
#include "task_ids.h"

#include <xc.h>

#define RETRY_QUEUE_SIZE 8
#define RETRY_QUEUE_MASK (RETRY_QUEUE_SIZE - 1)
#define RETRY_TICK_MS TASK_MIN_MS

typedef struct {
    uint8_t button_id : 3;
    uint8_t pressed : 1;
    uint8_t mode : 2;
} ButtonEventEntry;

static volatile ButtonEventEntry queue[RETRY_QUEUE_SIZE];
static volatile uint8_t q_head;
static volatile uint8_t q_tail;

static void retry_task(TaskId id, void* ctx);
static void apply_button_effect(CommButtonEffect* eff);

uint8_t comm_address(void) {
    return PORTBbits.RB0 == 0 ? COMM_ADDRESS_BUTTON_BOARD_L : COMM_ADDRESS_BUTTON_BOARD_R;
}

void comm_init(TaskController* ctrl) {
    q_head = q_tail = 0;
    task_controller_add(ctrl, TASK_COMM_RETRY, RETRY_TICK_MS, retry_task, 0);
}

void comm_send_button_event(uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
    INTERRUPT_PUSH;
    uint8_t next = (uint8_t)((q_tail + 1) & RETRY_QUEUE_MASK);
    if (next != q_head) {
        queue[q_tail].button_id = button_id & 0x07;
        queue[q_tail].pressed = pressed & 0x01;
        queue[q_tail].mode = (uint8_t)mode & 0x03;
        q_tail = next;
    }
    INTERRUPT_POP;
}

static void retry_task(TaskId id, void* ctx) {
    (void)id;
    (void)ctx;
    while (q_head != q_tail) {
        I2cResult res = comm_send_button_changed(
            queue[q_head].button_id, queue[q_head].pressed,
            (CommButtonMode)queue[q_head].mode);
        if (res != I2C_RESULT_OK) {
            break;
        }
        q_head = (uint8_t)((q_head + 1) & RETRY_QUEUE_MASK);
    }
}

/* ============================================================================
 * Adopter callbacks: incoming write handlers (main-loop context)
 * ============================================================================
 */

void comm_on_button_effect_received(CommButtonEffect* effect) {
    apply_button_effect(effect);
}

void comm_on_button_trigger_received(CommButtonTrigger* trigger) {
    button_set_trigger(trigger->button_id, trigger->config);
}

void comm_on_config_received(CommConfig* config) {
    config_write_byte(config->address, config->value);
}

void comm_on_reset(void) {
    RESET();
}

void comm_on_button_changed_received(CommButtonChanged* event) {
    (void)event;
}
void comm_on_relay_state_received(CommRelayState* state) {
    (void)state;
}
void comm_on_channel_changed_received(CommChannelChanged* event) {
    (void)event;
}
void comm_on_level_mode_received(CommLevelMode* mode) {
    (void)mode;
}

/* ============================================================================
 * Adopter callbacks: incoming read-request handlers (main-loop context)
 *
 * Update the client TX buffer so the response is ready for the next
 * read from this device.
 * ============================================================================
 */

void comm_on_button_state_read_requested(void) {
    uint8_t state = input_state_current().integer;
    comm_respond(&state, 1);
}

void comm_on_button_trigger_read_requested(uint8_t button_id) {
    CommTriggerConfig cfg = button_get_trigger(button_id);
    uint8_t raw = *(uint8_t*)&cfg;
    comm_respond(&raw, 1);
}

void comm_on_config_read_requested(uint8_t address) {
    uint8_t value = config_read_byte(address);
    comm_respond(&value, 1);
}

void comm_on_relay_state_read_requested(void) {
}
void comm_on_channel_state_read_requested(void) {
}
void comm_on_battery_read_requested(void) {
}
void comm_on_levels_read_requested(void) {
}
void comm_on_level_mode_read_requested(void) {
}
void comm_on_sensors_read_requested(void) {
}

/* ============================================================================
 * Adopter callbacks: write completion handlers (main-loop context)
 *
 * Button board only initiates button_changed writes and does not react
 * to the completion.  All others are unused.
 * ============================================================================
 */

void comm_on_reset_completion(I2cResult result, uint8_t addr) {
    (void)result;
    (void)addr;
}
void comm_on_config_completion(I2cResult result, uint8_t addr, uint8_t config_addr, uint8_t value) {
    (void)result;
    (void)addr;
    (void)config_addr;
    (void)value;
}
void comm_on_button_effect_completion(I2cResult result, uint8_t addr, CommButtonEffect* effect) {
    (void)result;
    (void)addr;
    (void)effect;
}
void comm_on_button_changed_completion(I2cResult result, uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
    (void)result;
    (void)button_id;
    (void)pressed;
    (void)mode;
}
void comm_on_button_trigger_completion(I2cResult result, uint8_t addr, uint8_t button_id, CommTriggerConfig config) {
    (void)result;
    (void)addr;
    (void)button_id;
    (void)config;
}
void comm_on_relay_state_completion(I2cResult result, uint16_t relays) {
    (void)result;
    (void)relays;
}
void comm_on_channel_changed_completion(I2cResult result, uint16_t prev_channels, uint16_t current_channels,
                                        uint8_t prev_sensors, uint8_t current_sensors) {
    (void)result;
    (void)prev_channels;
    (void)current_channels;
    (void)prev_sensors;
    (void)current_sensors;
}
void comm_on_level_mode_completion(I2cResult result, CommMeterMode mode_0, CommMeterMode mode_1) {
    (void)result;
    (void)mode_0;
    (void)mode_1;
}

/* ============================================================================
 * Adopter callbacks: read response handlers (main-loop context)
 *
 * Button board does not initiate reads — empty stubs.
 * ============================================================================
 */

void comm_on_button_state_read_response(uint8_t addr, CommButtonState* state) {
    (void)addr;
    (void)state;
}
void comm_on_button_trigger_read_response(uint8_t addr, CommTriggerConfig* config) {
    (void)addr;
    (void)config;
}
void comm_on_relay_state_read_response(CommRelayState* state) {
    (void)state;
}
void comm_on_channel_state_read_response(CommChannelState* state) {
    (void)state;
}
void comm_on_battery_read_response(CommBattery* battery) {
    (void)battery;
}
void comm_on_levels_read_response(CommLevels* levels) {
    (void)levels;
}
void comm_on_level_mode_read_response(CommLevelMode* mode) {
    (void)mode;
}
void comm_on_sensors_read_response(CommSensors* sensors) {
    (void)sensors;
}
void comm_on_config_read_response(uint8_t addr, uint8_t* value) {
    (void)addr;
    (void)value;
}

/* ============================================================================
 * Helpers
 * ============================================================================
 */

static void apply_button_effect(CommButtonEffect* eff) {
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        CommButtonOutputEffect out;
        if (comm_button_effect_get(eff, i, &out) == 0) {
            led_effect_set(i, out);
        }
    }
}
