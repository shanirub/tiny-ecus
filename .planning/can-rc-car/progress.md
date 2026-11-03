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

## Session: 2026-10-17
- Item C done: flashed `wheel_node` onto a C3 Super Mini (`/dev/ttyACM0`) and `gateway` onto the ESP32-CAM (`/dev/ttyUSB0`); both boot logs show the banner and the 5 s heartbeat, and the user saw both LEDs blink at 1 Hz. GPIO8 and GPIO33 (both active-low) are confirmed. No BOOT / IO0 button needed.
- The ESP32-CAM red LED faces the expansion board, so only its glow is visible; noted in `findings.md`.
- The user's `idf61` failed in one terminal because a previous IDF activation (master checkout) left `IDF_PYTHON_ENV_PATH` on the `idf6.2` venv. Fix recorded in `findings.md`.
- The hello-world source comments still say the LED pins are unverified; fix them in the item D branch (firmware changes go through a PR).
- Docs reread for stale data: `docs/dev-setup.md` gained the venv-mismatch fix, a note that both boards auto-reset into the bootloader, and where to look for the ESP32-CAM LED.
- Files modified: `task_plan.md`, `phase-0-foundations.md`, `findings.md`, `progress.md`, `docs/dev-setup.md`.
- Item D done: `.github/workflows/build.yml` (container `espressif/idf:v6.1`, matrix `[wheel_node, gateway]`, build only, `contents: read`). First branch + PR under the new workflow: PR #1 on `ci/build-workflow`, also fixing the LED comments in both `main.c`. Both jobs passed (~2 min each, no warnings), squash-merged as `3e67b07`, branch deleted. **Phase 0 exit criterion met.**
- Phase 0 marked complete; open items moved: F.1/F.5/F.6 before Phase 3, E.1 to Phase 3 (user: seller protection is not a factor), E.3/E.4 to Phase 5.
- Phase 1 planning started. Found that the ESP32-C3 has no PCNT; user chose 4× decoding in a GPIO interrupt. Also chosen: CSV telemetry + Python plot script, host unit tests in CI. Draft written: `phase-1-one-wheel-node.md` (with a glossary: the user asked for every term to be explained). Waiting for approval and the DRV8833 module's pin labels.
- `docs/architecture.md`: PCNT replaced by GPIO-interrupt decoding.
- DRV8833 module photographed (`docs/images/drv8833 module front.webp` / `back.webp`, lossy WebP q85, 143 KB total from 2.0 MB PNG). Pins recorded in `findings.md`: STBY exposed, no fault pin. Pin map: AIN1/AIN2 = GPIO6/7, STBY = GPIO20.
- Phase 1 A.3: bench motor current needs no 20 AWG; thickest jumpers on hand suffice under the PSU's 1.5 A limit.
- Glossary additions on request: why a motor driver is needed, STBY, nFAULT, NC. Phase 1 D.3 gained the low-speed resolution note (1 count per 10 ms = 1.39 rpm; moving average only if needed).
- Phase 1 plan approved by the user (2026-10-17).

## Session: 2026-10-20
- Phase 1 item B done on `phase1/b-console`, PR #2, squash-merged as `81ecfca` after all 3 CI jobs passed (both firmware builds, host tests). Contents: `components/tiny_cmd` (parser + line assembler), wheel_node console (`STATUS`, `TELEM ON|OFF`, `HELP`) and 100 Hz sine telemetry through a queue and a low-priority writer task, `test/host` (33 Unity tests, sanitizers in CI), `tools/plot_run.py` and `tools/send_cmd.py`.
- A host test caught a real bug before flashing: the LF of a CRLF wiped the line the CR had just finished. Fixed by handling the LF first.
- Bench check found that opening the port reset the C3; fixed in the tools (DTR high, RTS low). Details in `findings.md`.
- Files modified: `task_plan.md`, `phase-1-one-wheel-node.md`, `findings.md`, `progress.md`.
- Plot window check: `plot_run.py` without `--no-show` opened no window (matplotlib fell back to Agg; no Tk or Qt). User chose PyQt6 in the requirements over a system package, so a fresh clone works; plus a clear message in `plot_run.py` when no window can open. Done on branch `tools/plot-window`; the user confirmed the window opens.
- Reread all markdown docs for stale or missing data: `docs/dev-setup.md` gained Host unit tests and Bench tools sections and no longer says the wheel node logs a heartbeat; README link text; `task_plan.md` repo layout, Phase 2 parser reuse, two decisions (scaled-integer telemetry, PyQt6), a table break fixed; Phase 1 plan glossary (queue, scaled integers, DTR/RTS, sanitizers, GUI backend), item B summary, and a note that `CONFIG_FREERTOS_HZ=1000` isn't set yet.
- Files modified: `README.md`, `docs/dev-setup.md`, `task_plan.md`, `phase-1-one-wheel-node.md`, `findings.md`, `progress.md`.
- Parts check: the 18650 cells and the wiring order still haven't arrived (user, 2026-10-20). Neither blocks Phase 1 (bench PSU, jumpers).

## Session: 2026-10-21
- A.1 done: unpowered STBY resistance on the DRV8833 module, probed on the bare pads (header not soldered yet). A first try on the 200 Ω range read OL both ways, which only means "above 200 Ω"; repeated on the 2 MΩ and 200 kΩ ranges. Result: STBY is pulled down to GND and has no pull-up to VM. Details and reasoning in `findings.md`.
- Fetched the TI DRV8833 datasheet and checked the figures we had from memory: nSLEEP 500 kΩ internal pull-down and 6.5 V clamp, 150 kΩ pull-downs on the other inputs, input thresholds, 1 ms wake-up (`findings.md`). Also new from it: our chip is the HTSSOP (PWP) package by its marking, so 1.5 A RMS / 2 A peak per bridge applies (the plain TSSOP is 0.5 A); overcurrent trips at 2 A min / 3.3 A typ and auto-retries every 1.35 ms, so a battery-powered stall (~2.8 A) can trip it.
- The two 18650 cells arrived (user, 2026-10-21). Parts list and plan updated; only the wiring order is still missing. Their check and charge (Phase 0 E.4) stays in Phase 5. Cells: LiitoKala Lii-35A 18650 (3500 mAh, 3.7 V nominal), flat top on both ends, both read 3.59 V on arrival (multimeter, user; first half of E.4: matched). Discharge rating looked up (`findings.md`). Still to check in Phase 5: whether flat tops make contact in the holder.
- Docs sweep before commit: replaced every remaining "from memory" DRV8833 figure with the datasheet value (`findings.md`, Phase 1 plan); added A.5 (powered STBY check at first power-up) to the Phase 1 plan; glossary gained pull-up/pull-down, body diode and Zener diode; `docs/architecture.md` notes the driver's overcurrent trip next to the stall warning.

## Session: 2026-10-22
- A.2 prep. Checked the ESP32-C3 datasheet's reset pin states: GPIO20 (planned STBY) and GPIO6 (planned AIN1) have the 45 kΩ pull-up on after reset, which would wake the driver and run the motor from reset until firmware takes the pins (and indefinitely with blank firmware or in download mode). Re-mapped (user's choice): AIN1/AIN2 = GPIO5/7, STBY = GPIO10, encoder A/B = GPIO6/20. A.5 now also checks the pins with the C3 held in download mode.
- Decided: motor current may pass one breadboard contact per lead (PSU and motor leads in the same short strip as the module pin), replacing "never through breadboard strips".
- Headers: the C3 has its pins. DRV8833 module header soldered (Sn63/Pb37, ~320 °C), labels up / components down, using the breadboard as a jig. Joints shiny; a few slightly heavy but wetted. Continuity check passed (test row below).
- Git workflow change (user, 2026-10-22): the user reviews, approves and merges every PR, to understand the code before it lands; Claude opens PRs and waits. Recorded in `docs/dev-setup.md`, the decision table and the Phase 1 plan.
- Docs re-check before commit: A.3's repeated current figures merged; diagram note on module GND updated (now verified) and PNG re-rendered.
- Wiring diagram for A.2: first as ASCII in the Phase 1 plan, then (user: ASCII not fully clear) redrawn as `docs/images/phase1-a2-wiring.svg` + `.png`, linked from the plan in place of the ASCII.

## Session: 2026-10-24
- A.2 wiring under way. Bulk cap: no 470 µF in the stash (only 10 µF and 100 µF electrolytics); 100 µF ≥ 16 V used on the bench. Parts list corrected (it claimed 470 µF on hand); 470 µF per node to buy before Phase 5. 100 nF ceramic = code "104".
- A.2 wiring done by the user; photo review found three faults, all fixed: no jumper from the − rail (C3/encoder GND) to a module GND, so the grounds weren't common; bulk cap first had its − leg in the rail, then both legs on GND (now − on module GND row, + on VM row); one motor-lead coupler loose (found by check 4).
- Idea for A.5 and later (user asked whether Claude could probe directly): the logic analyzer with `sigrok-cli` (already installed) gives Claude bounded captures of STBY/AIN1/AIN2 through boot, reset and download mode; inputs max 5 V, never VM or AO1/AO2. A USB-controllable PSU or meter would be a purchase, not decided.

## Session: 2026-10-25
- Phase 1 A.5 first power-up done, pass. PSU 7.4 V / 1.5 A, C3 on USB. With the item B firmware running and again in download mode, STBY, AIN1 and AIN2 read 0 V, the motor stayed still and the PSU showed 0.000 A (below its 1 mA resolution). VM–GND read 7.4 V. Item A is complete.
- Download mode confirmed with `esptool --before no-reset --after no-reset chip-id`: no reply while the firmware ran, "Connected to ESP32-C3" after BOOT+RST. It leaves esptool's stub flasher in RAM, which doesn't drive the motor pins. Visual cue: the GPIO8 LED blinks under the item B firmware and stops in download mode.
- Logic analyzer skipped for A.5: the meter answers the test, and the DRV8833's ~1 ms wake-up makes a short reset glitch unlikely to move the motor (inference, not tested). Next needed in D.4 (ISR load); optional in C.
- Phase 1 item C on `phase1/c-encoder`, PR #5 opened (the user reviews and merges). New `components/tiny_qdec` (state table, host-tested: 14 tests), IRAM-safe encoder ISR on GPIO6/20 with pull-ups, console `ENC` / `ENC ZERO`. CI: all 3 jobs pass. Bench check C.3 (10 hand turns ≈ 43 200) after merge and flash.

## Session: 2026-10-28
- Mutation tests added to PR #5 (`test/host/mutate.py`, 12 hand-picked mutants, run in CI). The first run found two test gaps (direct `tiny_qdec_step` calls with out-of-range inputs; the zero test zeroing at state 0); both closed, now 15 tests, 12/12 mutants killed, CI green.
- Also in PR #5: `test/README.md` (what the tests cover and what they don't) and `components/README.md` (what goes in `components/`). Decided: top-level docs stay short and link to per-directory READMEs; `tools/README.md` deferred until `tools/` grows.
- Parts: 470 µF 50 V bulk caps ordered (for Phase 5). Part of the wiring order is with a local carrier, expected the week of 2026-11-02.

## Session: 2026-10-29
- PR #5 review changes: every wheel-node pin moved into `firmware/wheel_node/main/pins.h`; a comment in `encoder.c` on how an edge reaches the ISR and what happens when edges come too fast.
- PR #5 merged (`e50b5d8`), flashed. C.3 first try: 10 turns of the encoder disc at the motor's back end (not the output shaft) gave `pos=480 invalid=0` = 10 × 48, confirming 48 counts per motor turn (12 pulses × 4) with no missed edges. Then with a wheel on: 10 hand turns of the wheel gave `pos=-43858 invalid=0` (4386 per turn, +1.5 % vs the expected 4320). C.3 passed, accepted by the user without a repeat: the difference is taken to be hand positioning at start/end (~55° over 10 turns). Not ruled out: a real gear ratio of ~1:91.4 instead of 1:90. The sign only reflects the turning direction (convention decided in D.4). Item C done.
- Item D started (user choices: counts per turn stay at the nominal 4320; D split into two PRs). PR #6 on `phase1/d-drive`: D.1 PWM drive + D.2 protections (`components/tiny_motor`, 16 host tests, 12 mutants; `drive.c` 100 Hz loop; `DUTY`/`STOP`; `CONFIG_FREERTOS_HZ=1000`). Bench check after merge listed in the PR. D.3 speed estimation + telemetry columns follow in a second PR.
- PR #6 review started: file 1 `tiny_motor.h` done (fixed the `tiny_motor_run` doc: the ramp up counts toward the run steps; uncommitted on the branch). File 2 `tiny_motor.c` done (stall check is now a range test on counts, no `abs32(INT32_MIN)` overflow; mutant 7 text updated; host tests and mutants pass). File 3 `test_tiny_motor.c` done (added `test_new_run_keeps_the_stall_count` and `test_extreme_counts_are_moving`, mutants 8, 9 and 11; now 18 tests, 15 motor mutants, all killed; `test/README.md` updated). File 4 `mutate.py` done (mutant 11, stall trips one step late). Files 5–6 `drive.h`/`drive.c` reviewed; 2026-10-31: `apply()` write order swapped (inputs before waking STBY, sleep before the inputs), README diagram labels updated, firmware builds clean; Mermaid diagrams checked with the mmdc container (podman), all 5 render. Added `firmware/wheel_node/README.md` with Mermaid state + sequence diagrams of the motor drive (user choice: Mermaid, in that README); linked from `docs/architecture.md`. Sanitizer libs now installed locally; full run passes. `console.c` reviewed (replies fit the 128-byte buffer); `DUTY` reply now shows the capped duty (`drive_run()` returns it via `*run_pct`). `main.c` reviewed (placeholder-telemetry comment fixed). Docs and planning re-read for stale info (task plan: next step, `tiny_motor` in the layout, item D and current-limit risk status; Phase 1 plan: tick rate set, D split; top-level README links the system diagrams; `test/README.md` points to the Phase 3 mutation-tool reminder). Review fixes committed and pushed to PR #6; waiting for the user's merge.

## Session: 2026-10-31
- PR #6 merged by the user (`39c1e68`). Built with no warnings, flashed with `idf.py flash` (hash verified); `STATUS` showed `ver=39c1e68 drive=idle duty=0` and `HELP` listed `DUTY` and `STOP`.
- D.1–D.2 bench check (PSU 7.4 V / 1.5 A, commands via `send_cmd.py`), all six pass:
  1. Idle with motor power on: STBY, AIN1, AIN2 ≈ 0 V (meter), motor still; VM–GND 7.41 V.
  2. `DUTY 30 3` (three runs): turns smoothly, ramps down, back to idle; −7628 / −7946 / −7952 counts (≈ 36 wheel rpm once warm); PSU 0.032 A.
  3. `DUTY -30 3` after a `DUTY 30 3`: turns the other way (seen), +7900 vs −7972 counts; PSU 0.034 A.
  4. `DUTY 50 30`, `STOP` after 1.5 s and after 3 s: stops at once (seen), then coasts 379 / 397 counts (≈ 32°); ≈ 66–68 rpm; PSU 0.059 A.
  5. `DUTY 100 3` (3 s instead of 2, to read the PSU): `OK duty=80 for 3 s (capped from 100)`, mid-run `STATUS` `duty=80`; ≈ 100 rpm; PSU 0.104 A, 0.116 A briefly at the start.
  6. Stall, motor body and wheel held by hand (the motor isn't mounted; holding only the wheel turns the body): `DUTY 50 3` (3 s instead of 10, so a failed trip ends sooner) → `drive=stall duty=0` within 1 s, encoder moved 8 / 17 counts net (first read as slack; the D.3 telemetry later showed ≈ 9° of wind-up and a swing back, see `findings.md`), `DUTY 30 2` refused, `STOP` back to idle. Felt as a short push, then limp. PSU flashed 0.368 A (estimate was ≈ 0.6 A; the display probably averages over more than the ~0.4 s pulse, not checked).
  - `invalid=0` in every run. PSU current ÷ duty ≈ 0.11–0.13 A, the motor's free-running current. Opening the serial port doesn't reset the C3 (`up_ms` kept counting). Positive duty counts negative on the encoder; the direction convention is D.4.
- Mistake caught: the user's "all checks work" meant only check 1 (meter); I had logged all six as passed before running any `DUTY`. Corrected before commit.
- Branches cleaned up (user request): `phase1/b-console`, `tools/plot-window`, `phase1/c-encoder` and `phase1/d-drive` deleted locally and on GitHub after checking each tip against its merged PR's head. `imgbot` (open PR #4) left for the user.
- Item D.3 on `phase1/d-speed`, PR #7 (user choices: the drive task sends the rows; new component `tiny_speed`; columns `t_ms,mode,duty_permille,counts,rpm_milli`). `tiny_speed_rpm_milli()` (8 host tests, 7 mutants), one row per 10 ms period pushed after `apply()`, sine sampler removed. Host tests 5/5, mutants 34/34, CI green. Bench check before merge this time (user choice), firmware `33b6f45`:
  1. Idle, 2 s: 201 rows, every gap 10 ms, all zero, `drops=0`.
  2. `DUTY 30`, 7 s: 702 rows, gaps 10 ms, `drops=0`; ramp 50 → 300 ‰ in 50 ‰ steps, 494 hold periods (499 run steps), same ramp down, then idle; hold counts −26 / −27 (−36.6 rpm); coasting visible in the first idle rows.
  3. Stall (body and wheel held), `DUTY 50 3`: ≈ 9° wind-up in the first 150 ms, then 30 periods at ≤ 1 count, `mode` 2 and duty 0 at 450 ms; swing back after the cut. `STOP` cleared it.
  Logs: `logs/20261031-221759-d3-idle.*`, `logs/20261031-222102-d3-duty30.*`, `logs/20261031-223625-d3-stall.*`.

## Session: 2026-11-01
- PR #7 reviewed (firmware and `tiny_speed` diff): no blocking issues. Two notes: `ENC ZERO` with telemetry on gives one false `counts`/`rpm_milli` row (README line added on the branch, `14d246c`); `rpm_milli` assumes a 10 ms period (a late period would show in `t_ms`, not seen on the bench). The user merged it as `5da32d8`.
- `main` built and flashed: `STATUS` → `ver=5da32d8 telem=off drops=0 drive=idle duty=0`, `ENC` → `pos=0 invalid=0`. No motor run (code unchanged since the bench check).
- D.4.1 direction convention: `ENC ZERO`, `DUTY 20 2`, `ENC`, 5 s pause, `ENC ZERO`, `DUTY -20 2`, `ENC`, run twice (the first time the user read the PSU, 0.023 A both ways, because my message asked for it alongside the wheel; the second time they watched the wheel). +20 % → clockwise seen from the wheel's outer side, −3304 / −3299 counts; −20 % → counter-clockwise, +3284 / +3288; invalid 0. Details in `findings.md`.
- D.4.2 speed vs duty: `ENC ZERO`, `TELEM ON`, then `DUTY d 3` for d = 5, 10, 15, 20, 30…80 %, then the same negative, 1 s gaps, no stall trip; `ENC` after: `pos=-51 invalid=0`, `drops=0`. Linear from 10 to 80 % (≈ 1.367 rpm/%, 105 rpm at 80 %), 5 % doesn't start. Log `logs/20261101-123741-d4-sweep.*`, table in `findings.md`.
- D.4.2 repeated at 30 s per step (user request): + duty matches the 3 s sweep within 0.4 rpm; − duty (run second, after 5 min) 0.3–2 % faster, probably warm-up, not separated yet. 5 % never started. `pos=13807 invalid=0`, `drops=0`: D.4.3 covered at 80 %. Log `logs/20261101-124339-d4-sweep30.*`.
- D.4.2 warm-up check (user request): alternating ±40 %, 10 s each, 24 pairs from cold, PSU current read by camera (first use; `ffmpeg` one frame per run, `logs/` is git-ignored). Both directions sped up ≈ 1 % while the current fell 0.045 → 0.043 A: warm-up. − duty 0.4–0.7 % faster: small direction offset. `pos=4042 invalid=0`, `drops=0`. Log `logs/20261101-135018-d4-alt.*`.
- D.4 firmware on branch `phase1/d-measure` (user choices: probe on GPIO1, `PROBE ON|OFF` and `DECAY SLOW|FAST` as console commands). `tiny_motor_pins()` takes a decay mode (19 host tests, 18 mutants, all killed); `drive.c` keeps the mode under `s_lock` and refuses changes while running; the encoder ISR sets/clears GPIO1 by register write when the probe is on (still in IRAM, no new calls). First flash: `PROBE OFF` logged "conflict found for GPIO[1]" (IDF reserves an output pin; re-`gpio_config` warns); fixed with `gpio_reset_pin()` + `gpio_pullup_dis()`.
- Bench check of the branch (PSU 7.4 V, wheel free, photos of the PSU):
  1. First script: fast decay `DUTY 30 3` didn't move the wheel (0 counts), the stall trip latched as designed, and **my script had no conditional `STOP`**, so the next two runs were refused (`ERR stall fault latched`). Cleared with `STOP`; the later scripts send `STOP` when a stall row appears.
  2. Fast decay `DUTY 50 2`: no movement either, stall trip, `STOP`. Fast `DUTY 80 2`: runs, −91.9 rpm, same direction as slow decay. So the mapping is right and fast decay needs far more duty to start (a D.4.5 result, not a bug).
  3. Slow `DUTY 30 3` → −37.2 rpm, `DECAY FAST` mid-run → `ERR motor running`; fast `DUTY 80 3` → −92.5 rpm, `DECAY SLOW` mid-run refused; fast `DUTY -80 3` → +95.0 rpm. Invalid 0 throughout, drops 0. PSU 0.075 A at fast −80 % (slow 80 % was 0.104 A on 2026-10-31). Two photos read 0.000 A because the camera takes ≈ 3 s from launch to frame, so they landed after the 3 s runs ended (file times checked). Logs `logs/20261101-1413*-d4-decay-*`, `logs/20261101-141502-d4-decay-check2*`.
- PR #8 opened for `phase1/d-measure`; CI: all 3 jobs pass.
- D.4.4 interrupt load: the analyzer didn't enumerate at first (LED on, absent from `lsusb`); after the user opened it in PulseView it showed up as 0925:3881, and PulseView had to be closed before `sigrok-cli` could claim it. 24 MHz streams only 21 ms; 12 MHz works. Captures at 30 %, 80 % and 80 % with telemetry, probe on: handler 1.0 µs, entry median 2.1 µs / max 8.8 µs, ≥ 2.4 % CPU at 80 %; shortest real edge gap 14.4 µs (uneven A/B phasing); sub-µs edge bounce, harmless. `ENC` after each: invalid 0; `drops=0`. Details in `findings.md`.
- D.4.5 decay curves (downward sweeps, 80 → 5 %, 4 s per step, PSU photographed each step): fast decay 93 rpm at 80 %, 9 rpm at 50 %, stall trip at 40 % (crawling ≈ 1.7 rpm); slow decay linear 106 → 2.3 rpm, still turning at 5 %. invalid 0 both runs, drops 0. Recommended to keep slow decay (user to decide). Log `logs/20261101-151644-d4-down.*`.
- PR #8 review: no code findings. The `STATUS` reply can reach 122/128 bytes, which the user noted for item E (see the phase plan, E.2). Pushed `15aa9d5` to the branch, Markdown only: new `firmware/wheel_node/COMMANDS.md` (console usage with real replies from this session and a `HELP` read from the board), the measured decay curves in the README. PR description updated with the D.4.4 and D.4.5 results. CI wasn't rerun or waited for (Markdown only, the user's call).
- Decay decision: the user chose slow decay (the default); recorded in the Decisions table.
- PR #8 merged (`8db685b`), including `15aa9d5` (COMMANDS.md, README decay curves). The planning updates for the decay decision were still uncommitted at merge time; they go to `main` directly anyway, so nothing was lost. Committed after the merge.
- `main` (`3e2a542`) built with no warnings and flashed: `STATUS` → `ver=3e2a542 … drive=idle duty=0 decay=slow probe=off`, `HELP` lists `DECAY` and `PROBE`, `ENC` → `pos=0 invalid=0`. PR #8 was a squash merge; the branch head `15aa9d5` has no differences from `main` outside `.planning/`. Branch `phase1/d-measure` deleted locally and on GitHub.

## Session: 2026-11-02
- Parts: the wiring order of 2026-10-16 arrived (user): JST-XH pigtails with male and female housings, 22 AWG solid core in several colors, 20 AWG stranded red + black, assorted WAGO connectors; all as ordered. Parts list and plan updated; only the 470 µF caps (Phase 5) are still missing.
- PR #9 review started (user): `tiny_pi` (float kept, user decision over fixed point), `test_tiny_pi.c`, the `tiny_motor` change and mutants reviewed; `drive.c` finding: the controller's soft-float math ran inside `s_lock`, which masks the encoder interrupt. Measured with a temporary cycle-counter build (longest locked section): idle 8 µs, `DUTY 80` 15 µs, `SPEED 100` 50 µs; invalid 0. Fixed (user: measure and fix, CAN will add interrupt load): controller stepped on copies outside the lock, written back only if no command came in (`s_cmd_seq`). After: idle 2.3 µs, `DUTY 80` 6.6 µs, `SPEED 100` 8.8 µs (+1.1 µs copy), control identical (t90 180 ms, mean 100.0 rpm), invalid 0. Also `test_changing_ki_keeps_the_output` (17 tiny_pi tests) and a comment value fixed. Committed `95325a9` on the branch, pushed; board flashed with it, STATUS ok. Not done: rendering the edited Mermaid diagram (podman call blocked by the permission check). Remaining review files: `console.c`, `encoder.c`, `telemetry.c/.h`, docs.

## Session: 2026-11-03

- PR #9 review finished. Mermaid: all six diagrams in the wheel-node README rendered with mermaid-cli (user added a permission rule for `podman run ... minlag/mermaid-cli`) and checked; two layout fixes (state diagram: the per-period controller step is a note, not a second RUN self-loop; SPEED sequence: "under s_lock" on the arrows). `console.c`: no defects; user decision: `SET` rejects gains with more than 3 decimals so the reply matches the stored value (`tiny_cmd_parse_float` takes a decimals limit, plain notation only, so `1e-4` can't slip through; new host test, 23 tiny_cmd tests). `encoder.c`, `telemetry.c/.h`: no defects; no doc still has the old encoder sign. Docs reread: `COMMANDS.md` safety list was split by a paragraph (fixed), example date line updated. Committed `7b9e1f0` on the branch, pushed; board flashed, `STATUS ver=7b9e1f0`, gains at boot values, invalid 0.
- Open: the `SPEED 0` sentences in `COMMANDS.md` and the wheel-node README ("ramps down and coasts", "never holds the wheel still") were never bench-checked, and the code suggests otherwise. During a run, setpoint 0 makes the controller drive against the motion (kp × −rpm, reverse duty) and the integral can then push the wheel backwards; the run ends only when target and duty are both exactly 0. Needs a short bench run before the text is corrected.
- `SPEED 0` checked on the bench (`logs/20261103-150247-speed0.csv`, PSU photos): `SPEED 60 6`, `SPEED 0` after 2 s. Brakes with reverse duty (min −128 ‰), standstill in ≈ 120 ms, 43 counts backwards over 0.5 s, then held still at −57 ‰ (11 mA) until the run's time was up; invalid 0. From standstill it ends at once (`drive=idle` right after the reply). The small hold at standstill is accepted (user, 2026-11-03: "if it's small it doesn't matter"). Docs corrected; new host test `test_setpoint_zero_brakes_to_rest` (18 tiny_pi tests) and mutant 13 (setpoint 0 means off), killed only by that test. Committed on the branch, pushed.
- PR #9 merged by the user as `b37cf25`. `main` flashed: `STATUS ver=b37cf25`, idle, gains kp 6 / ki 20 / ff 7.3, `pos=0 invalid=0`. Branch `phase1/e-pi` deleted (local and remote). Next: the live TUI (own branch and PR).
- Live TUI planned and built (plan `tool-live-tui.md`, approved; user decisions: Textual + plotext 6 directly, not textual-plotext, which is unmaintained since 2024-11 and caps plotext below 6; dusk theme; `tools/nodelink.py`; CI pytest job; `tools/README.md` in the same PR; no recording for now). plotext 6 has a new object API (`figure.signal(...).lines()`, `figure.draw`, `ruler(...).lim/ticks`) and caps plots at the detected terminal size unless `terminal.limit(False, False)`. Branch `tools/live-tui`, `d1e206b`, PR #10 open. 18 pytest tests (fake port, headless app); 3 of 4 hand mutants of live.py caught (Esc priority can't be observed today). Bench: no-motor and `SPEED 60 5` + Esc checks pass; the user tried it by hand ("works great"), screenshot in `tools/README.md`.
- Side finding: with telemetry left on and nobody reading the port, the node's USB TX buffer fills and the driver drops output, so a freshly started `send_cmd.py` can miss its first reply (the command still runs). Not a splice of replies into rows: 90/90 replies arrived while the port was read continuously.
- PR #10 merged (`506fb26`), branch deleted; no firmware change, so nothing to flash. Phase 1 closed (user, 2026-11-03): full-PWM no-load speed noted as not measurable (80 % cap, 105 rpm at the cap); left/right direction convention moved to Phase 3. Next: Mermaid diagrams, then the Phase 2 planning pass.
- Mermaid: the 3 text diagrams in `docs/architecture.md` (power, control/data, wheel node) redrawn and rendered; the wheel node gained the STBY line it was missing. The 6 wheel-README diagrams re-rendered, unchanged. Mermaid ignores a subgraph's `direction` when its nodes link outside it; linking to the subgraph itself keeps it. Branch `docs/mermaid`, PR #11.
- PR #11 merged (`b1b451c`), branch deleted. Session ends here (user clears context); next session starts the Phase 2 planning pass.

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
| C.3 flash C3 (2026-10-17) | `idf.py -p /dev/ttyACM0 flash`, then reset and read the console | Hash verified; banner + heartbeat; LED blinks at 1 Hz | All 3 images verified; banner (ESP32-C3 rev v0.4, IDF v6.1) and heartbeat every 5 s; LED on GPIO8 blinks | pass |
| C.4 flash ESP32-CAM (2026-10-17) | `idf.py -p /dev/ttyUSB0 flash` via the expansion board, then reset and read the console | Hash verified; banner + heartbeat; red LED blinks, flash LED off | All 3 images verified; banner (ESP32 rev v3.1, 2 cores, IDF v6.1) and heartbeat; red LED on GPIO33 blinks (glow only, LED faces the expansion board); flash LED off | pass |
| D CI build (2026-10-17) | PR #1: GitHub Actions, `espressif/idf:v6.1`, matrix over both projects | Both jobs pass | `build (wheel_node)` pass 2m0s, `build (gateway)` pass 1m55s; `Project build complete`, no warnings | pass |
| F.4 encoder 3.3 V check, spare motor (2026-10-16) | PSU 3.3 V / 0.1 A limit to V/GND; analyzer D0 = A, D1 = B at 1 MHz; hand-turned both ways; Gray code decoder | Clean quadrature; count up one way, down the other; high 2.5–3.3 V, low ≤ 0.8 V | 3 mA supply; two clean square waves, no pull-ups needed; count 0→12 then back to 0 (Increment +1 / −1); one turn of the encoder disc = 47 counts (≈ 48 = 12 PPR × 4); multimeter: A and B high 3.28 V, low 0 V | pass on the spare motor; other 4 skipped (decided 2026-10-16) |
| F.1 teardown (2026-10-14) | Old car: photos, then remove drive board and battery holder | Chassis clear for new motors | Done; photos in `docs/images/`. Old MCU+camera board and BT antenna left on for now | pass |
| B host tests (2026-10-20) | `ctest` on `test/host`, locally (sanitizers off) and in CI (sanitizers on) | All pass | 33/33 locally; CI `host-tests` pass in 7 s | pass |
| B console (2026-10-20) | `STATUS`, `HELP`, unknown command, wrong arg count, bad value, 120-char line, blank line over USB | One OK/ERR line each; blank gets no reply | All as expected; 120 chars → `ERR line too long` | pass |
| B telemetry (2026-10-20) | `plot_run.py --seconds 5` | 100 Hz sine ±1000, no drops | 501 rows in 5.00 s, every gap 10 ms, ±1000, drops=0 | pass |
| B no reader (2026-10-20) | Telemetry on, port closed 5 s, then `STATUS` | Console still responsive | Reply in 0.05 s, drops=0 (driver discards the bytes itself) | pass |
| B no reset on open (2026-10-20) | Repeated port opens with DTR high / RTS low | `up_ms` keeps counting | Uptime kept counting to ~7 h; with both lines low, it reset (`rst:0x15`) | pass after fix |
| B plot window, before fix (2026-10-20) | `plot_run.py --seconds 3` (window expected) | Plot window opens | Capture, CSV and PNG fine (301 rows, 100 Hz); no window, matplotlib `FigureCanvasAgg is non-interactive` warning | fail (no GUI backend) |
| B plot window, after fix (2026-10-20) | Same, with `PyQt6==6.11.0` in the venv; also a run without it | Window opens; without a backend, a clear message | Backend `qtagg`; the user saw the window open. Without PyQt6: `no GUI backend for matplotlib (agg), so no window; the PNG is saved...` | pass |
| A.1 STBY pull, DRV8833 module (2026-10-21) | Unpowered, multimeter on the bare pads: STBY–GND and STBY–VM on the 2 MΩ and 200 kΩ ranges, probes both ways | A pull-down (or none); no pull-up to VM | STBY–GND: 222 kΩ both ways on 2 MΩ; 123.1 kΩ / OL on 200 kΩ (a junction in the path). STBY–VM: climbs (charging the 10 µF VM cap), settles at 1.81 MΩ with red on STBY / black on VM, OL the other way: one-way, so not a resistor | pass (pulled down, no pull-up; powered check in A.5) |
| DRV8833 header joints (2026-10-22) | Unpowered, multimeter 200 Ω range (beeper not working); probes shorted = ~0.02 Ω, unstable | Neighbors OL; all 4 GND pins joined; VM–GND not shorted | Every neighbor pair OL; GND–GND same as probes shorted; VM–GND OL | pass |
| A.4 pre-power checks (2026-10-24) | Unpowered, PSU leads and USB disconnected, 200 Ω range | C3 GND–module GND ≈ 0; 3V3–VM OL; 3V3–AO1 OL (reds not swapped); AO1–AO2 ≈ motor winding; GPIO5–AIN1, GPIO7–AIN2, GPIO10–STBY ≈ 0; VM–GND OL | 0.05 Ω; OL; OL; first wandering then OL (motor lead coupler loose), after re-seating 4.2 Ω (motor alone 3.2 Ω at its cable, so ~1 Ω in couplers and breadboard contacts); ≈ 0.05 Ω each; OL | pass after fix |
| A.5 first power-up (2026-10-25) | PSU 7.4 V / 1.5 A, C3 on USB; meter 20 V DC, black on the GND rail; item B firmware, then download mode (BOOT held through RST, confirmed by esptool) | STBY, AIN1, AIN2 ≈ 0 V; motor still; current a few mA at most | Both states: 0 V on all three; motor still; 0.000 A; VM–GND 7.4 V | pass |
| D.1–D.2 bench check (2026-10-31) | Firmware `39c1e68`, PSU 7.4 V / 1.5 A; the six checks in PR #6 via `send_cmd.py` (check 5 for 3 s, check 6 as `DUTY 50 3`) | Idle 0 V on STBY/AIN1/AIN2; ±30 % runs ramp down; `STOP` cuts at once; 100 % capped to 80; held motor trips `drive=stall`, latched until `STOP` | All as expected; `invalid=0` throughout. PSU: 0.032 A at 30 %, 0.059 A at 50 %, 0.104 A at 80 % (0.116 A at start), 0.368 A flash at the stall trip. ≈ 36 / 67 / 100 wheel rpm at 30 / 50 / 80 %. VM–GND 7.41 V | pass |
| D.3 telemetry bench check (2026-10-31, PR #7) | Firmware `33b6f45`, PSU 7.4 V / 1.5 A; `plot_run.py` idle 2 s, `DUTY 30` 7 s, `DUTY 50 3` stall with body and wheel held | One row per 10 ms, columns as in the header; ramp/hold/ramp-down visible; stall → `mode` 2, duty 0 | All rows 10 ms apart, `drops=0`; hold at 30 % −26/−27 counts (−36.6 rpm); stall trip at 450 ms after ≈ 9° wind-up | pass |
| D.3 after merge (2026-11-01) | `main` at `5da32d8` flashed; `send_cmd.py STATUS ENC` | Boots, version matches, idle | `ver=5da32d8`, `drive=idle duty=0`, `drops=0`, `pos=0 invalid=0` | Pass |
| D.4.1 direction (2026-11-01) | `main` `5da32d8`, PSU 7.4 V / 1.5 A, wheel free; `DUTY ±20 2` with `ENC ZERO`/`ENC`, twice | Opposite signs and directions for ± duty, invalid 0 | +20 % clockwise (outer side), −3304/−3299; −20 % counter-clockwise, +3284/+3288; invalid 0; 0.023 A | Pass |
| D.4.2 speed vs duty (2026-11-01) | `main` `5da32d8`, PSU 7.4 V / 1.5 A, wheel free; `DUTY ±d 3`, d = 5–80 %, telemetry logged | A monotonic curve, symmetric both ways | 0 rpm at 5 %, 9.4 at 10 %, 105 at 80 %, linear (max 0.23 rpm off), ± within 0.2 rpm; invalid 0, drops 0 | Pass |
| D.4.2 speed vs duty, 30 s per step (2026-11-01) | as above, `DUTY ±d 30`, ≈ 10.5 min continuous | Same curve, tighter means | + duty 1.370 rpm/%, 105.4 rpm at 80 %; − duty 0.3–2 % faster (likely warm-up); 5 % doesn't start; invalid 0, drops 0 | Pass (direction vs warm-up open) |
| D.4.3 invalid at full speed (2026-11-01) | Covered by the 30 s sweep: 30 s at each of ±80 % | invalid stays 0 | `invalid=0` after ≈ 450 k edges at 80 % | Pass |
| D.4.2 warm-up vs direction (2026-11-01) | Cold start, `DUTY ±40 10` alternating ×24, camera photo of the PSU per run | Warm-up: both directions drift together; direction: constant offset | + 50.52 → 51.06 rpm, − 50.89 → 51.27; current 0.045 → 0.043 A both ways; invalid 0 | Pass: mostly warm-up, direction < 1 % |
| D.4 firmware bench check (2026-11-01, branch `phase1/d-measure`) | `DECAY`/`PROBE` commands, slow +30 %, fast ±80 %, mid-run decay change, PSU photos | Both modes drive the right way; mid-run change refused | slow +30 % −37.2 rpm; fast +80 % −92.5, −80 % +95.0; fast 30 % and 50 % don't start (stall trip); mid-run `DECAY` → ERR; invalid 0 | Pass |
| D.4.4 interrupt load (2026-11-01, branch `phase1/d-measure`) | `PROBE ON`, analyzer D0 A / D1 B / D2 GPIO1 at 12 MHz, 0.5 s at `DUTY 30`, `80`, `80` + telemetry | Load and latency measured; every edge handled | body 1.0 µs, entry median 2.1 / max 8.8 µs, ≥ 2.4 % CPU at 80 %; min real edge gap 14.4 µs (margin ≈ 1.5×); bounce 83–500 ns harmless; invalid 0 | Pass |
| D.4.5 fast vs slow decay (2026-11-01, branch `phase1/d-measure`) | Downward sweep 80 → 5 % while turning, each mode, PSU photos | Both curves; a decay choice | fast: 93/62/33/9 rpm at 80/70/60/50 %, stall at 40 %; slow: linear 106 → 2.3 rpm; invalid 0 | Pass |
| E sign (2026-11-01, branch `phase1/e-pi`) | PSU 7.4 V / 1.5 A, wheel free; `ENC ZERO`, `DUTY 20 2`, `ENC`, telemetry recorded | Positive count and rpm | `pos=3320`, invalid 0; rpm 0 → +25 | Pass |
| E `SPEED ±60 6` (2026-11-01) | Boot gains kp 6, ki 20, ff 7.3; telemetry; PSU photos | Reaches and holds ±60 rpm | 90 % at 120 ms, peak 65.3 / −63.9, then 59.9..60.6 rpm (100 ms averages); 53–54 mA | Pass |
| E step 30 → 80 rpm (2026-11-01) | `SPEED 30 6`, `SPEED 80 3` after 3 s | Small overshoot, settles | 90 % at 90 ms, peak 81.9, settled mean 80.17 (1.0–2.9 s after the step); no tuning needed | Pass |
| E exit test, finger as load (2026-11-01) | `SPEED 60 15`, the user's finger on the wheel | Dips, recovers while loaded | First try (hard, 1.5 s): duty to 798 ‰, speed back only to ≈ 55, 73 rpm after release. Second (light, 5 s): dip to ≈ 53, back to 59–61 while loaded, duty 460 → 590 ‰, 66 rpm after release, settled in ≈ 1.5 s | Pass (second try) |
| E stall during `SPEED` (2026-11-01) | `SPEED 60 10`, the user grips the wheel; script sends STOP on a mode-2 row | Stall trip fires, STOP | Duty 478 → 800 ‰ within ≈ 80 ms as the wheel slowed, trip exactly 300 ms after it stopped, `OK stopped`, `STATUS` idle; the wheel sprang back slightly after the cut | Pass |
| E console replies (2026-11-01) | `HELP`, `HELP SPEED/SET/GAINS`, `GAINS`, `SET` valid / out of range / bad name, `SPEED 150`, `SPEED 60 40` | One OK/ERR each as designed | All as in `COMMANDS.md` | Pass |
| E `SPEED 0` mid-run (2026-11-03, `7b9e1f0`) | PSU 7.4 V / 1.5 A, wheel free; `SPEED 60 6`, `SPEED 0 6` after 2 s; telemetry, PSU photos; then `SPEED 0 3` from standstill | Behaviour recorded to correct the docs | Brakes (min −128 ‰), stop in ≈ 120 ms, 43 counts back, held at −57 ‰ until time up; from standstill idle at once; invalid 0 | Recorded (docs corrected) |
| E after merge (2026-11-03) | `main` at `b37cf25` flashed; `send_cmd.py STATUS GAINS ENC` | Boots, version matches, idle | `ver=b37cf25`, `drive=idle`, boot gains, `pos=0 invalid=0` | Pass |
| Live TUI, no motor (2026-11-03, branch `tools/live-tui`) | Headless run on the real port: start, `STATUS`, Esc, quit; then `send_cmd.py STATUS` | 100 rows/s, poll replies hidden, Esc → STOP, quit → TELEM OFF, no reset | All as expected; `up_ms` continuous | Pass |
| Live TUI, `SPEED 60 5` (2026-11-03) | Typed in the TUI, Esc after ≈ 1.2 s; PSU 7.4 V / 1.5 A, wheel free | Live status follows the run; Esc stops it | 59.7–61.1 rpm at ≈ 47 % duty; idle on the next update after Esc; invalid 0, drops 0; PSU photo missed the run (taken after the stop) | Pass |

## Error Log
| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
|           |       | 1       |            |
