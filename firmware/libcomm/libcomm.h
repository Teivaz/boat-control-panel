#ifndef LIBCOMM_H
#define LIBCOMM_H

#include <xc.h>

#define CMD_ADDRESS_MAIN 0x40 // Main control board
#define CMD_ADDRESS_SWITCHING 0x42 // Switching board
#define CMD_ADDRESS_INPUT_L1 0x44 // input panel L
#define CMD_ADDRESS_INPUT_L2 0x45 // input panel L2
#define CMD_ADDRESS_INPUT_R1 0x46 // input panel R
#define CMD_ADDRESS_INPUT_R2 0x47 // input panel R2
#define CMD_ADDRESS_RTC 0x68 // RTC module (DS3231)

/**
 * Message types enumeration
 */
typedef enum {
    CMD_MESSAGE_NONE = 0x00,
    CMD_MESSAGE_EVENT_INPUT_STATE = 0x01,
    CMD_MESSAGE_GET_INPUT_STATE = 0x02,
    CMD_MESSAGE_SET_OUTPUT_STATE = 0x03,
} CmdMessageTypeIdentifier;

/**
 * Output state structure - represents 8 outputs (0-7)
 * Each output occupies 4 bits (a nibble), allowing states 0-15
 */
typedef union {
    /* Register-based access */
    struct {
        uint8_t state0;  /* Outputs 1 and 0 */
        uint8_t state1;  /* Outputs 3 and 2 */
        uint8_t state2;  /* Outputs 5 and 4 */
        uint8_t state3;  /* Outputs 7 and 6 */
    };
    
    /* Bitfield access - individual nibbles */
    struct {
        uint8_t out0 : 4;  /* Output 0 (bits 0-3) */
        uint8_t out1 : 4;  /* Output 1 (bits 4-7) */
        uint8_t out2 : 4;  /* Output 2 (bits 8-11) */
        uint8_t out3 : 4;  /* Output 3 (bits 12-15) */
        uint8_t out4 : 4;  /* Output 4 (bits 16-19) */
        uint8_t out5 : 4;  /* Output 5 (bits 20-23) */
        uint8_t out6 : 4;  /* Output 6 (bits 24-27) */
        uint8_t out7 : 4;  /* Output 7 (bits 28-31) */
    };
} CmdOutputState;

/**
 * Input state event structure
 */
typedef struct {
    uint8_t device_address;
    uint8_t prev_value;
    uint8_t value;
} CmdEventInputState;

typedef struct {
    uint8_t value;
} CmdGetInputState;

typedef struct {
    CmdMessageTypeIdentifier message_type;
    union {
        CmdEventInputState event_input_state;
        CmdGetInputState get_input_state;
        CmdOutputState set_output_state;
    };
} CmdMessage;

// Current device address
uint8_t cmd_address(void);

/* ============================================================================
 * Event Input State - Send
 * ============================================================================ */

/**
 * Compose and send event_input_state message over I2C
 * 
 * This message is transmitted to notify that the input register has changed.
 * Message format:
 *   Byte 0: device_address - address of the sending device
 *   Byte 1: prev_value - previous value of input_register
 *   Byte 2: value - current value of input_register
 * 
 * @param target_address: I2C target address to send to
 * @param prev_value: Previous value of input_register
 * @param current_value: Current value of input_register
 */
void cmd_send_event_input_state(uint8_t target_address, 
                                uint8_t prev_value, 
                                uint8_t current_value);

/* ============================================================================
 * Get Input State - Request/Response
 * ============================================================================ */

/**
 * Request current input_register value from a device
 * 
 * This is a read operation to get the input_register value.
 * 
 * Response format:
 *   Byte 0: value - current value of input_register
 * 
 * @param slave_address: I2C slave address to query
 * @param input_value: Pointer to store the returned input_register value
 * @return: 0 on success, -1 on error
 */
int cmd_get_input_state(uint8_t slave_address, uint8_t *input_value);

/**
 * Send response with current input_register value
 * 
 * This is called by the device to respond to a get_input_state request.
 * 
 * @param slave_address: I2C slave address to send to
 * @param input_value: Current value of input_register
 */
void cmd_send_get_input_state_response(uint8_t slave_address, uint8_t input_value);

/**
 * Parse get_input_state response
 * 
 * @param data: Pointer to 1-byte response buffer
 * @param input_value: Pointer to store the input_register value
 * @return: 0 on success, -1 on error
 */
int cmd_parse_get_input_state_response(const uint8_t *data, uint8_t *input_value);

/* ============================================================================
 * Set Output State - Command
 * ============================================================================ */

/**
 * Compose and send set_output_state message over I2C
 * 
 * This message configures the output states of 8 outputs (0-7).
 * Each output occupies 4 bits (a nibble), allowing states 0-15 per output.
 * 
 * Message format:
 *   Byte 0: state3 - outputs 7 (upper nibble) and 6 (lower nibble)
 *   Byte 1: state2 - outputs 5 (upper nibble) and 4 (lower nibble)
 *   Byte 2: state1 - outputs 3 (upper nibble) and 2 (lower nibble)
 *   Byte 3: state0 - outputs 1 (upper nibble) and 0 (lower nibble)
 * 
 * @param slave_address: I2C slave address to send to
 * @param output_state: Pointer to CmdOutputState structure with output values
 */
void cmd_send_set_output_state(uint8_t slave_address, const CmdOutputState *output_state);

/**
 * Parse set_output_state message from I2C data
 * 
 * @param data: Pointer to 4-byte message buffer
 * @param output_state: Pointer to CmdOutputState structure to populate
 * @return: 0 on success, -1 on error
 */
int cmd_parse_set_output_state(const uint8_t *data, CmdOutputState *output_state);

/* ============================================================================
 * Helper Functions - Output State Manipulation
 * ============================================================================ */

/**
 * Initialize output_state structure with all outputs set to 0
 * 
 * @param output_state: Pointer to CmdOutputState to initialize
 */
void cmd_output_state_init(CmdOutputState *output_state);

/**
 * Set a single output value (0-7)
 * 
 * Each output can hold a value from 0-15 (4 bits).
 * 
 * @param output_state: Pointer to CmdOutputState structure
 * @param output_index: Output index (0-7)
 * @param value: Value to set (0-15)
 * @return: 0 on success, -1 on invalid parameters
 */
int cmd_output_state_set_output(CmdOutputState *output_state, uint8_t output_index, uint8_t value);

/**
 * Get a single output value (0-7)
 * 
 * @param output_state: Pointer to CmdOutputState structure
 * @param output_index: Output index (0-7)
 * @param value: Pointer to store the output value
 * @return: 0 on success, -1 on invalid parameters
 */
int cmd_output_state_get_output(const CmdOutputState *output_state, uint8_t output_index, uint8_t *value);

#endif /* LIBCOMM_H */
