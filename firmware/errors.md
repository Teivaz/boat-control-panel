# Compilation Warnings Audit

Snapshot of XC8 v3.10 and clang diagnostics across `board1-switching`,
`board2-buttons`, `board3-main` as of 2026-04-24. All three boards build
clean (no errors). Warnings are grouped by mechanism; each entry lists the
root cause and whether action is warranted.

## 1. Non-reentrant duplication for `run_in_main_loop` (resolved)

```
task.c:120: advisory (2098) indirect function call via a NULL pointer ignored
task.c:120: warning  (759)  expression generates no code
task.c:100..102: advisory (1498) pointer (run_in_main_loop@c) may have no targets
```

These warnings applied when no board called `run_in_main_loop`. Since then,
`board2-buttons/input.c:62` calls `run_in_main_loop(ctrl, dispatch_pending, 0)`
from its IOC ISR to defer button debounce into main context. With a live
caller the pointer analysis resolves and the dispatcher is emitted. These
warnings should no longer appear for board2-buttons builds.

## 2. Dead code in board modules (520 — never called)

Functions declared and defined in board-local modules with no caller in the
linked image. Linker drops them from the final hex; cost is source clutter.

### board3-main/controller.c
- `controller_power_on` (211)
- `controller_nav_mode` (214)
- `controller_relay_target` (220)
- `controller_sensors` (232)

Four getters that were likely intended for `comm.c` to serve read requests
but aren't wired. `board3-main/comm.c` has no callers for them.

### board2-buttons
- `config.c:131` `config_set_button` — wrapper over `config_write_byte`.
  The dispatcher in `comm.c:100` calls `button_set_trigger` directly
  instead, which internally persists.
- `config.c:146` `config_set_effect` — same pattern; dispatcher calls
  `led_effect_set` directly (`comm.c:156`).
- `led_effect.c:35` `led_effect_get` — no reader of current effect state.

**Action**: either delete the dead wrappers or route the dispatcher through
them (would consolidate persistence into `config.c`). Low priority.

## 3. Unused libcomm API per board (520)

`libcomm/libcomm.c` exports builders and parsers for every command in the
protocol. Each board links all of them but only calls a subset:

| Board | Unused command families (sample) |
|---|---|
| board1-switching | all button\_\* builders/parsers, button\_effect\_\*, config builders, relay\_mask\_\*, relay\_changed, reset, sensors\_read |
| board2-buttons   | relay\_\*, battery, levels, level\_mode, sensors, config builders |
| board3-main      | build\_button\_changed, button\_trigger\_\*, most read-side builders |

**Impact**: none — XC8 linker discards unreferenced symbols.

**Action**: none. This is inherent to sharing one library across
heterogeneous devices. Could be eliminated with per-board `-D` guards
around optional builders, but the added conditional compilation costs more
than the dead code.

## 4. Non-reentrant duplication (1510)

```
... non-reentrant function "_X" appears in multiple call graphs
    and has been duplicated by the compiler
```

Affected across all boards: any module whose API is reachable from both
main and ISR contexts — `config_read_byte`, `config_write_byte`,
`queue_lookup`, `eeprom_offset_for`, `nvm_read`, `i2c_*`, `sensors_state`,
`button_set_trigger`, `button_index`, `led_effect_set`, `input_state_current`,
`task_controller_add`, `task_controller_remove`, `find_slot`, `comm_address`,
and the XC8 runtime helpers (`__awdiv`, `__lwdiv`, `__lwmod`).

**Cause**: XC8's 8-bit model disallows reentrancy by default — locals sit
in fixed allocation banks rather than on a stack. When the same function
is reached from two call graphs (ISR + main), XC8 duplicates the code so
each context has its own locals. This is correct and is the mechanism that
makes our ISR-callable annotations safe.

**Impact**: modest code-size hit (one extra copy per duplicated function).
Already accounted for in the 7.8% / 10.0% program-space usage reported by
the build.

**Action**: none. Confirms the ISR-callable annotations accurately reflect
reality. If code space ever gets tight, candidates for ISR→defer rework
would reduce duplication.

## 5. Function-pointer targets unresolved (1498 advisory)

```
advisory: (1498) pointer (<fn>@<param>) in expression may have no targets
```

Fires for every libcomm builder/parser taking `CommMessage*` or typed
output pointers. XC8 cannot resolve the targets because the callers pass
`&local` from a different translation unit.

**Impact**: none — pure static-analysis limitation. The calls are
resolvable by ordinary C semantics at link time.

**Action**: none.

## 6. u8g2 vendor warnings (board3-main only)

Two groups, both from the vendored `u8g2/` subtree:

- **clang sign-conversion / int-conversion** (~100 hits across
  `u8g2_arc.c`, `u8g2_font.c`, `u8g2_polygon.c`, `u8g2_selection_list.c`,
  `u8g2_ll_hvline.c`, `u8g2_kerning.c`, `u8g2_message.c`,
  `u8g2_input_value.c`, `u8x8_cad.c`). Upstream style; not our code.
- **XC8 (1481) "call from non-reentrant function … to `_byte_cb` might
  corrupt parameters"** on the u8x8 display-driver dispatch pointers.
  `_byte_cb` is u8g2's callback registered from a single context, but XC8
  sees it reachable from multiple drivers and cannot prove reentrancy
  safety.

**Impact**: vendor code runs only from main context in our use, so the
(1481) parameter-corruption warning is spurious for our call pattern. The
clang sign warnings are cosmetic.

**Action**: none. Touching `u8g2/` would diverge from upstream.

## 7. Summary

- 1 resolved concern (§1) — `run_in_main_loop` now has a live caller in
  board2-buttons; warnings should no longer appear for that board.
- 7 locally dead functions (§2) — candidates for cleanup in a follow-up.
- Everything else (§3–§6) is structural: consequence of a shared library
  over heterogeneous devices, the XC8 non-reentrancy model, or vendor code.

Build output captured with `../build.sh` from each board directory after
`make clean CONF=default`.
