#ifndef I2C_BOARD_H
#define I2C_BOARD_H

/* Board-specific I2C pin setup and bus recovery for the main board.
 * RB1 = SDA, RB2 = SCL on I2C1. */

/* Configure RB1/RB2 as open-drain I2C pins with PPS routing.
 * Call before i2c_init(). */
void i2c_pins_init(void);

/* Bit-bang up to 9 SCL pulses to free a stuck SDA, then issue a manual
 * STOP.  No-op when SDA is already high.  Main context only. */
void i2c_bus_recover(void);

#endif /* I2C_BOARD_H */
