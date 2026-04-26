# I2C Multi-Master Host Driver

Interrupt-driven I2C host driver with DMA for PIC18F27/47/57Q84.

## Hardware used

| Resource | Role |
|----------|------|
| I2C1     | Multi-master host, 7-bit addressing, 400 kHz Fast mode |
| DMA7     | TX path: RAM buffer → I2C1TXB |
| DMA8     | RX path: I2C1RXB → RAM buffer |
| Timer1   | 1 ms tick for software operation timeouts |

## How it works

```
    Main loop                       ISR context
    ─────────                       ───────────

  i2c_submit_write() ──┐
  i2c_submit_read()  ──┤
                       ▼
                ┌────────────┐
                │ Pending Q  │  ring buffer, 4 slots
                │ [0] [1]... │  each with 16-byte DMA buffer
                └─────┬──────┘
                      │  i2c_poll()
                      ▼
                ┌────────────┐
                │ Active op  │  owns the I2C bus
                │ DMA7 → TX  │  zero CPU during data phase
                │ RX → DMA8  │
                └─────┬──────┘
                      │  i2c_i2c1_isr()  sets status flags
                      │  i2c_timer1_isr() enforces timeout
                      ▼
                ┌────────────┐
                │ Completed  │  i2c_poll() fires read callback
                └────────────┘  and starts next queued op
```

All byte-level clocking is handled by DMA. The ISRs do only flag
manipulation — no loops, no callbacks, no unbounded work. `i2c_poll()`
is O(1) per call.

## Context separation

| Function | Context | Notes |
|----------|---------|-------|
| `i2c_init` | Main | Call once before super-loop |
| `i2c_submit_write` | Main | Copies data into queue, returns immediately |
| `i2c_submit_read` | Main | Queues read, callback fires from `i2c_poll` |
| `i2c_poll` | Main | Dequeues ops, starts transfers, fires callbacks |
| `i2c_timer1_isr` | ISR | 1 ms tick, timeout enforcement |
| `i2c_i2c1_isr` | ISR | Bus event handler (collision, NACK, Stop, timeout) |

No function is called from both contexts.

## Usage

```c
#include "i2c.h"

static I2CDriver i2c;

void read_done(uint8_t addr, const uint8_t *data, uint8_t len,
               I2CStatus status, void *ctx)
{
    if (status == I2C_STATUS_OK) {
        /* process data[0..len-1] — copy it, the pointer is
           only valid during this callback */
    }
}

void main(void)
{
    /* ... pin setup, oscillator, etc ... */
    i2c_init(&i2c);

    /* Write 3 bytes to device 0x50 (fire and forget) */
    uint8_t tx[] = {0x00, 0x42, 0x43};
    i2c_submit_write(&i2c, 0x50, tx, sizeof(tx), 100);

    /* Read 4 bytes from device 0x68 */
    i2c_submit_read(&i2c, 0x68, 4, 100, read_done, NULL);

    for (;;) {
        i2c_poll(&i2c);
        /* ... other work ... */
    }
}

/* In your interrupt dispatcher: */
void __interrupt(irq(TMR1)) tmr1_handler(void) {
    i2c_timer1_isr(&i2c);
}
void __interrupt(irq(I2C1), irq(I2C1E)) i2c1_handler(void) {
    i2c_i2c1_isr(&i2c);
}
```

## Configuration

Override before including `i2c.h`:

| Define | Default | Description |
|--------|---------|-------------|
| `I2C_QUEUE_SIZE` | 4 | Queue depth (must be power of 2) |
| `I2C_MAX_PAYLOAD` | 16 | Max bytes per operation (DMA buffer size) |
| `I2C_DEFAULT_RETRIES` | 3 | Retry count on bus collision |

## State diagrams

### Operation lifecycle

```
                  i2c_submit_write()
                  i2c_submit_read()
                         │
                         ▼
                   ┌──────────┐
                   │ PENDING  │  in queue, waiting for bus
                   └────┬─────┘
                        │ i2c_poll() dequeues, calls start_transfer()
                        ▼
                   ┌──────────┐
              ┌───▶│  ACTIVE  │  DMA + I2C hardware own the bus
              │    └──┬───┬───┘
              │       │   │
              │       │   ├─── PCIF (Stop complete) ──────▶ OK
              │       │   ├─── NACKIF ────────────────────▶ NACK
              │       │   ├─── BTOIF (hw bus timeout) ────▶ TIMEOUT
              │       │   ├─── timer_ms reaches 0 ────────▶ TIMEOUT
              │       │   └─── BCLIF (collision) ─────────▶ BUS_BUSY
              │       │                                        │
              │       │              retries > 0?              │
              │       │              ┌────┴────┐               │
              │       │              │ yes     │ no            │
              │       │              ▼         ▼               │
              │       │         (re-arm)    ERROR              │
              │       │            │                           │
              └───────┘            └───────────────────────────┘
                  retry

         Terminal states: OK, NACK, TIMEOUT, ERROR
         i2c_poll() fires read callback (if any) then advances queue.
```

### Write transaction (bus level)

```
  Main loop          I2C1 Hardware + DMA7             Client
  ─────────          ─────────────────────            ──────
      │
      │ start_transfer():
      │   I2C1ADB1 = addr<<1
      │   I2C1TXB  = buf[0]
      │   I2C1CNT  = len
      │   arm DMA7 for buf[1..len-1]
      │   set S bit
      │
      │               ┌─── wait for bus free (BFRE) ───┐
      │               │                                │
      │               ▼                                │
      │         [S] Start condition                    │
      │               │                                │
      │               ▼                                │
      │         [ADDR+W] ──────────────────────▶ match?
      │               │                           │
      │               │                      ◀── ACK ──
      │               ▼
      │         [buf[0]] from TXB ─────────▶ receive
      │               │                       │
      │               │                  ◀── ACK ──
      │               ▼
      │         [buf[1]] DMA7 loads TXB ───▶ receive     ← zero CPU
      │               │                       │
      │               │                  ◀── ACK ──
      │              ...                     ...
      │               │
      │         [buf[n-1]] last byte ──────▶ receive
      │               │                       │
      │               │  I2C1CNT = 0     ◀── ACK ──
      │               ▼
      │         [P] Stop condition
      │               │
      │               ▼
      │          PCIF fires ──▶ i2c_i2c1_isr()
      │                          status = OK
      │                          active = 0
      │
      │ i2c_poll():
      │   advance queue head
      ▼
```

### Read transaction (bus level)

```
  Main loop          I2C1 Hardware + DMA8             Client
  ─────────          ─────────────────────            ──────
      │
      │ start_transfer():
      │   I2C1ADB1 = addr<<1 | 1
      │   I2C1CNT  = len
      │   arm DMA8 for buf[0..len-1]
      │   set S bit
      │
      │         [S] Start condition
      │               │
      │               ▼
      │         [ADDR+R] ──────────────────────▶ match?
      │               │                           │
      │               │                      ◀── ACK ──
      │               ▼
      │         host clocks ◀───────────── [data 0]      ← zero CPU
      │         DMA8 copies to buf[0]         │
      │         ACK (CNT > 0) ─────────────▶  │
      │               │
      │         host clocks ◀───────────── [data 1]
      │         DMA8 copies to buf[1]
      │         ACK (CNT > 0) ─────────────▶
      │              ...                     ...
      │               │
      │         host clocks ◀───────────── [data n-1]
      │         DMA8 copies to buf[n-1]
      │         NACK (CNT = 0, ACKCNT=1) ──▶  │
      │               │
      │               ▼
      │         [P] Stop condition
      │               │
      │               ▼
      │          PCIF fires ──▶ i2c_i2c1_isr()
      │                          status = OK
      │                          active = 0
      │
      │ i2c_poll():
      │   callback(addr, buf, len, OK, ctx)
      │   advance queue head
      ▼
```

### Bus collision (multi-master arbitration)

```
  This host           Bus                  Other host
  ─────────           ───                  ──────────
      │                                        │
      │──── [S] Start ────────────── [S] Start─┤
      │                                        │
      │──── [ADDR] bit 7 ──────── [ADDR] bit 7─┤  both drive SDA
      │──── [ADDR] bit 6 ──────── [ADDR] bit 6─┤
      │──── [ADDR] bit 5 = 1      [ADDR] bit 5 = 0 ─┤
      │         │                                │
      │    SDA reads 0, but                      │
      │    we drove 1 → mismatch!                │
      │         │                                │
      │    BCLIF set                        (wins, continues)
      │         │
      │    i2c_i2c1_isr():
      │      status = BUS_BUSY
      │      active = 0
      │         │
      │    i2c_poll():
      │      retries > 0?
      │      ├── yes: retries--, re-arm
      │      └── no:  status = ERROR, advance queue
      ▼
```

### Timeout sequence

```
  Main loop          Timer1 ISR              I2C bus
  ─────────          ──────────              ────────
      │
      │ start_transfer():
      │   timer_ms = timeout_ms
      │
      │               ├─ 1 ms ─▶ timer_ms--
      │               ├─ 1 ms ─▶ timer_ms--
      │              ...
      │               ├─ 1 ms ─▶ timer_ms-- → 0!
      │               │
      │               │  i2c_timer1_isr():
      │               │    disable DMA7, DMA8
      │               │    I2C1CON0.EN = 0   ──▶  bus released
      │               │    I2C1CON0.EN = 1        (SDA/SCL float high)
      │               │    status = TIMEOUT
      │               │    active = 0
      │               │
      │ i2c_poll():
      │   deliver TIMEOUT to callback (reads)
      │   advance queue, start next op
      ▼
```

## Bus collision handling

On a multi-master bus, two hosts may start transmitting simultaneously.
The I2C hardware detects the collision (BCLIF) and the ISR marks the
operation as `I2C_STATUS_BUS_BUSY`. On the next `i2c_poll()`, if retries
remain, the same operation is re-armed. After all retries are exhausted,
the status becomes `I2C_STATUS_ERROR`.

## Timeout

Each operation carries a `timeout_ms` value. Timer1 decrements a
countdown each millisecond. When it reaches zero the driver disables
DMA and resets the I2C module to release the bus. The operation status
becomes `I2C_STATUS_TIMEOUT`.

## Return codes

`i2c_submit_write` / `i2c_submit_read`:

| Code | Meaning |
|------|---------|
| 0 | Queued successfully |
| -1 | Queue full |
| -2 | Invalid length (0 or > I2C_MAX_PAYLOAD) |

## Files

- `lib/i2c.h` — public types and API
- `lib/i2c.c` — implementation
- `lib/README.md` — this file
