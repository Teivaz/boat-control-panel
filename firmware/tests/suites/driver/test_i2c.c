#include "bus.h"
#include "i2c.h"
#include "test_support.h"

#include <string.h>
#include <xc.h>

/*
 * The I2C multi-master driver, exercised through the peripheral it talks to.
 *
 * This file is compiled twice — once against libcomm/i2c.c and once against
 * libcomm/alt/i2c.c — because both implement the same header and the useful
 * question is whether they behave the same.  Anything guarded by
 * BUS_CAN_MOVE_BYTES is a payload assertion the DMA-based driver cannot be
 * asked (see bus.h); everything else applies to both.
 *
 * The behaviours under test are the ones the driver's own documentation calls
 * load-bearing: retry accounting funnelled through one terminal, terminal
 * NACK treated as success, client hardware taking priority over host, and a
 * staged read reply never surviving into the next transaction.
 */

#define SELF 0x40
#define PEER 0x42

/* ── Completion capture ───────────────────────────────────────────────── */

typedef struct {
    unsigned count;
    I2cResult result;
    uint8_t addr;
    uint8_t tx[I2C_TX_MAX];
    uint8_t tx_len;
    uint8_t rx[I2C_RX_MAX];
    uint8_t rx_len;
    uint8_t tx_was_null;
} Completion;

static Completion g_done;
static Completion g_cold;
static unsigned g_sync_calls;
static uint8_t g_sync_data[I2C_RX_MAX];
static uint8_t g_sync_len;
static uint8_t g_sync_verdict; /* what the sync handler returns */

static void capture(Completion* c, I2cResult result, uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t* rx,
                    uint8_t rx_len) {
    c->count++;
    c->result = result;
    c->addr = addr;
    c->tx_was_null = (tx == 0);
    c->tx_len = tx_len;
    if (tx && tx_len) {
        memcpy(c->tx, tx, tx_len);
    }
    c->rx_len = rx_len;
    if (rx && rx_len) {
        memcpy(c->rx, rx, rx_len);
    }
}

static void on_done(I2cResult r, uint8_t a, uint8_t* tx, uint8_t txl, uint8_t* rx, uint8_t rxl) {
    capture(&g_done, r, a, tx, txl, rx, rxl);
}

static void on_cold(I2cResult r, uint8_t a, uint8_t* tx, uint8_t txl, uint8_t* rx, uint8_t rxl) {
    capture(&g_cold, r, a, tx, txl, rx, rxl);
}

static uint8_t on_sync(uint8_t* data, uint8_t len) {
    g_sync_calls++;
    g_sync_len = len;
    if (len > sizeof(g_sync_data)) {
        len = sizeof(g_sync_data);
    }
    memcpy(g_sync_data, data, len);
    return g_sync_verdict;
}

static void* setup(const MunitParameter p[], void* user_data) {
    (void)p;
    (void)user_data;
    memset(&g_done, 0, sizeof(g_done));
    memset(&g_cold, 0, sizeof(g_cold));
    g_sync_calls = 0;
    g_sync_len = 0;
    g_sync_verdict = 1; /* defer to the async queue unless a test says otherwise */
    bus_reset(SELF);
    i2c_set_cold_rx_handler(on_cold);
    i2c_set_sync_cold_rx_handler(on_sync);
    return NULL;
}

/* ── Init ─────────────────────────────────────────────────────────────── */

static MunitResult test_init_programs_multihost(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Multi-Host 7-bit is the only mode that is host and client at once, and
     * the only one where I2CxADB1 stays writable — the host path needs both. */
    assert_uint8(I2C1CON0bits.MODE, ==, 0b110);
    assert_uint8(I2C1CON0bits.EN, ==, 1);

    /* The two multi-host prerequisites (DS §37.4.3). */
    assert_uint8(I2C1CON1bits.CSD, ==, 0);
    assert_uint8(I2C1PIEbits.ADRIE, ==, 1);

    /* All four address comparators hold addr << 1. */
    assert_uint8(I2C1ADR0, ==, SELF << 1);
    assert_uint8(I2C1ADR1, ==, (SELF << 1) & 0xFE);
    assert_uint8(I2C1ADR2, ==, SELF << 1);
    assert_uint8(I2C1ADR3, ==, (SELF << 1) & 0xFE);

    assert_uint8(I2C1BAUD, ==, I2C_BAUD);
    assert_uint8(I2C1CLK, ==, 0x01); /* FOSC */

    /* Transitions the FSM is built on. */
    assert_uint8(I2C1PIEbits.PCIE, ==, 1);
    assert_uint8(I2C1PIEbits.RSCIE, ==, 1);
    assert_uint8(I2C1PIEbits.CNTIE, ==, 1);
    assert_uint8(I2C1ERRbits.NACKIE, ==, 1);
    assert_uint8(I2C1ERRbits.BCLIE, ==, 1);
    assert_uint8(I2C1ERRbits.BTOIE, ==, 1);
    return MUNIT_OK;
}

/* ── Submission validation ────────────────────────────────────────────── */

static MunitResult test_submit_rejects_bad_args(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[I2C_TX_MAX + 1] = {1, 2, 3};

    assert_int(i2c_submit(PEER, 0, 3, 0, on_done), ==, I2C_RESULT_BAD_ARG);
    assert_int(i2c_submit(PEER, tx, 0, 0, on_done), ==, I2C_RESULT_BAD_ARG);
    assert_int(i2c_submit(PEER, tx, I2C_TX_MAX + 1, 0, on_done), ==, I2C_RESULT_BAD_ARG);
    assert_int(i2c_submit(PEER, tx, 1, I2C_RX_MAX + 1, on_done), ==, I2C_RESULT_BAD_ARG);

    /* The boundaries themselves are legal. */
    assert_int(i2c_submit(PEER, tx, I2C_TX_MAX, I2C_RX_MAX, on_done), ==, I2C_RESULT_OK);
    return MUNIT_OK;
}

/* The ring keeps one slot empty to tell full from empty apart, so usable
 * depth is I2C_QUEUE_SIZE - 1. */
static MunitResult test_queue_full(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[2] = {0xAA, 0xBB};
    for (uint8_t i = 0; i < I2C_QUEUE_SIZE - 1u; i++) {
        assert_int(i2c_submit(PEER, tx, 2, 0, 0), ==, I2C_RESULT_OK);
    }
    assert_int(i2c_submit(PEER, tx, 2, 0, 0), ==, I2C_RESULT_QUEUE_FULL);
    return MUNIT_OK;
}

/* ── Host write ───────────────────────────────────────────────────────── */

static MunitResult test_host_write(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[3] = {0x05, 0x34, 0x12};
    assert_int(i2c_submit(PEER, tx, 3, 0, on_done), ==, I2C_RESULT_OK);

    /* Nothing happens until the main loop gets a turn. */
    assert_uint8(I2C1CON0bits.S, ==, 0);

    i2c_poll();
    assert_uint8(I2C1ADB1, ==, PEER << 1); /* R/W clear = write */
    assert_uint8(I2C1CNTL, ==, 3);         /* exact byte count, not count-1 */
    assert_uint8(I2C1CON0bits.RSEN, ==, 0); /* no read phase to come */
    assert_uint8(I2C1CON0bits.S, ==, 1);   /* Start issued */
    assert_uint(g_done.count, ==, 0);

    bus_count_zero();
    i2c_poll();
    assert_uint(g_done.count, ==, 1);
    assert_int(g_done.result, ==, I2C_RESULT_OK);
    assert_uint8(g_done.addr, ==, PEER);
    assert_uint8(g_done.tx_len, ==, 3);
    assert_memory_equal(3, g_done.tx, tx);
    assert_uint8(g_done.rx_len, ==, 0);
    return MUNIT_OK;
}

/* ── Host write-then-read ─────────────────────────────────────────────── */

static MunitResult test_host_write_then_read(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[2] = {0x88, 0x42};
    assert_int(i2c_submit(PEER, tx, 2, 3, on_done), ==, I2C_RESULT_OK);

    i2c_poll();
    /* RSEN = 1 makes the 9th falling edge of the last written byte pause on
     * MDR instead of auto-stopping, so the Restart can follow. */
    assert_uint8(I2C1CON0bits.RSEN, ==, 1);
    assert_uint8(I2C1ADB1, ==, PEER << 1);
    assert_uint8(I2C1CNTL, ==, 2);

    bus_count_zero(); /* write phase done */
    assert_uint8(I2C1ADB1, ==, (PEER << 1) | 1u); /* address re-sent as a read */
    assert_uint8(I2C1CNTL, ==, 3);
    assert_uint8(I2C1CON0bits.RSEN, ==, 0);  /* now let hardware auto-Stop */
    assert_uint8(I2C1CON1bits.ACKCNT, ==, 1); /* terminal NACK on the last byte */
    assert_uint8(I2C1CON0bits.S, ==, 1);      /* Restart */
    assert_uint(g_done.count, ==, 0);

    const uint8_t payload[3] = {0x11, 0x22, 0x33};
    const uint8_t moved = bus_deliver_rx(payload, 3);
    bus_set_cnt(0);
    bus_stop();
    i2c_poll();

    assert_uint(g_done.count, ==, 1);
    assert_int(g_done.result, ==, I2C_RESULT_OK);
    if (moved) {
        assert_uint8(g_done.rx_len, ==, 3);
        assert_memory_equal(3, g_done.rx, payload);
    }
    return MUNIT_OK;
}

/* ── Failure and retry ────────────────────────────────────────────────── */

/* Every non-OK outcome spends one retry, and only the last one reaches the
 * caller — that is what stops a dead peer from producing a callback storm. */
static MunitResult test_nack_retries_then_fails(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x0F};
    i2c_submit(PEER, tx, 1, 0, on_done);

    for (uint8_t attempt = 0; attempt <= I2C_RETRY_COUNT; attempt++) {
        i2c_poll();
        if (I2C1CON0bits.S != 1) {
            munit_errorf("attempt %u never started", attempt);
        }
        I2C1CON0bits.S = 0;
        bus_nack();
        i2c_poll();
        if (attempt < I2C_RETRY_COUNT && g_done.count != 0) {
            munit_errorf("gave up after %u attempts, expected %u retries", attempt + 1u, I2C_RETRY_COUNT);
        }
    }

    assert_uint(g_done.count, ==, 1);
    assert_int(g_done.result, ==, I2C_RESULT_NACK);
    return MUNIT_OK;
}

static MunitResult test_collision_reports_busy(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x0F};
    i2c_submit(PEER, tx, 1, 0, on_done);
    for (uint8_t attempt = 0; attempt <= I2C_RETRY_COUNT; attempt++) {
        i2c_poll();
        bus_collision();
    }
    i2c_poll();
    assert_uint(g_done.count, ==, 1);
    assert_int(g_done.result, ==, I2C_RESULT_BUSY);
    return MUNIT_OK;
}

static MunitResult test_timeout_reports_timeout_and_recovers(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x0F};
    i2c_submit(PEER, tx, 1, 0, on_done);
    for (uint8_t attempt = 0; attempt <= I2C_RETRY_COUNT; attempt++) {
        i2c_poll();
        bus_timeout();
    }
    i2c_poll();
    assert_uint(g_done.count, ==, 1);
    assert_int(g_done.result, ==, I2C_RESULT_TIMEOUT);

    /* The module must be left usable — the timeout path is the only escape
     * from a wedged bus, so it has to re-enable rather than just report. */
    assert_uint8(I2C1CON0bits.EN, ==, 1);
    assert_uint8(I2C1CON0bits.MODE, ==, 0b110);
    return MUNIT_OK;
}

/*
 * A host NACKs the last byte of a read to say "enough".  NACKIF is set for
 * it (§37.5.8 note 4), so software has to recognise it — otherwise every
 * successful read is reported as a failure and burns retries.
 */
static MunitResult test_terminal_nack_is_success(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x88};
    i2c_submit(PEER, tx, 1, 2, on_done);
    i2c_poll();
    bus_count_zero(); /* into the read phase */

    bus_set_cnt(0);   /* all requested bytes received */
    bus_nack();       /* our own terminal NACK */
    i2c_poll();
    assert_uint(g_done.count, ==, 0); /* not a failure, not yet finished */

    bus_stop();
    i2c_poll();
    assert_uint(g_done.count, ==, 1);
    assert_int(g_done.result, ==, I2C_RESULT_OK);
    return MUNIT_OK;
}

/* A NACK with bytes still outstanding is a genuine failure. */
static MunitResult test_early_nack_during_read_fails(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x88};
    i2c_submit(PEER, tx, 1, 2, on_done);

    for (uint8_t attempt = 0; attempt <= I2C_RETRY_COUNT; attempt++) {
        i2c_poll();
        bus_count_zero();
        bus_set_cnt(2); /* nothing received yet */
        bus_nack();
    }
    i2c_poll();
    assert_uint(g_done.count, ==, 1);
    assert_int(g_done.result, ==, I2C_RESULT_NACK);
    return MUNIT_OK;
}

/* ── Multi-host arbitration ───────────────────────────────────────────── */

/*
 * "Client hardware has priority over host hardware in Multi-Host mode.  Host
 * mode communication can only be initiated when SMA = 0" (§37.4.3).  SMA goes
 * high on the 8th falling edge of a matching address — before the driver's own
 * state machine knows anything — so it, not the software state, is what has to
 * gate the start.  Deferring must not cost a retry, or a busy bus would
 * exhaust every task without a single attempt reaching the wire.
 */
static MunitResult test_host_defers_while_addressed(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x05};
    i2c_submit(PEER, tx, 1, 0, on_done);

    I2C1STAT0bits.SMA = 1;
    i2c_poll();
    assert_uint8(I2C1CON0bits.S, ==, 0); /* held back */

    I2C1STAT0bits.SMA = 0;
    i2c_poll();
    assert_uint8(I2C1CON0bits.S, ==, 1);

    /* And the full retry budget is still intact. */
    for (uint8_t attempt = 0; attempt <= I2C_RETRY_COUNT; attempt++) {
        bus_nack();
        i2c_poll();
        if (attempt < I2C_RETRY_COUNT) {
            assert_uint(g_done.count, ==, 0);
        }
    }
    assert_uint(g_done.count, ==, 1);
    return MUNIT_OK;
}

/* Being addressed mid-transaction means we lost arbitration: the task goes
 * back in the queue and the peer's transfer is honoured. */
static MunitResult test_address_match_during_host_op_yields(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x05};
    i2c_submit(PEER, tx, 1, 0, on_done);
    i2c_poll();
    assert_uint8(I2C1CON0bits.S, ==, 1);

    bus_address_match(0); /* another master is writing to us */
    i2c_poll();
    assert_uint(g_done.count, ==, 0); /* not failed — retried */

    bus_stop();
    i2c_poll();
    assert_uint8(I2C1CON0bits.S, ==, 1); /* our transaction restarts */
    return MUNIT_OK;
}

/* ── Client role ──────────────────────────────────────────────────────── */

static MunitResult test_client_tx_rejects_bad_args(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t buf[I2C_TX_MAX + 1] = {1};
    assert_int(i2c_set_client_tx(0, 1), ==, I2C_RESULT_BAD_ARG);
    assert_int(i2c_set_client_tx(buf, 0), ==, I2C_RESULT_BAD_ARG);
    assert_int(i2c_set_client_tx(buf, I2C_TX_MAX + 1), ==, I2C_RESULT_BAD_ARG);
    assert_int(i2c_set_client_tx(buf, I2C_TX_MAX), ==, I2C_RESULT_OK);
    return MUNIT_OK;
}

/*
 * A read we cannot answer must be NACKed at the address, not answered with
 * whatever is left in the buffer.  ACKDT = 1 is what puts the NACK on the
 * wire; a master then sees a clean refusal it can retry.
 */
static MunitResult test_unstaged_read_is_nacked(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    bus_address_match(1);
    assert_uint8(I2C1CON1bits.ACKDT, ==, 1);
    return MUNIT_OK;
}

static MunitResult test_staged_read_is_acked_and_clocked_out(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t reply[3] = {0xDE, 0xAD, 0x5C};
    assert_int(i2c_set_client_tx(reply, 3), ==, I2C_RESULT_OK);

    bus_address_match(1);
    assert_uint8(I2C1CON1bits.ACKDT, ==, 0);
    /* I2CxCNT is the exact byte count.  With count-1 the counter hits zero one
     * byte early, TXIF stops firing and the final byte — always the CRC — never
     * reaches the wire. */
    assert_uint8(I2C1CNTL, ==, 3);

    uint8_t got[8];
    uint8_t n = bus_collect_tx(got, sizeof(got));
    if (n) {
        assert_uint8(n, ==, 3);
        assert_memory_equal(3, got, reply);
    }
    return MUNIT_OK;
}

/*
 * The staged reply must not survive the transaction.  If it did, a request
 * the board refuses to answer would be served with the *previous* reply —
 * a stale frame that still passes CRC and is therefore indistinguishable
 * from fresh data.
 */
static MunitResult test_staged_reply_does_not_survive(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t reply[2] = {0x01, 0x02};
    i2c_set_client_tx(reply, 2);

    bus_address_match(1);
    assert_uint8(I2C1CON1bits.ACKDT, ==, 0);
    bus_stop();

    /* Next read, nothing staged this time. */
    bus_address_match(1);
    assert_uint8(I2C1CON1bits.ACKDT, ==, 1);
    return MUNIT_OK;
}

/* ── Inbound client writes ────────────────────────────────────────────── */

static MunitResult test_cold_rx_offers_to_sync_handler_first(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t frame[4] = {0x8E, 0x10, 0x00, 0x00};

    g_sync_verdict = 0; /* handled synchronously */
    bus_address_match(0);
    bus_deliver_rx(frame, 4);
    bus_set_rx_remaining((uint8_t)(I2C_RX_MAX - 4u));
    bus_stop();

    assert_uint(g_sync_calls, ==, 1);
    assert_uint8(g_sync_len, ==, 4);
#if BUS_CAN_MOVE_BYTES
    assert_memory_equal(4, g_sync_data, frame);
#endif

    /* Consumed in ISR context, so nothing reaches the main-loop queue. */
    i2c_poll();
    assert_uint(g_cold.count, ==, 0);
    return MUNIT_OK;
}

static MunitResult test_cold_rx_defers_to_main_loop(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t frame[3] = {0x05, 0xFF, 0x00};

    g_sync_verdict = 1; /* declined */
    bus_address_match(0);
    bus_deliver_rx(frame, 3);
    bus_set_rx_remaining((uint8_t)(I2C_RX_MAX - 3u));
    bus_stop();

    assert_uint(g_sync_calls, ==, 1);
    assert_uint(g_cold.count, ==, 0); /* not from ISR context */

    i2c_poll();
    assert_uint(g_cold.count, ==, 1);
    assert_int(g_cold.result, ==, I2C_RESULT_OK);
    assert_uint8(g_cold.addr, ==, 0); /* a client does not learn the sender */
    assert_uint8(g_cold.tx_was_null, ==, 1);
    assert_uint8(g_cold.tx_len, ==, 0);
    assert_uint8(g_cold.rx_len, ==, 3);
#if BUS_CAN_MOVE_BYTES
    assert_memory_equal(3, g_cold.rx, frame);
#endif
    return MUNIT_OK;
}

/* A write phase that ends in a Restart rather than a Stop is the first half
 * of somebody's read request; the driver has to complete the reception there
 * too, or the reply is staged too late. */
static MunitResult test_cold_rx_completes_on_restart(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    const uint8_t frame[2] = {0x88, 0x42};

    g_sync_verdict = 0;
    bus_address_match(0);
    bus_deliver_rx(frame, 2);
    bus_set_rx_remaining((uint8_t)(I2C_RX_MAX - 2u));
    bus_restart();

    assert_uint(g_sync_calls, ==, 1);
    assert_uint8(g_sync_len, ==, 2);
    return MUNIT_OK;
}

/* An empty reception is not a message. */
static MunitResult test_empty_cold_rx_is_dropped(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    bus_address_match(0);
    bus_set_rx_remaining(I2C_RX_MAX); /* nothing consumed */
    bus_stop();
    i2c_poll();
    assert_uint(g_sync_calls, ==, 0);
    assert_uint(g_cold.count, ==, 0);
    return MUNIT_OK;
}

/*
 * Cold receives jump the queue: they are injected at the head so an inbound
 * command is delivered before host completions that were already waiting.
 * Without that a burst of outbound traffic would delay every inbound event
 * behind it.
 */
static MunitResult test_cold_rx_is_delivered_ahead_of_host_completions(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x05};
    i2c_submit(PEER, tx, 1, 0, on_done);
    i2c_poll();
    bus_count_zero(); /* host write finished, completion queued */

    const uint8_t frame[2] = {0x0F, 0xDE};
    g_sync_verdict = 1;
    bus_address_match(0);
    bus_deliver_rx(frame, 2);
    bus_set_rx_remaining((uint8_t)(I2C_RX_MAX - 2u));
    bus_stop();

    i2c_poll();
    assert_uint(g_cold.count, ==, 1);
    assert_uint(g_done.count, ==, 0);

    i2c_poll();
    assert_uint(g_done.count, ==, 1);
    return MUNIT_OK;
}

/* ── Wire log ─────────────────────────────────────────────────────────── */

static MunitResult test_log_is_newest_first_and_bounded(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Stage twelve replies; the ring holds eight. */
    for (uint8_t i = 0; i < 12; i++) {
        uint8_t reply[1] = {i};
        i2c_set_client_tx(reply, 1);
    }

    I2cLogEntry out[16];
    uint8_t n = i2c_log_snapshot(out, 16);
    assert_uint8(n, ==, 8);
    /* Newest first, and the four oldest were evicted rather than the newest
     * being dropped — a log that discards new entries when full is useless
     * for diagnosing what just went wrong. */
    for (uint8_t i = 0; i < n; i++) {
        assert_uint8(out[i].data[0], ==, (uint8_t)(11 - i));
    }

    /* Snapshot honours the caller's capacity. */
    n = i2c_log_snapshot(out, 3);
    assert_uint8(n, ==, 3);
    assert_uint8(out[0].data[0], ==, 11);
    return MUNIT_OK;
}

static MunitResult test_log_records_host_and_client_traffic(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[2] = {0x05, 0x7B};
    i2c_submit(PEER, tx, 2, 0, on_done);
    i2c_poll();
    bus_count_zero();
    i2c_poll();

    I2cLogEntry out[8];
    uint8_t n = i2c_log_snapshot(out, 8);
    assert_uint8(n, >, 0);
    assert_uint8(out[0].addr, ==, PEER);
    assert_uint8(out[0].len, ==, 2);
    assert_memory_equal(2, out[0].data, tx);
#if defined(I2C_DRIVER_ALT)
    assert_uint8(out[0].kind, ==, I2C_LOG_W);
    assert_uint8(out[0].result, ==, I2C_RESULT_OK);
#else
    assert_uint8(out[0].kind, ==, I2C_LOG_WA);
#endif
    return MUNIT_OK;
}

/* Both phases of a write-then-read share a request id, which is the only way
 * to pair them up in the on-screen monitor. */
static MunitResult test_log_pairs_read_phases_by_id(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    uint8_t tx[1] = {0x88};
    i2c_submit(PEER, tx, 1, 2, on_done);
    i2c_poll();
    bus_count_zero();
    bus_set_cnt(0);
    bus_stop();
    i2c_poll();

    I2cLogEntry out[8];
    uint8_t n = i2c_log_snapshot(out, 8);
    assert_uint8(n, ==, 2);
    assert_uint8(out[0].req_id, ==, out[1].req_id);
    return MUNIT_OK;
}

/*
 * The receive span is zeroed at submit so a short or absent response reads as
 * zeros rather than as bytes left by a previous user of the slot.  For the
 * fixed 2- and 3-byte payloads on this bus, stale data could otherwise
 * CRC-validate as a well-formed frame of the wrong kind.
 */
static MunitResult test_rx_buffer_is_zeroed_between_uses(const MunitParameter p[], void* f) {
    (void)p;
    (void)f;
    /* Fill one queue slot's rx with a recognisable pattern by completing a
     * read, then reuse the whole ring so the same slot comes round again. */
    uint8_t tx[1] = {0x88};
    const uint8_t payload[3] = {0xAA, 0xBB, 0xCC};

    i2c_submit(PEER, tx, 1, 3, on_done);
    i2c_poll();
    bus_count_zero();
    bus_deliver_rx(payload, 3);
    bus_set_cnt(0);
    bus_stop();
    i2c_poll();

    for (uint8_t i = 0; i < I2C_QUEUE_SIZE; i++) {
        i2c_submit(PEER, tx, 1, 0, 0);
        i2c_poll();
        bus_count_zero();
        i2c_poll();
    }

    memset(&g_done, 0, sizeof(g_done));
    i2c_submit(PEER, tx, 1, 3, on_done);
    i2c_poll();
    bus_count_zero();
    bus_set_cnt(0);
    bus_stop(); /* nothing delivered this time */
    i2c_poll();

    assert_uint(g_done.count, ==, 1);
    for (uint8_t i = 0; i < g_done.rx_len; i++) {
        assert_uint8(g_done.rx[i], ==, 0);
    }
    return MUNIT_OK;
}

#define T(name, fn) {name, fn, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}

static MunitTest tests[] = {
    T("/init_programs_multihost", test_init_programs_multihost),
    T("/submit_rejects_bad_args", test_submit_rejects_bad_args),
    T("/queue_full", test_queue_full),
    T("/host_write", test_host_write),
    T("/host_write_then_read", test_host_write_then_read),
    T("/nack_retries_then_fails", test_nack_retries_then_fails),
    T("/collision_reports_busy", test_collision_reports_busy),
    T("/timeout_reports_timeout_and_recovers", test_timeout_reports_timeout_and_recovers),
    T("/terminal_nack_is_success", test_terminal_nack_is_success),
    T("/early_nack_during_read_fails", test_early_nack_during_read_fails),
    T("/host_defers_while_addressed", test_host_defers_while_addressed),
    T("/address_match_during_host_op_yields", test_address_match_during_host_op_yields),
    T("/client_tx_rejects_bad_args", test_client_tx_rejects_bad_args),
    T("/unstaged_read_is_nacked", test_unstaged_read_is_nacked),
    T("/staged_read_is_acked_and_clocked_out", test_staged_read_is_acked_and_clocked_out),
    T("/staged_reply_does_not_survive", test_staged_reply_does_not_survive),
    T("/cold_rx_offers_to_sync_handler_first", test_cold_rx_offers_to_sync_handler_first),
    T("/cold_rx_defers_to_main_loop", test_cold_rx_defers_to_main_loop),
    T("/cold_rx_completes_on_restart", test_cold_rx_completes_on_restart),
    T("/empty_cold_rx_is_dropped", test_empty_cold_rx_is_dropped),
    T("/cold_rx_is_delivered_ahead_of_host_completions", test_cold_rx_is_delivered_ahead_of_host_completions),
    T("/log_is_newest_first_and_bounded", test_log_is_newest_first_and_bounded),
    T("/log_records_host_and_client_traffic", test_log_records_host_and_client_traffic),
    T("/log_pairs_read_phases_by_id", test_log_pairs_read_phases_by_id),
    T("/rx_buffer_is_zeroed_between_uses", test_rx_buffer_is_zeroed_between_uses),
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite i2c_suite(void) {
    MunitSuite s = {"/i2c", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};
    return s;
}
