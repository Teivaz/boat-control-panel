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

Only `build.sh`, `toolchain/`, `protocol.md`, `board2-buttons/`, and `libcomm/` are git-tracked. MPLAB X project directories (`*.X/`), `sample/`, and `u8g2/` are intentionally git-ignored.

| Path | Description |
|---|---|
| `board2-buttons/` | C sources for the button board board |
| `libcomm/` | Shared I2C communication library (linked into each board) |
| `toolchain/Dockerfile` | Docker build environment |
| `protocol.md` | I2C multi-board communication protocol spec |

The `*.X/` directories are experimental MPLAB X projects used to prototype and verify chip capabilities. They will not make it to the final product but serve as reference for how specific hardware features were explored.

## Architecture

### Execution model

`main.c` calls each module's `init()`, then `interrupt_init()` last (interrupts must be enabled after all peripherals are configured). The `while(1)` loop is idle (`__nop()`); all logic runs in a 100 Hz timer callback (`on_update`).

### ISR wiring

All ISRs use `__interrupt(irq(...), base(8))` with IVT locked to `0x0008`. The `interrupt` module registers callbacks for IOC and TMR0. Do not use the MCC-generated interrupt dispatcher — hand-written ISRs are registered directly via `interrupt_set_handler_*`.

### libcomm

`libcomm/` is a shared library compiled to `.p1` (XC8 pre-linked LLVM bitcode) objects and linked directly into each board project — it cannot be archived with `xc8-ar`. The board's `Makefile` compiles libcomm via `make -C ../libcomm dist` and links the resulting `.p1` objects.

`cmd_address()` returns the device's I2C address at runtime, selected by a compile-time `DEVICE_TYPE_*` macro (`DEVICE_TYPE_INPUT`, `DEVICE_TYPE_MAIN`, `DEVICE_TYPE_SWITCHING`) set in the board's Makefile via `-DDEVICE_TYPE_INPUT`.

### I2C (client mode)

`libcomm/i2c.c` implements an interrupt-driven I2C client on I2C1 (pins RC3/RC4). The hardware ISR dispatches `I2cClientEvent` values to `_i2c_interrupt_handler`. Clock stretching (`CSTR`) is released at the end of every ISR to avoid bus stalls. The client address is hardcoded to `0x22` in `i2c_init` — `cmd_address()` is used for outbound messages, not the peripheral register.

### Multi-Board Protocol

Boards communicate over I2C in multi-master mode. All messages begin with a command byte. See `protocol.md` for the full specification.

**Button board messages**

| Message | Direction | Summary |
|---|---|---|
| `button_effect` | main → button board (write) | 8 outputs × 4-bit nibbles (on/off per output) |
| `button_changed` | button board → main | sender address, prev state, current state (bitmasks) |
| `button_state` | main → input (read) | current physical button state (1 byte bitmask) |
| `button_trigger` | main → input (read/write) | per-button mode and timing (`MMEETTTT` format) |

**Switching board messages**

| Message | Direction | Summary |
|---|---|---|
| `relay_changed` | switching → main | sender address, prev/current channel states (2 × 2-byte bitmasks), prev/current sensor states (2 × 1-byte bitmasks) |
| `relay_state` | main → switching (read/write) | target state of all 16 relays (2-byte bitmask) |
| `relay_mask` | main → switching (read/write) | event mask for physical state change events (2-byte bitmask) |
| `battery` | main → switching (read) | battery voltage (uint16) |
| `levels` | main → switching (read) | both level meter values (2 × uint8) |
| `level_mode` | main → switching (read/write) | operating mode for each level meter (1 byte) |
| `sensors` | main → switching (read) | state of 3 on/off sensors (1 byte bitmask) |

