#ifndef COMM_H
#define COMM_H

/* Wires i2c RX/read handlers to main-board dispatch: routes button_changed
 * and relay_changed to the controller, handles universal config / reset on
 * behalf of this device. Call after i2c_init(). */
void comm_init(void);

#endif /* COMM_H */
