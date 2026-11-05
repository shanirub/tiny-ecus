# Encoder timing margin without a hardware counter

The ESP32-C3 has no PCNT (the pulse-counter peripheral that counts encoder edges in hardware), so every edge of the wheel encoder's two channels raises a GPIO interrupt and software decodes it (`components/tiny_qdec`). That works only while the interrupt handler finishes before the next edge arrives. This note records how that margin was measured, what it is, and what in the firmware can eat into it.

## Question

How close together can two encoder edges come at full speed, and how long does the CPU take from an edge to the end of the handler? The ratio is the timing margin. Below 1× the decoder sees two edges as one jump and counts an `invalid` transition.

## Method

- A spare GPIO (GPIO1) goes high while the handler runs (`PROBE ON`), and a logic analyzer records encoder A, encoder B and that probe pin together, 0.5 s per capture at 12 MHz (83 ns resolution).
- The longest critical section in the drive task (a section that masks interrupts, which delays the encoder handler) is timed on the chip with the CPU cycle counter.
- Pass criterion on the bench: the firmware's `invalid` counter stays 0 over long runs at the duty cap.

## Result

- Shortest gap between real edges at 80 % duty: **14.4 µs**. Worst edge-to-handler-end time seen: **≈ 9.8 µs**. Margin **≈ 1.5×** (≈ 1.15× extrapolated to 100 % duty).
- Interrupt load: **≥ 2.4 % CPU** at 80 % duty.
- The longest interrupt-masked section was 50 µs with the PI controller's software-float math inside it, which is longer than the shortest edge gap. Moving the math outside the lock brought it to **8.8 µs**.
- `invalid` stayed 0 in every run, including 30 s at each of ±80 %.

## Consequences

- The margin is the number to re-measure when CAN interrupts are added in Phase 2 (item E.5 of the Phase 2 plan); fallbacks are listed there.
- Only short integer work runs under the drive task's lock (`firmware/wheel_node/README.md`, "Motor drive").
- The edge spacing is uneven (median A→B 84 µs, B→A 180 µs at 80 %, from uneven magnet poles), so the margin has to come from the shortest measured gap, not from the edge rate.

## Bench record

The full measurements, moved here from the project's findings log. Paths under `logs/` are local bench recordings that aren't in the repo; the two runs in the figure are in `docs/data/`.

### Encoder interrupt load (D.4.4, 2026-11-01, branch `phase1/d-measure`, logic analyzer)
- Setup: `PROBE ON` (GPIO1 high while `encoder_isr` runs), analyzer D0 = A, D1 = B, D2 = GPIO1, `sigrok-cli` fx2lafw, 0.5 s per capture. The clone only streams 21 ms at 24 MHz ("Device only sent 504320 samples"); 12 MHz (83 ns resolution) and 8 MHz capture the full 0.5 s. Captures: `logs/20261101-1512*-d4-isr-*.sr` (open in PulseView). Slow decay, no load.
- Handler body (probe width): 0.92–1.08 µs, median 1.00, identical at every speed.
- Entry (encoder edge → probe rise: CPU entry + IDF GPIO dispatcher): median 2.08 µs, p99 2.5 µs at 80 % (4.1 µs with telemetry on), max 8.3–8.8 µs in all three captures. The return path isn't visible.
- Load: 7650 edges/s at 80 % (≈ 105 rpm): body alone 0.76 % of the time; entry + body ≈ 3.1 µs per interrupt → ≥ 2.4 % CPU (lower bound). At 30 %: 2690 edges/s, ≥ 0.8 %. Telemetry on doesn't change the load, only stretches the latency tail.
- **Edge spacing is far from even.** A and B aren't 90° apart: at 80 % the A→B gap has median 84 µs, B→A 180 µs, and the pattern repeats every motor turn (48 edges, std 6.6 µs across turns), so the magnet disc's poles are uneven. Shortest gap between real edges: 14.4 µs (p0.1 18.9, p1 24 µs). Against the worst entry + body seen (≈ 9.8 µs), the margin at 80 % is ≈ 1.5×; at 100 % duty (≈ 133 rpm, gaps × 0.79) it would be ≈ 1.15×. Two edges inside one handler's latency read as one invalid step, so this is the number to watch when Phase 2 adds CAN (TWAI) interrupts, or if a wheel is driven faster than the motor drives it (pushed, downhill).
- **Edge bounce:** some real edges (mostly A) bounce for 83–500 ns (8 at 30 %, 14 at 80 %, 22 with telemetry). The handler reads the pins ≥ 1.8 µs later, when they've settled, so the bounce adds at most one extra interrupt that counts 0; `invalid` stayed 0 in every run. Whether the bounce is on the wire or in the analyzer's input (no hysteresis) isn't known. If it ever matters, the C3 has a per-pin GPIO glitch filter.

### Locked-section length vs the encoder (2026-11-02, PR #9 review)
- On the C3 (single core), `portENTER_CRITICAL` raises the interrupt threshold to level 4 (ESP-IDF `xPortSetInterruptMaskFromISR`, `RVHAL_EXCM_LEVEL`), so the GPIO/encoder interrupt waits until the lock is released.
- Measured with `esp_cpu_get_cycle_count()` inside the lock, max per 10 s run, 160 MHz: idle 1275 cycles (8 µs), `DUTY 80` 2466 (15 µs), `SPEED 100` with the PI inside 7984 (50 µs). After moving the PI out: 365 / 1052 / 1414 cycles (2.3 / 6.6 / 8.8 µs). Even integer-only code costs µs here; likely flash-cache misses (the loop runs every 10 ms), not verified.
- Encoder edges can be 14.4 µs apart at full speed (D.4.4), so the lock should stay short; CAN will add another interrupt.
