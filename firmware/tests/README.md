# Host-side unit tests

282 tests across six binaries, covering libcomm, both I2C drivers, and every
module on all three boards. They run on a desktop compiler in under a second —
no PIC, no simulator, no Docker.

```sh
cd firmware/tests
make            # build and run everything
make board2     # one binary (libcomm | driver | alt | board1 | board2 | board3)
make list       # every registered test name
make asan=1     # AddressSanitizer + UndefinedBehaviorSanitizer
make V=1        # show compiler command lines

./build/test_board2 /board2/button/hold_fires_after_the_delay   # a single test
./build/test_board3 --no-fork                                   # debuggable
```

## Why this works at all

The firmware is PIC18 C, but very little of it is PIC18-*specific*. It is
protocol framing, state machines, debouncing and bit mapping wrapped around a
few dozen special function registers. Mock the registers and the rest is
ordinary C.

`mocks/xc.h` replaces the XC8 device header. It supplies the language
extensions (`__interrupt`, `asm`, `RESET()`, `__delay_us`, `uint24_t`) and
pulls in `mocks/pic_sfr_mock.h`, where every register the firmware touches
resolves to a macro over one backing array. `LATA` and `LATAbits` share a byte,
exactly as on silicon.

Interrupt handlers become ordinary functions, so a test raises an interrupt by
setting the flag the handler checks and calling it.

## Layout

```
tests/
  Makefile
  mocks/
    xc.h              stand-in for the XC8 device header
    pic_mock.{h,c}    backing store, DMA banking, NVM/flash models, watchers
    pic_sfr_mock.h    GENERATED — the registers the firmware uses
    i2c_fake.{h,c}    fake I2C driver used by the board binaries
  support/
    test_support.{h,c}  scheduler driving, frame assertions
  tools/
    gen_pic_mock.py   regenerates pic_sfr_mock.h from the XC8 DFP header
  suites/
    libcomm/  crc, task scheduler, protocol library, dispatcher
    driver/   both I2C drivers (compiled twice against the same suite)
    board1/   relay out/monitor, config, sensors, ADC, controller
    board2/   buttons, LED chain, config, comm
    board3/   nav lights, button feedback, controller, display
```

One binary per board because each board is its own program and they all define
`config_init`, `comm_init`, `rgbled_set` and friends.

## What the mock actually models

Plain memory is not enough for three things, so `pic_reg()` — the single
accessor every register goes through — handles them:

**DMA banking.** `DMASELECT` really does switch which channel's registers you
see, which the I2C driver depends on.

**Peripherals the firmware spins on.** `while (NVMCON0bits.GO);` terminates
because the read that evaluates the condition is what completes the NVM
operation and clears the bit. The EEPROM behind it is a real 1 KB array, so
config tests assert on cell contents and on the *number of programs* performed
(`pic_eeprom_writes`) — which is how the write-coalescing tests prove anything.

**Table reads.** `asm("TBLRD*+")` is routed to `pic_asm()`, which interprets the
handful of mnemonics the firmware emits. That keeps the ADC's factory FVR
calibration real instead of zero, so the whole conversion chain is testable.

Beyond that, `pic_watch(addr, fn, ctx)` fires on every access to a register.
Two things become testable through it that otherwise could not be:

- **Bit-banged outputs.** `suites/board1/hw_sim.c` watches `LATA` and
  reconstructs the 16-bit word `relay_out.c` clocks into its 74HC595 pair, by
  looking for rising clock edges. The watcher runs *before* the pending write
  lands, so what it sees on one call is the result of the previous one — which
  is exactly the edge the real shift register samples.
- **Inputs that depend on outputs.** The same file watches `PORTA` and computes
  what the two SN74LV4051A multiplexers would present for the currently
  selected address, just in time for `relay_mon.c` to read it.

## The fake I2C driver

Board binaries link `mocks/i2c_fake.c` instead of `libcomm/i2c.c`. Testing a
board through a bus-level simulation of DMA, arbitration and clock stretching
would obscure every board assertion. The fake presents the same `i2c.h` and
makes both directions directly drivable:

```c
comm_send_relay_state(0x0102);
const I2cFakeTx* tx = i2c_fake_last_tx();
assert_frame(tx->tx, tx->tx_len, COMM_RELAY_STATE, 0x02, 0x01);

uint8_t frame[8];
uint8_t n = i2c_fake_frame(frame, COMM_CONFIG, (uint8_t[]){0x10, 0x64}, 2);
i2c_fake_deliver_write(frame, n);
i2c_poll();     /* runs the real dispatcher and the board's real callback */
```

`libcomm_interface.c` is the real one throughout, so an inbound frame travels
its actual path: CRC check, dispatch, board callback, hardware.

The drivers themselves are covered separately in `suites/driver/`, against the
real peripheral registers.

## Both I2C drivers, one suite

`suites/driver/test_i2c.c` is compiled twice — once against `libcomm/i2c.c` and
once against `libcomm/alt/i2c.c`. They implement the same header, so anything
that passes for one and fails for the other is a real behavioural divergence.
Both currently pass all 25.

One asymmetry, guarded by `BUS_CAN_MOVE_BYTES`: payload movement cannot be
driven for the DMA-based driver. It programs `(uint16_t)task->rx` into the
channel — a 64-bit host pointer truncated to 16 bits, which no mock can turn
back into an address. The byte-driven driver feeds `I2CxTXB` and drains
`I2CxRXB` from two interrupt vectors, which are just functions, so its data
path is reachable. Control-flow assertions apply to both.

## Regenerating the register mock

`mocks/pic_sfr_mock.h` is committed, so the tests build without MPLAB
installed. Regenerate it only when the firmware starts using a register it did
not use before — the build will fail with an "undeclared identifier" naming it:

```sh
make mocks                              # finds the DFP header automatically
python3 tools/gen_pic_mock.py --header /path/to/pic18f27q84.h
```

The generator keeps only the registers the firmware mentions (156 of 1177), and
retypes the bitfield unions from `unsigned` to `uint8_t` — XC8's `unsigned` is
16 bits and allocates bitfields LSB-first within a byte, where a host
compiler's 32-bit `unsigned` would make each union four bytes wide and let a
single-bit write clobber the three following registers.

## Conventions

- One assertion per behaviour, and a comment saying *why the behaviour
  matters* rather than restating the code.
- Prefer driving a module through its real entry points over reaching into it.
  Button tests press pins; controller tests deliver frames.
- `test_advance_ms(&ctrl, n)` runs the scheduler forward `n` simulated
  milliseconds, ticking and polling at the cadence the device does.
- `assert_frame(buf, len, id, ...)` checks a protocol frame's id, payload and
  CRC in one line.

## Known-failing test

`/board3/controller/power_off_drops_every_channel` is marked
`MUNIT_TEST_OPTION_TODO`: it asserts the behaviour the code intends and does
not yet have. `clear_channels()` in `board3-main/controller.c` resets the
per-button feedback slots but never clears `g_relay_target`, so turning the
panel off leaves every circuit energised. munit reports a TODO that *starts*
passing as an error, so fixing it will surface here rather than be missed.
