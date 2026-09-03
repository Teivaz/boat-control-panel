# I2C Communication Protocol Description

All devices should be able to operate in standard (100 kHz) and fast (400 kHz) modes.
The system operates in multi-master mode with an event-based system. If a device needs to implement polling the time interval for polling calls should be ≥ 20 ms.
Addressing scheme is 7-bit.
Signal lines are pulled up to 3.3V.

## Addresses

0x40 - Main control board
0x41 - reserved
0x42 - Switching board
0x43 - reserved
0x44 - button board L
0x45 - button board L2
0x46 - button board R
0x47 - button board R2
0x48..0x4F - reserved
0x68 - RTC module (DS3231)

## Messages

All write messages begin with a command byte (byte 0) with the MSB clear (0x00–0x7F). Read messages use a write phase to send the command byte with the MSB set (0x80–0xFF), followed by a repeated-start read phase. The read command ID equals the write command ID OR 0x80.

### Checksum

Every on-wire message ends with a single CRC-8 byte (polynomial `0x07`, initial value `0x00`, no final XOR — the SMBus/SAE-J1850 CRC). The CRC is computed over every preceding byte of that message:

- **Write messages** (id + payload): CRC spans `[id, payload[0..N-1]]`. Total wire length is `1 + N + 1` bytes.
- **Read-phase responses** (payload only, no id): CRC spans `[payload[0..N-1]]`. Total wire length is `N + 1` bytes.
- **Command-only messages** (`reset`, most `*_read` write phases): CRC spans `[id]` alone. Total wire length is 2 bytes.

Receivers drop any message whose trailing byte doesn't match the computed CRC. Per-byte payload sizes below do **not** include the CRC byte; add 1 to the byte count on the wire.

| Command | Value | Direction |
| --- | --- | --- |
| `reset` | 0x0F | main → any (write) |
| `config` | 0x0E | main → any (write) |
| `config_read` | 0x8E | main → any (read) |
| `button_effect` | 0x01 | main → button board |
| `button_changed` | 0x02 | button board → main |
| `button_state_read` | 0x83 | main → button board (read) |
| `button_trigger` | 0x04 | main → button board (write) |
| `button_trigger_read` | 0x84 | main → button board (read) |
| `relay_state` | 0x05 | main → switching board (write) |
| `relay_state_read` | 0x85 | main → switching board (read) |
| `channel_changed` | 0x06 | switching board → main |
| `channel_state_read` | 0x87 | main → switching board (read) |
| `battery_read` | 0x88 | main → switching board (read) |
| `levels_read` | 0x89 | main → switching board (read) |
| `level_mode` | 0x0A | main → switching board (write) |
| `level_mode_read` | 0x8A | main → switching board (read) |
| `sensors_read` | 0x8B | main → switching board (read) |
| `test_echo` | 0x0C | any → any (write) |
| `test_echo_response` | 0x0D | any → requester (write) |
| `test_read` | 0x8C | any → any (read) |

### Common

The following commands are supported by every addressable device.

- reset - perform a soft reset; the device restarts as if powered on. No response is emitted.
  - write byte 0: command - 0x0F

- config - write one byte of the device configuration. Persisted to non-volatile storage; survives power cycles. Address space and byte meaning are device-specific.
  - write byte 0: command - 0x0E
  - write byte 1: address - byte offset within the device configuration
  - write byte 2: value - configuration byte to store

- config_read - read one byte of the device configuration
  - write byte 0: command - 0x8E
  - write byte 1: address - byte offset within the device configuration
  - read byte 0: value - configuration byte at the requested address

The low end of the configuration address space is reserved for universal fields present on every device. Device-specific fields live at `0x10` and above.

| Address | Field | Access | Description |
|---|---|---|---|
| 0x00 | device_id | read-only | 7-bit I2C address of this device; matches the address used to reach it |
| 0x01 | hw_revision | read-only | Hardware revision, monotonic per board |
| 0x02 | sw_revision | read-only | Firmware revision, monotonic per build |

#### Diagnostic test commands

Bus-bring-up / integration-test helpers. Every device services them with no application involvement.

- test_echo - the requester writes its own address and an arbitrary value; the receiver replies with a `test_echo_response` write addressed to that requester, carrying the responder's own address and the same value. The `address` field always identifies the sender, so the requester learns which device answered. Exercises the multi-master write path in both directions (requester and receiver must each act as host).
  - write byte 0: command - 0x0C
  - write byte 1: address - requester's own I2C address (where the response is sent)
  - write byte 2: value - arbitrary value, echoed back unchanged

- test_echo_response - the reply emitted by a device that received a `test_echo`.
  - write byte 0: command - 0x0D
  - write byte 1: address - responder's own I2C address
  - write byte 2: value - the value from the test_echo, unchanged

- test_read - the write phase carries one value byte; the read phase returns that same byte. Exercises the client-TX read-staging path with a single host (no second master needed).
  - write byte 0: command - 0x8C
  - write byte 1: value - arbitrary value to be echoed
  - read byte 0: value - the value from the write phase, unchanged

### Main board (0x40)

- button_changed - pushed by a button board when a button's trigger fires, or when its trigger is (re)configured
  - write byte 0: command - 0x02
  - write byte 1: device_address - address of the sending button board
  - write byte 2: button_id - `[7:6]` = 0, `[5:4]` = button mode, `[3]` button pressed, `[2:0]` button index

- channel_changed - pushed by the switching board when any channel-voltage bit (mux-observed downstream of each relay's fuse) or any sensor changes state
  - write byte 0: command - 0x06
  - write byte 1: device_address - address of the sending switching board
  - write byte 2: prev_channels_lo - previous channel-voltage state of channels 0–7; bit N = channel N
  - write byte 3: prev_channels_hi - previous channel-voltage state of channels 8–15; bit N = channel N−8
  - write byte 4: current_channels_lo - current channel-voltage state of channels 0–7
  - write byte 5: current_channels_hi - current channel-voltage state of channels 8–15
  - write byte 6: prev_sensors - previous sensor state; `[7:3]` = 0, `[2]` sensor_2, `[1]` sensor_1, `[0]` sensor_0
  - write byte 7: current_sensors - current sensor state; same layout as prev_sensors

### Button board (0x44–0x47)

The input register is 1 byte representing the physical state of 8 buttons (bit N = button N; 1 = pressed, 0 = released). The trigger mode determines when a `button_changed` is emitted per button, but the register value always reflects the raw button state.

Each button has an independent mode and timing configured via `button_trigger`:

| Mode | Bits | Behaviour |
|---|---|---|
| `unknown` | `00` | No trigger configured; no events fire. Default on power-on. |
| `release` | `01` | Event fires when the button is released after being held for ≥ the configured time. No event if released early. |
| `hold`    | `10` | Event fires once when the button has been held for ≥ the configured time. |
| `change`  | `11` | Event fires on every button state change. No time parameter. |

- button_effect - set the visual effect for each of the 8 button outputs
  - write byte 0: command - 0x01
  - write byte 1: outputs_76 - upper nibble = output 7, lower nibble = output 6
  - write byte 2: outputs_54 - upper nibble = output 5, lower nibble = output 4
  - write byte 3: outputs_32 - upper nibble = output 3, lower nibble = output 2
  - write byte 4: outputs_10 - upper nibble = output 1, lower nibble = output 0

  Each nibble encodes one output as `CC MM` — `[3:2]` color, `[1:0]` mode.

  | CC | Color |
  |---|---|
  | 00 | white |
  | 01 | red |
  | 10 | green |
  | 11 | blue |

  | MM | Mode |
  |---|---|
  | 00 | disabled |
  | 01 | enabled |
  | 10 | flashing (fast) |
  | 11 | pulsating (slow) |

- button_state_read - polled read; returns the current physical state of all buttons
  - write byte 0: command - 0x83
  - read byte 0: current_state - current value of the input register

- button_trigger - write the trigger configuration for a single button
  - write byte 0: command - 0x04
  - write byte 1: `[7:3]` = 0, `[2:0]` button_id
  - write byte 2: `MM EE TTTT` — mode and timing for the button

  Default value on power-on is `0x00` (`unknown`, no events).

  **Time encoding:** `t = TTTT × 10^EE ms`. TTTT=0 means immediate (t=0ms).

  | EE | Resolution | Range |
  |---|---|---|
  | 0 | 1 ms | 1–15 ms |
  | 1 | 10 ms | 10–150 ms |
  | 2 | 100 ms | 100–1500 ms |
  | 3 | 1000 ms | 1–15 s |

- button_trigger_read - read the trigger configuration for a single button
  - write byte 0: command - 0x84
  - write byte 1: `[7:3]` = 0, `[2:0]` button_id
  - read byte 0: `MM EE TTTT` for the requested button

### Switching board (0x42)

The switching board has 16 relay channels. Each channel has three observable states:

- **relay target** — what the firmware was last commanded to set (`relay_state` write).
- **channel voltage** — the voltage observed downstream of the relay's fuse, read via the relay-output mux. A blown fuse can leave a relay commanded ON while its channel reads 0; a stuck contactor can do the inverse.

Both states are 2-byte bitmasks transmitted low byte first: byte 0 = bits 0–7, byte 1 = bits 8–15 (bit N = channel N; 1 = on, 0 = off).

- relay_state - write the target state of all 16 relays
  - write byte 0: command - 0x05
  - write byte 1: relays_lo - target state of relays 0–7
  - write byte 2: relays_hi - target state of relays 8–15

- relay_state_read - read the commanded target state of all 16 relays (the last value written via `relay_state`)
  - write byte 0: command - 0x85
  - read byte 0: relays_lo
  - read byte 1: relays_hi

- channel_state_read - read the channel-voltage state of all 16 channels (mux-observed; reflects fuse / contactor health, not just the commanded relay target)
  - write byte 0: command - 0x87
  - read byte 0: channels_lo
  - read byte 1: channels_hi

- battery_read - polled read; returns battery voltage as an unsigned 16-bit value
  - write byte 0: command - 0x88
  - read byte 0: voltage_lo - low byte
  - read byte 1: voltage_hi - high byte

- levels_read - polled read; returns both level meter values in a single message
  - write byte 0: command - 0x89
  - read byte 0: level_0 - unsigned 8-bit value of level meter 0
  - read byte 1: level_1 - unsigned 8-bit value of level meter 1

- level_mode - write the operating mode of both level meters
  - write byte 0: command - 0x0A
  - write byte 1: `[7:4]` = 0, `[3:2]` mode_1, `[1:0]` mode_0

  Mode values: `00` = unknown (default on power-on), `01` = 240–33 Ω, `10` = 0–190 Ω, `11` reserved.

- level_mode_read - read the operating mode of both level meters
  - write byte 0: command - 0x8A
  - read byte 0: `[7:4]` = 0, `[3:2]` mode_1, `[1:0]` mode_0

- sensors_read - polled read; returns the state of all 3 on/off sensors
  - write byte 0: command - 0x8B
  - read byte 0: `[7:3]` = 0, `[2]` sensor_2, `[1]` sensor_1, `[0]` sensor_0 — 1 = on, 0 = off

## Configuration

Configuration is per-device byte-addressable storage backed by data EEPROM. `config` writes are persisted across power cycles; `config_read` returns the in-RAM shadow that mirrors EEPROM. A byte that has never been written reads back as 0xFF (the EEPROM erase pattern); accessors with a sensible fallback substitute the per-field default listed below.

Universal addresses 0x00..0x0F are documented in [Common](#common). Device-specific fields:

### Main board configuration (0x40)

| Address | Field | Size | Default | Description |
|---|---|---|---|---|
| 0x10 | nav_enabled_mask | 1 | 0x1F | 5-bit mask of physically-installed nav lights — `[0]` anchoring, `[1]` tricolor, `[2]` steaming, `[3]` bow, `[4]` stern. Limits which lights the panel attempts to drive when a nav-mode button is pressed; cleared bits are silently skipped and a mode that requires a missing light raises a config error on the indicator. |
| 0x11 | indicator_brightness | 1 | 0x7F | Peak per-channel intensity (0..255) for the nav-indicator RGB ring. Re-read each frame so a runtime change takes effect within ~50 ms. |

### Button board configuration (0x44–0x47)

| Address | Field | Size | Default | Description |
|---|---|---|---|---|
| 0x10..0x16 | button_trigger[7] | 7 | hold trigger | One CommTriggerConfig (MMEETTTT) per button — same wire format as `button_trigger` command payload. Default mode is `hold` with a 1 ms time on most buttons; the L-board's button 0 defaults to 800 ms. |
| 0x17..0x1A | button_effect | 4 | disabled / white | Four packed CommButtonEffect bytes (two outputs per byte; upper nibble = odd output, lower nibble = even output) — same wire format as `button_effect` command payload. Default colour is white, mode disabled (LEDs dark). |
| 0x1B | led_brightness | 1 | 0x7F | Peak per-channel intensity (0..255) for the per-button RGB LEDs. Applied uniformly across R/G/B so hue is preserved; re-read each frame. |

### Switching board configuration (0x42)

| Address | Field | Size | Default | Description |
|---|---|---|---|---|
| 0x10 | water_cal | 1 | 100 | Water-level scale factor — byte the channel reports with a 100 Ω reference applied. The ADC processor inverts the factor when displaying Ω so a tolerance-shifted current source / divider just changes this byte. |
| 0x11 | fuel_cal | 1 | 100 | Fuel-level scale factor — same calibration semantics as `water_cal`. |
| 0x12 | battery_cal | 1 | 120 | Battery scale factor — byte the channel reports at a 12000 mV reference, in 100 mV units. |
| 0x13 | level_mode | 1 | 0x05 | Packed CommLevelMode — `[3:2]` mode_1, `[1:0]` mode_0; same layout as `level_mode` command payload. Persists meter mode across reboot. Default 0x05 = both meters in 240–33 Ω mode. |
