# Firmware style & consistency notes

Derived from a file-by-file audit of `board1-switching/` and `board2-buttons/`. These are the conventions the board already follows (and which `.clang-format` does not by itself capture), followed by the inconsistencies found during the audit and the fixes applied.

## Conventions in use

### Module layout
- One `foo.h` / `foo.c` pair per responsibility (`adc`, `i2c`, `relay_out`, `relay_mon`, `sensors`, `controller`, `config`, `comm`, `interrupt`).
- `foo_init(...)` is always the first exported symbol. Modules that schedule periodic work take `TaskController* ctrl` so the caller owns the scheduler; modules with no timed work take no argument (`adc_init`, `i2c_init`, `relay_out_init`, `relay_mon_init`, `comm_init`).
- Header comments describe *what the unit owns and how to call it*, not line-by-line behavior.

### Naming
- Exported functions: `module_verb[_noun]` (`relay_out_write`, `adc_read_battery_mv`, `controller_set_relay_target`, `sensors_state`).
- File-static helpers: verb-first, no module prefix (`apply_target`, `start_burst`, `handle_event`, `flush_task`, `ioc_handler`).
- Scheduled callbacks end in `_task` (`monitor_task`, `retry_task`, `flush_task`, `poll_task`).
- Task IDs live in each board's `task_ids.h` as an anonymous `enum`, `TASK_*` prefix.
- Pin/register aliases use `PIN_*` or `SEL_*` / `COM_*` macros (`PIN_NRST`, `SEL_A`, `PIN_BILGE`).
- Compile-time constants: `SCREAMING_SNAKE_CASE` with a module-specific prefix (`ADC_FVR_MV`, `SENSORS_POLL_MS`, `WRITE_QUEUE_SIZE`).

### Types & argument style
- `uint8_t` is the default integer. `uint16_t` for relay masks and raw ADC counts, `uint32_t` only where required (EMA accumulators).
- Pointer declarations use `Type* name` (matches `.clang-format`'s `PointerAlignment: Left`).
- Every no-arg function is declared as `func(void)` — including ISRs.
- Buffer parameters always come as `(const uint8_t* data, uint8_t len)` pairs; response buffers as `(uint8_t* response, uint8_t response_max)` with the return value being the number of bytes written.
- Status enums (`I2cResult`) are returned; scheduler-style functions return `int8_t` with documented negative codes.
- Bitfield struct fields keep the LSB-first layout that matches the wire format — never reorder.

### Concurrency discipline
- **Main context**: `main()`, `task_controller_poll`, every `*_task` callback.
- **ISR context**: each peripheral ISR (`I2C1_ISR`, `AD_ISR`, `IOC` / `TMR0` via `interrupt_set_handler_*`), plus the `on_rx` / `on_read` handlers that `i2c.c` invokes from its ISR.
- Any variable shared between the two is `volatile`. Multi-byte volatile reads/writes from main context wrap access in `INTERRUPT_PUSH` / `INTERRUPT_POP` (from `libcomm.h`). Pure-ISR shared variables don't need the guard because ISRs don't preempt each other here.
- ISR context → main context handoff uses a `push_dirty` flag plus a periodic task that drains the state; handlers invoked from an ISR (`on_rx`, `on_read`, change callbacks) must stay short and avoid blocking peripherals.

### Interrupts
- All vectors use `__interrupt(irq(NAME), base(8))` with the IVT locked at `0x0008` by `interrupt_init`.
- `interrupt.c` owns `IOC` and `TMR0` dispatch through `interrupt_set_handler_IOC` / `_TMR0`; all other IRQs are either defined directly by the owning module (`I2C1_ISR` in `i2c.c`, `AD_ISR` in `adc.c`) or stubbed to `__asm("RESET")` to trap unexpected interrupts.
- The function pointers backing the dispatched handlers are `static` in `interrupt.c` — they're implementation detail, not API.

### Comments
- Top-of-file comment: what the module owns, how the hardware is wired, and any non-obvious invariants (e.g. the wire-bit ↔ protocol-bit mapping tables, EMA time constant rationale).
- Inline: only where the *why* is not obvious (register magic numbers, timing assumptions, mid-flight glitch handling).
- No running narration of what the next line does.

## Findings and fixes

### Fixed
1. **`controller_level` parameter name drift**
   Header declared `controller_level(uint8_t meter_index)` but the definition used `uint8_t i`. Renamed to `meter_index` to match.

2. **Dead accessor `controller_relay_physical`**
   Declared in `controller.h` and defined in `controller.c`, never called. `relay_physical` is still used internally by `monitor_task` / `retry_task`; only the unused getter and its declaration were removed.

3. **Stray-vector ISR signatures**
   `interrupt.c` declared every reset-trap ISR with an empty `()` argument list (e.g. `SWINT_ISR()`), while the two real handlers in the same file (`IOC_ISR(void)`, `TMR0_ISR(void)`) used `(void)`. All reset-trap ISRs now use `(void)` for consistency.

4. **Module-private state marked `extern`**
   `interrupt_handler_IOC` and `interrupt_handler_TMR0` were file-scope non-`static`, exposing them as global symbols even though only `interrupt.c` uses them. Marked `static`.

### Accepted as-is
- **`comm.c` `COMM_LEVEL_MODE` handling** skips `comm_parse_level_mode_write` and passes `data[1]` directly to `controller_set_level_mode`, which masks with `0x0F`. The parser itself just does the same `& 0x0F`, so the net effect is identical and routing a 1-byte bitfield through a parser-and-cast would be noisier than the current line. If more fields are added to `CommLevelMode` later, switch to the parser.
- **`adc_read_level_fresh_water` / `adc_read_level_fuel` duplication**. Two near-identical one-liners; per the "reuse after the third copy" rule this is still acceptable. Revisit if a third level meter appears.
- **clangd false positives** (`task.h`/`xc.h` not found, `GIE` undeclared). The Makefile supplies `-I../libcomm` and the XC8 DFP path at build time; standalone clangd doesn't see them. Not a source issue.

## For new code in this repo

- Mirror the `_init(TaskController*)` + `_task` pattern for anything that needs periodic work — don't add a second scheduling mechanism.
- Keep I2C-command dispatch in `comm.c`; keep transport (`i2c.c`) free of libcomm types.
- When a handler has to move data from ISR to main context, use a `volatile` shadow + dirty flag drained by a task — not a callback that blocks in the ISR.
- Wire a fresh IRQ either via `interrupt_set_handler_*` (if it's already dispatched there) or by adding the `__interrupt(irq(...), base(8))` definition in the module that owns the peripheral, and delete the matching reset-trap stub in `interrupt.c` if one exists.
