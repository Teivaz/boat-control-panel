#ifndef I2C_BOARD_H
#define I2C_BOARD_H

/* Board-specific I2C pin setup for the main board.
 * RB1 = SDA, RB2 = SCL on I2C1. */

/* Configure RB1/RB2 as open-drain I2C pins with PPS routing.
 * Call before i2c_init(). */
void i2c_pins_init(void);

#endif /* I2C_BOARD_H */
