# I2C driver timeline analysis (`libcomm/i2c.c`)

Register-level walkthrough of three scenarios in sequence: a host read, a
client receive, then a host read again. Includes a finding about an
uncommitted edit to the host-TX `CNT` load (the earlier client-TX edit has
since been reverted).

Registers referenced: `I2C1CON0` (`EN`, `MODE`, `S`, `RSEN`, `CSTR`),
`I2C1CON1` (`ACKCNT` host terminal-NACK, `ACKDT` client ACK value),
`I2C1CNTH/L` (byte counter), `I2C1ADB1` (host address+R/W), `I2C1STAT0.R`
(client matched-address R/W), `I2C1STAT1.CLRBF`, plus `g_fsm` and the two
DMA channels. MODE values: `0b100` = host 7-bit, `0b000` = client 7-bit.

## Timeline 1 — Host read (write-then-read)

Example: `comm_send_config_read` → `i2c_submit(addr, {CMD,arg,crc}, tx_len=3, rx_len=2)`.
Bus idle, `g_fsm=IDLE`, MODE=client, RX DMA armed to `g_client_rx`.

| Event | Handler | Register / state changes |
|---|---|---|
| `i2c_submit` | — | task `MT_IDLE`, `g_q_tail++` |
| `i2c_poll` | `switch_to_host` | `MODE=0b100`, `ACKCNT=0`, `CLRBF=1` |
| ″ | `arm_event` → `dma_set_host` | TX DMA `SSA=tx,SSZ=3,EN=1`; RX DMA `DSA=rx,DSZ=2,EN=1` |
| ″ | `arm_event` | `ADB1=addr<<1` (W), **`CNTL=2`** (`tx_len-1` — see finding; was `tx_len=3`), **`RSEN=1`** (rx_len>0), state=`MT_RUNNING`, `g_fsm=HOST_TX`; TX DMA still armed `SSZ=3` |
| ″ | `i2c_poll` | `S=1` → START |
| wire | — | `[addr|W]`,CMD,arg ACK — **CNT hits 0 after 2 bytes; crc byte is never clocked out**. TX DMA has 1 byte left (`SCNT=1`). |
| `CNTIF` | `isr_on_transmit_exhausted` (HOST_TX) | `tx_done=1`; **log WA** (logs full `task->tx`, 3 bytes — wire only carried 2, so the log overstates what was sent); `g_fsm=HOST_RX`; `ADB1=addr<<1|1` (R); `CNTL=2` (rx_len); **`RSEN=0`**; **`ACKCNT=1`**; `S=1` (Restart from MDR); `CSTR=0` |
| wire | — | reSTART, `[addr|R]` ACK, data0 (host ACK), data1 (host NACK via ACKCNT), STOP; RX DMA fills `task->rx` |
| `CNTIF` | `isr_on_transmit_exhausted` (HOST_RX) | `break` — keep FSM, HW auto-Stops (RSEN=0) |
| `NACKIF` | `isr_on_nack` (HOST_RX, `CNTL==0`) | `break` — terminal NACK is *not* a failure |
| `PCIF` | `isr_on_stop` (HOST_RX) | **log RA**; `g_fsm=IDLE`; `disarm_event(OK)`: state=`MT_FINISHED`, `RSEN=0`, both DMA `SIRQEN=0/EN=0`; `switch_to_client`: `MODE=0b000`,`ACKCNT=0`,`CLRBF=1`, re-arm client RX |
| `i2c_poll` | — | dispatch `cb(OK,addr,tx,3,rx,2)`; `g_q_head++` |

**End state:** MODE=client, `ACKCNT=0`, `RSEN=0`, `g_fsm=IDLE`, RX DMA armed. Structurally clean and the FSM still completes, **but the write phase put a truncated frame on the wire** — the target received `{CMD,arg}` with no CRC, so it rejects the command (CRC/length check) and for a read request stages no response. The host's read phase then reads stale/zeroed bytes (or gets NACK'd), and the trampoline's CRC check fails. The host nevertheless logs WA/RA as if successful. See finding.

## Timeline 2 — Receive as client

### Case A — plain write (someone writes us a CONFIG)

| Event | Handler | Changes |
|---|---|---|
| `ADRIF` (addr|W) | `isr_on_address` | IDLE→nop; `R==0` → else-branch: `g_fsm=CLIENT_RX`, arm client RX, `ACKDT=0`; `CSTR=0` |
| wire | — | bytes ACK'd into `g_client_rx`, DCNT counts down |
| `PCIF` | `isr_on_stop` (CLIENT_RX) | `g_fsm=IDLE`; `on_cold_rx_complete`: `received=RX_MAX−DCNT`, **log CR**, `g_client_tx_len=0`, sync handler returns 1 (writes aren't read cmds) → `prepend_completed_task` (`MT_FINISHED`, cb=`g_cold_rx`); re-arm RX |
| `i2c_poll` | — | dispatch `cold_rx_dispatch` → `comm_on_*_received` |

Clean.

### Case B — read request (master does CONFIG_READ: write `{CMD,arg,crc}`, reSTART, read)

This ends in **client TX** — the path the edit affects.

| Event | Handler | Changes |
|---|---|---|
| `ADRIF` (addr|W) | `isr_on_address` | `g_fsm=CLIENT_RX`, arm RX, `ACKDT=0`, `CSTR=0` |
| wire | — | write-phase bytes → `g_client_rx` |
| `RSCIF` (reSTART) | `isr_on_restart` (CLIENT_RX) | `g_fsm=IDLE`; `on_cold_rx_complete`: log CR, `g_client_tx_len=0`, **sync handler** `sync_cold_rx_dispatch` sees MSB-set id → `comm_on_config_read_requested` → `comm_respond` → `i2c_set_client_tx` → fills `g_client_tx`, `g_client_tx_len = N` (value+CRC), returns 0; re-arm RX |
| `ADRIF` (addr|R) | `isr_on_address` | IDLE→nop; `R==1 && g_client_tx_len>0` → `g_fsm=CLIENT_TX`; `CLRBF`; `i2c_dma_client_tx` (TXB=byte0, DMA arms bytes 1..N−1); **`CNTL = g_client_tx_len − 1`**; `ACKDT=0`; `CSTR=0` |

The peripheral checks `(TXBE && CNT>0)` at each 8th falling SCL and stretches
if true. The counter reaches 0 `(N−1)` cycles in, so at the last (Nth) byte
`CNT = N−1 − (N−1) = 0` → no stretch, host clocks the 9th bit for its terminal
NACK + STOP. **This line has been reverted to `− 1u` and is now correct** (the
earlier `+ 1u` that hung the bus until BTO is gone).

| `CNTIF` | `isr_on_transmit_exhausted` (CLIENT_TX) | fires after byte N−2; `g_client_tx_len=0`, `CLRBF`; benign no-op `CSTR=0` |
| `PCIF`/`RSCIF` | `isr_on_stop`/`isr_on_restart` (CLIENT_TX) | `g_fsm=IDLE`, `g_client_tx_len=0`, `CLRBF`, re-arm RX |

Clean — the read is served correctly.

## Timeline 3 — Host read again

Every host read repeats Timeline 1, so it has the **same write-phase
truncation**: the command's final byte (CRC) is dropped on the wire. The
client-RX path of Timeline 2 is unaffected (it's a separate count), so the
fault is purely on this device's *outbound* writes/reads — independent of
whatever happened in scenario 2.

## Finding

The earlier client-TX edit has been **reverted** — line 632 is back to
`I2C1CNTL = (uint8_t)(g_client_tx_len - 1u)`, which matches its comment and
serves read responses correctly. No issue there now.

The current (only) uncommitted change is in `arm_event`, on the **host TX**
count:

```c
     if (task->tx_len > 0) {
         I2C1CNTH = 0;
-        I2C1CNTL = task->tx_len;
+        I2C1CNTL = task->tx_len - 1;
         I2C1CON0bits.RSEN = task->rx_len > 0;
         return FSM_HOST_TX;
     }
```

In host mode `I2C1CNT` is the number of **data bytes to clock out** (the
address comes from `ADB1` and isn't counted). The DMA is still armed for the
full length (`i2c_dma_set_host`: `DMAnSSZ = task->tx_len`), but the peripheral
stops and fires `CNTIF` once `CNT` hits 0 — after only `tx_len-1` bytes. So
**every host write transmits one byte short, dropping the trailing CRC**:

- Write-then-read (host read): the target receives `{id, payload…}` with no
  CRC, fails its CRC/length check, and stages no response → the read phase
  returns stale/zeroed bytes or a NACK; the trampoline's `response_crc_ok`
  fails. Effectively no host read ever succeeds.
- Write-only: same truncation; the receiver rejects the command, yet the host
  still logs `WA` (OK) because the log is built from `task->tx` (full length),
  not from what the wire carried.
- Leftover state: TX DMA finishes with `SCNT=1` (one undelivered byte),
  cleaned up by `disarm_event` at end-of-transaction — benign but a sign the
  count and DMA length now disagree.

Note the inconsistency with the read-phase load, which is unchanged at
`I2C1CNTL = task->rx_len` (lines 348, 665) — host RX uses the exact count, so
host TX should too. Unlike client TX, host TX is master-clocked and needs no
stretch-release trick; the `−1` pattern from the client path does not transfer
here. Reverting line 341 to `I2C1CNTL = task->tx_len` restores correct framing.
