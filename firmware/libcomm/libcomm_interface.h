#ifndef LIBCOMM_H
#define LIBCOMM_H

#include "libcomm.h"
#include <stdint.h>

void _comm_write_message(uint8_t addr, CommMessage* message);
uint8_t _comm_message_length(CommMessage* message);

uint8_t comm_send_button_changed(uint8_t button_id, uint8_t pressed, CommButtonMode mode);

// Implemented in the library
uint8_t comm_send_button_state_read(uint8_t addr);

// Implemented by the adopter of the library
// The code should construct the structure
// Dispatched in the interrupt
// Return value is 0 is OK, or 1 is ERROR
uint8_t comm_on_button_state_read_request(CommButtonState* state);

// Implemented by the adopter of the library
// The code handles the button state response message
// If there was an error the state will be a null pointer
// Function runis in the main loop
void comm_on_button_state_read_response(uint8_t addr, CommButtonState* state);


void _comm_write_message(uint8_t addr, CommMessage* message) {
  const uint8_t length = _comm_message_length(message) + 1; // 1 for message type
  (void)i2c_submit(addr, message.raw, length, 0, 0, 0, 0);
}

uint8_t _comm_message_length(CommMessage* message) {
  switch (message->id) {
    case COMM_BUTTON_EFFECT:
      return 4;
    case COMM_BUTTON_CHANGED:
      return 2;
    case COMM_BUTTON_TRIGGER:
      return 2;
    case COMM_RELAY_STATE:
      return 2;
    case COMM_RELAY_CHANGED:
      return 7;
    case COMM_RELAY_MASK:
      return 2;
    case COMM_LEVEL_MODE:
      return 1;
    case COMM_CONFIG:
      return 2;
    case COMM_RESET:
      return 0;
    default:
      return 0;
  }
}

uint8_t comm_send_button_changed(uint8_t button_id, uint8_t pressed, CommButtonMode mode) {
  CommMessage message;
  comm_build_button_changed(&message, button_id, pressed, mode);
  _comm_write_message(COMM_ADDRESS_MAIN, &message);
}

uint8_t comm_send_button_state_read(uint8_t addr) {
  CommMessage message = {.id = COMM_BUTTON_STATE_READ};
  const uint8_t length = _comm_message_length(message) + 1; // 1 for message type
  i2c_submit(addr, message.raw, 1, message.raw, length,
                     &_comm_on_button_state_read_response, (void*)addr);
}

void _comm_on_button_state_read_response(I2CStatus status, uint8_t* rx_buf, uint8_t rx_len, void* ctx) {
  CommButtonState state;
  comm_parse_button_state_response(rx_buf, &state);
  comm_on_button_state_read_response((uint8_t)ctx, &state)
}


#endif