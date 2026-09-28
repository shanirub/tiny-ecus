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

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| IDF v6.1 install | `. export.sh && idf.py --version` | `ESP-IDF v6.1` | `ESP-IDF v6.1`; xtensa-esp32-elf-gcc and riscv32-esp-elf-gcc esp-15.2.0_20251204; Python 3.14.7 | pass |

## Error Log
| Timestamp | Error | Attempt | Resolution |
|-----------|-------|---------|------------|
|           |       | 1       |            |
