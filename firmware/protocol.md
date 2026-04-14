# I2C communication protocol description

All devices should be able to operate in standard (100kHz) and fast (400kHz) modes.
The system operates in multi-master mode with event based system. If a device needs to implement polling the time interval for polling calls should be >= 100ms.
Addressing scheme is 7 bit.
Signal lines are pulled up to 3.3V.

## Address

0x40 - Main control board
0x41 - reserved
0x42 - Switching board
0x43 - reserved
0x44 - input panel L
0x45 - input panel L2
0x46 - input panel R
0x47 - input panel R2
0x48..0x4F - reserved
0x68 - RTC module (DS3231)

## Messages

- event_input_state - event message transmitting the previous and the current states of the input_register of the device. Should be emitted when the device's input_register value is changed.
  - write byte 0: device_address - the address of the device that transmitts the state
  - write byte 1: prev_value - the previous value of the input_register
  - write byte 2: value - the value of the input_register

- get_input_state - request the value of the input_register of the device
  - read byte 0: value - the value of the input_register

- set_output_state - set the state of the outputs of the device
  - write byte 0: state3 - most significant nibble corresponds to state of the output index 7, least significant nibble corresponds to state of the output 6
  - write byte 1: state2 - state of the outputs 5 and 4
  - write byte 2: state1 - state of the outputs 3 and 2
  - write byte 3: state0 - state of the outputs 1 and 0
