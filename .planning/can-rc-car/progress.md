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
- **Status:** in_progress (plan approved 2026-09-28; A done, B next)
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

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| IDF v6.1 install | `. export.sh && idf.py --version` | `ESP-IDF v6.1` | `ESP-IDF v6.1`; xtensa-esp32-elf-gcc and riscv32-esp-elf-gcc esp-15.2.0_20251204; Python 3.14.7 | pass |
| E.2 CAN adapter termination (2026-10-14) | Unpowered, CANH–CANL resistance on each of the 5 SN65HVD230 adapters | ~120 Ω where termination is fitted | 118–120 Ω on all 5: every adapter has a 120 Ω terminator fitted | pass (fixed SMD terminators, no jumper; 3 to remove in Phase 3) |
| F.1 teardown (2026-10-14) | Old car: photos, then remove drive board and battery holder | Chassis clear for new motors | Done; photos in `docs/images/`. Old MCU+camera board and BT antenna left on for now | pass |

## Error Log
| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
|           |       | 1       |            |
