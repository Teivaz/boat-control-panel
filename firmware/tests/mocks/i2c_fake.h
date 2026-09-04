/*
 * i2c_fake.h — inspection and stimulus API for the fake I2C driver.
 *
 * Board test binaries link mocks/i2c_fake.c in place of libcomm/i2c.c.  The
 * real driver is worth testing, but not while testing a board: it would drag
 * every board assertion through a bus-level simulation of DMA, arbitration and
 * clock stretching.  The fake presents the same i2c.h surface and turns the
 * two directions into something a test can drive directly:
 *
 *   outbound   the board calls comm_send_* -> i2c_submit; the test reads back
 *              the exact bytes with i2c_fake_tx() and decides the outcome with
 *              i2c_fake_respond().
 *   inbound    the test calls i2c_fake_deliver_write() or
 *              i2c_fake_deliver_read_request(); the fake runs the same handler
 *              path the driver would, so libcomm_interface's dispatcher and
 *              the board's comm_on_* callbacks are exercised for real.
 *
 * The real driver is covered separately, in its own binary, by
 * suites/libcomm/test_i2c.c.
 */

#ifndef I2C_FAKE_H
#define I2C_FAKE_H

#include "i2c.h"

#include <stdint.h>

/* Generous: a board3 test that advances a couple of seconds runs hundreds of
 * button_fx refresh ticks, and the log has to outlast them or later
 * submissions start reporting the queue as full. */
#define I2C_FAKE_MAX_TX 250 /* stays under 256 so the uint8_t index/count cannot wrap */

typedef enum {
    I2C_FAKE_PENDING = 0, /* submitted, no outcome decided yet */
    I2C_FAKE_READY,       /* outcome set; next i2c_poll fires the callback */
    I2C_FAKE_DONE,        /* callback fired */
} I2cFakeState;

typedef struct {
    uint8_t addr;
    uint8_t tx[I2C_TX_MAX];
    uint8_t tx_len;
    uint8_t rx_len; /* bytes requested; 0 = write-only */
    I2cCompletion cb;

    I2cFakeState state;
    I2cResult result;
    uint8_t rx[I2C_RX_MAX];
    uint8_t rx_got; /* bytes the test chose to return */
} I2cFakeTx;

/* Clear every recorded transaction, handler registration and staged reply.
 * Call from each test's setup. */
void i2c_fake_reset(void);

/* ── Outbound inspection ──────────────────────────────────────────────── */

uint8_t i2c_fake_tx_count(void);
const I2cFakeTx* i2c_fake_tx(uint8_t index);
const I2cFakeTx* i2c_fake_last_tx(void); /* NULL when nothing was submitted */

/* Index of the first not-yet-completed transaction whose first tx byte is
 * `id`, or 0xFF if there is none.  Boards emit several different commands
 * from one poll cycle, so tests match on the command rather than position. */
uint8_t i2c_fake_find(uint8_t id);

/* Make i2c_submit return `result` instead of I2C_RESULT_OK — for exercising
 * the retry paths boards take when the queue is full. */
void i2c_fake_set_submit_result(I2cResult result);

/* Whether write-only transactions complete by themselves on the next
 * i2c_poll.  On by default: a write to a device that is present succeeds, and
 * making every test spell that out adds noise rather than coverage.  Reads
 * always need an explicit i2c_fake_respond. */
void i2c_fake_set_autocomplete_writes(uint8_t on);

/* Decide the outcome of a submitted transaction.  The completion callback
 * fires on the next i2c_poll(), which is where the real driver fires it.
 * `rx` may be NULL for a failure or a write. */
void i2c_fake_respond(uint8_t index, I2cResult result, const uint8_t* rx, uint8_t rx_len);

/* Respond to the first pending transaction carrying command `id`, appending a
 * CRC-8 over `payload` exactly as a real peer's comm_respond would.  This is
 * the form nearly every read test wants. */
void i2c_fake_respond_to(uint8_t id, const uint8_t* payload, uint8_t payload_len);

/* ── Inbound stimulus ─────────────────────────────────────────────────── */

/* Deliver a complete inbound write frame (id + payload + CRC) as if another
 * master had written it to us.  Runs the synchronous handler first, exactly
 * as the driver does, and queues the frame for main-loop delivery if that
 * handler declines it.  Call i2c_poll() afterwards to drain. */
void i2c_fake_deliver_write(const uint8_t* frame, uint8_t len);

/* Same, but for the write phase of a write-then-read: the synchronous handler
 * is expected to stage a reply via comm_respond().  Returns the number of
 * reply bytes staged (0 = the request was refused, which on the wire means
 * the read-phase address gets NACKed). */
uint8_t i2c_fake_deliver_read_request(const uint8_t* frame, uint8_t len);

/* The reply currently staged by i2c_set_client_tx (payload + CRC), or NULL. */
const uint8_t* i2c_fake_client_tx(uint8_t* len_out);

/* Convenience: build `id + payload + crc` into `out` and return its length. */
uint8_t i2c_fake_frame(uint8_t* out, uint8_t id, const uint8_t* payload, uint8_t payload_len);

#endif /* I2C_FAKE_H */
