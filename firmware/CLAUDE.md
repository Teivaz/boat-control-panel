# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded firmware for a 3-board boat panel controller written in C. All boards run an 8-bit **PIC18F27Q84**, so `uint8_t` is the primary integer type. Compiled with **Microchip XC8 v3.10**.

| Board | Role |
|---|---|
| Main board | Central controller with display |
| Button board (`board2-buttons`) | Button inputs + per-button RGB LEDs |
| Switching board | 16 relay channels, 2 level meters, battery voltage, 3 on/off sensors |

## Build

Run from the board source directory (e.g. `board2-buttons/`):

```sh
../build.sh
```

`build.sh` derives the board name from the current directory, builds (or reuses) a `pic18f-xc8:3.10` Docker image, and produces a `.hex` in `dist/default/production/`. First run downloads ~300 MB.

To clean:
```sh
make clean CONF=default
```

There are no unit tests — validation is done via hardware flashing with a PICkit/ICD programmer.

## Repository Structure

Git-tracked: `build.sh`, `.clang-format`, `toolchain/`, `protocol.md`, `libcomm/`, and one directory per board (`board1-switching/`, `board2-buttons/`, `board3-main/`). MPLAB X project directories (`*.X/`), `sample/`, and a top-level `u8g2/` are git-ignored.

| Path | Description |
|---|---|
| `board1-switching/` | C sources for the switching board (`DEVICE_TYPE_SWITCHING`) |
| `board2-buttons/` | C sources for the button board (`DEVICE_TYPE_INPUT`) |
| `board3-main/` | C sources for the main board (`DEVICE_TYPE_MAIN`); vendors a trimmed subset of `u8g2/` for the SSD1322 OLED — this copy **is** tracked (only the top-level `u8g2/` is ignored) |
| `libcomm/` | Shared protocol library + task scheduler, linked into each board |
| `toolchain/Dockerfile` | Docker build environment |
| `protocol.md` | I2C multi-board protocol spec — source of truth for command IDs, addresses, and bit layouts |
| `board{1,3}/readme.md` | Per-board hardware notes (pinout, relay map, connected peripherals) — read these first when touching a board |

The `*.X/` directories are experimental MPLAB X projects used to prototype and verify chip capabilities. They will not make it to the final product but serve as reference for how specific hardware features were explored.

Code style is enforced by `.clang-format` (4-space indent, 120 col, attached braces, pointer-left, `InsertBraces: true`). Match it when editing.

## Architecture

### Execution model

Every board's `main.c` follows the same shape: initialize peripherals, `task_controller_init(&ctrl)`, hand `&ctrl` to each module's `_init` so they can register tasks, configure the 1 ms TMR0 tick (local `tick_init` in each `main.c`), then call `interrupt_init()` last (interrupts must be enabled after all peripherals are configured). The `while(1)` loop body is `task_controller_poll(&ctrl)` — every task callback runs in main context, never in ISR context.

### Task scheduler (`libcomm/task.c` / `libcomm/task.h`)

Shared scheduling primitive used by all boards. Design notes that matter for callers:

- Deferred dispatch: the TMR0 ISR calls `task_controller_tick(&ctrl)` which only decrements `remaining_ms` and sets `pending`; callbacks are fired from `task_controller_poll`.
- Interval range 1..15000 ms. After a callback returns, `remaining_ms` is reloaded from the task's *current* `interval_ms` — so a callback may change its own interval via `task_controller_set_interval` and have the new value take effect on the next fire.
- Removing a task (including the currently-executing one) from inside a callback is safe; the poll loop rechecks `id` and `pending` before the post-callback reload.
- The scheduler does **not** own TMR0. Each board configures TMR0 itself in `tick_init` (typically 8-bit mode, Fosc/4, /128 prescaler, match = 124 → 1 ms) and defines `TMR0_ISR` locally (usually in `main.c`) to call `task_controller_tick`. Task IDs live in each board's `task_ids.h`.
- `run_in_main_loop(ctrl, cb, ctx)` is ISR-callable and schedules `cb(ctx)` to run once on the next `task_controller_poll`. Use it from ISRs to push non-trivial work out of interrupt context. Queue depth is `TASK_DEFERRED_QUEUE_SIZE` (4); on overflow the call drops — callers that need lossless delivery must dedupe via a flag cleared by their callback.

### ISR wiring

All ISRs use `__interrupt(irq(...), base(8))` with IVT locked to `0x0008`. Each ISR is defined in the module that owns the peripheral it serves (e.g. `TMR0_ISR` in `main.c`, `IOC_ISR` in the input/sensors module that enables it, `I2C1_ISR` in `i2c.c`, `AD_ISR` in `adc.c`). `interrupt.c` holds only `interrupt_init` (IVT lock/unlock + GIE) and `__asm("RESET")` stubs for vectors no board module claims. Do not use the MCC-generated interrupt dispatcher.

### libcomm

`libcomm/` is a shared library compiled to `.p1` (XC8 pre-linked LLVM bitcode) objects and linked directly into each board project — it cannot be archived with `xc8-ar`. The board's `Makefile` compiles libcomm via `make -C ../libcomm dist` and links the resulting `.p1` objects.

libcomm is **transport-agnostic** — it contains no I2C code. The API splits cleanly into:

- **Builders** (`comm_build_*`): fill a caller-provided `CommMessage` (tagged union: `id` byte + payload union) and return the total byte count to transmit.
- **Parsers** (`comm_parse_*`): read a raw payload byte array (no `id` byte) into a typed struct.

Command IDs follow a fixed pattern: write form `0x0X`, read form `0x8X = write | 0x80`. Bitfield structs (`CommTriggerConfig` `MMEETTTT`, `CommLevelMode`, etc.) rely on XC8's LSB-first allocation to match the wire format directly — some parsers write to a bitfield via `*(uint8_t *)&field = data[0]` and assume this layout.

`comm_address()` returns the device's I2C address at runtime, selected by a compile-time `DEVICE_TYPE_*` macro (`DEVICE_TYPE_INPUT`, `DEVICE_TYPE_MAIN`, `DEVICE_TYPE_SWITCHING`) set in the board's Makefile via `-DDEVICE_TYPE_INPUT`. For `DEVICE_TYPE_INPUT` boards the address further depends on `PORTB.RB0` (L vs R variant).

### I2C (multi-master, button board)

`board2-buttons/i2c.c` runs in client mode most of the time and briefly switches to host mode for outbound transmits. Pins RC3/RC4, fast mode (400 kHz, BAUD=79 at Fosc=64 MHz). The client-side ISR assembles complete messages into a fixed-size buffer and invokes two application callbacks registered via `i2c_set_rx_handler` / `i2c_set_read_handler`; callbacks run in ISR context and must stay short. On a read request the handler fills a response buffer that the ISR then shifts out byte-by-byte on TX_READY. Clock stretching (`CSTR`) is released at the end of every ISR. The client match address comes from `comm_address()`.

`i2c_transmit` is the host-mode entry point: waits for bus-free (`I2C1STAT0.BFRE`), masks just the I2C interrupt group (TMR0/IOC stay live), switches `MODE` to host, drives the transaction, and restores client mode on exit. Returns `I2cResult` — callers should treat `BUSY` / `COLLISION` as retryable.

`comm.c` is the protocol dispatcher layered on top: maps incoming command IDs to the button/led_effect/config modules and builds outbound `button_changed` messages. `i2c.c` has no knowledge of libcomm.

### Multi-board protocol

Boards communicate over I2C in multi-master mode. Every message begins with a command byte; MSB clear (`0x00–0x7F`) is a write, MSB set (`0x80–0xFF`) is a read (write phase sends the command, repeated-start read phase returns the payload). See `protocol.md` for addresses, the full command set, and per-byte bit layouts.
