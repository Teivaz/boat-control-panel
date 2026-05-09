# I2C Slave-TX (client-TX) DMA — open issue

Target: PIC18F27Q43, XC8 v3.10. File: `libcomm/i2c.c`.

## Symptom

A combined write+read transaction from a remote master (e.g. command `0x88` =
read with MSB set) reaches `isr_on_address` for the read phase correctly:
the slave ACKs the write address, receives the command byte via DMA into
`g_client_rx`, ACKs the restart, and ACKs the read address. After CSTR is
released, the slave fails to deliver the data bytes from `g_client_tx[]`
to the master.

What we observe on the wire varies depending on the experiment, but in the
working scenarios at most byte 0 ships correctly. Byte 1 onwards either:

- arrives as `0x00` (zeros — see "CNTIF gotcha" below), or
- causes the slave to clock-stretch via `CSD = 0` until BTO recovery fires,
  and then the master reads `0xFF` as the bus floats high.

## What works (and is using the same DMA channels)

- **Cold RX** (`i2c_dma_client_rx`): slave receives bytes from master, DMA
  drains `I2C1RXB` to `g_client_rx[]`. Reliable.
- **Host TX** (`i2c_dma_set_host`): master mode transmit driven by DMA from
  `task->tx`. Reliable.

Both use the same `DMASELECT` / `DMAnCON0` setup pattern (`EN = 0` →
`SSA/SSZ` (or `DSA/DSZ`) → `SIRQEN = 1; AIRQEN = 1; EN = 1`). So the DMA
configuration template itself is sound.

## What we changed and why (chronological)

1. **`DMAnAIRQ = 0` override restored** in `i2c_dma_init` for both TX and
   RX channels.
   The repo had drifted to leave `DMAnAIRQ = 0x3b` (I2C1E) live, which —
   combined with `AIRQEN = 1` on every arm — would abort the channel on
   any latched `NACKIF` / `BTOIF` / `BCLIF`. Unwiring the abort source
   eliminates that as a suspect.

2. **Stop programming `I2C1CNT` in `i2c_dma_client_tx`**.
   Slave-TX length is master-controlled (master decides when to NACK).
   Programming `I2C1CNTL = g_client_tx_len` made `CNTIF` fire mid-transfer
   in slave mode, which dispatches `isr_on_transmit_exhausted`.

3. **`isr_on_transmit_exhausted` `FSM_CLIENT_TX` arm now `return`s early**
   instead of dropping `CSTR`.
   Original code zeroed `g_client_tx_len` and unconditionally fell through
   to `I2C1CON0bits.CSTR = 0;`. On Q43 in slave-TX, that has the effect
   of "continue, you were stretching" — and since TXB is empty, whatever
   the SR latches happen to be (typically `0x00`) gets shifted out for as
   long as the master keeps clocking. **This is the "starts sending zeros
   when CNT exhausts" gotcha — leave the early return in place.**

4. **Made `i2c_dma_client_tx` a byte-for-byte mirror of the host-TX arm
   in `i2c_dma_set_host`** (no direct TXB write, no error-flag prelude,
   full buffer + full length, same EN cycle). Result: two `0xFF` bytes
   on the wire — the DMA never fired even once.

5. **Added `DMAnCON0bits.DGO = 1`** after `EN = 1` in
   `i2c_dma_client_tx` (current state). Theory: in slave-TX the
   peripheral is already in TX mode by the time `ADRIF` fires, so
   `I2C1TXIF` has been level-asserted with no fresh rising edge for the
   edge-triggered `SIRQEN` to catch — manual `DGO` should kick the first
   transfer. Result: **no change, still `0xFF` on the wire.** This is the
   current state and the open question.

## What this latest result tells us

- `DGO = 1` after a real `EN = 0 → 1` cycle (so `SPTR`/`SCNT` reload from
  `SSA`/`SSZ`) did not produce a transfer. That means *either*:
  - the manual-fire-via-DGO mechanism does not actually do what we think
    on this part / in this DMA mode, or
  - the DMA *did* transfer but the byte never reached `I2C1TXB` for the
    peripheral to clock out (DMA destination write somehow ignored in
    this peripheral state), or
  - the byte reached `I2C1TXB` but the peripheral didn't propagate it
    into the shift register before the master clocked the data byte.
- Either way, slave-TX byte loading via DMA is fundamentally not behaving
  like the host-TX byte loading does, despite identical channel config.

## Hypotheses ruled out

- AIRQ aborting the channel — unwired (`DMAnAIRQ = 0`).
- Stale error flags pre-arming the abort — pre-cleared in an earlier
  iteration; effect was nil.
- `I2C1CNT` driving `CNTIF` mid-transfer — no longer programmed in this
  path.
- `isr_on_transmit_exhausted` racing CSTR drop with the in-flight DMA —
  early return now in place.
- `DMAnCON0.EN` not actually transitioning 0 → 1 (so `SPTR`/`SCNT`
  wouldn't reload) — verified the cycle is in place.

## Hypotheses still open

- **Slave-TX I2C1TX SIRQ never edges.** In host-TX the `S = 1` kick puts
  the peripheral through a transmit-mode entry transition that pulses
  `I2C1TX`. In slave-TX, by the time `ADRIF` fires, the peripheral is
  already in TX mode and `TXBE` has been level-high since power-up, so
  there is nothing for `SIRQEN`'s edge detector to ride. (`DGO` should
  have been the workaround — but it didn't help, which is the puzzle.)
- **`I2C1TXB` writes from DMA might not be honored when the peripheral
  is mid-state-transition** (between ADRIF and CSTR release). If the
  peripheral only accepts TXB writes within a narrow window, the DMA
  transfer may be silently dropped.
- **The Q43 errata (DS80000870F) may apply to this path.** The init
  workaround is in place for the BTO recovery, but there may be a
  related quirk for slave-TX-after-restart that isn't documented here.

## Concrete next things to try

1. **TXIE-driven byte loading**, drop DMA for slave-TX only.
   Add an `irq(I2C1TX)` handler that writes the next byte from
   `g_client_tx[g_client_tx_idx++]` into `I2C1TXB` and disables
   `PIE7bits.I2C1TXIE` when the buffer is exhausted. Pre-load byte 0
   directly in `isr_on_address` (the direct-write path is already known
   to work for the first byte) and enable TXIE for byte 1+. This is a
   ~10-line change and matches what most reference designs do for slave-TX.

2. **Direct-write byte 0, then arm DMA for `&g_client_tx[1] / tx_len-1`,
   plus `DGO = 1`.** Combines the working byte-0 mechanism with a manual
   DMA kick. We tried direct-write+DMA without DGO (byte 0 OK, byte 1
   stretched); the DGO addition wasn't tested in *that* combination.

3. **Scope `I2C1TXIF` (or its SIRQ line) during a slave-TX read** to
   confirm/deny whether the rising edge actually happens between bytes.
   If it doesn't, DMA will never work for this path no matter what we
   try, and we have to use TXIE.

4. **Try `DMAnCON0bits.SSTP = 0`** for the TX channel as an experiment.
   Currently `SSTP = 1` (DMA stops + clears EN when SCNT hits 0). If
   something is causing SCNT to hit 0 prematurely, `SSTP = 0` would let
   it wrap and continue — useful diagnostically even if not the right
   final fix.

## Current state of `i2c_dma_client_tx`

```c
static void i2c_dma_client_tx(void) {
    if (g_client_tx_len == 0) {
        return;
    }
    INTERRUPT_PUSH;
    DMASELECT = DMA_TX_CHANNEL;
    DMAnCON0bits.EN = 0;
    DMAnSSA = (uint24_t)g_client_tx;
    DMAnSSZ = g_client_tx_len;
    DMAnCON0bits.SIRQEN = 1;
    DMAnCON0bits.AIRQEN = 1;
    DMAnCON0bits.EN = 1;
    DMAnCON0bits.DGO = 1;
    INTERRUPT_POP;
}
```

`isr_on_address` for the read path (still has the temporary `0x33`/`0x44`
test fill — see TODO in code):

```c
g_client_tx_len = 2;
g_client_tx[0] = 0x33;
g_client_tx[1] = 0x44;
if (I2C1STAT0bits.R && g_client_tx_len > 0) {
    g_fsm = FSM_CLIENT_TX;
    i2c_dma_client_tx();
    I2C1CON1bits.ACKDT = 0;
}
...
I2C1CON0bits.CSTR = 0;
```

## What was reset back to default along the way (don't redo)

- `I2C1CNT` programming in `i2c_dma_client_tx` — removed deliberately.
- `isr_on_transmit_exhausted` `FSM_CLIENT_TX` falling through to
  `CSTR = 0` — replaced with `return`.
- `DMAnAIRQ = 0` override after the `0x3b` line in `i2c_dma_init` for
  both channels — restored.

## References

- DS80000870F (PIC18F27/47/57Q43 silicon errata) — already linked in
  `i2c.c` near the BTO recovery path. Worth re-reading the slave-mode
  entries once more.
- `protocol.md` for the on-wire framing.
