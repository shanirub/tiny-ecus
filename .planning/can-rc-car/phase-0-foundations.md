# Phase 0: Foundations — Detailed Plan

Status: **approved 2026-09-28**; A done, B next; E.2 done 2026-10-14. Parts arrived 2026-10-10 except the 18650 cells (expected the week of 2026-10-12); bench PSU in the meantime

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
2. **CAN adapters (SN65HVD230):** (done 2026-10-14: all 5 measure 118–120 Ω, so all have a terminator fitted. It is a fixed SMD part, not jumper-selectable; 3 come off in Phase 3.) Unpowered, measure CANH–CANL resistance on each. About 120 Ω means onboard termination is fitted. Record which boards have it; only the two end nodes should keep it.
3. **MP1584 bucks:** deferred (2026-10-14). Until Phases 3–5 every board runs from USB, so nothing connects to a buck output yet. Rule: check each buck before anything is connected to its output. MP1584 modules often ship with the trim pot at an arbitrary setting, and one set high would destroy a C3. Method: feed from the bench PSU at ~7.4–8.4 V with a current limit (~0.5 A). Measure the output at no load, then with a load resistor: 100 Ω draws 50 mA and dissipates 0.25 W, so use a ½ W part or two 220 Ω ¼ W in parallel. Confirm 5.0 V ±0.1 V; trim if the module allows it. About 2 minutes per module.
4. **18650 cells (when bought):** measure each cell before inserting (matched pair within ~0.05 V). Do one charge cycle on the 2S board and confirm about 8.4 V at the end.
5. Log every measurement in `progress.md` under Test Results.

### F. Chassis prep + motor/encoder bench tests (added 2026-10-11; no firmware needed)
Not a blocker for the Phase 0 exit. F.2–F.4 on the spare motor must be done before Phase 1 starts. Log every result in `progress.md` under Test Results.
Scope trimmed 2026-10-14: checks that firmware can do in Phase 1 moved there (counts per rev, no-load speed, direction convention). What stays here either protects hardware or must happen before mounting.

1. **Teardown of the old car.** (done 2026-10-14)
   - Before photos: `docs/images/before teardown up.webp` and `docs/images/before teardown bottom.webp`.
   - The old drive board (Keyes L298N V2, screw terminals) and the old battery holder are removed. The old MCU-with-camera board and its BT antenna are still on the chassis for now.
   - The two motors on each side appear to be wired in parallel through the centre hole. The photos don't show per-terminal polarity or suppression caps; not needed, since the motors are being replaced.
2. **Motor bench test, all 5 new motors.** (2 of 5 done 2026-10-14: both mounted motors, R ≈ 3.0 / 3.1 Ω, no-load 0.098 / 0.100 A at 6 V; see `progress.md`)
   - Label them M1–M5.
   - Measure the winding resistance R of each motor, unpowered: zero the multimeter leads, then take the lowest reading over several shaft positions. The stall current is about V / R. The listing doesn't say which voltage the 0.82 A stall rating is for: R ≈ 7 Ω means about 6 V, R ≈ 15 Ω means about 12 V. This is the only Phase 0 input to the current-budget risk.
   - Spin each briefly from the bench PSU at about 6 V, current limit about 1 A, one direction. Record the no-load current and note any grinding or roughness. Don't hold a stall.
   - Pick the 4 best for the chassis; the remaining one is the bench spare.
   - Reduced for the remaining 3 motors (2026-10-14): the first two matched closely, so ~3 Ω and ~0.1 A are taken as the model's typical values and their resistance measurement is skipped. Each still gets a quick 6 V spin before it is mounted, to catch a defective unit: expect about 0.1 A in CV, with no grinding. A clearly different current or sound makes that motor the bench spare.
   - Dropped (2026-10-14): the 8.4 V run and the second direction. Low value; Phase 1 runs the motor both ways at full PWM anyway.
3. **Encoder pinout.**
   - Identify the 6-wire cable pinout (M+, M−, encoder VCC, GND, A, B) from the seller listing and the encoder PCB silkscreen; these cables aren't standardized.
   - Confirm VCC and GND with the multimeter before powering.
   - Record the pinout in `findings.md`.
4. **Encoder 3.3 V check.** Some Hall sensors need more than 3.3 V; if these do, every wheel node needs level shifting or a different encoder supply, which changes the wiring plan.
   - Power the encoder at 3.3 V, current limit about 0.1 A, with the motor leads unpowered.
   - Connect A and B to the logic analyzer (PulseView) and turn the output shaft by hand.
   - Expect two clean square waves 90° apart (quadrature). Reversing the direction swaps which one leads.
   - Check the high level is ≤ 3.3 V; the C3 GPIOs are not 5 V tolerant. If the edges are missing or slow, the outputs may be open-collector and need pull-ups (try 10 kΩ to 3.3 V): note it.
   - One motor answers the 3.3 V question. Repeat it on the other 4 before mounting: that only saves taking a motor back out later, and takes about a minute each.
   - Moved to Phase 1 (2026-10-14): counts per output revolution, no-load speed and the left/right direction convention. Firmware measures them with PCNT. Optional, if you want a no-firmware reference to check that firmware against: hand-count edges per output revolution on the spare. Expect 1080 rising edges on A, or 4320 across A and B; 12 or 48 means the PPR is quoted at the output shaft (see `findings.md`).
5. **Mechanical fit check.** (motor fit passed 2026-10-14: the two mounted motors fit the old mounts with about 1 mm between one's encoder end and the next gearbox, and no rubbing at 6 V even when pressed together. Wheels, 2S holder, switch and fuse holder still to check.) The most likely F item to fail: from the bottom photo, the gap between each old motor's back end and the next motor's gearbox is roughly 10 mm (estimate, scaled from a ~70 mm TT motor). The encoder adds length at that end.
   - Do the new motors fit the old mounts? Check the holes, the extra length from the encoder PCB, and clearance for the cable.
   - Do the metal D-shafts fit the wheels, and do the wheels stay on?
   - Does the 2S 18650 holder fit where the AA holder was? (The old holder is removed; the new one hasn't been tried yet.)
   - Is there room for the KCD11 switch (10×15 mm cutout) and the fuse holder?
   - For any misfit, decide a workaround or a reorder.
6. **Mount the 4 chosen motors** as FL/FR/RL/RR and record which M-number went where.
   - Solder a 100 nF cap across each motor's terminals.
   - Leave the motor and encoder leads unterminated; don't wire them to drivers or nodes yet.
   - Fit the wheels.

**Done when:** 4 tested motors are mounted with their wheels on; all 5 encoders show clean quadrature at 3.3 V; the pinout is recorded, plus the winding resistance of 2 motors and each motor's 6 V no-load spin result; and every fit problem has a decided fix.

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
