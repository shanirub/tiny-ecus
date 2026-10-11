# Phase 0: Foundations — Detailed Plan

Status: **approved 2026-09-28**; A done, B next. Parts arrived 2026-10-10 except the 18650 cells (expected the week of 2026-10-12); bench PSU in the meantime

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
| Chassis teardown, motor swap and motor/encoder bench tests are pulled forward into Phase 0 as item F (added 2026-10-11) | Mechanical fit (mounts, shafts, battery space) is the riskiest hardware unknown, and a reorder takes weeks. Only mechanical work happens now: no electronics are wired into the chassis until Phases 1–3 settle pinout and harness. |
| Of the 5 encoder motors, 4 go in the chassis and the spare stays on the bench for Phase 1 | Phase 1 needs one easily reachable motor; the chassis motors don't have to be removed again. |
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
1. Install IDF v6.1 next to the existing ones: `git clone -b v6.1 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v6.1`, then `./install.sh esp32,esp32c3`. (Done as a shallow clone.)
2. Leave `~/esp/esp-idf-v5.5.5` and the `master` checkout untouched.
3. Add a shell helper that sources `~/esp/esp-idf-v6.1/export.sh`. (Done as `alias idf61=...` in `~/.zshrc`.)
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
3. **MP1584 bucks:** feed from the bench PSU at ~7.4–8.4 V with a current limit (~0.5 A) until the cells arrive. Before connecting anything, measure the output at no load, then with a load resistor. Confirm 5.0 V ±0.1 V, and adjust if the modules are trimmable.
4. **18650 cells (when bought):** measure each cell before inserting (matched pair within ~0.05 V). Do one charge cycle on the 2S board and confirm about 8.4 V at the end.
5. Log every measurement in `progress.md` under Test Results.

### F. Chassis prep + motor/encoder bench tests (added 2026-10-11; no firmware needed)
Not a blocker for the Phase 0 exit. F.2–F.4 on the spare motor must be done before Phase 1 starts. Log every result in `progress.md` under Test Results.
1. **Teardown of the old car.**
   - Take the AA batteries out first.
   - Before desoldering anything, photograph and note the old wiring: motor polarity per side, and any suppression caps across the motor terminals.
   - Remove the old motor drive board and the AA holder.
   - Keep the old motors and board in a labeled bag, as a reference and a fallback.
2. **Motor bench test, all 5 new motors.**
   - Label them M1–M5.
   - Run each from the bench PSU at about 6 V, current limit about 1 A (the spec stall current is 0.82 A), in both directions. Use short runs and don't hold a stall.
   - Record the no-load current per motor and note any grinding or roughness.
   - First measure the winding resistance R of each motor: zero the multimeter leads, then take the lowest reading over several shaft positions. The stall current is about V / R. The listing doesn't say which voltage the 0.82 A stall rating is for: R ≈ 7 Ω means about 6 V, R ≈ 15 Ω means about 12 V.
   - Also do a short run at 8.4 V, the full-charge 2S voltage the motors will see through the DRV8833.
   - Pick the 4 best for the chassis; the remaining one is the bench spare.
3. **Encoder pinout.**
   - Identify the 6-wire cable pinout (M+, M−, encoder VCC, GND, A, B) from the seller listing and the encoder PCB silkscreen; these cables aren't standardized.
   - Confirm VCC and GND with the multimeter before powering.
   - Record the pinout in `findings.md`.
4. **Encoder signal check**, on each motor before it is mounted.
   - Power the encoder at 3.3 V, current limit about 0.1 A, with the motor leads unpowered.
   - Connect A and B to the logic analyzer (PulseView) and turn the output shaft by hand.
   - Expect two clean square waves 90° apart (quadrature). Reversing the direction swaps which one leads.
   - Check the high level is ≤ 3.3 V; the C3 GPIOs are not 5 V tolerant. If the edges are missing or slow, the outputs may be open-collector and need pull-ups: note it.
   - On the bench spare, count the edges per output-shaft revolution (mark the shaft and turn it exactly once). This is the counts-per-rev figure Phase 1 needs for speed calculation. Record it.
   - Expected, if the listing's "12 PPR" is per channel at the motor shaft: 1080 rising edges on A, or 4320 edges across A and B. A result of 12 or 48 means the PPR is quoted at the output shaft. See `findings.md`.
   - **Powered run, no MCU and no firmware.** Wiring:
     - Motor M+/M− from the bench PSU, current-limited.
     - Encoder VCC/GND at 3.3 V from a second source. Use a second PSU channel, or the 3V3 pin of any C3 board on USB (its firmware doesn't matter; no GPIOs are used). With a single channel, run motor and encoder together at 3.3 V: that still gives counts and direction, but not the maximum speed.
     - Logic analyzer channels on A and B, analyzer GND to encoder GND. The motor supply ground doesn't need tying in.
     - Sample at ≥ 1 MHz. Channel A is expected to run at a few kHz at 8.4 V; that's an estimate, since the real rpm is unknown.
   - **No-load speed.** At 6 V and at 8.4 V, measure f_A with the PulseView cursors or an edge count over a fixed window (sigrok's "timing" decoder may help; unverified). A multimeter Hz mode on A works too.
     - Motor rpm = f_A / 12 × 60; output rpm = motor rpm / 90.
     - The 8.4 V figure is the wheel's top speed, which caps the Phase 1 setpoint range. It also cross-checks the hand-turn counts-per-rev.
   - **Direction convention.** For each polarity (M+ to PSU + versus reversed), record which way the output shaft turns (seen from the shaft end) and which channel leads (A or B).
     - Left and right motors are mounted mirrored, so "forward" is opposite polarity on the two sides. Record which polarity and which leading channel mean forward for left-side and for right-side motors. Phase 1 firmware uses this.
5. **Mechanical fit check.**
   - Do the new motors fit the old mounts? Check the holes, the extra length from the encoder PCB, and clearance for the cable.
   - Do the metal D-shafts fit the wheels, and do the wheels stay on?
   - Does the 2S 18650 holder fit where the AA holder was?
   - Is there room for the KCD11 switch (10×15 mm cutout) and the fuse holder?
   - For any misfit, decide a workaround or a reorder.
6. **Mount the 4 chosen motors** as FL/FR/RL/RR and record which M-number went where.
   - Solder a 100 nF cap across each motor's terminals.
   - Leave the motor and encoder leads unterminated; don't wire them to drivers or nodes yet.
   - Fit the wheels.

**Done when:** 4 tested motors are mounted with their wheels on; all 5 encoders show clean quadrature at 3.3 V; the pinout, counts-per-rev, no-load speed at 6 V and 8.4 V, and the left/right direction convention are recorded; and every fit problem has a decided fix.

## Unverified (check during implementation)
- The C3 Super Mini onboard LED is believed to be on GPIO8, active-low.
- The ESP32-CAM (AI-Thinker) red LED is believed to be on GPIO33, active-low; the flash LED is on GPIO4.
- The new encoder motors fit the old chassis mounts and wheels (checked in F.5).
- The encoder outputs are push-pull at VCC level, with no pull-ups needed (checked in F.4).
- The `~6.1.0` constraint syntax in `idf_component.yml`, and whether it makes the build hard-fail rather than warn. Test C.2 checks this. `docs/dev-setup.md` already states it fails; fix that doc if C.2 shows otherwise.
- Some C3 Super Minis need BOOT held during reset for the first flash. Some ESP32-CAM-MB boards need IO0 held if auto-reset fails.
- The exact package format of the `espressif/idf` container when used as a GitHub Actions `container:` (whether it needs `export.sh` sourced in the `run` step).

## Out of scope for Phase 0
Motor, encoder, CAN traffic and any protocol code (Phases 1–3). Pin allocation beyond the onboard LEDs. Wiring any electronics into the chassis (item F covers only mechanical work and motor caps).
