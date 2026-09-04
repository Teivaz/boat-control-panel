/*
 * bus.h — a hand-cranked I2C peripheral for driving the real drivers.
 *
 * Both libcomm/i2c.c and libcomm/alt/i2c.c are interrupt-driven state
 * machines: everything they do is a reaction to a flag being raised in
 * I2C1PIR or I2C1ERR.  With the interrupt attribute erased by the mock, the
 * handlers are ordinary functions, so a test can play the part of the
 * peripheral — raise the flag, call the handler, inspect the registers the
 * driver wrote.
 *
 * The one thing that cannot be reproduced for the original driver is payload
 * movement.  It moves bytes with DMA, and its channel setup stores
 * `(uint16_t)task->rx` — a 64-bit host pointer truncated to 16 bits, which no
 * amount of mocking can turn back into an address.  The alternative driver
 * feeds I2CxTXB / drains I2CxRXB from two interrupt vectors, which are just
 * functions, so its data path *is* reachable from here.
 *
 * That asymmetry is why bus_deliver_rx() reports whether it did anything:
 * shared tests assert control flow, and only guard payload assertions on
 * BUS_CAN_MOVE_BYTES.
 */

#ifndef BUS_H
#define BUS_H

#include <stdint.h>

#if defined(I2C_DRIVER_ALT)
#define BUS_CAN_MOVE_BYTES 1
#else
#define BUS_CAN_MOVE_BYTES 0
#endif

/* Interrupt handlers, as plain functions. */
void I2C1_ISR(void);
void I2C1_ERROR_ISR(void);

/* Put the peripheral registers back to reset state and re-init the driver at
 * `addr`.  Returns with the driver idle and listening as a client. */
void bus_reset(uint8_t addr);

/* ── Events the peripheral raises ─────────────────────────────────────── */

void bus_count_zero(void);            /* CNTIF — I2CxCNT reached 0          */
void bus_address_match(uint8_t read); /* ADRIF — we were addressed          */
void bus_stop(void);                  /* PCIF                               */
void bus_restart(void);               /* RSCIF                              */
void bus_nack(void);                  /* NACKIF                             */
void bus_collision(void);             /* BCLIF — arbitration lost           */
void bus_timeout(void);               /* BTOIF                              */

/* ── Payload movement ─────────────────────────────────────────────────── */

/* Hand `len` received bytes to the driver.  Returns 1 if they were actually
 * delivered, 0 on the DMA-based driver where they cannot be. */
uint8_t bus_deliver_rx(const uint8_t* bytes, uint8_t len);

/* Pull up to `max` bytes out of the driver as a master would clock them.
 * Returns the number obtained; 0 on the DMA-based driver. */
uint8_t bus_collect_tx(uint8_t* out, uint8_t max);

/* Tell the driver how many bytes are still outstanding in the client-receive
 * window.  The DMA-based driver derives its received length from this
 * register; the byte-driven one ignores it and keeps its own count. */
void bus_set_rx_remaining(uint8_t remaining);

/* Set I2CxCNT, which several driver decisions read back (notably whether a
 * NACK during a host read is the terminal one). */
void bus_set_cnt(uint16_t value);

#endif /* BUS_H */
