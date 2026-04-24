# Panel 01

Electrical control panel for a boat. A single three-board unit that drives the
cabin relays, exposes them through a button panel with per-button RGB
feedback, and displays system state (battery, fuel and fresh-water level,
sensors, clock, nav-light selection) on a small OLED.

## Hardware layout

Three PIC18F27Q84 boards communicate over a shared I2C bus (multi-master):

| Board | Directory | Role |
|---|---|---|
| Switching | `circuit/board1-switching/` | 16 relay outputs (shift-register + mux back-read), 2 float-meter inputs, battery divider, 3 digital sensors (bilge / shore / AC). |
| Button (L/R) | `circuit/board2-buttons/` | 7 momentary buttons with per-button RGB indicator LEDs and configurable trigger modes. Two physical boards per unit (left and right), address selected by strap. |
| Main | `circuit/board3-main/` | SSD1322 OLED, DS3231 RTC, config switch. Orchestrates the other boards: receives button events, maintains relay intent, drives nav-light logic, and hosts the config menu. |

## Repository layout

- `firmware/` — C firmware for all three boards. Self-contained build via
  Docker (`firmware/build.sh`). See [`firmware/CLAUDE.md`](firmware/CLAUDE.md)
  and [`firmware/protocol.md`](firmware/protocol.md) for the I2C protocol,
  module conventions, and per-board details.
- `circuit/` — KiCad schematics and PCB layouts, one directory per board plus
  a shared symbol/footprint library (`panel-library.*`) and connection
  diagrams.
- `design/` — Front-panel artwork (Affinity Designer / Photo sources) and DXF
  exports for panel cutouts.
- `models/` — STEP files for mechanical assemblies and 3D-printed parts.
- `playground/` — Experimental sketches (currently the SSD1322 I2C display
  bring-up).

## Functional overview

- **Power**: a panel-wide power button on the left board gates every
  non-nav-light relay. Relay and nav-mode state are restored from a RAM
  snapshot when power is toggled back on.
- **Relays (5..15)**: instruments, autopilot, inverter, water pump, fridge,
  deck/cabin lights, USB, two aux channels. Each is tied to a single button
  and toggles on press. A button flags ERROR when the physical relay state
  doesn't converge to the requested one.
- **Nav lights (relays 0..4)**: anchoring / tricolor / steaming / bow / stern
  driven by a 3-button nav-mode selector. The main board filters requested
  combinations against the configured "enabled lights" mask (set in the
  config menu) and surfaces an error if the request is incompatible.
- **Config mode**: toggling the RA7 switch on the main board enters a menu
  (nav-light mask, time-of-day, water/fuel float offsets). The same buttons
  become menu navigation while active.
- **Button trigger modes**: each button has an independent trigger
  (release / hold / change) and time threshold, configured via the protocol
  and persisted to each board's data EEPROM.

## Firmware highlights

- No RTOS — all boards share a cooperative 1 ms-tick scheduler (`libcomm/
  task.c`) with a deferred ISR-to-main-loop queue (`run_in_main_loop`).
- Transport-agnostic protocol library (`libcomm/`) linked into all three
  boards. I2C, buttons, relays, NVM, and display code are board-local.
- Built with Microchip XC8 v3.10 inside a pinned Docker image; no MPLAB X
  install required. `./firmware/build.sh` from a board source directory
  produces a `.hex` in `dist/default/production/`.
- No unit tests — validation is done on hardware via a PICkit/ICD
  programmer. Per-board readmes in `firmware/board{1,3}/readme.md` document
  pinout and wiring.
