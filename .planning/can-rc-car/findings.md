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
- **Motors:** TT encoder motor, 3–12 V, 0.82 A stall, 1:90 gearing, Hall A/B encoder powered from 3.3 V.
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
  - F.4 measures this. If the result is 12 or 48, the PPR is quoted at the output shaft instead.
- Stall current at a given voltage: estimate it from the winding resistance, I ≈ V / R (F.2).

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
