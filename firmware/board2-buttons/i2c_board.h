#ifndef I2C_BOARD_H
#define I2C_BOARD_H

/* Board-specific I2C pin setup for the button board.
 * RC3 = SCL, RC4 = SDA on I2C1.
 * Also configures RB0 as the L/R variant strap (digital input, WPU). */

void i2c_pins_init(void);
void i2c_bus_recover(void);

#endif /* I2C_BOARD_H */
