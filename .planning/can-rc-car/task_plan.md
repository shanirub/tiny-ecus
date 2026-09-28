# Task Plan: CAN RC Car (tiny-ecus) — Project Roadmap

## Goal
A 4WD skid-steer RC car built as a distributed system: an ESP32-C3 node per wheel, all nodes talking over CAN, and an ESP32-CAM gateway. The car is driven remotely and fails safe. Learning comes first (see `docs/can-rc-car-project-overview.md`).

## Next Step
Run a detailed planning pass for Phase 0 (Foundations): pick and pin the ESP-IDF version, choose the repo/component layout, work out how one repo builds for both chip targets, and write the hardware bring-up checklist.

## Current Phase
Phase 0 (top-level roadmap approved 2026-09-28; Phase 0 not yet detailed)

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
- **Gateway (ESP32-CAM):** translates operator commands into CAN messages and aggregates status back. Owns the operator-link watchdog. Streams video later.
- **Wheel nodes:** run a local closed-loop speed controller. The gateway sends *setpoints*, not PWM duty. Each node has its own fail-safe: it stops if setpoints stop arriving. Each node also drives its corner LEDs.
- **Application protocol on raw CAN:** 11-bit IDs encode priority. Safety and sync messages get the lowest IDs, then setpoints, then telemetry.
- **Monorepo, planned layout:** a shared `protocol` component, `firmware/wheel_node`, `firmware/gateway`, and `docs/` for the protocol spec.

## Phases

### Phase 0: Foundations
- [ ] Detailed planning pass for this phase
- [ ] Pin an ESP-IDF version and set up the repo skeleton
- [ ] Build and flash both targets (ESP32-C3, and the ESP32 on the CAM board)
- [ ] Hardware bring-up as parts arrive: set each buck to 5 V and measure before connecting; check cells and transceivers
- **Exit:** one repo builds and flashes a hello-world onto a C3 and onto the ESP32-CAM.
- **Status:** in_progress

### Phase 1: One wheel node on the bench
- [ ] DRV8833 PWM via LEDC and encoder counting via PCNT
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
- [ ] Four wheel nodes plus the gateway on a bench harness, all on one firmware image with node identity in NVS
- **Learning:** protocol design, failure detection. **Exit:** skid-steer commands drive all four wheels correctly, and losing any single node stops the whole car.
- **Status:** pending

### Phase 4: Clock sync + coordinated actions
- [ ] Choose a sync algorithm (candidate: PTP-style two-step SYNC + FOLLOW_UP)
- [ ] Scheduled "execute at time T" actions: synchronized blinkers, coordinated start/stop
- **Learning:** distributed clock sync. **Exit:** blinker edges on all four corners line up within a measured bound on the logic analyzer.
- **Status:** pending

### Phase 5: Power system + chassis integration
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
5. Which ESP-IDF version to pin? (Phase 0)

## Risks (settle in phase plans)
- **ESP32-CAM GPIO scarcity (likely, unverified):** the camera uses most pins, so TWAI probably needs pins freed from the SD-card interface. Some of those are boot-strapping pins (GPIO12, GPIO2). Verify in Phase 2.
- **Gateway overload:** camera + WiFi + CAN + UART on one ESP32. Measure in Phase 7.
- **Current budget:** 4 × 0.82 A stall ≈ 3.3 A, above the PCM's 3 A continuous rating. A 5 A fuse is the planned fallback, but the PCM limit is also a factor.
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

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
|       |         |            |
