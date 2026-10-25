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
- **Drivers:** DRV8833, one per wheel node. Its motor supply range is 2.7–10.8 V (TI datasheet, checked 2026-10-21), and 2S full charge is 8.4 V, so it is within range.
- **Power:** 2S 18650 with PCM (3 A continuous, no balancing), 8.4 V charger board, MP1584EN buck to 5 V, 3 A mini blade main fuse with 5 A as fallback, KCD11 switch.
- **Nodes:** four ESP32-C3 Super Mini, five CAN transceiver adapters (SN65HVD230), one ESP32-CAM with expansion board.
- **Tools:** multimeter, logic analyzer (PulseView), Raspberry Pi 3B+ as an optional sniffer.

Technical notes (from general knowledge, verify when relevant):
- The ESP32-C3 has native USB-Serial-JTAG. On the ESP32-CAM, UART0 goes through the expansion board's USB-UART and is shared with boot logs.
- TWAI pins route through the GPIO matrix, so any free GPIO can be used. On the ESP32-CAM, candidate free pins are the SD-card pins (GPIO 2, 4, 12, 13, 14, 15) when no SD card is used. GPIO12 and GPIO2 are strapping pins, and GPIO4 drives the flash LED.

### Phase 0 discovery (2026-09-28)
- **Upstream ESP-IDF releases** (`gh release list -R espressif/esp-idf`): v6.1 is "Latest" (2026-08-27). Maintenance lines: v6.0.3 (2026-09-02), v5.5.5 (2026-07-17), v5.3.6, v5.2.8.
- **This desktop:** `~/esp/esp-idf-v5.5.5` (stable), and `~/esp/esp-idf` (a `master` dev checkout at v6.1-dev-5253; do not use for the project). Tools are in `~/.espressif` for Xtensa and RISC-V, with Python envs for idf5.5 and idf6.2 on Python 3.14 (idf6.1 added by item A; the idf6.2 env belongs to the master checkout). `idf.py` is not on PATH by default, so `export.sh` must be sourced.
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
  - **Measured 2026-10-16 (F.4, spare motor):** one full hand turn of the encoder magnet disc (motor shaft) = 47 counts in PulseView's Gray code decoder, i.e. 48 within hand-positioning error. So 12 PPR is per motor-shaft revolution, as assumed: 48 counts per motor revolution in 4× decoding. The 4320 per output revolution still depends on the 1:90 ratio, which Phase 1 firmware confirms.
  - Capture: `docs/images/f4 encoder quadrature pulseview.webp` (PulseView, 1 MHz, D0 = A, D1 = B, Gray code decoder). It shows the end of that turn, count 12 → 47, with the phase cycling 1 → 2 → 3 → 0 (the decoder prints the Gray state as a binary step number). The uneven pulse widths come from turning by hand, not from the encoder.
  - Encoder supply current at 3.3 V: 3 mA. Outputs are push-pull or internally pulled up: clean square waves with no external pull-ups. Multimeter: high 3.28 V, low 0 V on both A and B, comfortably inside the C3 input thresholds (≈ 2.5 V / 0.8 V at 3.3 V, recalled from the datasheet).
- Stall current at a given voltage: estimate it from the winding resistance, I ≈ V / R (F.2).
- Sound (2026-10-14, 2 motors): gearbox whine with a periodic "wow-wow", the same on both motors, so a trait of this gearbox, not a fault. Source gear stage not identified. Phase 1: expect a small periodic speed ripple in the encoder data (possibly once per output turn); don't mistake it for a firmware or controller bug.
- Measured (2026-10-14): R ≈ 3.0 Ω and 3.1 Ω on the first two motors (3.2 / 3.3 Ω read, 0.2 Ω leads). Stall ≈ 2.0 A at 6 V, ≈ 2.8 A at 8.4 V; 4 motors stalled ≈ 11 A. The listing's 0.82 A doesn't fit V/R at any listed voltage. No-load at 6 V ≈ 0.1 A on all 5 (2026-10-14/15). Caveats: R measured on two motors only; an ohmmeter's tiny test current can overstate brush resistance, so the real stall current may be somewhat higher.

### Encoder connector pinout (F.3, 2026-10-14)
- Encoder PCB silkscreen "HallEncoder"; pins left to right: `+  GND  A  B  V  -`.
- `+`/`-` (outer pins) are the motor terminals M+/M−; confirmed by ohmmeter (they read the ~3 Ω winding). `V`/`GND` are the encoder supply and `A`/`B` the quadrature outputs (inferred from the silkscreen; confirmed by F.4 on 2026-10-16). The motor power and the encoder share the one 6-pin cable.
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

### 2-deck chassis (2026-10-15)
- Photo: `docs/images/2-deck chassis before teardown.webp` (from an earlier, unrelated car project; none of its parts are reused).
- Common 2-deck acrylic 4WD kit: wider than the first chassis, many holes and slots, four brass standoffs for an upper plate. Motors mount with acrylic T-brackets through deck slots; front and rear motors face each other, gearboxes outward.
- Plan: mount it flipped, motors underneath and the whole top for electronics; ground clearance rises by roughly 20 mm (estimate).
- Fit risk: the middle gap between motor ends is roughly 12 mm from the photo (scale estimate); measure it with the encoder and connector in mind (F.5).

### Wiring research (2026-10-15, web; secondary sources unless noted)
- **WAGO 221-415:** 0.2–4 mm² solid/stranded, 0.14–4 mm² fine-stranded (24–12 AWG), strip 11 mm (element14 / datasheet mirror). Typical Dupont jumpers (26–28 AWG) are below that range.
- **Wire current (practical, short runs in a small vehicle):** sources disagree because they mix "chassis wiring" and "power transmission" tables. Conservative working figures: 26 AWG ≈ 1 A, 24 AWG ≈ 2 A (jumper listings), 22 AWG ≈ 3–5 A, 20 AWG ≈ 5–7 A, 18 AWG ≈ 10+ A. Treat as rules of thumb, not specs.
- **Breadboards:** BB830-style spec ≈ 36 V / 2 A per contact, 21–26 AWG insertion; practical advice ≈ 1 A per contact.
- **DRV8833 parallel mode:** TI datasheet (SLVSAR1E) application section: both H-bridges can be paralleled (inputs and outputs tied) for about double the current; internal dead time prevents cross-conduction between bridges. Per bridge 1.5 A RMS / 2 A peak, paralleled 3 A RMS / 4 A peak, for the HTSSOP (PWP) package our module has (TI datasheet, checked 2026-10-21; see DRV8833 datasheet below).
- **JST XH:** 2.5 mm pitch (not 2.54; fits 0.1" perfboard acceptably for small pin counts), 3 A with 22 AWG, wire 22–30 AWG. Pre-crimped leads are easy to buy.
- **DroneCAN convention:** 4-pin JST-GH (1.25 mm) daisy-chain cable carrying 5 V, CAN_H, CAN_L, GND; devices have two connectors (in/out); termination only at the two ends (~60 Ω check). Validates the "power + CAN in one 4-wire chain" idea. GH needs a fine crimp tool or pre-made cables; XH is more hobby-friendly.

### Board bring-up (item C, 2026-10-17)
- **C3 Super Mini:** ESP32-C3 (QFN32) rev v0.4, 4 MB embedded flash (XMC), on `/dev/ttyACM0` (USB-Serial-JTAG, `303a:1001`). Onboard LED on GPIO8, active-low: confirmed. No BOOT button needed to flash.
- **ESP32-CAM:** ESP32-D0WD-V3 rev v3.1, 4 MB flash (BoyaMicro), on `/dev/ttyUSB0` via the expansion board's CH340 (`1a86:7523`). Auto-reset into the bootloader works, so no IO0 jumper is needed. Console is on UART0 (GPIO1/3).
- **ESP32-CAM red LED is hidden:** it sits on the back of the ESP32-CAM, the side that plugs into the expansion board, so the LED itself can't be seen with the board stacked. Only its glow leaks out at the edge, and it is easy to miss. Look at the gap between the two boards. The bright white flash LED (GPIO4) on the camera side stays off.
- Both boot logs warn that the image header says 2 MB of flash while the chip has 4 MB (`CONFIG_ESPTOOLPY_FLASHSIZE` default). Harmless; change it only if the space is needed. The ESP32-CAM also logs "Detected boya flash chip but using generic driver"; the generic driver works.
- **Shell pitfall:** running `idf61` in a terminal where another IDF was already active (here `~/esp/esp-idf`, the master checkout, probably injected by the VS Code ESP-IDF extension; not confirmed) keeps `IDF_PYTHON_ENV_PATH` pointing at the `idf6.2` venv, and the v6.1 export fails its Python dependency check (`esp-idf-kconfig`, `esp-idf-monitor` too old). Fix: `unset IDF_PATH IDF_PYTHON_ENV_PATH` before `idf61`, or use a fresh terminal.

### DRV8833 module (2026-10-17, photos)
- Photos: `docs/images/drv8833 module front.webp` (components) and `docs/images/drv8833 module back.webp` (pin labels). Red board marked "V318", TI DRV8833 (marking "DRV8833 83K AV16"; per TI's marking table, "DRV8833" is the HTSSOP-16 PowerPAD package, PWP, while the plain TSSOP is marked "8833PW").
- Pins, labeled side, left row top to bottom: NC, AIN2, AIN1, STBY, BIN1, BIN2, NC, GND. Right row: VM, NC, GND, AO1, AO2, BO2, BO1, GND. NC = not connected. All four GND pins are joined on the board (continuity, 2026-10-22, after soldering the header).
- **STBY** is the chip's sleep input (nSLEEP in TI's datasheet): low = asleep, outputs off; high = awake. **No fault output (nFAULT) is brought out**, so firmware can't read driver faults directly; it relies on its own stall detection.
- No logic-supply pin: the DRV8833 makes its own internal supply from VM, and its inputs take 3.3 V logic (checked 2026-10-21: input-high thresholds in DRV8833 datasheet below).
- Components: a 10 µF 25 V tantalum capacitor (likely on VM) and two small SMD parts, probably the chip's required VINT and VCP capacitors (not confirmed). STBY on the board: pulled down, no pull-up (A.1 below).
- **A.1 STBY measurement (2026-10-21, unpowered, probes on the bare pads):** STBY–GND on the 2 MΩ range reads 222 kΩ with the probes either way round. On the 200 kΩ range it reads 123.1 kΩ one way and OL the other. A value that changes with range and probe direction means a semiconductor junction is in the path (the pin's 6.5 V clamp Zener or its protection diode), not only a resistor. The steady 222 kΩ is a pull-down to GND; it's below the datasheet's 500 kΩ, so the board may add a resistor in parallel (about 400 kΩ would give 222 kΩ), or the unpowered chip reads low. Either way it pulls STBY low, which is what the plan needs. STBY–VM on the 2 MΩ range: the reading climbs (the meter charging the 10 µF VM capacitor through the STBY–GND path), then settles at 1.81 MΩ one way and climbs to OL with the probes swapped. A pull-up resistor would read the same both ways, so this is no pull-up: the one-way path is most likely STBY → pull-down → GND → the H-bridge FETs' body diodes (the diode built into every power transistor) → VM, which conducts only from GND towards VM. The probe direction fits: 1.81 MΩ with red (the meter's + in ohm mode) on STBY and black on VM, the direction those diodes conduct. Result: STBY has no pull-up; it sits low unless driven. The real check is powered: VM on, STBY unconnected, STBY should read about 0 V (Phase 1 A.5). Done 2026-10-25: 0 V with VM at 7.4 V, both with the item B firmware running and in download mode.

### DRV8833 datasheet (TI SLVSAR1E, rev. July 2015, fetched 2026-10-21)
- nSLEEP (our STBY): internal 500 kΩ pull-down to GND, plus a Zener that clamps the pin at 6.5 V; more than 250 µA into the pin can damage it. TI suggests a 20–75 kΩ pull-up if nSLEEP is tied to VM. Wake-up (tWAKE): up to 1 ms from nSLEEP high to the H-bridge on.
- Input thresholds: nSLEEP low ≤ 0.5 V, high ≥ 2.5 V; all other inputs low ≤ 0.7 V, high ≥ 2 V. A 3.3 V C3 output meets both, with less margin on nSLEEP (0.8 V).
- Other inputs (AIN1/2, BIN1/2): internal 150 kΩ pull-downs, so the motor inputs also sit low while the C3 boots.
- VM undervoltage lockout: the outputs turn off below 2.6 V (max), with 90 mV hysteresis.
- Current, HTSSOP (PWP) package: 1.5 A RMS / 2 A peak per bridge; VM 2.7–10.8 V. (The plain TSSOP, PW, is only 0.5 A RMS; ours is PWP by its marking.)
- Overcurrent protection (OCP): trips at 2 A minimum, 3.3 A typical, after a 4 µs deglitch; that bridge turns off for 1.35 ms, then retries, and keeps cycling while the fault lasts. The other bridge keeps running. A stalled motor at 8.4 V (~2.8 A, see Encoder motor specs) can reach the trip level, so on the battery a stall may show as the driver cutting in and out rather than a steady current. On the bench the PSU's 1.5 A limit comes first.
- Thermal shutdown at 150 °C minimum.
- Option for later, if one bridge's current is too low: TI's datasheet allows paralleling both bridges (AIN1+BIN1, AIN2+BIN2, AO1+BO1, AO2+BO2) for about double the current (see Wiring research). Not used in Phase 1.

### ESP32-C3 pin states at reset (2026-10-22, Espressif ESP32-C3 datasheet pin table)
- "After reset" settings, kept until firmware reconfigures a pin: GPIO6 (MTCK) and GPIO20 (U0RXD) have the internal weak pull-up on (45 kΩ typical); GPIO9 too (strapping); GPIO21 (U0TXD) is an output. GPIO2, 3, 4, 5, 7, 8 and 10: input only, no pull. GPIO0/1: no pull.
- Consequence: a pin with a reset pull-up must not drive a "keep off" input. Against the DRV8833's 500 kΩ nSLEEP pull-down, 45 kΩ gives ≈ 3.0 V (≈ 2.7 V with the measured 222 kΩ), above its 2.5 V high threshold. This led to the Phase 1 pin re-map.
- The wheel node's console is USB-Serial-JTAG (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, no secondary console), so UART0's pins (GPIO20/21) are free for other use.

### ESP32-C3 peripherals (2026-10-17, from IDF v6.1 `components/soc/esp32c3/include/soc/soc_caps.h`)
- **No PCNT** (pulse counter): `SOC_PCNT_SUPPORTED` is defined for ESP32, S2, S3, C5, C6, H2, P4 and others, but not for the C3. The roadmap had assumed PCNT for the encoders; changed to a GPIO interrupt with 4× quadrature decoding (decision in `task_plan.md`).
- Present: LEDC (6 channels, 14-bit timers), GPTimer, RMT, dedicated GPIO (fast CPU pin access), a per-pin glitch filter (removes only spikes a few clock cycles long), one CPU core.

### 18650 cells (2026-10-21)
- LiitoKala Lii-35A: label 3500 mAh, 3.7 V nominal, flat top on both ends. Both 3.59 V on arrival, so matched (well within 0.05 V).
- No LiitoKala datasheet found. Retail listings claim 10 A continuous (marketing, unverified). A forum user measured 3268–3345 mAh and 25–30 mΩ internal resistance on four cells, below the label, and another thought it resembles the Samsung 35E, which Samsung rates at 8 A continuous (inference, not confirmed). Working figure: about 8 A continuous, treated as unverified.
- Fit with our budget: the holder's PCM cuts off at 3 A continuous, well below 8 A, so in normal use the PCM, not the cells, is the limit. The all-four-stalled worst case (~11 A) is above the cells' likely rating too; firmware stall detection and the fuse must stop it before it lasts.
- Sources: budgetlightforum.com/t/q-about-liitokala-35a-ncr18650ga-based-and-other-battery-tips/46686, 18650batterystore.com (Samsung 35E 8 A), rozetka.com.ua listing (10 A claim).

### USB console on the C3 (item B, 2026-10-20, bench-verified)
- **Opening the port can reset the board.** The C3's USB-Serial-JTAG resets the chip when RTS is high and DTR low. Linux raises both lines on open, and pyserial applies DTR before RTS, so asking for both low passes through the reset state (`rst:0x15 USB_UART_CHIP_RESET`). Opening with DTR high and RTS low never resets it: checked over 3 open/close cycles and hours of tool use. `tools/plot_run.py` `open_port()` does this; `send_cmd.py` reuses it.
- **With no host reading, the USB driver drops output itself.** Writes wait once for up to 50 ms, then the bytes are discarded. Telemetry on with the port closed for 5 s: the console still answered at once, and `drops` stayed 0, because that loss happens in the driver, not in our queue. `drops` only counts samples the writer task fell behind on.
- The host machine lacks `libasan`/`libubsan`, so local host tests run with `-DSANITIZE=OFF`; CI always runs the sanitizers.
- **Plot window (2026-10-20):** on this Fedora desktop, matplotlib had no GUI backend (`tkinter` comes in the separate `python3-tkinter` package, not installed; no Qt in the venv), so it fell back to Agg silently and `plot_run.py` opened no window. Fixed by pinning `PyQt6==6.11.0` (released 2026-03-30; pulls `PyQt6-Qt6` 6.11.2): matplotlib picks `qtagg`, and the window opens on Wayland. Size: 91 MB download, ~260 MB installed; the venv went from 180 MB to 440 MB. Qt still needs common desktop libraries (libxkbcommon, EGL, fontconfig), so a minimal or headless machine can still lack a window; the script then says so.

### Bench tools (2026-10-16)
- Logic analyzer: 8-channel 24 MHz clone, PulseView/sigrok with the fx2lafw driver; inputs are named D0–D7. PulseView 0.4.2, sigrok-cli 0.7.2 and the libsigrok udev rules are installed on the desktop.
- Lesson from an earlier I2C project: PulseView decoders assign channels by their own defaults, so check the decoder's channel mapping against the actual wiring.

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
