# Choosing the PWM decay mode

The DRV8833 motor driver can spend the off part of each PWM period in two ways. In **slow decay** the motor's current keeps circulating through the driver (the winding is shorted); in **fast decay** the current is returned to the supply and collapses. The datasheet describes both but doesn't say which suits a speed controller, so both were measured on the bench motor.

## Question

Which mode gives a speed that is a usable, preferably linear, function of the duty, so that the PI controller has a nearly constant gain over the whole range?

## Method

The duty is stepped down from 80 % to 5 % while the wheel turns, 4 s per step, once per mode; the speed is the mean of the last 2 s of each step, and the PSU current is read from a camera photo of its display. A downward sweep is needed because from rest the stall protection trips before fast decay starts the wheel.

## Result

- **Slow decay is linear**: ≈ 1.37 rpm per % duty from 10 % to 80 %, both directions within 0.2 rpm, and the wheel keeps turning down to 5 %.
- **Fast decay is not usable** below ≈ 45 % duty and steep above it (≈ 3 rpm per %). A third of the duty range does nothing.
- **Decision: slow decay** (the firmware default). Its straight line (fitted from the D.4.2 sweep, in the findings log) is the PI controller's feedforward and plant model (`docs/design/pi-speed-control.md`).

## Bench record

The full measurements, moved here from the project's findings log. Paths under `logs/` are local bench recordings that aren't in the repo; the two runs in the figure are in `docs/data/`.

### Fast decay, first look (D.4.5, 2026-11-01, branch `phase1/d-measure`)
- Same motor, PSU 7.4 V, no load. Fast decay (IN1 PWM, IN2 low forward; off part coasts) drives the right way, but much more weakly than slow decay: no start from rest at 30 % or 50 % (the stall check tripped both times, as designed), ≈ 92 rpm at +80 % and 95 rpm at −80 % against ≈ 105 in slow decay. PSU 0.075 A at fast −80 % (slow 80 %: 0.104 A, 2026-10-31).
- Likely cause (textbook, not measured here): in fast decay the winding current collapses during each off-time instead of circulating, so the average current, and with it the torque, is lower for the same duty. Slow decay keeps the current flowing and gives the near-linear curve measured in D.4.2.
- For the full curve: from rest the stall check (≥ 30 % duty, ≤ 1 count, 300 ms) trips before fast decay starts, so sweep downward from 80 % while the wheel is turning and find where it stops.

### Fast vs slow decay, downward sweep (D.4.5, 2026-11-01, branch `phase1/d-measure`)
- Method: one run per mode, the duty stepped down every 4 s while the wheel turns (`DUTY d 30` resent as a new target), mean of the last 2 s; PSU photographed ≈ 3 s into each step. Log `logs/20261101-151644-d4-down.csv`, photos `…-d4-down-*.jpg`, current sheet `…-d4-down-amps.png`. No load, 7.4 V.

  | duty % | 80 | 70 | 60 | 50 | 40 | 30 | 25 | 20 | 15 | 10 | 5 |
  |---|---|---|---|---|---|---|---|---|---|---|---|
  | fast: rpm | 93.3 | 61.8 | 33.2 | 9.2 | ≈ 1.7, stall trip | | | | | | |
  | fast: PSU A | 0.100 | 0.074 | 0.053 | 0.036 | | | | | | | |
  | slow: rpm | 106.3 | 92.9 | 79.1 | 65.3 | 51.4 | 37.5 | 30.6 | 23.4 | 16.5 | 9.6 | 2.3 |
  | slow: PSU A | 0.099 | 0.083 | 0.068 | 0.054 | 0.042 | 0.030 | 0.025 | 0.020 | 0.016 | 0.012 | 0.009 |

- Fast decay is steep and non-linear: ≈ 3 rpm per % between 50 and 80 %, nothing usable below ≈ 45 %, so a third of the duty range does nothing and the rest is very sensitive. Slow decay stays linear (≈ 1.37 rpm/%, matches D.4.2 within 0.5 rpm).
- Slow decay sweeping down keeps turning at 5 % (2.3 rpm), though it never started from rest at 5 % (D.4.2): the static friction is higher than the running friction, so starting needs more duty than keeping going.
- The PSU current isn't the motor current: in slow decay the off-time current circulates inside the driver, in fast decay it returns to the supply. So the two current rows can't be compared directly.
- **Decision (user, 2026-11-01): slow decay**, already the default. Linear, symmetric, usable down to a few rpm, and it gives the PI controller an almost constant gain.
