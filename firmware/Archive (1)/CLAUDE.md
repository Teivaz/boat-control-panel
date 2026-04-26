# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

PIC18 microcontroller embedded systems project targeting the Microchip PIC18F27/47/57Q84 family (64 MHz, 128 KB Flash, 12.8 KB SRAM). Two core libraries:

- **I2C Multi-Master Host Driver** (`lib/i2c.h`, `lib/i2c.c`) — interrupt-driven, DMA-backed, queue-based I2C host with collision handling and automatic retry
- **Task Scheduler** (`lib/task.h`, `lib/task.c`) — cooperative multitasking with periodic tasks and ISR-to-main deferred callbacks

## Build

No Makefile/CMake. Built via **MPLAB X IDE** with the **XC8 compiler**. For syntax checking without a full toolchain:

```bash
cc -std=c99 -fsyntax-only -Iinclude -Ilib lib/i2c.c
```

No automated test harness — testing requires PIC18 hardware or simulator.

## Code Organization

- `include/` — Auto-generated hardware register definition headers (37 files). **Do not edit these.**
- `include/pic18f_q84.h` — Master include; defines `SFR8`/`SFR16`/`SFR32` macros for volatile register access
- `lib/` — Core drivers and utilities
- `lib/README.md` — Detailed I2C design documentation with bus-level diagrams and state machines

## Architecture: Key Constraints

**Context separation is critical.** Both drivers split functions strictly between main-loop context and ISR context — no function is called from both. See the context tables in `lib/README.md` (I2C) and `lib/task.h` (scheduler). Never call a main-only function from ISR context.

**FOSC = 64 MHz is hardcoded.** Timer1 1 ms period and I2C 400 kHz baud calculations both assume this clock. Changing FOSC requires reconfiguring both drivers.

**Hardware resource allocation:**
| Resource | Owner | Purpose |
|----------|-------|---------|
| I2C1 | i2c driver | 7-bit host, 400 kHz Fast mode |
| DMA7 | i2c driver | TX: RAM → I2C1TXB |
| DMA8 | i2c driver | RX: I2C1RXB → RAM |
| Timer1 | shared | 1 ms tick for both i2c timeouts and task scheduler |

**Shared patterns across both drivers:**
- `volatile` flags for ISR-to-main signaling (no locks on 8-bit PIC)
- `INTERRUPT_PUSH`/`INTERRUPT_POP` guards for atomic shared-state mutations
- Power-of-2 ring buffers with mask-based wrapping (no modulo)
- `I2C_QUEUE_SIZE` and deferred callback queue size must be powers of 2
