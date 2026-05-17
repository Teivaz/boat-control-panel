# Calibration

Procedures for bringing a freshly-flashed switching board into agreement
with its sensors. Each calibration writes a single byte into the
switching board's EEPROM via the inspector CLI, so the values survive a
reboot.

The three calibrations (water level, fuel level, battery voltage) are
independent — pick whichever needs adjustment, in any order. None of
them depend on the others.

## Prerequisites

- Switching board flashed with current firmware.
- Main board powered (so the I2C bus has its pull-ups energised), or
  external pull-ups in place. The inspector talks to the switching board
  directly; the main board doesn't have to be doing anything.
- CH347 USB-to-I2C dongle wired to the switching board's I2C lines:
  RC3 → SDA, RC4 → SCL, GND common.
- `tools/inspector/inspector` built (`make` from that directory; depends
  on `libusb-1.0`).
- The float harness for the channel being calibrated **disconnected**
  during the bench reference steps; reconnect after.

## Workflow at a glance

1. (Optional) Confirm fresh defaults survived flashing:
   ```
   inspector sw config read 0x10        # water cal — should be 100
   inspector sw config read 0x11        # fuel cal  — should be 100
   inspector sw config read 0x12        # battery cal — should be 120
   inspector sw config read 0x13        # level mode — should be 0x05
   ```
2. Calibrate the level meters (sections below). Order between water/fuel
   doesn't matter.
3. Calibrate the battery.
4. Reconnect the harnesses.

If any of step 1's reads return values that aren't the defaults above,
the EEPROM was written previously — calibrate from scratch and overwrite,
or `config read` each before starting if you want to preserve them.

## Level meters (water, fuel)

Each level channel has one calibration byte:

| Channel | Protocol address |
| --- | --- |
| Water | `0x10` (`CONFIG_ADDR_WATER_CAL`) |
| Fuel  | `0x11` (`CONFIG_ADDR_FUEL_CAL`) |

The byte is the displayed Ω that the channel produced when a 100 Ω
reference was applied. Default is `100` (no correction). The firmware
multiplies by `100 / cal_byte`, so a 100 Ω input always reads 100 Ω
regardless of harness resistance and current-source tolerance.

### Procedure

1. **Switch both meters to calibration mode (raw-Ω passthrough):**

   ```
   inspector sw meter write 0 0
   ```

   In this mode the `levels` byte for each channel is the calibrated Ω
   directly, clamped to `0..255`. Sloshing-compensation EMA is still
   applied, so allow ~30 s for each reading to settle.

2. **Open the inspector in repeat mode** so the reading refreshes
   live:

   ```
   inspector -r 1000 sw levels
   ```

3. **Zero check** — short the channel's input to GND. The reading must
   converge to 0. If it sits non-zero (anything > a count or two), there's
   a hardware fault upstream of the firmware (current-source ground
   reference, harness short, or ADC offset) — the new calibration model
   has no offset knob to compensate, so resolve the hardware issue
   before continuing.

4. **Span measurement** — apply a precision (1% or better) 100 Ω
   reference resistor between the channel input and GND. After ~30 s,
   note the displayed value `V`. If the hardware is within spec, expect
   `V` somewhere in the 95..105 range.

5. **Write the calibration byte** using the value you just read:

   ```
   inspector sw config write 0x10 <V>     # water  (use 0x11 for fuel)
   ```

   `<V>` is the literal number you noted in step 4 (decimal `97` or hex
   `0x61` — both work). The next conversion frame (~1 ms) picks up the
   new byte; the EMA then washes in over ~30 s.

6. **Verify** — keep the 100 Ω reference applied. After ~30 s the
   reading should sit at 100 Ω ±1. If it's off by more than that, the
   value you read in step 4 hadn't fully settled — re-read it and
   re-write step 5.

7. **Swap to the other channel** and repeat steps 3–6.

8. **Switch both meters back to the operating mode:**

   ```
   inspector sw meter write 1 1     # European 0..190 Ω (default)
   # or
   inspector sw meter write 2 2     # American 240..33 Ω
   ```

   The mode is persisted to EEPROM (`CONFIG_ADDR_LEVEL_MODE = 0x13`),
   so subsequent reboots start in the chosen scale.

9. **Reconnect the float harness.**

### Mode reference

| Value | Name | Meaning |
| --- | --- | --- |
| `0` | `COMM_METER_MODE_CALIBRATION` | Raw-Ω passthrough, byte clamped to 0..255 |
| `1` | `COMM_METER_MODE_0_190` | European; full = 190 Ω, empty = 0 Ω. **Default** |
| `2` | `COMM_METER_MODE_240_33` | American; full = 33 Ω, empty = 240 Ω |

`meter write <m0> <m1>` — `m0` is water, `m1` is fuel; both expressed
in the values above.

### Verifying

```
inspector sw config read 0x10        # water cal byte
inspector sw config read 0x11        # fuel cal byte
inspector sw config read 0x13        # packed level mode (low nibble = mode_1<<2 | mode_0)
inspector sw meter read              # decoded mode for both channels
```

## Battery

Same shape as the levels — one byte storing what the channel reported
when a known reference was applied. The reference is **12 000 mV**.
Because the reading is 16 bits but the cal byte is 8, the byte is in
**100 mV units**: write `round(displayed_mV / 100)`, default `120`.
The firmware then computes `corrected_mV = raw_mV × 120 / cal_byte`.

A first-order EMA with τ ≈ 400 ms is applied — fast enough to track
real load sag from a starter or inverter while smoothing alternator and
switching-transient noise.

| Field | Address | Default | Meaning |
| --- | --- | --- | --- |
| Battery cal | `0x12` (`CONFIG_ADDR_BATTERY_CAL`) | 120 | `round(displayed_mV / 100)` at 12 000 mV reference |

### Procedure

1. **Apply a 12.000 V reference** to the battery input. Either:
   - A bench supply trimmed against a calibrated DMM, or
   - A known-charged battery measured with the DMM, used in place of
     the boat battery.

   The accuracy of your reference is the floor on calibration accuracy.
   100 mV at 12 V is ~0.83%, so any DMM ≤ 0.5% is fine.

2. **Watch the live reading** — give it ~2 s after each connection for
   the EMA to settle:

   ```
   inspector -r 1000 sw voltage
   ```

3. **Compute the byte** as `round(displayed_mV / 100)`. Examples:

   | Displayed mV | Byte (decimal / hex) |
   | --- | --- |
   | 11 800 | 118 / `0x76` |
   | 11 850 | 119 / `0x77` (rounded up from 118.5) |
   | 12 000 | 120 / `0x78` (default) |
   | 12 200 | 122 / `0x7A` |

4. **Write the byte:**

   ```
   inspector sw config write 0x12 <byte>
   ```

5. **Verify** — keep the 12 V reference applied. After ~2 s the reading
   should sit at 12 000 mV ±50 mV (half of the 100 mV cal step).

Granularity is one cal step ≈ 100 mV (~0.83% on 12 V). For a state-of-
charge gauge that's fine; the divider's actual tolerance is typically
the dominant error.

### Verifying

```
inspector sw config read 0x12        # battery cal byte
inspector -r 1000 sw voltage         # live mV reading
```

## Persistence

Calibration values live in NVM EEPROM on the switching board, so they
survive a reboot or power cycle. Re-running the calibration procedure
overwrites them; flashing new firmware doesn't (unless the firmware
itself rewrites the EEPROM layout, which the firmware notes in its
release notes).

To revert to the in-firmware defaults, set each cal byte by hand:

```
inspector sw config write 0x10 100       # water cal
inspector sw config write 0x11 100       # fuel cal
inspector sw config write 0x12 120       # battery cal
inspector sw meter write 1 1             # level mode → European default
```

## Troubleshooting

**`config write` returns silently but `config read` shows the old
value.** The address you're writing isn't in `eeprom_offset_for`'s
switch — check the address against `config.h`. The protocol-level write
is delivered to the slave but `config_write_byte` drops unknown
addresses.

**Level reading still wrong after writing the cal byte.** Most common
cause: step 4's reading wasn't yet settled when you wrote it. The EMA
τ is ~6 s, so anything less than ~30 s of stability isn't reliable.
Re-read with the reference still applied, then re-write step 5.

**Battery reads ~0 mV.** With the new model and `cal=120` default,
that means the ADC isn't seeing the divider output — check the harness
to RA6 and the divider itself.

**Same value across two different meter modes.** The mode write didn't
land — the slave is probably still on a firmware older than the one
this doc assumes. Reflash and `inspector sw meter read` to confirm.

**Floating-input garbage.** With nothing connected, the channel may
read non-zero or rail-saturated. That's expected — the calibration
procedure assumes a known reference is wired in for each step.
