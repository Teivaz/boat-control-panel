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

Interrupt-driven I2C host + client driver on I2C1.  400 kHz Fast mode,
7-bit addressing, multi-master with automatic collision retry.

No pin setup, no ISR definitions — boards provide those.

### API

```c
void i2c_init(uint8_t client_addr);     /* 0 = host-only                */
void i2c_set_rx_handler(I2cRxHandler);   /* slave write callback (ISR)   */
void i2c_set_read_handler(I2cReadHandler); /* slave read callback (ISR)  */
I2cResult i2c_submit(addr, tx, tx_len, rx_buf, rx_len, cb, ctx);
void i2c_poll(void);                     /* main loop — fires callbacks  */
void i2c_tick_ms(void);                  /* call from 1 ms ISR           */
void i2c_isr(void);                      /* call from I2C1/TX/RX vector  */
void i2c_error_isr(void);               /* call from I2C1E vector       */
```

### How it works

```
    Main loop                       ISR context
    ─────────                       ───────────

  i2c_submit() ──────┐
                     ▼
              ┌────────────┐
              │ Pending Q  │  ring buffer, 8 slots
              │ [0] [1]... │  each with 8-byte TX buffer
              └─────┬──────┘
                    │  i2c_poll()
                    ▼
              ┌────────────┐
              │ Active op  │  owns the I2C bus
              │ byte-level │  ISR shifts TX/RX per byte
              └─────┬──────┘
                    │  i2c_isr()       sets status flags
                    │  i2c_tick_ms()   enforces timeout
                    ▼
              ┌────────────┐
              │ Completed  │  i2c_poll() fires callback
              └────────────┘  and starts next queued op
```

### Context separation

| Function | Context | Notes |
|----------|---------|-------|
| `i2c_init` | Main | Call once before super-loop |
| `i2c_set_rx_handler` | Main | Set slave write handler |
| `i2c_set_read_handler` | Main | Set slave read handler |
| `i2c_submit` | Main | Copies tx into queue, returns immediately |
| `i2c_poll` | Main | Fires callbacks, starts next op |
| `i2c_tick_ms` | ISR | 1 ms tick, timeout enforcement |
| `i2c_isr` | ISR | Bus event handler |
| `i2c_error_isr` | ISR | Error handler (NACK, collision) |

No function is called from both contexts.

### Configuration

Override before including `i2c.h`:

| Define | Default | Description |
|--------|---------|-------------|
| `I2C_QUEUE_SIZE` | 8 | Queue depth (must be power of 2) |
| `I2C_TX_MAX` | 8 | Max bytes per TX payload |
| `I2C_CLIENT_BUF_SIZE` | 8 | Max inbound message size (client mode) |
| `I2C_RETRY_COUNT` | 3 | Retry count on bus collision / timeout |

### Board integration

```c
// In main.c:
i2c_pins_init();                  // board-specific pin setup
i2c_init(comm_address());         // 0 = host-only
comm_interface_init();            // registers protocol dispatchers

// ISR definitions:
void __interrupt(irq(I2C1TX, I2C1RX, I2C1), base(8)) I2C1_ISR(void) {
    i2c_isr();
}
void __interrupt(irq(I2C1E), base(8)) I2C1_ERROR_ISR(void) {
    i2c_error_isr();
}

// Main loop:
while (1) {
    i2c_poll();
    task_controller_poll(&ctrl);
}
```

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
comm_send_button_changed(button_id, pressed, mode);  // → main board
comm_send_relay_state(relays);                        // → switching board
comm_send_button_effect(addr, &effect);               // → button board

// Read commands (async response via callback):
comm_send_battery_read();     // response → comm_on_battery_read_response()
comm_send_config_read(addr, config_addr);
```

### Inbound (adopter implements)

```c
// ISR context — incoming writes:
void comm_on_button_changed_received(const CommButtonChanged* event);
void comm_on_relay_changed_received(const CommRelayChanged* event);
void comm_on_reset(void);

// ISR context — incoming read requests:
uint8_t comm_on_config_read_request(uint8_t address, uint8_t* value);

// Main-loop context — read responses:
void comm_on_battery_read_response(CommBattery* battery);
```
