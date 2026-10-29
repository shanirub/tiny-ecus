# Phase 1: One Wheel Node on the Bench — Detailed Plan

Status: **approved 2026-10-17.** DRV8833 module pins known from photos (2026-10-17, see `findings.md`).

## Goal and exit criterion
One ESP32-C3 drives the bench spare motor through a DRV8833, measures the wheel's real speed from the encoder, and holds a commanded speed with a feedback controller. Commands and data go over the USB cable for now (CAN replaces it in Phase 2).
**Exit:** the wheel holds a commanded speed under finger-drag load and reports its actual speed.

## Glossary (terms used in this plan)
Short definitions; ask for more depth on any of them.

**Hardware and peripherals**
- **GPIO** (general-purpose input/output): a chip pin that firmware can read (input) or set high/low (output).
- **Peripheral:** a hardware block inside the chip that does one job on its own (a timer, a PWM generator, a CAN controller), so the CPU doesn't have to.
- **PCNT** (pulse counter): a peripheral on some ESP32 chips that counts encoder edges in hardware with no CPU work. **The ESP32-C3 doesn't have one** (checked in the IDF v6.1 source, 2026-10-17), which is why this plan counts edges in software instead.
- **PWM** (pulse-width modulation): switching a pin on and off very fast. The fraction of time it is on is the **duty cycle** (0–100 %). The motor sees roughly that fraction of the supply voltage, so duty sets its speed.
- **LEDC** ("LED control"): the ESP32's PWM peripheral. Despite the name it drives anything, motors included.
- **DRV8833:** a motor driver chip (our module's pins: `findings.md`). Its **H-bridge** is four switches around the motor that can apply the supply either way round, so the motor turns forwards or backwards. The C3 tells it what to do through two input pins per motor (on our module AIN1, AIN2). **STBY** puts the whole chip to sleep (outputs off) when low.
- **Why a motor driver at all:** a C3 pin can supply only about 20–40 mA at 3.3 V; the motor needs ~0.1 A running and ~2 A or more at stall, at 6–8.4 V. A pin also can't reverse polarity, and the motor's coil kicks back a voltage spike at every switch-off that would destroy a pin. The driver switches the motor supply on the C3's command and absorbs those spikes. PWM is used instead of an in-between voltage because a fully-on or fully-off switch wastes almost no power, while a linear stage burns the difference as heat (and the C3 has no DAC, digital-to-analog converter, anyway).
- **Pull-up / pull-down resistor:** a high-value resistor from a pin to the supply (pull-up) or to GND (pull-down) that sets the pin's level while nothing drives it. Something driving the pin overrides it.
- **Body diode:** the diode built into every power transistor (MOSFET); in an H-bridge they conduct from GND towards the supply and absorb the motor's switch-off spikes. **Zener diode:** a diode that conducts in reverse above a set voltage, used to clamp a pin.
- **STBY** (TI: nSLEEP): low = chip asleep, all outputs disconnected ("high impedance"), the motor coasts; high = awake (up to 1 ms wake-up, TI datasheet). Kept low until the firmware is ready, so the motor can't twitch during boot; also a second, independent "off" switch.
- **nFAULT** (the "n" means active-low): the chip pulls it low when it shuts itself off for overcurrent, overtemperature or undervoltage, and recovers by itself. Our module doesn't bring it out, so firmware can't read it; the chip still protects itself, and our stall detection covers the "why did it stop" gap.
- **NC** (not connected): a header position wired to nothing. Here probably filler, so the board matches the common TB6612FNG module's pin layout (inference). Never use an NC pin as a spare tie point.
- **Decay mode:** what the H-bridge does during the "off" part of each PWM cycle. **Fast decay** lets the motor coast (its current dies out quickly); **slow decay** shorts the motor's terminals (it brakes gently). Slow decay usually gives speed that follows duty more linearly.
- **Strapping pin:** a pin the chip reads at power-up to decide how to boot (normal or flashing mode). Wiring something to it can stop the chip from booting. On the C3: GPIO2, 8, 9.
- **ADC** (analog-to-digital converter): reads a voltage as a number. Needed in Phase 5 for battery voltage; the C3's reliable ADC pins are GPIO0–4.

**Encoder and speed**
- **Quadrature encoder:** two sensor outputs, A and B, each a square wave, a quarter cycle (90°) apart. Which one changes first tells the direction; counting the changes tells how far the shaft turned (what F.4 showed in PulseView).
- **4× decoding:** counting every edge (rise and fall) on both A and B, so 4 counts per pulse cycle: 48 counts per motor turn, about 4320 per wheel turn through the 1:90 gearbox (measured by hand in C.3: 4386).
- **State table:** the decoder's lookup table. From the previous and the new A/B levels it gives +1, −1, 0, or "impossible" (both changed at once, so an edge was missed). The impossible count is a free health check.
- **Speed estimation:** turning counts into speed. Each control period, take how many counts arrived since the last one, divide by the period, convert to wheel rpm (revolutions per minute).

**Firmware**
- **Interrupt / ISR** (interrupt service routine): the chip pauses whatever it's running the instant a pin changes and runs a short function, the ISR. Here, every encoder edge triggers one. ISRs must be very short.
- **IRAM:** the chip's fast internal RAM. ISRs live there so they keep running even while the flash memory is busy.
- **FreeRTOS task:** an independent thread of work, like a small program. The scheduler shares the single C3 core between tasks by priority. The control loop is one task.
- **Tick rate:** how often FreeRTOS's internal clock ticks (100 Hz by default in IDF). Task timing is measured in ticks, so 1000 Hz gives finer timing.
- **Critical section:** a few lines during which interrupts are briefly blocked, so the control task reads the ISR's counter without the ISR changing it halfway.
- **Component:** an ESP-IDF code module (a folder with its own CMakeLists.txt) that projects can share, like `tiny_common`.

**Control**
- **Open loop:** set a duty and hope the speed comes out right. Load, battery voltage and friction all change the result.
- **Closed loop / feedback control:** measure the actual speed, compare it with the wanted speed, and correct the duty continuously.
- **Setpoint:** the wanted value (here, wheel rpm). **Error** = setpoint − measured speed.
- **PI controller** (proportional-integral): the feedback rule used here.
  - **P term:** duty correction proportional to the current error (big error, big push). Alone, it always leaves a small remaining error under load.
  - **I term:** adds up the error over time and pushes until the error is truly zero. This is what holds speed when your finger drags the wheel.
  - The two gains **Kp** and **Ki** set how hard each term pushes; finding good values is **tuning**.
- **Anti-windup:** if the duty is already at its limit, stop the I term from adding up further. Otherwise it keeps growing and the wheel overshoots badly once the load goes away.
- **Feedforward:** an initial duty guess from the measured duty-vs-speed curve, which the PI only has to correct. Optional; makes tuning easier.
- **Step response:** command a sudden setpoint change and record how the speed follows (how fast, how much overshoot, how much ripple). The main tuning tool.
- **Slew limit (soft start):** the duty may change only so fast, e.g. 0 → 100 % over at least 200 ms. Limits the current surge at start-up.
- **Stall detection:** high duty but the wheel doesn't turn for a while means it's blocked; cut the motor before something overheats.

**Commands and tooling**
- **Serial console:** the text channel over the USB cable (what the boot log appeared on). It also accepts typed lines.
- **Command parser:** code that reads a typed line such as `SPEED 60`, splits it into a command word and numbers, checks them, and calls the matching function. Written without any USB dependency, so the same parser later reads commands arriving from the gateway.
- **Telemetry:** data the node reports continuously while running (time, setpoint, speed, duty). Here as **CSV** lines (comma-separated values, one row per sample), which a script can save and plot.
- **Queue:** a FreeRTOS mailbox between tasks. The 100 Hz task drops each sample in without waiting; a lower-priority task takes them out and prints them, so slow USB output never delays the fast task. If the queue is full, the sample is dropped and counted (`drops` in `STATUS`).
- **Scaled integers (`_milli`):** telemetry sends whole numbers, e.g. `sine_milli` = value × 1000, so the node never formats floating-point numbers. The column name says how to scale back.
- **DTR / RTS:** two control lines of a serial port. On the C3's USB port they double as a reset and boot-mode switch, so the tools set them carefully on open to avoid resetting the board.
- **Host unit test:** a small automated test of a piece of logic (the PI controller, the parser) compiled and run on the PC rather than the chip. **Unity** is a small C test framework for this; **CMake** builds it. CI runs these tests on every PR. **Sanitizers** (ASan, UBSan) are compiler options that make a test crash loudly on memory errors or undefined behavior that would otherwise pass silently.
- **GUI backend:** the part of matplotlib that draws on screen. Without one (e.g. no Qt or Tk installed) it can only save image files; `PyQt6` provides it here.

## Decisions for this phase
| Decision | Rationale |
|---|---|
| **Encoder: 4× decoding in a GPIO ISR**, state-table decoder in IRAM (chosen 2026-10-17) | The C3 has no PCNT. Full resolution plus missed-edge detection. Estimated load at full speed: about 8–14 k interrupts/s, a few % CPU (estimate: speed and ISR cost are both unmeasured). Measured in item D; fallback is 2× (interrupt on A only). |
| **Bench power:** motor supply from the bench PSU at 7.4 V (2S nominal) with a 1.5 A current limit; C3 from USB; all grounds joined | A stall at ~3 Ω would draw ~2.4 A, above the DRV8833's 1.5 A RMS per bridge (TI datasheet; its overcurrent trip is 2 A minimum, 3.3 A typical). The PSU limit is the hardware protection on the bench; the firmware protections come on top. |
| **Control loop:** one FreeRTOS task at 100 Hz to start, with `CONFIG_FREERTOS_HZ=1000` (not set yet: still the default 100; set it with the control loop in item D) | 100 Hz is ample for a motor this slow, and the finer tick keeps the period steady. The rate is a tunable, revisited with the measured data. |
| **Units:** setpoints and reported speed in wheel rpm, signed (+ forward) | Same unit later in CAN setpoints; no wheel diameter needed yet. |
| **PWM:** LEDC at 20 kHz, 10-bit duty (1024 steps); slow decay first, fast decay compared as an experiment | 20 kHz is above hearing. 10 bits is within the C3's limit at 20 kHz (80 MHz / 20 kHz = 4000 timer counts, so up to 11 bits). |
| **Commands are timed runs:** `DUTY` and `SPEED` hold for at most N seconds (default 5, max 30), then the motor ramps to a stop | A first fail-safe (motion needs a fresh command), and it makes every run a clean recording window for tuning. |
| **Pure logic in hardware-free C files** (decoder table, speed estimator, PI, stall detector, parser) with **host unit tests in CI** (chosen 2026-10-17) | Bugs in the maths show up on the PC in seconds, without the bench. Plain CMake + Unity (pinned release) as a second CI job, independent of the IDF container. |
| **Telemetry:** CSV lines over the USB console, switched on by command; `tools/plot_run.py` captures a fixed time window, saves the CSV and plots it with matplotlib (chosen 2026-10-17) | Saved runs make tunings comparable. The capture is time-bounded by design (no never-ending monitor). |
| **One branch + PR per work item** below | The git workflow decided 2026-10-16; CI gates each step. |

## Pin map (DRV8833 module pins from the photos; re-mapped 2026-10-22)
In the firmware, every pin is defined once in `firmware/wheel_node/main/pins.h` (2026-10-29); this table keeps the reasons. Change both together.

The Super Mini exposes GPIO0–10, 20 and 21. GPIO18/19 are the USB pins and GPIO11–17 the internal flash.

Re-mapped 2026-10-22 (was AIN1/AIN2 = 6/7, encoder = 5/10, STBY = 20): the C3 datasheet's pin table shows GPIO20 and GPIO6 with the internal 45 kΩ pull-up on after reset. On GPIO20 that pull-up beats the driver's 500 kΩ pull-down (STBY ≈ 2.7–3.0 V, above the 2.5 V "high" threshold), and GPIO6 would hold AIN1 high: the motor would run at full voltage from reset until the firmware configured the pins, and indefinitely with blank or crashed firmware or in download mode. Rule: driver inputs only on pins with no pull at reset (GPIO5, 7, 10 here); pins with a reset pull-up go to inputs.

| Function | GPIO | Reason |
|---|---|---|
| Motor: module AIN1 / AIN2 (PWM) | 5 / 7 | No pull-up or pull-down at reset (C3 datasheet), so the driver's 150 kΩ pull-downs hold them low until firmware drives them. Motor on AO1 / AO2; bridge B unused |
| Encoder A / B (interrupt inputs) | 6 / 20 | These two pins have the C3's 45 kΩ pull-up on at reset, harmless on an input the encoder drives push-pull. GPIO20 is UART0 RX, free because our console is USB-Serial-JTAG |
| Onboard status LED | 8 | Already used, active-low; a strapping pin, but fine as an output after boot |
| Reserved: CAN TX / RX (Phase 2) | 3 / 4 | Keeps CAN off GPIO21, which the boot ROM drives with log output at power-up; that would put garbage on the bus |
| Reserved: battery ADC (Phase 5, one node) / corner LED (Phase 4) | 0 / 1 | ADC1 pins |
| Module STBY (driver sleep) | 10 | No pull at reset, so the driver's internal 500 kΩ pull-down keeps it asleep until the firmware wakes it (A.1: no pull-up on the board). Also gives firmware a hard "all outputs off" |
| Driver fault output | none | The module doesn't bring nFAULT out |

## Work items
Each item is one branch and PR, CI must pass, the user reviews and merges it (not Claude), and each ends with a short bench check logged in `progress.md`.

### A. Bench wiring
1. Module pins are recorded (photos, 2026-10-17). Unpowered, measure STBY–GND and STBY–VM resistance to see whether the board pulls STBY up or down. (Done 2026-10-21: pulled down, no pull-up; `findings.md`.)
2. On a breadboard: C3, DRV8833, the spare motor (`+`/`-` to the driver outputs), encoder (`V` to 3V3, `GND`, A, B to the C3). A 100 nF cap across the motor terminals, a bulk cap (e.g. 470 µF) at the driver's motor supply. On hand only 10 µF and 100 µF (2026-10-24): the bench uses 100 µF (≥ 16 V), enough with the PSU's short leads and 1.5 A limit. (Done 2026-10-24; the C3-side GND rail joins the module at its right-row bottom GND pin instead of the left one, equivalent since all four are joined.)
   Wiring diagram (2026-10-22, pin map above): [`docs/images/phase1-a2-wiring.png`](../../docs/images/phase1-a2-wiring.png) (source: `phase1-a2-wiring.svg`; edit the SVG and re-render with `magick -density 96 phase1-a2-wiring.svg phase1-a2-wiring.png`). C3 pin labels per the Super Mini silkscreen. The module's header is soldered labels up, components down (2026-10-22), so its pin labels are readable on the breadboard.
   Unconnected: module NC, BIN1, BIN2 (held low by the chip's pull-downs), BO1, BO2; C3 5V and the other GPIOs. Module pin numbers count from the top of each row as recorded in `findings.md` (left: NC, AIN2, AIN1, STBY, BIN1, BIN2, NC, GND; right: VM, NC, GND, AO1, AO2, BO2, BO1, GND).
3. Motor supply from the bench PSU, 7.4 V, 1.5 A limit. Motor current goes through one breadboard contact per lead at most: PSU and motor leads plug into the same short strip as the module's VM, GND, AO1 and AO2 pins, never through the power rails or long jumpers (decided 2026-10-22; a contact is rated ~2 A, ≈ 1 A practical, so never hold a stall). The ordered 20 AWG wire isn't needed for this: bench current is ~0.1–0.6 A in use (estimate) and capped at 1.5 A by the PSU, so the thickest jumpers or clip leads on hand are fine for these short runs (the same setup as F.2). Connect the motor supply last, and switch it off before rewiring.
4. Before power: check for shorts between the motor supply and 3V3, and that all grounds are joined. (The module's own four GND pins were checked joined on 2026-10-22.) (Done 2026-10-24, pass after fixing a missing GND link, the bulk cap's placement and a loose motor coupler; readings in `progress.md`.)
5. First power-up, with the C3 running firmware that doesn't touch the motor pins (the item B firmware) and wired as in A.2: STBY, AIN1 and AIN2 should each read about 0 V and the motor must not move. Repeat while holding the C3's BOOT button through a reset (download mode, pins at their reset state). This is the powered half of A.1 and the check of the 2026-10-22 re-map. (Done 2026-10-25, pass with the meter: all three 0 V, motor still, PSU 0.000 A, in both states; the logic analyzer was skipped. Readings in `progress.md`.)

### B. Command line, telemetry and host tests (done 2026-10-20, PR #2)
1. Command parser (hardware-free) plus a console task reading lines from USB. First commands: `STATUS`, `HELP`.
2. Host-test setup: `test/host/` with CMake + Unity; a CI job that builds and runs it. First tests: the parser.
3. `tools/plot_run.py` (pyserial + matplotlib, own Python venv): sends a command, records CSV for N seconds, saves to `logs/`, plots.

Built: `components/tiny_cmd` (parser with built-in `HELP`, line assembler), console commands `STATUS` and `TELEM ON|OFF`, a 100 Hz test sine as telemetry (stand-in until the encoder exists), 33 host tests with sanitizers in CI, `plot_run.py` plus `send_cmd.py` (one-shot commands). Follow-up PR #3: PyQt6 so the plot window opens. Usage: `docs/dev-setup.md`.

### C. Encoder (done 2026-10-29, PR #5)
1. Quadrature decoder: the state table as pure logic (host-tested: every valid step ±1, every invalid one flagged), the ISR in IRAM, a 32-bit position counter read under a critical section.
2. Commands: `ENC` (position, invalid-transition count), `ENC ZERO`.
3. **Measure counts per wheel revolution:** motor unpowered, mark the wheel shaft, turn it by hand exactly 10 turns, read the count. Expect ~43 200 (4320 per turn). This confirms the 1:90 gearbox. (Done 2026-10-29: `pos=-43858 invalid=0`, 4386 per turn, +1.5 %, accepted as hand-positioning error; a ratio of ~1:91.4 isn't ruled out. Disc-only check: exactly 48 per motor turn. Details in `progress.md`.)

Built: `components/tiny_qdec` (state-table decoder, placed in IRAM by `linker.lf`), `encoder.c` (IRAM-safe GPIO ISR on both edges of A and B, decoder under a spinlock), console `ENC` / `ENC ZERO`, `pins.h`. Host tests: 15 decoder tests plus 12 mutation tests in CI. Docs: `test/README.md`, `components/README.md`.

### D. Open-loop drive and protections
1. LEDC PWM on IN1/IN2, signed duty, slow decay. Command `DUTY <−100…100>` as a timed run.
2. Protections (pure logic, host-tested where possible): duty cap (start at 80 %), slew limit, stall detection (|duty| above ~30 % with near-zero speed for 300 ms → stop and latch a fault until `STOP`).
3. Speed estimation from counts per control period; added to telemetry. Resolution: at 100 Hz one count per period = 1.39 wheel rpm, so below roughly 20 rpm the reading flickers between neighbouring steps (at 5 rpm the count per period is 0 or 1). Start with the plain 10 ms window, look at low-speed telemetry, and add a moving average over several periods (or a period method: timing the gap between edges) only if the noise shows up in the data.
4. **Measure:**
   - direction convention: does positive duty turn the wheel forward, and which channel leads? Recorded for left and right mounting;
   - no-load speed vs duty curve at 7.4 V (data for the feedforward and the PI);
   - ISR load at full speed: toggle a spare pin in the ISR and measure its duty on the logic analyzer;
   - invalid transitions at full speed (should stay 0);
   - fast vs slow decay: the speed-vs-duty curve for both.

### E. PI speed control
1. PI controller as pure logic with anti-windup and output limits (host-tested: steady state reaches the setpoint, windup stays bounded at saturation).
2. Command `SPEED <rpm>` as a timed run; `SET KP|KI|FF <value>` for live tuning; gains printed by `STATUS`.
3. Tune with step responses recorded by `plot_run.py`: start with P only, add I, check overshoot and settling time. Expect the gearbox's periodic ripple in the data (see `findings.md`); don't tune against it.
4. **Exit test:** hold e.g. 60 rpm, drag the wheel with a finger, record the run: speed dips and recovers, duty rises, and the reported speed stays within a tolerance to be set from the measured ripple.

## Unverified (check during implementation)
- DRV8833 module: STBY reads as a pull-down to GND, 222 kΩ unpowered (A.1, 2026-10-21; `findings.md`); no pull-up to VM (one-way reading only, 1.81 MΩ / OL). Powered check done in A.5 (2026-10-25): STBY 0 V with VM on. What the two small SMD parts are.
- ~~DRV8833 figures from memory or third-party sources.~~ Checked against the TI datasheet 2026-10-21 (`findings.md`): 1.5 A RMS / 2 A peak per bridge, 2.7–10.8 V, input pull-downs (500 kΩ on nSLEEP, 150 kΩ on the others), input thresholds, 1 ms wake-up, overcurrent trip 2 A min / 3.3 A typ with 1.35 ms auto-retry.
- GPIO0/1 are free on this board (no 32 kHz crystal fitted).
- The no-load speed (estimated 110–190 wheel rpm) and so the interrupt rate; measured in D.
- The GPIO ISR cost in IDF (estimated 1–3 µs per interrupt); measured in D.
- The C3 glitch filter only removes spikes of a few clock cycles; real noise immunity comes from clean wiring and the state table.

## Out of scope for Phase 1
CAN and the gateway (Phase 2). More than one node, node identity (Phase 3). Corner LEDs and clock sync (Phase 4). Battery power, chassis wiring (Phase 5).
