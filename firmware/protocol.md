# I2C Communication Protocol Description

All devices should be able to operate in standard (100 kHz) and fast (400 kHz) modes.
The system operates in multi-master mode with an event-based system. If a device needs to implement polling the time interval for polling calls should be ≥ 100 ms.
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

All write messages begin with a command byte (byte 0). Read messages use a write phase to send the command byte, followed by a repeated-start read phase.

| Command | Value | Direction |
| --- | --- | --- |
| `button_effect` | 0x01 | main → button board |
| `button_changed` | 0x02 | button board → main |
| `button_state` | 0x03 | main → button board (read) |
| `button_trigger` | 0x04 | main → button board (read/write) |
| `relay_state` | 0x05 | main → switching board (read/write) |
| `relay_changed` | 0x06 | switching board → main |
| `relay_mask` | 0x07 | main → switching board (read/write) |
| `battery` | 0x08 | main → switching board (read) |
| `levels` | 0x09 | main → switching board (read) |
| `level_mode` | 0x0A | main → switching board (read/write) |
| `sensors` | 0x0B | main → switching board (read) |

### Main board (0x40)

- button_changed - pushed by a button board when any button state changes
  - write byte 0: command - 0x02
  - write byte 1: device_address - address of the sending button board
  - write byte 2: prev_state - button state bitmask before the event; bit N = button N
  - write byte 3: current_state - button state bitmask after the event; bit N = button N

- relay_changed - pushed by the switching board when any masked relay or any sensor changes state
  - write byte 0: command - 0x06
  - write byte 1: device_address - address of the sending switching board
  - write byte 2: prev_relays_lo - previous physical state of relays 0–7; bit N = relay N
  - write byte 3: prev_relays_hi - previous physical state of relays 8–15; bit N = relay N−8
  - write byte 4: current_relays_lo - current physical state of relays 0–7
  - write byte 5: current_relays_hi - current physical state of relays 8–15
  - write byte 6: prev_sensors - previous sensor state; `[7:3]` = 0, `[2]` sensor_2, `[1]` sensor_1, `[0]` sensor_0
  - write byte 7: current_sensors - current sensor state; same layout as prev_sensors

### Button board (0x44–0x47)

The input register is 1 byte representing the physical state of 8 buttons (bit N = button N; 1 = pressed, 0 = released). The trigger mode determines when a `button_changed` is emitted, but the register value always reflects the raw button state.

Each button has an independent mode and timing configured via `button_trigger`:

| Mode | Bits | Behaviour |
|---|---|---|
| `release` | `00` | Event fires when the button is released after being held for ≥ the configured time. No event if released early. Default mode. |
| `hold`    | `01` | Event fires once when the button has been held for ≥ the configured time. |
| `change`  | `10` | Event fires on every button state change. No time parameter. |
| reserved  | `11` | — |

- button_effect - set the visual effect for each button output
  - write byte 0: command - 0x01
  - write byte 1: outputs_76 - upper nibble = output 7, lower nibble = output 6
  - write byte 2: outputs_54 - upper nibble = output 5, lower nibble = output 4
  - write byte 3: outputs_32 - upper nibble = output 3, lower nibble = output 2
  - write byte 4: outputs_10 - upper nibble = output 1, lower nibble = output 0

  Each nibble: `0x0` = off, `0x1` = on; values `0x2`–`0xF` reserved.

- button_state - polled read; returns the current physical state of all buttons
  - write byte 0: command - 0x03
  - read byte 0: current_state - current value of the input register

- button_trigger - read or write the trigger configuration for a single button
  - write byte 0: command - 0x04
  - write byte 1: `[7:3]` = 0, `[2:0]` button_id
  - write byte 2 (write only): `MM EE TTTT` — mode and timing for the button

  Read transaction: write phase sends bytes 0–1 only, followed by a repeated-start read that returns one byte: `MM EE TTTT` for the requested button. Default value on power-on is `0x00` (`release`, immediate).

  **Time encoding:** `t = TTTT × 10^EE ms`. TTTT=0 means immediate (t=0ms).

  | EE | Resolution | Range |
  |---|---|---|
  | 0 | 1 ms | 1–15 ms |
  | 1 | 10 ms | 10–150 ms |
  | 2 | 100 ms | 100–1500 ms |
  | 3 | 1000 ms | 1–15 s |

### Switching board (0x42)

The switching board has 16 relay channels. Each relay has a target state (desired) and a physical state (actual). Physical state normally tracks target state with a small delay but may diverge due to external factors. Relay state is represented as a 2-byte bitmask transmitted low byte first: byte 0 = relays 0–7, byte 1 = relays 8–15 (bit N = relay N; 1 = on, 0 = off).

- relay_state - read or write the target state of all 16 relays
  - write byte 0: command - 0x05
  - write byte 1 (write only): relays_lo - target state of relays 0–7
  - write byte 2 (write only): relays_hi - target state of relays 8–15

  Read transaction: write phase sends byte 0 only, followed by a repeated-start read that returns 2 bytes (relays_lo, relays_hi).

- relay_mask - read or write the event mask; only relays with their mask bit set trigger `relay_changed` messages
  - write byte 0: command - 0x07
  - write byte 1 (write only): mask_lo - event mask for relays 0–7
  - write byte 2 (write only): mask_hi - event mask for relays 8–15

  Read transaction: write phase sends byte 0 only, followed by a repeated-start read that returns 2 bytes (mask_lo, mask_hi). Default on power-on is 0x0000 (all events suppressed).

- battery - polled read; returns battery voltage as an unsigned 16-bit value
  - write byte 0: command - 0x08
  - read byte 0: voltage_lo - low byte
  - read byte 1: voltage_hi - high byte

- levels - polled read; returns both level meter values in a single message
  - write byte 0: command - 0x09
  - read byte 0: level_0 - unsigned 8-bit value of level meter 0
  - read byte 1: level_1 - unsigned 8-bit value of level meter 1

- level_mode - read or write the operating mode of both level meters
  - write byte 0: command - 0x0A
  - write byte 1 (write only): `[7:2]` = 0, `[1]` mode_1, `[0]` mode_0 — 0 = mode A, 1 = mode B

  Read transaction: write phase sends byte 0 only, followed by a repeated-start read that returns 1 byte.

- sensors - polled read; returns the state of all 3 on/off sensors
  - write byte 0: command - 0x0B
  - read byte 0: `[7:3]` = 0, `[2]` sensor_2, `[1]` sensor_1, `[0]` sensor_0 — 1 = on, 0 = off
