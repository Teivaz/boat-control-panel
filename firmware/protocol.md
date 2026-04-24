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
| `relay_changed` | 0x06 | switching board → main |
| `relay_mask` | 0x07 | main → switching board (write) |
| `relay_mask_read` | 0x87 | main → switching board (read) |
| `battery_read` | 0x88 | main → switching board (read) |
| `levels_read` | 0x89 | main → switching board (read) |
| `level_mode` | 0x0A | main → switching board (write) |
| `level_mode_read` | 0x8A | main → switching board (read) |
| `sensors_read` | 0x8B | main → switching board (read) |

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

### Main board (0x40)

- button_changed - pushed by a button board when a button's trigger fires, or when its trigger is (re)configured
  - write byte 0: command - 0x02
  - write byte 1: device_address - address of the sending button board
  - write byte 2: button_id - `[7:3]` = 0, `[2:0]` button index
  - write byte 3: event - one of the values below

  | Value | Event | When emitted |
  |---|---|---|
  | 0x00 | `none` | Reserved; never sent on the wire. |
  | 0x01 | `enabled` | Sent once after `button_trigger` configures a button to a non-`unknown` mode. |
  | 0x02 | `disabled` | Sent once after `button_trigger` clears a button back to `unknown`. |
  | 0x03 | `triggered` | The button's configured trigger condition fired. |

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

The switching board has 16 relay channels. Each relay has a target state (desired) and a physical state (actual). Physical state normally tracks target state with a small delay but may diverge due to external factors. Relay state is represented as a 2-byte bitmask transmitted low byte first: byte 0 = relays 0–7, byte 1 = relays 8–15 (bit N = relay N; 1 = on, 0 = off).

- relay_state - write the target state of all 16 relays
  - write byte 0: command - 0x05
  - write byte 1: relays_lo - target state of relays 0–7
  - write byte 2: relays_hi - target state of relays 8–15

- relay_state_read - read the target state of all 16 relays
  - write byte 0: command - 0x85
  - read byte 0: relays_lo
  - read byte 1: relays_hi

- relay_mask - write the event mask; only relays with their mask bit set trigger `relay_changed` messages
  - write byte 0: command - 0x07
  - write byte 1: mask_lo - event mask for relays 0–7
  - write byte 2: mask_hi - event mask for relays 8–15

  Default on power-on is 0x0000 (all events suppressed).

- relay_mask_read - read the event mask
  - write byte 0: command - 0x87
  - read byte 0: mask_lo
  - read byte 1: mask_hi

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
