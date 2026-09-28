# Phase 0: Foundations — Detailed Plan

Status: **approved 2026-09-28**; work item A in progress

## Goal and exit criterion
Toolchain, repo skeleton and hardware are ready for Phase 1.
**Exit:** from a clean clone, one repo builds and flashes a hello-world onto an ESP32-C3 and onto the ESP32-CAM, and CI builds both on push.

## Decisions for this phase
| Decision | Rationale |
|---|---|
| **ESP-IDF v6.1** is pinned, allowing 6.1.x patches | Current release, so no 5→6 migration mid-project. The new TWAI driver API is identical to 5.5 (see findings.md). |
| **Native install** per machine, plus a build-time version check | Simplest flash/monitor loop, no USB passthrough. The check turns version drift across machines into an immediate build error. |
| Version check via the **component manager**: `dependencies: idf: "~6.1.0"` in each project's `main/idf_component.yml` | The idiomatic mechanism (esp32-camera uses the same field). No custom CMake needed. |
| **Two ESP-IDF projects**, `firmware/wheel_node` and `firmware/gateway`, plus a shared `components/` directory | They are different chips with different apps. Each project locks its own target. Shared code (later: `protocol`) lives once in `components/` and is pulled in via `EXTRA_COMPONENT_DIRS`. |
| Target locked per project via `CONFIG_IDF_TARGET` in `sdkconfig.defaults` | Verified: IDF v6.1 `tools/cmake/targets.cmake` reads the target from `sdkconfig.defaults`, so a fresh clone builds the right chip without running `set-target`. |
| Commit `sdkconfig.defaults` and `dependencies.lock`; gitignore `sdkconfig`, `sdkconfig.old`, `build/`, `managed_components/` | The defaults hold intent, the lock file makes managed components reproducible, and the rest is generated. |
| **CI:** GitHub Actions, `espressif/idf:v6.1` container (tag verified on Docker Hub), matrix over both projects, build only, no artifact upload | Free on public repos with standard runners. Artifact storage would count against quota. |
| A tiny shared component (`components/tiny_common`) used by both hello-worlds | Proves the shared-component plumbing in Phase 0 rather than discovering problems in Phase 2. It prints a boot banner with project, app version (IDF derives it from `git describe`), chip, and IDF version. |

## Planned repo layout
```
.github/workflows/build.yml
components/
  tiny_common/            # boot banner; later joined by `protocol`
firmware/
  wheel_node/             # target esp32c3
    CMakeLists.txt        # EXTRA_COMPONENT_DIRS → ../../components
    sdkconfig.defaults    # CONFIG_IDF_TARGET="esp32c3"
    main/ (main.c, CMakeLists.txt, idf_component.yml)
  gateway/                # target esp32 (ESP32-CAM)
    ...same shape...
docs/
  dev-setup.md            # install, build, flash, monitor on a new machine
```

## Work items

### A. Toolchain (this desktop, then any other machine)
1. Install IDF v6.1 next to the existing ones: `git clone -b v6.1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v6.1`, then `./install.sh esp32,esp32c3`.
2. Leave `~/esp/esp-idf-v5.5.5` and the `master` checkout untouched.
3. Add a shell helper (for example `get_idf61`) that sources `~/esp/esp-idf-v6.1/export.sh`.
4. Write `docs/dev-setup.md` with these steps, plus build/flash/monitor commands and the serial-port permissions note (`dialout` group).

### B. Repo skeleton
1. Create both projects, `components/tiny_common`, and `.gitignore` additions per the layout above.
2. `wheel_node` hello-world: banner + blink the onboard LED at 1 Hz + a heartbeat log line.
3. `gateway` hello-world: the same, using the ESP32-CAM's small red LED, not the bright flash LED.
4. Add a single "Development" link in `README.md` pointing to `docs/dev-setup.md`. This is a one-time change; the README stays stable afterwards.

### C. Build + flash both targets
1. Build both projects with v6.1 from a clean clone.
2. **Negative test:** building with v5.5.5 must fail on the IDF version constraint.
3. Flash + monitor a C3 through its native USB (USB-Serial-JTAG).
4. Flash + monitor the ESP32-CAM through the expansion board (USB-UART, auto-reset via DTR/RTS).

### D. CI
1. Write `.github/workflows/build.yml`: `container: espressif/idf:v6.1`, a matrix over `[wheel_node, gateway]`, and `idf.py build`.
2. Push and confirm both jobs pass.

### E. Hardware bring-up (as parts arrive; not a blocker for the exit criterion)
1. **ESP32-C3 boards:** flash the hello-world on each of the 4 plus spares. Label the physical boards (FL/FR/RL/RR/spare) and record which ones fail.
2. **CAN adapters (SN65HVD230):** unpowered, measure CANH–CANL resistance on each. About 120 Ω means onboard termination is fitted. Record which boards have it; only the two end nodes should keep it.
3. **MP1584 bucks:** before connecting anything, measure the output at no load, then with a load resistor. Confirm 5.0 V ±0.1 V, and adjust if the modules are trimmable.
4. **18650 cells (when bought):** measure each cell before inserting (matched pair within ~0.05 V). Do one charge cycle on the 2S board and confirm about 8.4 V at the end.
5. Log every measurement in `progress.md` under Test Results.

## Unverified (check during implementation)
- The C3 Super Mini onboard LED is believed to be on GPIO8, active-low.
- The ESP32-CAM (AI-Thinker) red LED is believed to be on GPIO33, active-low; the flash LED is on GPIO4.
- The `~6.1.0` constraint syntax in `idf_component.yml`, and whether it makes the build hard-fail rather than warn. Test C.2 checks this.
- Some C3 Super Minis need BOOT held during reset for the first flash. Some ESP32-CAM-MB boards need IO0 held if auto-reset fails.
- The exact package format of the `espressif/idf` container when used as a GitHub Actions `container:` (whether it needs `export.sh` sourced in the `run` step).

## Out of scope for Phase 0
Motor, encoder, CAN traffic and any protocol code (Phases 1–3). Pin allocation beyond the onboard LEDs.
