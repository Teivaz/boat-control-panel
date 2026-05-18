# libcomm — Shared I2C Protocol Library

Shared library for multi-board I2C communication on PIC18F27/47/57Q84.

## Components

| File | Description |
|------|-------------|
| `i2c.h` / `i2c.c` | Async, interrupt-driven I2C multi-master driver (host + client) |
| `libcomm.h` / `libcomm.c` | Protocol message builders, parsers, and type definitions |
| `libcomm_interface.h` / `libcomm_interface.c` | High-level send/dispatch layer over I2C + libcomm |
| `task.h` / `task.c` | Cooperative task scheduler with ISR-safe deferred callbacks |

## I2C Driver (`i2c.h`)

Interrupt-driven I2C host + client driver on I2C1.  Default 100 kHz (400 kHz
when `I2C_FME=1`), 7-bit addressing, multi-master with automatic collision
retry.  Bus timeout uses the peripheral BTO (LFINTOSC).

No pin setup, no ISR definitions — boards provide pin/PPS in `i2c_board.c`;
ISRs are defined internally in `i2c.c`.

### API

```c
void i2c_init(uint8_t client_addr);     /* 0 = host-only                */
void i2c_set_cold_rx_handler(I2cCompletion); /* cold-rx delivery (main) */
I2cResult i2c_set_client_tx(uint8_t* tx, uint8_t tx_len); /* stage read reply */
I2cResult i2c_submit(addr, tx, tx_len, rx_len, cb, ctx);
void i2c_poll(void);                     /* main loop — fires callbacks  */
```

ISR-level functions (`I2C1_ISR`, `I2C1_ERROR_ISR`) are defined internally
in `i2c.c` — boards do not need to wire them manually.

### How it works

```
    Main loop                       ISR context
    ─────────                       ───────────

  i2c_submit() ──────┐
                     ▼
              ┌────────────┐
              │ Pending Q  │  ring buffer, 16 slots
              │ [0] [1]... │  each with 8-byte TX buffer
              └─────┬──────┘
                    │  i2c_poll()
                    ▼
              ┌────────────┐
              │ Active op  │  owns the I2C bus
              │ byte-level │  ISR shifts TX/RX per byte
              └─────┬──────┘
                    │  I2C1_ISR()     sets status flags
                    │  (BTO timeout)  enforces timeout
                    ▼
              ┌────────────┐
              │ Completed  │  i2c_poll() fires callback
              └────────────┘  and starts next queued op
```

### Context separation

| Function | Context | Notes |
|----------|---------|-------|
| `i2c_init` | Main | Call once before super-loop |
| `i2c_set_cold_rx_handler` | Main | Set cold-rx delivery handler |
| `i2c_set_client_tx` | Main | Stage client read-reply buffer |
| `i2c_submit` | Main | Copies tx into queue, returns immediately |
| `i2c_poll` | Main | Fires callbacks, starts next op |

ISRs (`I2C1_ISR`, `I2C1_ERROR_ISR`) are internal to `i2c.c`.
No function is called from both contexts.

### Configuration

Override before including `i2c.h`:

| Define | Default | Description |
|--------|---------|-------------|
| `I2C_QUEUE_SIZE` | 16 | Queue depth (must be power of 2) |
| `I2C_TX_MAX` | 8 | Max bytes per TX payload |
| `I2C_RX_MAX` | 8 | Max bytes per RX payload |
| `I2C_CLIENT_BUF_SIZE` | 8 | Max inbound message size (client mode) |
| `I2C_RETRY_COUNT` | 1 | Retry count on bus collision / timeout |

### Board integration

```c
// In main.c:
i2c_pins_init();                  // board-specific pin setup (i2c_board.c)
i2c_init(comm_address());         // 0 = host-only
comm_interface_init();            // registers protocol dispatchers

// Main loop:
while (1) {
    i2c_poll();
    task_controller_poll(&ctrl);
}
```

ISRs are defined inside `libcomm/i2c.c` — boards do not need to provide them.

## Protocol Interface (`libcomm_interface.h`)

High-level layer that connects the I2C driver with the protocol
builders/parsers.  Provides:

1. **Send functions** — `comm_send_*()` for every protocol command
2. **Dispatchers** — automatically parse incoming I2C messages and
   call typed adopter callbacks
3. **Adopter callbacks** — board implements `comm_on_*()` for the
   commands it handles; empty stubs for the rest

### Outbound

```c
// Write commands (fire-and-forget):
comm_send_button_changed(button_id, pressed, mode, cb, ctx); // → main board
comm_send_relay_state(relays, cb, ctx);                       // → switching board
comm_send_button_effect(addr, &effect, cb, ctx);              // → button board

// Read commands (async response via callback):
comm_send_battery_read();     // response → comm_on_battery_read_response()
comm_send_config_read(addr, config_addr);
```

### Inbound (adopter implements)

```c
// Main-loop context — incoming writes:
void comm_on_button_changed_received(const CommButtonChanged* event);
void comm_on_channel_changed_received(const CommChannelChanged* event);
void comm_on_reset(void);

// Main-loop context — read responses:
void comm_on_battery_read_response(CommBattery* battery);

// ISR context — incoming read requests:
void comm_on_config_read_requested(uint8_t address);
```
