#ifndef COMM_H
#define COMM_H

/* Wires i2c RX/read handlers to switching-board dispatch: routes inbound
 * relay/level/config writes to the controller and serves the polled reads
 * (relay state/mask, battery, levels, level mode, sensors, config). Call
 * after i2c_init(). */
void comm_init(void);

#endif /* COMM_H */
