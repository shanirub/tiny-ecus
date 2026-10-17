# Task Plan: CAN RC Car (tiny-ecus) — Project Roadmap

## Goal
A 4WD skid-steer RC car built as a distributed system: an ESP32-C3 node per wheel, all nodes talking over CAN, and an ESP32-CAM gateway. The car is driven remotely and fails safe. Learning comes first (see `docs/can-rc-car-project-overview.md`).

## Next Step
Phase 0 item D (CI), the first change done on a branch with a PR. Item C done 2026-10-17 (both boards flashed, both LEDs confirmed).
In parallel (no firmware needed): item F. Done so far: F.2 on all 5 (≈ 3 Ω on the first 2, ≈ 0.1 A no-load on all; none faulty), F.3 pinout (wire colors mapped 2026-10-15, same on all 5). Chassis switched 2026-10-15 to the wider 2-deck kit, so F.1, F.5 and F.6 restart on it. F.4 passed on the spare motor 2026-10-16 (A/B high 3.28 V, low 0 V; 47 counted per encoder-disc turn, ≈ 48 expected); the other 4 motors are skipped and only checked if one misbehaves while debugging. Next: F.1 teardown of the 2-deck chassis, then F.5 on it (flipped-mount test fit, middle gap, wheels, 2S holder, switch, fuse holder) and F.6. E.3 (bucks) is deferred until something first connects to a buck output.

## Current Phase
Phase 0: plan approved 2026-09-28 (`phase-0-foundations.md`); items A, B, C done (B 2026-10-16, C 2026-10-17), D next; parts arrived 2026-10-10 (cells on the way; wiring order of 2026-10-16 not arrived); E.2 done 2026-10-14; F.2 done 2026-10-15 (all 5 spun, none faulty); F.3 (pinout incl. wire colors) done 2026-10-15; F.4 done 2026-10-16 on the spare motor; chassis switched to the 2-deck kit 2026-10-15 (F.1/F.5/F.6 restart on it)

## Working method
- Plan top-down, one level at a time. Each phase gets its own detailed planning pass before any implementation starts.
- Each phase must end in a working, demonstrable state (the "Phased" principle). Its exit criterion is its acceptance test.

## System architecture (summary)
```
 operator ──UART (text cmds)──► ESP32-CAM gateway ──WiFi──► video (data plane, later)
            (later: TCP socket)        │
                                  TWAI + SN65HVD230
      ═══[120Ω]═════════ CAN bus (control plane) ═════════════[120Ω]═══
            │              │              │              │
         FL node        FR node        RL node        RR node   (ESP32-C3 each:
                                                                  DRV8833+motor, Hall encoder, corner LEDs)
```
Full diagrams (power chain with battery, PCM, fuse, switch and rails; CAN topology; one wheel node): `docs/architecture.md`.
- **Gateway (ESP32-CAM):** translates operator commands into CAN messages and aggregates status back. Owns the operator-link watchdog. Streams video later.
- **Wheel nodes:** run a local closed-loop speed controller. The gateway sends *setpoints*, not PWM duty. Each node has its own fail-safe: it stops if setpoints stop arriving. Each node also drives its corner LEDs.
- **Application protocol on raw CAN:** 11-bit IDs encode priority. Safety and sync messages get the lowest IDs, then setpoints, then telemetry.
- **Monorepo, planned layout:** a shared `protocol` component, `firmware/wheel_node`, `firmware/gateway`, and `docs/` for the protocol spec.

## Phases

### Phase 0: Foundations
Detail: [`phase-0-foundations.md`](phase-0-foundations.md)
- [x] Detailed planning pass (approved 2026-09-28)
- [x] A. Install ESP-IDF v6.1 natively; write `docs/dev-setup.md` (done 2026-09-28)
- [x] B. Repo skeleton: `firmware/wheel_node` (esp32c3), `firmware/gateway` (esp32), shared `components/tiny_common`, hello-worlds (done 2026-10-16; both build with 0 warnings)
- [x] C. Build and flash both targets; negative test (v5.5.5 build must fail). Build and negative test done 2026-10-16; both boards flashed 2026-10-17 (LEDs GPIO8 / GPIO33 confirmed)
- [ ] D. CI: GitHub Actions build of both projects
- [ ] E. Hardware bring-up as parts arrive (C3 boards, CAN termination, bucks, cells). Parts arrived 2026-10-10 except the cells; bench PSU in the meantime. E.2 done 2026-10-14 (all 5 adapters terminated). E.3 deferred: check each buck before anything connects to its output
- [ ] F. Chassis prep + motor/encoder bench tests: teardown (2-deck chassis; first chassis done 2026-10-14, then dropped), motor resistance and 6 V spin (done 2026-10-15: R on 2, spin on all 5), encoder pinout (done 2026-10-15), encoder 3.3 V check (done 2026-10-16 on the spare), fit check, mount 4 motors (mechanical only; added 2026-10-11, trimmed 2026-10-14)
- **Exit:** from a clean clone, one repo builds and flashes a hello-world onto a C3 and onto the ESP32-CAM, and CI builds both on push. (E and F are not exit blockers; F.2–F.4 on the spare motor are needed before Phase 1.)
- **Status:** in_progress

### Phase 1: One wheel node on the bench
- [ ] Uses the bench spare motor (5th motor, tested in Phase 0 F); the chassis motors stay mounted
- [ ] DRV8833 PWM via LEDC and encoder counting via PCNT
- [ ] Measure with firmware (moved from Phase 0 F.4): counts per output revolution (expect 1080 on A or 4320 in 4× mode), no-load speed at full PWM, and the left/right direction convention (which polarity and leading channel mean forward on each side)
- [ ] Speed measurement, then a PI speed controller
- [ ] Commands over USB serial (temporary)
- **Learning:** embedded control. **Exit:** the wheel holds a commanded speed under finger-drag load and reports actual speed.
- **Status:** pending

### Phase 2: CAN bring-up + gateway v0
- [ ] ESP32-CAM and one C3 over TWAI; 120 Ω only at the two end nodes; decode frames in PulseView
- [ ] Verify which ESP32-CAM GPIOs are free for TWAI (see Risks)
- [ ] First text commands (`DRIVE`, `STOP`, `STATUS`) on the gateway UART, with a transport-independent parser
- [ ] Fail-safe from the start: heartbeat plus setpoint timeout
- **Learning:** CAN protocol, arbitration, termination. **Exit:** a typed command spins the bench wheel, and pulling the CAN wire stops it within a defined timeout.
- **Status:** pending

### Phase 3: Protocol spec + four nodes
- [ ] Write the spec: message catalog, ID/priority allocation, node addressing, error and bus-off handling
- [ ] Termination: all 5 adapters have a 120 Ω terminator fitted (Phase 0 E.2). It is a fixed SMD resistor marked "121", beside the "120R" silkscreen label, with no jumper (the 2-pin header is just a second CANH/CANL connector), so lift one end of the resistor on the 3 adapters that aren't at the bus ends. Use the 2-pin headers for daisy-chaining the harness. Then, with the bus unpowered, measure about 60 Ω between CANH and CANL across the whole bus.
- [ ] Four wheel nodes plus the gateway on a bench harness, all on one firmware image with node identity in NVS
- **Learning:** protocol design, failure detection. **Exit:** skid-steer commands drive all four wheels correctly, and losing any single node stops the whole car.
- **Status:** pending

### Phase 4: Clock sync + coordinated actions
- [ ] Choose a sync algorithm (candidate: PTP-style two-step SYNC + FOLLOW_UP)
- [ ] Scheduled "execute at time T" actions: synchronized blinkers, coordinated start/stop
- **Learning:** distributed clock sync. **Exit:** blinker edges on all four corners line up within a measured bound on the logic analyzer.
- **Status:** pending

### Phase 5: Power system + chassis integration
- [ ] Motors are already mounted (Phase 0 F); this phase does the electrical integration
- [ ] Move from bench breadboards to soldered wiring: each side pod (option D) on a perfboard, the C3 in female headers (socketed), plug-in connectors (JST-type) between modules, heat shrink and tie-downs at every joint; motor power on heavier wire with soldered or screw-terminal joints
- [ ] Battery chain: 2S holder with PCM → fuse → switch → 5 V buck logic rail and motor rail, with bulk and motor caps
- [ ] Measure stall and start current
- [ ] Battery voltage through a divider into one node's ADC; low-battery shutdown on top of the PCM hardware cutoff
- **Learning:** power and battery safety. **Exit:** the car drives on the floor on battery power (UART-tethered), the fuse holds on starts, and the car stops itself below a set voltage threshold.
- **Status:** pending

### Phase 6: Wireless control
- [ ] Same text protocol over a TCP socket on the ESP32-CAM's WiFi
- [ ] The operator-link watchdog stops the car on disconnect
- **Exit:** the car drives untethered, and killing the laptop connection stops it within a defined timeout.
- **Status:** pending

### Phase 7: Video data plane
- [ ] Camera streaming alongside the gateway duties
- [ ] Isolate the control plane: task priorities and core pinning on the dual-core ESP32
- **Learning:** control/data plane separation. **Exit:** with streaming active, control latency and fail-safe timing still meet the Phase 2/6 bounds.
- **Status:** pending

Order note: Phases 4 and 5 can swap. Driving on the floor sooner is more motivating; syncing first keeps everything on the bench longer.

Future (not committed): image processing, an Android app, a Raspberry Pi CAN sniffer, custom PCBs.

## Key Questions
1. How is node identity assigned: NVS written at provisioning, or strap pins? (Phase 3)
2. Which CAN bitrate? 500 kbit/s is assumed. (Phase 2)
3. Which clock-sync algorithm? (Phase 4)
4. Which node measures battery voltage? (Phase 5)
5. ~~Which ESP-IDF version to pin?~~ Resolved: v6.1 (see Decisions).

## Risks (settle in phase plans)
- **ESP32-CAM GPIO scarcity (likely, unverified):** the camera uses most pins, so TWAI probably needs pins freed from the SD-card interface. Some of those are boot-strapping pins (GPIO12, GPIO2). Verify in Phase 2.
- **Gateway overload:** camera + WiFi + CAN + UART on one ESP32. Measure in Phase 7.
- **Current budget (worse than planned):** the first two motors measure R ≈ 3.0 / 3.1 Ω (Phase 0 F.2, 2026-10-14), so stall ≈ 2.8 A per motor at 8.4 V and ≈ 11 A for 4. The listing's 0.82 A was wrong, or quoted at a lower voltage. That is far above the PCM's 3 A continuous rating and the planned 5 A fuse, and above the DRV8833's per-channel rating (about 1.5 A continuous / 2 A peak; check the datasheet for its overcurrent behavior). Phase 1 and Phase 5 must plan current limiting (e.g. a PWM duty cap, soft-start ramps, stall detection from the encoder) and revisit the fuse and PCM choice.
- **ESP-IDF TWAI API:** it has changed across IDF versions. Verify against the pinned version before writing code.

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| ESP-IDF (C, FreeRTOS) on all nodes | Native TWAI driver and FreeRTOS tasks, the best fit for the embedded and distributed-systems learning goals |
| SN65HVD230 transceivers (3.3 V) | Already owned; connect directly to ESP32 GPIO |
| First operator interface: line-based ASCII commands over UART | A web UI is too big for a first step; text commands are easy to type and to script |
| Transport-independent command parser | The same command layer moves to a WiFi TCP socket (Phase 6) unchanged |
| ESP32-CAM is the gateway from day one (UART via its expansion board) | User choice; avoids migrating the gateway role later |
| Gateway sends setpoints; each wheel node closes its own speed loop | Keeps the control loop local and CAN traffic low; matches the ECU model |
| One firmware image for all wheel nodes; identity in NVS (assumed) | Avoids four builds; to be confirmed in Phase 3 |
| Top-level roadmap: phases 0–7 as above | Approved 2026-09-28 |
| ESP-IDF **v6.1**, allowing 6.1.x patches | Current release. The new TWAI driver (`esp_driver_twai`) has the same API in 5.5 and 6.1, so the choice was about avoiding a later 5→6 migration |
| Use the **new** TWAI driver (`esp_twai.h`/`esp_twai_onchip.h`), never the legacy `driver/twai.h` | Legacy is deprecated in 6.1, and the two must not be mixed |
| Native IDF install per machine; the component-manager `idf` constraint enforces the version at build time | Easiest flashing; version drift across machines fails fast |
| Two IDF projects plus shared `components/`, target locked via `sdkconfig.defaults` | Different chips and apps; a clean clone builds without `set-target` |
| CI: GitHub Actions, `espressif/idf:v6.1` container, build only | Free for public repos on standard runners; no artifacts, to avoid storage quota |
| Mechanical chassis prep pulled forward into Phase 0 (item F); no electronics in the chassis until Phases 1–3 settle wiring | Surfaces fit problems early, while a reorder is cheap; avoids rework of an unplanned harness (2026-10-11) |
| Phase 0 F trimmed: counts/rev, no-load speed and direction convention move to Phase 1 (measured by firmware); F.2 drops the 8.4 V and reverse runs; E.3 deferred until first buck use | Keep in Phase 0 only what protects hardware or must precede mounting; Phase 1 measuring them is a good PCNT exercise. Cost: no independent no-firmware reference, unless the optional hand count is done (2026-10-14) |
| Phase detail plans live in separate `phase-N-*.md` files next to `task_plan.md` | Keeps the top of `task_plan.md` (what the hooks inject) short |
| Switch to the wider 2-deck acrylic 4WD kit, flipped: motors on the underside, the whole top face for electronics (2026-10-15) | Wider, many more holes and slots, standoffs for a second deck; flipping raises ground clearance. Cost: the 2 motors mounted on the first chassis come off; F.1/F.5/F.6 restart (mechanical only) |
| Wiring: breadboards on the bench for Phases 1–4 (PSU powered); soldered perfboard per pod from Phase 5 (2026-10-15; "per node" revised to "per pod" by the layout decision below) | Breadboards suit the rewiring of the bench phases. Vibration and battery current start in Phase 5, where loose jumpers would cause intermittent faults. Motor power never goes through breadboard strips (contacts ~1 A, stall ~2.8 A). Soldered nodes keep the C3 socketed and modules on connectors so a node stays replaceable |
| Wiring layout: option D, two side pods (left FL+RL, right FR+RR) between the wheels, wired the option B way: a 4-wire bus cable (5 V, GND, CANH, CANL; JST-XH) gateway → left pod → right pod, and a motor power star from a central WAGO pair to a WAGO 221-413 on each pod (2026-10-15) | Only 2 boards, which fits the space limit; short motor cables; the same harness carries over from bench to car. Motor current stays off breadboard strips. Bus order is tentative until Phase 3. Fallback if the F.5 middle gap is too small: option C, a stripboard backplane on the upper deck. Options, cable list and wire types: `docs/design/wiring-options.html` |
| Git workflow: firmware, CI and build changes go on a branch with a PR to `main`, CI must pass, squash-merge; planning files, bench logs and docs are committed straight to `main` (2026-10-16). No branch protection for now | PRs give a CI gate and a per-feature record where a mistake would break the build; docs-only commits gain nothing from a PR. Starts with item D, so the first PR also proves the workflow runs |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
|       |         |            |
