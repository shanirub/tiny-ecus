# Findings & Decisions

## Requirements
From `docs/can-rc-car-project-overview.md`:
- Four wheel nodes drive together smoothly, with no twitching or veering from timing mismatches.
- Each wheel knows its actual speed (encoder feedback), not just the commanded one.
- Blinkers and lights at all four corners stay in sync.
- Fail-safe: if a node or the remote link goes silent, the car stops on its own.
- Battery protection: the car stops before the battery runs too low.
- The camera streams video over WiFi, separate from control traffic. The ESP32-CAM's exact role was open; it is now decided as the gateway.
- Principles: learning over importance, skid-steer only, safety enforced in hardware first, phased development, not a race car.

## Research Findings
Hardware (from `docs/can-rc-car-parts-list-final.md`):
- **Motors:** TT encoder motor, 3–12 V, 0.82 A stall (listing figure; contradicted by measurement, see Encoder motor specs below), 1:90 gearing, Hall A/B encoder powered from 3.3 V.
- **Drivers:** DRV8833, one per wheel node. Its motor supply maximum is 10.8 V (from memory, unverified), and 2S full charge is 8.4 V, so it should be within range.
- **Power:** 2S 18650 with PCM (3 A continuous, no balancing), 8.4 V charger board, MP1584EN buck to 5 V, 3 A mini blade main fuse with 5 A as fallback, KCD11 switch.
- **Nodes:** four ESP32-C3 Super Mini, five CAN transceiver adapters (SN65HVD230), one ESP32-CAM with expansion board.
- **Tools:** multimeter, logic analyzer (PulseView), Raspberry Pi 3B+ as an optional sniffer.

Technical notes (from general knowledge, verify when relevant):
- The ESP32-C3 has native USB-Serial-JTAG. On the ESP32-CAM, UART0 goes through the expansion board's USB-UART and is shared with boot logs.
- TWAI pins route through the GPIO matrix, so any free GPIO can be used. On the ESP32-CAM, candidate free pins are the SD-card pins (GPIO 2, 4, 12, 13, 14, 15) when no SD card is used. GPIO12 and GPIO2 are strapping pins, and GPIO4 drives the flash LED.

### Phase 0 discovery (2026-09-28)
- **Upstream ESP-IDF releases** (`gh release list -R espressif/esp-idf`): v6.1 is "Latest" (2026-08-27). Maintenance lines: v6.0.3 (2026-09-02), v5.5.5 (2026-07-17), v5.3.6, v5.2.8.
- **This desktop:** `~/esp/esp-idf-v5.5.5` (stable), and `~/esp/esp-idf` (a `master` dev checkout at v6.1-dev-5253; do not use for the project). Tools are in `~/.espressif` for Xtensa and RISC-V, with Python envs for idf5.5 and idf6.2 on Python 3.14. `idf.py` is not on PATH by default, so `export.sh` must be sourced.
- **Host tools present:** docker, podman, pulseview, sigrok-cli. The user is in the `dialout` group. picocom and minicom are not installed; `idf.py monitor` is enough.
- **esp32-camera component** (v2.1.8, 2026-09-25) declares `idf: ">=5.1"` with no upper bound. The dependency constraint is fine for 6.x, but runtime compatibility is not proven until Phase 7.

- **TWAI drivers** (checked in the local v5.5.5 source and in upstream v6.1 via the GitHub API):
  - Both versions ship two drivers. The legacy `driver/twai.h` (blocking `twai_receive`, `twai_read_alerts`) and the new `esp_driver_twai` (`esp_twai.h` + `esp_twai_onchip.h`). The new driver is event-driven: `on_rx_done`, `on_state_change` and `on_error` callbacks, with `twai_node_receive_from_isr` only inside `on_rx_done`, and no blocking receive API. Source: the v5.5 migration guide, `docs/en/migration-guides/release-5.x/5.5/peripherals.rst`.
  - The new driver was introduced in 5.5. Its public function set is identical in 6.1. 6.1 only adds `enable_scheduled_tx` (a hardware-dependent TX at a trigger time, not yet checked for ESP32/C3) and `tx_queue_priority`.
  - The legacy driver is still available in 5.5 with no compile warning. In 6.1 it emits `#warning "The legacy TWAI driver is deprecated"`. The two drivers must not be mixed.
  - Both versions support RX frame timestamps (`timestamp_resolution_hz`), which is relevant to Phase 4 clock sync.
- **GitHub Actions** (docs.github.com, product-billing/github-actions): "free for self-hosted runners and for public repositories that use standard GitHub-hosted runners." Larger runners are always billed. Artifact storage counts against the account's plan quota, and cache storage above 10 GB per repo is billed. So: standard runners only, and don't upload build artifacts.

- **IDF v6.x install methods** (from `~/esp/esp-idf-v6.1/docs/en/get-started/`): the default since v6.0 is **EIM** (ESP-IDF Installation Manager, available from an Espressif DNF repo). `git clone` + `install.sh` is still documented as "legacy" in `linux-macos-setup-legacy.rst`, with no removal notice. We use the legacy route, which matches the existing installs on this desktop. The legacy RPM prerequisites line is `git wget flex bison gperf python3 cmake ninja-build ccache dfu-util libusbx`; the doc says `yum` / CentOS, and on Fedora the same packages install with `dnf`.
- **Shell convention on this desktop:** `~/.zshrc:120` has `alias idf55='. ~/esp/esp-idf-v5.5.5/export.sh'`, so the new alias follows it as `idf61`.

### Encoder motor specs (2026-10-11, from the user's AliExpress listing)
- Listing text: "TT encoder motor, 12PPR hall encoder, high torque 1:90 gearbox, metal shaft dc geared motor, dc 3-12v". Manufacturer: Kongxiang Lab Tech ("Nulllab").
- Not stated in the listing: the voltage at which the 0.82 A stall current applies, the encoder supply range, the output type (push-pull or open-collector) and the cable pinout.
- A web search found no Nulllab datasheet. Similar TT encoder motors (DFRobot FIT0450) have conflicting PPR figures and a wrong pinout on their wiki, so measure rather than trust listings.
- Expected resolution, if "12 PPR" means pulses per motor-shaft revolution per channel:
  - 12 × 90 = 1080 rising edges on channel A per output-shaft revolution.
  - 4320 counts per output revolution with 4× quadrature decoding (both edges on both channels).
  - Phase 1 firmware measures this (optional no-firmware hand count in F.4). If the result is 12 or 48, the PPR is quoted at the output shaft instead.
- Stall current at a given voltage: estimate it from the winding resistance, I ≈ V / R (F.2).
- Sound (2026-10-14, 2 motors): gearbox whine with a periodic "wow-wow", the same on both motors, so a trait of this gearbox, not a fault. Source gear stage not identified. Phase 1: expect a small periodic speed ripple in the encoder data (possibly once per output turn); don't mistake it for a firmware or controller bug.
- Measured (2026-10-14): R ≈ 3.0 Ω and 3.1 Ω on the first two motors (3.2 / 3.3 Ω read, 0.2 Ω leads). Stall ≈ 2.0 A at 6 V, ≈ 2.8 A at 8.4 V; 4 motors stalled ≈ 11 A. The listing's 0.82 A doesn't fit V/R at any listed voltage. No-load at 6 V ≈ 0.1 A on all 5 (2026-10-14/15). Caveats: R measured on two motors only; an ohmmeter's tiny test current can overstate brush resistance, so the real stall current may be somewhat higher.

### Encoder connector pinout (F.3, 2026-10-14)
- Encoder PCB silkscreen "HallEncoder"; pins left to right: `+  GND  A  B  V  -`.
- `+`/`-` (outer pins) are the motor terminals M+/M−; confirmed by ohmmeter (they read the ~3 Ω winding). `V`/`GND` are the encoder supply and `A`/`B` the quadrature outputs (inferred from the silkscreen; F.4 confirms). The motor power and the encoder share the one 6-pin cable.
- Cable colors (2026-10-15, read visually at the PCB connector on all 5 motors; identical on every motor):

  | Pin | Function | Color |
  |-----|----------|-------|
  | `+` | motor M+ | red (outer) |
  | `GND` | encoder ground | yellow |
  | `A` | encoder channel A | green |
  | `B` | encoder channel B | blue |
  | `V` | encoder supply | red (inner, second red) |
  | `-` | motor M− | black (outer) |
- Two red wires: `+` (motor, outer) and `V` (encoder supply, inner). Never pick by color alone; feeding motor voltage into `V` would likely kill the Hall sensors.
- The encoder has its own supply (`V`/`GND`) because the Hall sensors are active ICs that need a steady logic supply, while the motor terminals see a PWM-chopped voltage whose polarity reverses with direction. Powering `V` at 3.3 V also sets the output high level (if push-pull) to the C3's logic level. Encoder `GND` must be common with the node's ground.
- The cable's far end is individual female Dupont housings; male-to-male jumpers in them give clip-able test points.

### CAN adapter termination (E.2, 2026-10-14)
- All 5 SN65HVD230 adapters measure 118–120 Ω between CANH and CANL, so each has a 120 Ω terminator fitted.
- A correct bus has exactly 2 terminators, about 60 Ω total. All 5 together would be about 24 Ω, which overloads the transceiver drivers and shrinks the differential signal.
- These boards have no termination jumper and no solder bridge on the back.
- The front 2-pin header connects directly to CANH and CANL (checked by continuity). It is a second bus connector, useful for daisy-chaining the harness, and is not related to termination.
- So the terminator is a fixed SMD resistor. Located from `docs/images/can adaptor front.webp`: the part marked "121" on the front, bottom edge, next to the "120R" silkscreen label. In Phase 3, confirm its ends read about 120 Ω, then lift one end on the 3 non-end adapters.

### Old car (F.1 teardown, 2026-10-14)
- Photos: `docs/images/before teardown up.webp` (top) and `docs/images/before teardown bottom.webp` (bottom).
- Drive board: Keyes L298N V2 with screw terminals and an onboard buck. Removed.
- Motors: 4 yellow TT motors; the two on each side appear wired in parallel through the centre hole. Per-terminal polarity and suppression caps aren't visible in the photos.
- Fit concern (rough estimate from the bottom photo, scaled from a ~70 mm TT motor): about 10 mm between each motor's back end and the next motor's gearbox. The encoder motors are longer at that end. Resolved 2026-10-14 (F.5): the encoder motors fit with about 1 mm to spare and don't rub.
- Still on the chassis: the old MCU-with-camera board and its BT antenna.

## Technical Decisions
See the "Decisions Made" table in `task_plan.md`, which is the source of truth.

## Issues Encountered
| Issue | Resolution |
|-------|------------|
|       |            |

## Resources
- `docs/can-rc-car-project-overview.md`: goals, learning goals, principles
- `docs/can-rc-car-parts-list-final.md`: bill of materials
- `~/.claude/plans/recursive-waddling-robin.md`: original plan-mode output for the roadmap, now mirrored in `task_plan.md`
