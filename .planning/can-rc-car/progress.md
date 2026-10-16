# Progress Log

## Session: 2026-09-28

### Top-level roadmap planning
- **Status:** complete
- Actions taken:
  - Read `docs/can-rc-car-project-overview.md` and `docs/can-rc-car-parts-list-final.md`.
  - Settled on ESP-IDF, SN65HVD230, a text command protocol over UART, the ESP32-CAM as gateway from day one, and roadmap-only scope for this pass.
  - Wrote the phases 0–7 roadmap; the user approved it.
  - Recorded the roadmap in the planning files.
- Files created/modified:
  - `.planning/can-rc-car/{task_plan,findings,progress}.md` (new; moved from repo root on user request)
  - `.planning/.active_plan` → `can-rc-car`
  - `README.md` (new): name, short goals, links to docs and the roadmap. Stable by design; status lives in the planning files, not the README.
  - `.gitignore` (new): ignores `.claude/settings.local.json` and the skill's machine-local runtime dotfiles. `.planning/` is committed so the plan works from any machine.
  - Git repo initialized (`main`) and pushed to public GitHub repo `shanirub/tiny-ecus`.

### Phase 0: Foundations
- **Status:** in_progress (plan approved 2026-09-28; see the later sessions for item status)
- Actions taken:
  - Checked the local toolchain: IDF v5.5.5 and a master checkout are installed, v6.1 is not; docker, podman and pulseview are present; user is in `dialout`.
  - Compared TWAI drivers in v5.5.5 and v6.1: new-driver API identical; legacy deprecated in 6.1.
  - Confirmed GitHub Actions is free for public repos, the `espressif/idf:v6.1` image exists, and the target can be locked in `sdkconfig.defaults`.
  - User decisions: IDF v6.1, native install + version check, build-only CI (free).
  - Wrote the Phase 0 detailed plan.
- Files created/modified:
  - `.planning/can-rc-car/phase-0-foundations.md` (new)
  - `.planning/can-rc-car/task_plan.md`, `findings.md` (updated)
- Work item A (done):
  - Shallow-cloned `v6.1` into `~/esp/esp-idf-v6.1` (733 MB) and ran `./install.sh esp32,esp32c3` (exit 0).
  - Added `alias idf61='. ~/esp/esp-idf-v6.1/export.sh'` to `~/.zshrc`, next to the existing `idf55`.
  - Wrote `docs/dev-setup.md` (prerequisites, install, alias, serial ports, build/flash/monitor, flashing fallbacks).

## Session: 2026-10-10
- All AliExpress parts have arrived. The two 18650 cells are still in transit and expected this week.
- Until the cells arrive, power comes from the user's bench power supply.
- Phase 0 item E (hardware bring-up) is now unblocked except for E.4 (cells). E.2 and E.3 need no firmware; E.1 needs item B first.
- Synced `phase-0-foundations.md` item A text with what was actually done (shallow clone, `idf61` alias); noted that the `dev-setup.md` version-fail claim awaits test C.2.
- Updated the parts list status. No code work this session; the user resumes with item B around 2026-10-11/12.

## Session: 2026-10-11
- The user asked whether to start mechanical work on the old car (remove the drive board and the AA holder, swap in the encoder motors).
- Decided: do the mechanical prep now; wire no electronics into the chassis yet.
- Added this as Phase 0 work item F rather than a separate "Phase 0.5", because it runs alongside items B–D and isn't a sequential gate.
- F covers the teardown, motor and encoder bench tests, encoder pinout and counts-per-rev, the fit check, and mounting 4 motors. The 5th motor is the Phase 1 bench spare.
- Files modified: `phase-0-foundations.md`, `task_plan.md`, `progress.md`.
- Recorded the motor listing specs (12 PPR, 1:90, 3–12 V; maker Kongxiang/Nulllab) in `findings.md`. No datasheet was found online. Added the expected counts per revolution to F.4, and the winding-resistance check plus an 8.4 V run to F.2.
- Added the powered encoder run to F.4: no-load speed from f_A at 6 V and 8.4 V, and the left/right direction convention. It needs no MCU or firmware: the bench PSU drives the motor, a separate 3.3 V source powers the encoder, and the logic analyzer (or a multimeter Hz mode) reads A and B.

## Session: 2026-10-14
- E.2 done: all 5 CAN adapters measure 118–120 Ω, so every adapter has its terminator fitted.
- Consequence: with all 5 on one bus, 5 × 120 Ω in parallel is 24 Ω, well below the 60 Ω the bus expects. 3 terminators must come off once the bus has more than 2 nodes.
- Phase 2 uses only 2 nodes, the gateway and one C3. Both are bus ends, so no rework is needed until Phase 3. Removing the terminators is deferred to Phase 3, when the physical topology decides which two nodes are the ends.
- The front 2-pin header was checked by continuity: it connects straight to CANH/CANL, so it is a second bus connector and not a termination jumper. There is no solder bridge on the back. The terminator is a fixed SMD resistor; Phase 3 removes it on 3 boards.
- Reordered F.2 so the winding-resistance measurement (unpowered) comes before the powered runs.
- Planning-file cleanup: removed the stale "resume around 2026-10-11/12" from the Next Step; linked the current-budget risk to the F.2 stall estimate.
- Files modified: `progress.md`, `task_plan.md`, `phase-0-foundations.md`, `findings.md`.
- F.1 teardown done. Before photos in `docs/images/` (`before teardown up.webp`, `before teardown bottom.webp`), plus CAN adapter front/back photos. Old drive board (Keyes L298N V2) and old battery holder removed; the old MCU-with-camera board and its BT antenna stay on the chassis for now. The new 2S holder hasn't been tried yet.
- Trimmed item F. F.2 keeps winding resistance and one 6 V spin per motor and drops the 8.4 V and reverse runs. F.4 is now a 3.3 V encoder check; counts/rev, no-load speed and direction convention move to Phase 1 (firmware measures them), with an optional hand count kept as a no-firmware reference. E.3 deferred until something first connects to a buck output.
- F.5 flagged as the most likely F item to fail: the photo suggests only ~10 mm between each old motor's back end and the next gearbox (rough estimate), and the encoder adds length there.
- Photos re-encoded to 1280 px WebP (q80): 1.0 MB to 0.46 MB for the 4 files. The CAN adapter front photo shows the terminator: SMD "121" next to the "120R" label.
- Files modified: `phase-0-foundations.md`, `task_plan.md`, `progress.md`, `findings.md`.

- F.3 pinout confirmed on the first motor: silkscreen `+ GND A B V -`; red = `+`, black = `-` (outer pins). Cable has a second red wire on an inner pin.
- F.2 winding R: first motor ≈ 3.0 Ω, second ≈ 3.1 Ω, far below the ~7–15 Ω assumed. Stall ≈ 2.8 A per motor at 8.4 V, ≈ 11 A for 4. Current-budget risk updated in `task_plan.md`.
- F.2 first motor 6 V spin: CV at 6.00 V, no-load 0.098 A; second mounted motor 0.100 A, a close match. No-load current is ~5 % of the ~2 A stall estimate, as expected for a small brushed motor.
- F.5 first look: encoder motors fit the old mounts, about 1 mm between one motor's encoder end and the next gearbox. No rubbing during the 6 V spin, including with the motors pressed together. Sound: gearbox whine with a periodic "wow-wow", the same on both chassis-mounted motors tested so far. Judged harmless; not traced to a gear stage (optional shaft-flag test skipped).

- Remaining 3 motors: skip the resistance measurement (the first two agree), but give each a 6 V spin before mounting, to catch a defective unit. Plan updated in `phase-0-foundations.md` F.2 and in the Next Step.

## Session: 2026-10-15
- F.2 done: the remaining 3 motors each got a 6 V spin. All settle at about 0.1 A no-load, like the first two. Two showed about 0.3 A briefly at start before dropping to 0.1 A: inrush (no back-EMF at standstill), normal. The true inrush peak is likely higher; the PSU display samples too slowly to catch it. All 3 have the same periodic gearbox "wow". No faulty motor, so any of the 3 can be the bench spare.
- PSU power readout: 0.7 W on two motors and 0.6 W on the third, consistent with 6 V × ~0.1 A. (The display pads with leading zeros, "000.7 W", which was first misread as 0.007 W.)
- Added `docs/architecture.md`: power chain, CAN topology and wheel-node diagrams; linked from `task_plan.md`.
- Files modified: `progress.md`, `task_plan.md`, `phase-0-foundations.md`; added `docs/architecture.md`.
- F.3 done (after a power cut lost the earlier session): inner wire colors read visually at the connector, GND yellow, A green, B blue, V red (second red). Same color order on all 5 motors. Recorded in `findings.md`.
- Files modified: `findings.md`, `progress.md`, `phase-0-foundations.md`, `task_plan.md`.
- 18650 cells: still not arrived, in customs; hopefully the week of 2026-10-19. The only part still missing; the bench PSU stands in until then.
- Decided: switch to the wider 2-deck acrylic chassis (photo in `docs/images/`), mounted flipped with motors underneath. The 2 motors on the first chassis come off; F.1/F.5/F.6 restart on the new one.
- Decided: breadboards on the bench for Phases 1–4; soldered perfboard per node (socketed C3, connectors, heat shrink) from Phase 5. Added a Phase 5 item.
- Files modified: `task_plan.md`, `phase-0-foundations.md`, `findings.md`, `progress.md`; added the chassis photo.
- Wiring research: 4 options written up with diagrams (A one-breadboard backbone, B 4-wire bus cable + power star, C stripboard backplane, D two side pods). Suggestion: D layout with B's cabling; C as fallback. Page: https://claude.ai/artifact/1grV3WivA4evrorcrMcqnv.
- Wiring decided: option D (two side pods, B-style bus cable and power star); recorded in `task_plan.md` Decisions. Page moved into the repo as `docs/design/wiring-options.html`, with a bill of cables and a wire-type guide added. Research notes in `findings.md`.

## Session: 2026-10-16
- Checked the C3 pin budget: 7–10 GPIOs needed per node (motor 2, encoder 2, CAN 2, LEDs 1–2, battery ADC 1 on one node, optional nSLEEP/nFAULT); 10 non-strapping GPIOs available (0, 1, 3–7, 10, 20, 21). Fits; pins assigned in Phase 1 planning.
- Item B done: `components/tiny_common` (boot banner), `firmware/wheel_node` (esp32c3, console on USB-Serial-JTAG), `firmware/gateway` (esp32), hello-worlds blink the onboard LED at 1 Hz with a 5 s heartbeat log. `.gitignore` and README Development link added.
- Item C partly: both projects build with IDF v6.1 (0 warnings); the v5.5.5 negative test fails as intended. Flashing pending.
- F.4 done on the spare motor: logic analyzer (PulseView Gray code decoder) shows clean quadrature, +1/−1 counts by direction, 47 counts per encoder-disc turn (≈ 48 = 12 PPR × 4). Multimeter: A and B high 3.28 V, low 0 V, within the C3 input thresholds. Decided: skip F.4 on the other 4 motors (same encoder board); check one only if it misbehaves while debugging.
- Files modified: `phase-0-foundations.md`, `task_plan.md`, `progress.md`, `findings.md`.
- Added the F.4 PulseView capture as `docs/images/f4 encoder quadrature pulseview.webp` (lossless WebP), linked from `findings.md`.
- Wiring parts ordered: WAGO 221 box (5- and 3-port), 22 AWG solid core in assorted colors, 20 AWG silicone stranded red + black, JST-XH 4-pin pigtail pair(s). 20 AWG stranded replaces the planned 22 AWG stranded for motor branches; solid core is for board-level links only. JST-XH pairs and half-size breadboards already on hand. Parts list updated. 18650 cells on the way. Still missing: the wiring order and the cells; everything else is on hand.
- Decided the git workflow: branch + PR + squash-merge for firmware/CI/build changes, direct to `main` for docs and planning. Item D (CI) is the first PR. Recorded in `task_plan.md` Decisions, `phase-0-foundations.md` item D and `docs/dev-setup.md`.

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| IDF v6.1 install | `. export.sh && idf.py --version` | `ESP-IDF v6.1` | `ESP-IDF v6.1`; xtensa-esp32-elf-gcc and riscv32-esp-elf-gcc esp-15.2.0_20251204; Python 3.14.7 | pass |
| E.2 CAN adapter termination (2026-10-14) | Unpowered, CANH–CANL resistance on each of the 5 SN65HVD230 adapters | ~120 Ω where termination is fitted | 118–120 Ω on all 5: every adapter has a 120 Ω terminator fitted | pass (fixed SMD terminators, no jumper; 3 to remove in Phase 3) |
| F.3 encoder pinout (2026-10-14) | Silkscreen + ohmmeter on first motor | Motor pair identifiable | `+ GND A B V -`; red = `+`, black = `-`; `+`↔`-` reads the winding | pass (inner colors mapped 2026-10-15, next row) |
| F.3 inner wire colors, all 5 motors (2026-10-15) | Visual check of wire order at the PCB connector against the silkscreen | Same order on all motors | GND = yellow, A = green, B = blue, V = red (inner); `+` red / `-` black outer; identical on all 5 | pass |
| F.2 winding R, first motor (2026-10-14) | Ohmmeter 200 Ω range across `+`/`-` via cable and jumpers | 7–15 Ω (guess) | 3.2 Ω read − 0.2 Ω leads = 3.0 Ω | recorded; lowest-over-positions not yet done; stall far above listing |
| F.2 winding R, second motor (2026-10-14) | Ohmmeter 200 Ω range across `+`/`-` via cable and jumpers | ~3 Ω (matches first motor) | 3.3 Ω read − 0.2 Ω leads = 3.1 Ω | recorded |
| F.2 6 V no-load spin, first motor (2026-10-14) | Bench PSU 6.00 V, 1 A limit, across `+`/`-` | Spins; PSU settles in CV | Spins; CV, 6.00 V, 0.098 A (0.5 W) | pass (no rubbing; periodic gearbox "wow", same on both mounted motors, harmless) |
| F.2 6 V no-load spin, second motor (2026-10-14) | Bench PSU 6.00 V, 1 A limit, across `+`/`-` | Spins; PSU settles in CV | Spins; 0.100 A | pass (no rubbing; same periodic gearbox "wow") |
| F.2 6 V no-load spin, remaining 3 motors (2026-10-15) | Bench PSU 6 V, 1 A limit, across `+`/`-` | ~0.1 A in CV, no grinding | All 3 settle at ~0.1 A (0.6–0.7 W); 2 show ~0.3 A briefly at start (inrush); same periodic gearbox "wow" | pass (no faulty motor) |
| C.1 build, both projects (2026-10-16) | `idf.py build` with IDF v6.1 | Builds, no warnings | Both build; 0 warnings | pass |
| C.2 negative test (2026-10-16) | Clean copy, `idf.py build` with IDF v5.5.5 | Configure fails on the IDF constraint | `Version solving failed: no versions of idf match ~6.1.0` | pass |
| F.4 encoder 3.3 V check, spare motor (2026-10-16) | PSU 3.3 V / 0.1 A limit to V/GND; analyzer D0 = A, D1 = B at 1 MHz; hand-turned both ways; Gray code decoder | Clean quadrature; count up one way, down the other; high 2.5–3.3 V, low ≤ 0.8 V | 3 mA supply; two clean square waves, no pull-ups needed; count 0→12 then back to 0 (Increment +1 / −1); one turn of the encoder disc = 47 counts (≈ 48 = 12 PPR × 4); multimeter: A and B high 3.28 V, low 0 V | pass on the spare motor; other 4 skipped (decided 2026-10-16) |
| F.1 teardown (2026-10-14) | Old car: photos, then remove drive board and battery holder | Chassis clear for new motors | Done; photos in `docs/images/`. Old MCU+camera board and BT antenna left on for now | pass |

## Error Log
| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
|           |       | 1       |            |
