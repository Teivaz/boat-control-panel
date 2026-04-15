#include "libcomm.h"
#include "libcomm_private.h"
#include "i2c.h"

/* ============================================================================
 * Event Input State - Send
 * ============================================================================ */

uint8_t cmd_address(void) {
#ifdef DEVICE_TYPE_MAIN
    return CMD_ADDRESS_MAIN;
#elif defined DEVICE_TYPE_SWITCHING
    return CMD_ADDRESS_SWITCHING;
#elif defined DEVICE_TYPE_INPUT
    if (PORTBbits.RB0 == 0) {
        return CMD_ADDRESS_INPUT_L1;
    }
    else {
        return CMD_ADDRESS_INPUT_L1;
    }
#else
#error("Undefined device type. Should be one of DEVICE_TYPE_MAIN DEVICE_TYPE_SWITCHING DEVICE_TYPE_INPUT")
#endif
}

void cmd_send_event_input_state(uint8_t target_address,
                                uint8_t prev_value, 
                                uint8_t current_value)
{
    CmdMessage message = {
        .message_type = CMD_MESSAGE_EVENT_INPUT_STATE,
        .event_input_state = {
            .device_address = cmd_address(),
            .prev_value = prev_value,
            .value = current_value,
        },
    };
    i2c_write_bytes(target_address, (uint8_t*)&message, sizeof(CmdEventInputState) + 1);
}

void cmd_parse_event_input_state(const uint8_t *data, CmdEventInputState *event) {
    event->device_address = data[0];
    event->prev_value = data[1];
    event->value = data[2];
}

/* ============================================================================
 * Get Input State - Request/Response
 * ============================================================================ */

int cmd_get_input_state(uint8_t slave_address, uint8_t *input_value)
{
    if (input_value == NULL) {
        return -1;
    }
    
    /* Read 1 byte (the input_register value) from the device */
    // TODO: Implement
    // i2c_read_bytes(slave_address, input_value, 1);
    
    return 0;
}

void cmd_send_get_input_state_response(uint8_t slave_address, uint8_t input_value)
{
    uint8_t response[1];
    response[0] = input_value;
    
    i2c_write_bytes(slave_address, response, 1);
}

int cmd_parse_get_input_state_response(const uint8_t *data, uint8_t *input_value)
{
    if (data == NULL || input_value == NULL) {
        return -1;
    }
    
    *input_value = data[0];
    
    return 0;
}

/* ============================================================================
 * Set Output State - Command
 * ============================================================================ */

void cmd_send_set_output_state(uint8_t slave_address, const CmdOutputState *output_state)
{
    uint8_t message[4];
    
    if (output_state == NULL) {
        return;
    }
    
    message[0] = output_state->state3;
    message[1] = output_state->state2;
    message[2] = output_state->state1;
    message[3] = output_state->state0;
    
    i2c_write_bytes(slave_address, message, 4);
}

int cmd_parse_set_output_state(const uint8_t *data, CmdOutputState *output_state)
{
    if (data == NULL || output_state == NULL) {
        return -1;
    }
    
    output_state->state3 = data[0];
    output_state->state2 = data[1];
    output_state->state1 = data[2];
    output_state->state0 = data[3];
    
    return 0;
}

/* ============================================================================
 * Helper Functions - Output State Manipulation
 * ============================================================================ */

void cmd_output_state_init(CmdOutputState *output_state)
{
    if (output_state != NULL) {
        output_state->state0 = 0x00;
        output_state->state1 = 0x00;
        output_state->state2 = 0x00;
        output_state->state3 = 0x00;
    }
}

int cmd_output_state_set_output(CmdOutputState *output_state, uint8_t output_index, uint8_t value)
{
    if (output_state == NULL || output_index > 7 || value > 15) {
        return -1;
    }
    
    uint8_t *state_register;
    uint8_t nibble_position;
    
    /* Determine which state register and nibble position */
    if (output_index <= 1) {
        state_register = &output_state->state0;
        nibble_position = output_index;
    } else if (output_index <= 3) {
        state_register = &output_state->state1;
        nibble_position = output_index - 2;
    } else if (output_index <= 5) {
        state_register = &output_state->state2;
        nibble_position = output_index - 4;
    } else {
        state_register = &output_state->state3;
        nibble_position = output_index - 6;
    }
    
    /* Clear the nibble and set new value */
    uint8_t mask = ~(0x0F << (nibble_position * 4));
    *state_register = (*state_register & mask) | (value << (nibble_position * 4));
    
    return 0;
}

int cmd_output_state_get_output(const CmdOutputState *output_state, uint8_t output_index, uint8_t *value)
{
    if (output_state == NULL || output_index > 7 || value == NULL) {
        return -1;
    }
    
    uint8_t state_register;
    uint8_t nibble_position;
    
    /* Determine which state register and nibble position */
    if (output_index <= 1) {
        state_register = output_state->state0;
        nibble_position = output_index;
    } else if (output_index <= 3) {
        state_register = output_state->state1;
        nibble_position = output_index - 2;
    } else if (output_index <= 5) {
        state_register = output_state->state2;
        nibble_position = output_index - 4;
    } else {
        state_register = output_state->state3;
        nibble_position = output_index - 6;
    }
    
    /* Extract the nibble */
    *value = (state_register >> (nibble_position * 4)) & 0x0F;
    
    return 0;
}
