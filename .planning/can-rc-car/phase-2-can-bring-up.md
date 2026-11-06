# Phase 2: CAN Bring-up + Gateway v0 — Detailed Plan

Status: **approved 2026-11-04.** All decisions below were made step by step with the user on 2026-11-03/04 (record: `task_plan.md`, "Next Step"). Pin and API checks: `findings.md`, "Phase 2 pre-checks".

## Goal and exit criterion
The ESP32-CAM gateway and one ESP32-C3 wheel node talk over CAN. The operator types `DRIVE <rpm>` on the gateway's serial console; the gateway streams speed setpoints over CAN; the wheel node runs its Phase 1 speed controller on them and reports its status back. If the setpoints stop arriving, the wheel stops by itself.
**Exit (roadmap):** a typed command spins the bench wheel, and pulling the CAN wire stops it within a defined timeout. **Defined timeout:** the speed run expires 100 ms after the last valid setpoint, then the duty ramps to 0 at the slew limit.

## Glossary (terms used in this plan)
Short definitions; ask for more depth on any of them. Phase 1 terms (ISR, IRAM, FreeRTOS task, setpoint, slew, …) are in `phase-1-one-wheel-node.md`.

**The bus**
- **CAN** (Controller Area Network): a two-wire bus where every node hears every frame. **TWAI** ("two-wire automotive interface") is Espressif's name for its CAN controller; one per chip on both the ESP32 and the C3.
- **Transceiver** (here SN65HVD230): converts the controller's logic-level TX/RX pins to the bus's two wires, **CANH** and **CANL**. Pin names on our adapters: **CTX** (to the MCU's TWAI TX) and **CRX** (to the MCU's TWAI RX). TX goes to TX, not crossed as on a UART.
- **Differential pair:** the signal is the voltage *between* CANH and CANL, not each wire to ground. That's why the logic analyzer probes the 3.3 V TX/RX side instead.
- **Dominant / recessive:** the two bus levels. Dominant (logic 0) is driven and overrides recessive (logic 1, the idle level). Any node can force a 0.
- **Termination:** a 120 Ω resistor at each *end* of the bus, so signals don't reflect. Two in parallel measure ≈ 60 Ω across CANH–CANL with power off.
- **Bitrate:** bits per second on the bus; here 500 kbit/s (2 µs per bit).

**Frames**
- **Frame:** one CAN message: an **ID**, a **DLC** (data length code: how many data bytes follow, 0–8), up to 8 data bytes, a CRC (checksum the hardware checks) and an ACK bit.
- **11-bit (standard) ID:** the frame's identifier, 0x000–0x7FF. It also sets priority (below).
- **Arbitration:** when two nodes start sending at once, the frame with the **lower ID wins** and the other retries automatically, so the ID is also the priority.
- **ACK:** every frame must be acknowledged by at least one *other* node. A node alone on the bus gets no ACK, retransmits forever and its error counters climb (see self-test mode).
- **Acceptance filter:** hardware in the TWAI controller that drops frames with IDs the node doesn't want *before* they reach the CPU, so they cost no interrupt.
- **Cyclic message:** sent at a fixed period whether the value changed or not. The receiver watches for it to stop arriving (**timeout monitoring**).
- **Alive counter:** a byte in the payload that goes up by 1 per frame (wrapping 255 → 0). If it stops changing or jumps, frames are stale or lost.
- **Raw value / scale:** the integer on the wire, and what to multiply it by to get the physical value (raw 550 × 0.1 = 55.0 rpm).
- **Little-endian:** a multi-byte number is sent low byte first ("Intel" byte order in DBC terms).
- **DBC file:** the standard text file describing every frame and signal on a CAN bus (created by Vector; the de facto exchange format in the car industry). Tools such as `cantools` read it.

**Errors**
- **TEC / REC** (transmit / receive error counters): every node counts errors it causes or sees. 0 is normal.
- **Error-active / error-passive / bus-off:** the node's state as TEC/REC grow. Error-active is normal. Above 127 the node goes error-passive (it stays quieter about errors). Above 255 TEC it goes **bus-off**: it disconnects itself and sends nothing until the application calls recover, after which it rejoins after 128 × 11 recessive bits.
- **Self-test mode:** a TWAI mode in which a node accepts its own frame without an ACK, so one node can be tested alone on the bus.

**Firmware**
- **Interrupt priority level:** on the C3, an interrupt at a higher level can **preempt** (pause) a running lower-level handler. Levels 1–3 can have C handlers (`ESP_INTR_FLAG_LOWMED`).
- **Transport:** the byte read/write functions behind a console: USB-Serial-JTAG on the C3, UART0 on the ESP32.
- **Re-arm:** after a timeout, the node needs a deliberate 0 setpoint before it accepts non-zero setpoints again.
- **Auto-reset circuit:** two transistors on the ESP32-CAM-MB board that drive the ESP32's EN (reset) and IO0 (boot mode) pins from the USB-serial chip's DTR and RTS lines, so `idf.py flash` can reset into the bootloader.

## Decisions for this phase
| # | Decision | Chosen (user, 2026-11-04) |
|---|---|---|
| A.1 | Reaching gateway GPIO13/14 | The ESP32-CAM-MB's male breakout pins (used in an earlier project); no soldering |
| A.4 | 10 kΩ pull-up on the C3's CAN TX | None for now; capture boot-time TX in B and decide from the measurement |
| B.1 | Bitrate | 500 kbit/s |
| B.2 | Bring-up order | Each node alone in self-test mode, then gateway → C3, then C3 → gateway |
| B.3 | Bring-up code | Shared `components/tiny_can` + standalone `firmware/can_probe` app for both chips; wheel node untouched until E |
| B.4 | Analyzer probes | D0 gateway RX, D1 gateway TX, D2 C3 TX |
| C | Console code | Shared `components/tiny_console` with a per-chip transport; **no copied code** (user) |
| C.1 | Gateway commands | `STATUS`, `DRIVE <rpm> [s]`, `STOP`, `HELP`, `CAN`, `CANMON ON\|OFF`; no raw `CANTX` |
| C.2 | `DRIVE` semantics | Time-limited like `SPEED` (default 5 s, max 30 s), then setpoint 0 |
| C.3 | Logs on UART0 | Kept; tools take only `OK`/`ERR`/`CANMON` lines |
| C.4 | Host tool | `nodelink.open_port()` takes a board type (C3 or CAM) with the right DTR/RTS |
| D.1 | ID length | 11-bit standard IDs |
| D.2 | Liveness | No separate heartbeat: the setpoint is cyclic (20 ms) with an alive counter; the node stops on a 100 ms timeout |
| D.3 | ID scheme | Function code + node number: setpoint `0x100+n`, status `0x180+n`; IDs below `0x100` reserved for a future e-stop; `n = 0` in Phase 2 |
| D.4 | Payloads | The byte tables below |
| D.5 | Frame layout source | The DBC is the single source; C is generated by `cantools` and committed; CI checks that regenerating gives no diff |
| D.6 | Docs for the frames | Tables + examples move to `components/tiny_proto/README.md`; a host test checks them against the DBC |
| E.1 | Timeout mechanism | Reuse the timed speed run: each valid setpoint refreshes it with 10 periods (100 ms) |
| E.2 | Setpoint 0, re-arm | 0 = stop (idle, coast). After a timeout, `fault_timeout` latches and non-zero setpoints are ignored until a 0 arrives |
| E.3 | Console vs CAN | A local console run has priority until it ends or `STOP`; `STOP` from either side always stops |
| E.4 | Interrupts | Encoder ISR at a higher level than TWAI; hardware filter accepts only `0x100` (+ the e-stop range) |
| E.5 | Encoder check under CAN | Stress test: `0x7FF` flood at ~3000 frames/s while the wheel runs at 80 % duty |
| F.1 | Exit test | Scripted where possible (`tools/`), manual steps prompted by the script |
| F.3 | Bus-off | Automatic recovery on both nodes, ~1 s between attempts, counted |
| F.4 | PRs | One PR per work item B–F, each bench-checked before merge |

## Pin map
| Board | TWAI TX → adapter CTX | TWAI RX ← adapter CRX | Adapter power | Why these pins |
|---|---|---|---|---|
| ESP32-CAM (gateway) | GPIO14 | GPIO13 | 3V3 + GND from the board | Free with the camera in use; not strapping pins. Reset pulls (datasheet v5.3): GPIO14 weak pull-up = recessive during boot; GPIO13's weak pull-down sits on RX, where it is harmless. Reached on the MB's male pins; which male pin is which gets read off at wiring time. |
| ESP32-C3 (wheel node) | GPIO4 | GPIO3 | 3V3 + GND from the board | Free and non-strapping (in use: 1, 5, 6, 7, 8, 10, 20; GPIO21 avoided, it is U0TXD where the boot ROM prints). No internal pulls at reset (datasheet v2.4), hence the A.4 boot capture. |

Bus: CANH–CANH, CANL–CANL, plus a GND wire between the two boards. All five adapters have a fixed 120 Ω, so two nodes on the bench are terminated correctly as they are (Phase 3 deals with the middle nodes).

## CAN frames (Phase 2)
Two cyclic frames, every 20 ms: **setpoint** `0x100`, 3 bytes, gateway → wheel node (int16 `target_rpm` in 0.1 rpm, uint8 alive `counter`), and **status** `0x180`, 7 bytes, wheel node → gateway (int16 `rpm`, int8 `duty`, `mode`, fault bits, `counter`, `setpoint_seen`). The byte tables, worked examples and the DBC moved (2026-11-04, item D) to [`components/tiny_proto/README.md`](../../components/tiny_proto/README.md) and [`tiny_proto.dbc`](../../components/tiny_proto/tiny_proto.dbc); the DBC is authoritative.

## Work items
Every item that turns the motor asks first (PSU on, wheel free). Serial reads are bounded; no interactive monitor.

### A. Bench wiring (no PR)
1. Wire one SN65HVD230 adapter to each board per the pin map: 3V3, GND, CTX, CRX. Bus: CANH–CANH, CANL–CANL, GND–GND between the boards.
2. Gateway: ESP32-CAM seated on the MB with the camera parallel to the USB-C connector (the reverse fit puts reverse polarity on the module's 5V input; `findings.md`).
3. Power off: measure CANH–CANL ≈ 60 Ω. Check no short between 3V3 and GND on either adapter.
4. Logic analyzer: D0 = gateway RX (GPIO13), D1 = gateway TX (GPIO14), D2 = C3 TX (GPIO4), GND to the bench ground.

Wiring diagram (2026-11-05): [`docs/images/phase2-a-wiring.png`](../../docs/images/phase2-a-wiring.png) (source: `phase2-a-wiring.svg`; edit the SVG and re-render with `magick -density 96 phase2-a-wiring.svg phase2-a-wiring.png`). The adapters' header pins are printed TX / RX (CTX / CRX in the glossary). The 120 Ω terminators are built into the adapters; the bus itself is just CANH–CANH, CANL–CANL.

### B. `tiny_can` + `can_probe`: first frames (PR)
1. `components/tiny_can`: TWAI node setup on the IDF 6.1 node API (`twai_new_node_onchip`, callbacks, `twai_node_enable`) with pins, bitrate, filter and interrupt priority as parameters; a send helper that keeps the frame buffer alive until `on_tx_done` (the driver queues by pointer); `on_rx_done` copies the frame into a FreeRTOS queue that a task reads (RX is only available in the ISR callback); state/TEC/REC/counters via `twai_node_get_info()`; bus-off recovery per F.3. Needs ESP-IDF, so it isn't host-tested; it stays thin.
2. `firmware/can_probe`: one app, built for `esp32` and `esp32c3` (separate build dirs), with a small console on `tiny_console` (item C; if C isn't merged yet, B uses a temporary print-only mode, no copied console code): send a counter frame every 100 ms, print every received frame, self-test mode on/off, and a flood mode for E.5.
3. Bench, in order: C3 alone in self-test mode (frames on its TX/RX in PulseView); gateway alone in self-test mode (this also proves the MB pin mapping); gateway → C3; C3 → gateway.
4. A.4 capture: D2 (C3 TX) across a C3 reset. If TX goes dominant during boot, add the 10 kΩ pull-up and recapture.
**Done when:** both directions run ~5 min at 100 ms with no counter gaps, TEC/REC = 0 on both, PulseView's CAN decoder shows the same ID/data with the ACK bit set, and the boot capture is logged in `findings.md`.

### C. `tiny_console` + gateway console (PR)
1. `components/tiny_console`: owns the console task, line assembly (`tiny_line`), command execution (`tiny_cmd`) and reply output. Each app passes its command table and a transport (USB-Serial-JTAG or UART0). The gateway uses it from this item on; the wheel node moves onto it in E.
2. Gateway commands (C.1): `STATUS` (uptime, version, heap, CAN state, last status age), `CAN` (state, TEC/REC, frame counters, bus-off count), `CANMON ON|OFF` (prints each received frame as a line with its own prefix, like `T,` for telemetry). `DRIVE` and `STOP` are added in E, when the frames exist.
3. `tools/nodelink.py`: `open_port()` takes a board type. For the CAM, find on the bench the DTR/RTS state that neither resets the ESP32 nor holds IO0 low, and record it in `findings.md`. Host tests for the new parsing (`CANMON` lines).
**Done when:** `HELP`, `STATUS` and `CAN` answer through `send_cmd.py` on `/dev/ttyUSB0`, and opening/closing the port neither resets the gateway nor leaves it in download mode (checked with a bounded read).

### D. `tiny_proto`: frames from the DBC (PR; host-only, can go before B)
1. `components/tiny_proto/tiny_proto.dbc` (the DBC above) and the generated `tiny_proto.c/.h`: `cantools generate_c_source --no-floating-point-numbers --database-name tiny_proto`. Pin `cantools==45.1.2` in `tools/requirements-test.txt` so regeneration is deterministic.
2. CI: regenerate and diff against the committed files, ignoring the generator's timestamp line in the file headers (see Verified during planning).
3. A thin hand-written layer on top if needed (e.g. ID + node number helpers, the mode/fault constants); the byte packing itself is only in the generated code.
4. Host tests (`test/host`): round trip for both frames, the three byte examples above, limits (±100.0 rpm, counter wrap, int8 duty ±100), DLC check on unpack.
5. `components/tiny_proto/README.md`: the frame tables and examples moved here from this plan, stating the DBC is authoritative; a host test (Python, `cantools`) checks the README tables against the DBC. Link it from `components/README.md` (index row) and `docs/architecture.md`.
**Done when:** the host tests and the CI no-diff check pass, and a PulseView capture (or `cantools decode`) shows decoded setpoint and status frames after E.

### E. Wheel node on CAN (PR)
1. Move the wheel node's console onto `tiny_console` (USB-Serial-JTAG transport). All Phase 1 commands keep working.
2. `drive_speed()` gets a variant with the duration in control periods (or ms). Each valid setpoint (ID `0x100`, DLC 3) refreshes a speed run with 10 periods left; expiry ramps down as a `SPEED` run ending does.
3. Rules (E.2, E.3): setpoint 0 = stop (idle). A run that expires on a missing setpoint sets `fault_timeout`, which latches; non-zero setpoints are ignored until a 0 arrives. A local console run has priority until it ends or `STOP`; `STOP` from either side stops at once.
4. Status frame every 20 ms from the drive task's data (rpm, duty, mode, faults, counter, `setpoint_seen`).
5. Interrupts (E.4): GPIO ISR service with `ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3` (encoder), TWAI `intr_priority = 1`; hardware filter for `0x100` plus the e-stop range. Keep the TWAI callback to copying the frame into the queue.
6. Gateway: `DRIVE <rpm> [s]` streams setpoints every 20 ms for the run's time, then 0; when no `DRIVE` is active it streams 0 (this also re-arms the node). `STOP` sends 0 at once.
7. E.5 stress check (test build: filter also accepts `0x7FF`): `can_probe` floods `0x7FF` at ~3000 frames/s while the wheel runs at 80 % duty with `PROBE ON`. Pass: encoder `invalid` stays 0; counts over a fixed number of wheel turns match a run without CAN; on the analyzer the encoder ISR is never delayed past the next edge. Fallbacks if it fails: 2× decoding; then an external counter or the ESP32-C6 (discussion in `task_plan.md`).
**Done when:** Phase 1 commands work on `tiny_console`; `DRIVE 50 5` from the gateway runs the wheel at ≈ 50 rpm with status frames showing it and `setpoint_seen` following; unplugging CANH/CANL mid-run stops the wheel (analyzer: last setpoint → PWM 0) with `fault_timeout` shown until a 0 setpoint re-arms; the E.5 check passes.

### F. Exit test (PR: script + results)
1. A `tools/` script that drives the gateway on `/dev/ttyUSB0` with bounded reads, checks replies and `CANMON` status lines, and stops to prompt for the manual steps. Its parsing gets host tests like `nodelink.py`.
2. Checklist (every motor run announced and confirmed first):
   1. Both boot; `CAN` shows error-active, TEC/REC 0.
   2. `DRIVE 50 10`: ≈ 50 rpm within the Phase 1 PI tolerance; status frames show it; `setpoint_seen` follows.
   3. `DRIVE -50 5`: reverse.
   4. `STOP` mid-run: stops, mode idle.
   5. A run ends by itself after its time.
   6. **Exit criterion:** pull CANH/CANL during `DRIVE`. Analyzer: last setpoint → PWM 0 ≈ 100 ms + ramp. `fault_timeout` set.
   7. Plug back in: no restart; a new `DRIVE` works (re-armed by the gateway's 0 stream).
   8. Reset the gateway during `DRIVE`: as 6.
   9. Soak: 10 min of `DRIVE` cycles, 0 counter gaps, TEC/REC still 0.
   10. E.5 stress check once more on the final build.
   11. Bus-off: short CANH to CANL briefly (every bit reads recessive, so a transmitter's own bits fail); the `busoff` count goes up and traffic resumes by itself.
3. Results in `progress.md`; tick the Phase 2 boxes in `task_plan.md`.

## Verified during planning (2026-11-04)
- **Nested interrupts on the C3, IDF 6.1:** `components/riscv/vectors.S` raises the interrupt threshold to the current level and re-enables interrupts before calling the C handler, so a higher-level interrupt preempts a lower one. `ESP_INTR_FLAG_LOWMED` = levels 1–3, "can be handled in C" (`esp_intr_alloc.h`). TWAI's `intr_priority` accepts 0–3 (0 = any of 1–3, allocator's choice), checked in `esp_twai_onchip.c`.
- **cantools 45.1.2 generator:** the DBC above parses; `generate_c_source --no-floating-point-numbers` gives pack/unpack in integer-only C that compiles clean with host gcc `-std=c11 -Wall -Wextra -Werror`. Two caveats: the `*_is_in_phys_range(double)` helpers are still generated (unused by us, so the linker drops them; the C3 has no FPU), and the file header carries a generation timestamp, which the CI no-diff check must ignore.

## Unverified (check during implementation)
- `gpio_install_isr_service()` passing `ESP_INTR_FLAG_LEVEL3` through to the allocator on the C3 (E.5 of the code; check the GPIO driver source or the return value).
- The DTR/RTS state that is safe with the MB's auto-reset circuit (C.3).
- Whether the TWAI acceptance filter suppresses the RX interrupt entirely for rejected frames (expected; E.5's stress design relies on it, so the stress frames are ones the filter accepts).
- ~~The generated code cross-compiles for the ESP32 and C3~~: checked 2026-11-04 with `riscv32-esp-elf-gcc` and `xtensa-esp32-elf-gcc`, `-Wall -Wextra -Werror`, clean. (Inside an IDF project build it's first compiled when a firmware requires it, in B/C/E.)
- Which MB male pin is IO13/IO14 (read off at wiring).

## Out of scope for Phase 2
- More than one wheel node, node identity/addressing, skid-steer mixing, the full four-wheel message spec, a combined all-wheels setpoint frame (Phase 3).
- The left/right direction convention (Phase 3).
- E-stop frame (ID range reserved only).
- Clock sync (Phase 4), battery/power (Phase 5), wireless operator link and latched driving (Phase 6), camera (Phase 7).
- More docs-vs-code host tests: a Key Question in `task_plan.md`, to revisit at the end of Phase 2.
