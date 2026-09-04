#include "i2c_fake.h"

#include "crc.h"

#include <string.h>

/* ── Registered handlers ──────────────────────────────────────────────── */

static I2cCompletion g_cold_rx;
static I2cSyncColdRxHandler g_sync_cold_rx;

/* ── Outbound ─────────────────────────────────────────────────────────── */

static I2cFakeTx g_tx[I2C_FAKE_MAX_TX];
static uint8_t g_tx_count;
static I2cResult g_submit_result = I2C_RESULT_OK;
static uint8_t g_autocomplete_writes = 1;

/* ── Inbound ──────────────────────────────────────────────────────────── */

static uint8_t g_client_tx[I2C_TX_MAX];
static uint8_t g_client_tx_len;

/* Frames the sync handler declined, waiting for main-loop delivery. */
#define COLD_QUEUE 8
static uint8_t g_cold[COLD_QUEUE][I2C_RX_MAX];
static uint8_t g_cold_len[COLD_QUEUE];
static uint8_t g_cold_head, g_cold_tail;

/* ── Log ring (board3's i2c_log.c reads this) ─────────────────────────── */

#define LOG_CAPACITY 8u
#define LOG_MASK     (LOG_CAPACITY - 1u)
static I2cLogEntry g_log[LOG_CAPACITY];
static uint8_t g_log_head, g_log_count;

static void log_append(I2cLogKind kind, uint8_t addr, const uint8_t* data, uint8_t len) {
    uint8_t slot;
    if (g_log_count < LOG_CAPACITY) {
        slot = (uint8_t)((g_log_head + g_log_count) & LOG_MASK);
        g_log_count++;
    } else {
        slot = g_log_head;
        g_log_head = (uint8_t)((g_log_head + 1u) & LOG_MASK);
    }
    g_log[slot].kind = (uint8_t)kind;
    g_log[slot].addr = addr;
    g_log[slot].req_id = 0;
    uint8_t n = (len > I2C_LOG_DATA_MAX) ? (uint8_t)I2C_LOG_DATA_MAX : len;
    g_log[slot].len = n;
    memcpy(g_log[slot].data, data, n);
}

/* ── i2c.h implementation ─────────────────────────────────────────────── */

void i2c_init(uint8_t client_addr) {
    (void)client_addr;
    i2c_fake_reset();
}

void i2c_set_cold_rx_handler(I2cCompletion cb) {
    g_cold_rx = cb;
}

void i2c_set_sync_cold_rx_handler(I2cSyncColdRxHandler handler) {
    g_sync_cold_rx = handler;
}

uint8_t i2c_log_snapshot(I2cLogEntry* out, uint8_t capacity) {
    uint8_t count = (g_log_count < capacity) ? g_log_count : capacity;
    for (uint8_t i = 0; i < count; i++) {
        out[i] = g_log[(uint8_t)((g_log_head + g_log_count - 1u - i) & LOG_MASK)];
    }
    return count;
}

I2cResult i2c_set_client_tx(uint8_t* tx, uint8_t tx_len) {
    if (tx == 0) {
        return I2C_RESULT_BAD_ARG;
    }
    if (tx_len == 0 || tx_len > I2C_TX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    memcpy(g_client_tx, tx, tx_len);
    g_client_tx_len = tx_len;
    log_append(I2C_LOG_CT, 0, tx, tx_len);
    return I2C_RESULT_OK;
}

I2cResult i2c_submit(uint8_t addr, uint8_t* tx, uint8_t tx_len, uint8_t rx_len, I2cCompletion cb) {
    if (tx == 0 || tx_len == 0 || tx_len > I2C_TX_MAX || rx_len > I2C_RX_MAX) {
        return I2C_RESULT_BAD_ARG;
    }
    if (g_submit_result != I2C_RESULT_OK) {
        return g_submit_result;
    }
    if (g_tx_count >= I2C_FAKE_MAX_TX) {
        return I2C_RESULT_QUEUE_FULL;
    }

    I2cFakeTx* t = &g_tx[g_tx_count++];
    memset(t, 0, sizeof(*t));
    t->addr = addr;
    t->tx_len = tx_len;
    memcpy(t->tx, tx, tx_len);
    t->rx_len = rx_len;
    t->cb = cb;
    t->state = I2C_FAKE_PENDING;

    if (rx_len == 0 && g_autocomplete_writes) {
        t->state = I2C_FAKE_READY;
        t->result = I2C_RESULT_OK;
    }
    return I2C_RESULT_OK;
}

void i2c_poll(void) {
    /* Cold-rx deliveries first, matching the real driver: an inbound frame is
     * prepended at the head of the queue, so it is dispatched ahead of host
     * completions that were already waiting. */
    if (g_cold_head != g_cold_tail) {
        uint8_t slot = g_cold_head;
        g_cold_head = (uint8_t)((g_cold_head + 1u) % COLD_QUEUE);
        if (g_cold_rx) {
            uint8_t buf[I2C_RX_MAX];
            uint8_t len = g_cold_len[slot];
            memcpy(buf, g_cold[slot], len);
            g_cold_rx(I2C_RESULT_OK, 0, 0, 0, buf, len);
        }
        return;
    }

    for (uint8_t i = 0; i < g_tx_count; i++) {
        I2cFakeTx* t = &g_tx[i];
        if (t->state != I2C_FAKE_READY) {
            continue;
        }
        t->state = I2C_FAKE_DONE;

        log_append(t->rx_len ? I2C_LOG_RA : I2C_LOG_WA, t->addr,
                   t->rx_len ? t->rx : t->tx, t->rx_len ? t->rx_got : t->tx_len);

        if (t->cb) {
            /* The real driver hands out its own buffers, valid only for the
             * duration of the call.  Copy so a callback that stashes the
             * pointer is caught by the same use-after-free it would hit on
             * the device. */
            uint8_t tx[I2C_TX_MAX], rx[I2C_RX_MAX];
            memcpy(tx, t->tx, t->tx_len);
            memcpy(rx, t->rx, t->rx_got);
            t->cb(t->result, t->addr, t->tx_len ? tx : 0, t->tx_len, rx, t->rx_got);
        }
        return; /* one completion per poll, as the real driver does */
    }
}

/* ── Test API ─────────────────────────────────────────────────────────── */

void i2c_fake_reset(void) {
    memset(g_tx, 0, sizeof(g_tx));
    g_tx_count = 0;
    g_submit_result = I2C_RESULT_OK;
    g_autocomplete_writes = 1;
    memset(g_client_tx, 0, sizeof(g_client_tx));
    g_client_tx_len = 0;
    g_cold_head = g_cold_tail = 0;
    memset(g_log, 0, sizeof(g_log));
    g_log_head = g_log_count = 0;
    /* Handlers are deliberately kept: comm_interface_init() registers them
     * once, and a test that resets between phases should not have to
     * re-register to keep receiving. */
}

uint8_t i2c_fake_tx_count(void) {
    return g_tx_count;
}

const I2cFakeTx* i2c_fake_tx(uint8_t index) {
    return (index < g_tx_count) ? &g_tx[index] : 0;
}

const I2cFakeTx* i2c_fake_last_tx(void) {
    return g_tx_count ? &g_tx[g_tx_count - 1u] : 0;
}

uint8_t i2c_fake_find(uint8_t id) {
    for (uint8_t i = 0; i < g_tx_count; i++) {
        if (g_tx[i].state != I2C_FAKE_DONE && g_tx[i].tx_len > 0 && g_tx[i].tx[0] == id) {
            return i;
        }
    }
    return 0xFF;
}

void i2c_fake_set_submit_result(I2cResult result) {
    g_submit_result = result;
}

void i2c_fake_set_autocomplete_writes(uint8_t on) {
    g_autocomplete_writes = on ? 1u : 0u;
}

void i2c_fake_respond(uint8_t index, I2cResult result, const uint8_t* rx, uint8_t rx_len) {
    if (index >= g_tx_count) {
        return;
    }
    I2cFakeTx* t = &g_tx[index];
    t->result = result;
    t->rx_got = 0;
    if (rx && rx_len) {
        if (rx_len > I2C_RX_MAX) {
            rx_len = I2C_RX_MAX;
        }
        memcpy(t->rx, rx, rx_len);
        t->rx_got = rx_len;
    }
    t->state = I2C_FAKE_READY;
}

void i2c_fake_respond_to(uint8_t id, const uint8_t* payload, uint8_t payload_len) {
    uint8_t index = i2c_fake_find(id);
    if (index == 0xFF) {
        return;
    }
    uint8_t buf[I2C_RX_MAX];
    if (payload_len > I2C_RX_MAX - 1u) {
        payload_len = (uint8_t)(I2C_RX_MAX - 1u);
    }
    memcpy(buf, payload, payload_len);
    buf[payload_len] = comm_crc8(buf, payload_len);
    i2c_fake_respond(index, I2C_RESULT_OK, buf, (uint8_t)(payload_len + 1u));
}

void i2c_fake_deliver_write(const uint8_t* frame, uint8_t len) {
    if (len > I2C_RX_MAX) {
        len = I2C_RX_MAX;
    }
    log_append(I2C_LOG_CR, 0, frame, len);

    /* The driver drops any previously staged reply before dispatch, so a
     * refused request cannot answer the next read with stale bytes. */
    g_client_tx_len = 0;

    uint8_t buf[I2C_RX_MAX];
    memcpy(buf, frame, len);
    if (g_sync_cold_rx && g_sync_cold_rx(buf, len) == 0) {
        return; /* consumed in "ISR" context */
    }
    uint8_t next = (uint8_t)((g_cold_tail + 1u) % COLD_QUEUE);
    if (next == g_cold_head) {
        return; /* queue full — dropped, exactly as the driver does */
    }
    memcpy(g_cold[g_cold_tail], frame, len);
    g_cold_len[g_cold_tail] = len;
    g_cold_tail = next;
}

uint8_t i2c_fake_deliver_read_request(const uint8_t* frame, uint8_t len) {
    i2c_fake_deliver_write(frame, len);
    return g_client_tx_len;
}

const uint8_t* i2c_fake_client_tx(uint8_t* len_out) {
    if (len_out) {
        *len_out = g_client_tx_len;
    }
    return g_client_tx_len ? g_client_tx : 0;
}

uint8_t i2c_fake_frame(uint8_t* out, uint8_t id, const uint8_t* payload, uint8_t payload_len) {
    out[0] = id;
    if (payload_len) {
        memcpy(&out[1], payload, payload_len);
    }
    out[1 + payload_len] = comm_crc8(out, (uint8_t)(1 + payload_len));
    return (uint8_t)(payload_len + 2u);
}
