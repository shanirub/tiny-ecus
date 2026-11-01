# Wheel node commands

How to drive the motor and read the encoder from a PC over the USB cable. What happens inside the firmware for each command is in `README.md`; this page is about using them.

Every example below is real: it was sent to the bench node on 2026-11-01 and the reply is copied as it came back. Most come from the firmware of PR #8; `SPEED`, `SET`, `GAINS` and the `ENC` direction example come from the item E firmware, which flipped the encoder sign (positive duty now counts up).

## Sending commands

```sh
tools/.venv/bin/python tools/send_cmd.py STATUS "DUTY 30 3" ENC
```

Each argument is one command. The script prints `>>> command` and `<<< reply`, waits for the reply (or a timeout), then exits; telemetry rows are skipped. It doesn't reset the board. Only one program can hold the port, so close any other serial tool first. Avoid `idf.py monitor`: it resets the board and never ends by itself.

Commands are one line, words separated by spaces, upper or lower case. Every command gets exactly one reply line: `OK …` if it was done, `ERR …` if not (with the reason). Wrong arguments give the usage:

```
>>> DUTY
<<< ERR usage: DUTY <-100..100> [seconds]
>>> DECAY BOGUS
<<< ERR usage: DECAY SLOW|FAST
```

## Safety limits

Bench supply: 7.4 V, current limit 1.5 A, wheel free to turn. The firmware adds these limits on its own:

- **Duty cap 80 %.** A higher duty is accepted and reduced to 80 (the reply says so).
- **Every run ends by itself**, after 5 s by default and 30 s at most, ramping down to a stop. There is no "run forever".
- **Slew ramp:** the duty changes gradually, never in one jump, including reversals.
- **Stall trip:** if the duty is 30 % or more and the encoder sees almost no movement for 300 ms, the driver switches off and stays off (`drive=stall`) until `STOP`. Every new `DUTY` or `SPEED` is refused until then.

These apply to `SPEED` runs too: the speed controller only chooses the duty, so it can't go past the cap, and a blocked wheel trips the stall check. A wheel that is slowed but still turning doesn't trip it; the controller then raises the duty up to the cap, and only the run's time limit ends it.
- **`STOP` always works** and switches the driver off at once.

When a script runs the motor, it should check for a stall and send `STOP` itself; otherwise the next runs are refused.

## The commands

`HELP` lists them; `HELP <command>` shows one command's usage and description:

```
>>> HELP
<<< OK HELP STATUS TELEM ENC DUTY SPEED SET GAINS STOP DECAY PROBE
>>> HELP DUTY
<<< OK DUTY <-100..100> [seconds]: open-loop run at a duty % (cap 80), default 5 s, max 30, then ramps to a stop
```

### STATUS

`STATUS: uptime, firmware version, free heap, telemetry state and drops, drive mode, duty %, decay, probe`

```
>>> STATUS
<<< OK up_ms=11448805 ver=eb7d80c-dirty heap=317844 telem=off drops=0 drive=idle duty=0 decay=slow probe=off
```

| Field | Meaning |
|---|---|
| `up_ms` | time since boot, ms |
| `ver` | the git commit the firmware was built from; `-dirty` means the working tree had uncommitted changes then |
| `heap` | free memory, bytes; it should stay steady |
| `telem`, `drops` | telemetry on/off, and rows lost because the USB port was too slow (should be 0) |
| `drive` | `idle`, `run` (a `DUTY` or `SPEED` run) or `stall` (stall trip latched, `STOP` clears it) |
| `duty` | output duty now, % (during a ramp it's between the start and the target) |
| `decay`, `probe` | see `DECAY` and `PROBE` below |

After a stall trip in fast decay:

```
<<< OK up_ms=448473 ver=eb7d80c-dirty heap=317844 telem=on drops=0 drive=stall duty=0 decay=fast probe=off
```

### DUTY: run the motor

`DUTY <-100..100> [seconds]: open-loop run at a duty % (cap 80), default 5 s, max 30, then ramps to a stop`

"Open loop" means the node applies this duty and doesn't correct the speed (`SPEED` does). The duty is the share of each PWM period the motor is driven. Positive duty turns the wheel clockwise seen from its outer side, and the encoder counts up; negative is the other way.

```
>>> DUTY 30 3
<<< OK duty=30 for 3 s
>>> DUTY -80 3
<<< OK duty=-80 for 3 s
>>> DUTY 100 3
<<< OK duty=80 for 3 s (capped from 100)
```

Sending `DUTY` while a run is going sets a new target and restarts the timer; the duty ramps there from where it is. This is how the decay sweeps stepped down without stopping:

```
>>> DUTY 80 30
<<< OK duty=80 for 30 s
>>> DUTY 70 30
<<< OK duty=70 for 30 s
```

Refused while a stall is latched:

```
>>> DUTY 30 3
<<< ERR stall fault latched, STOP clears it
```

What speed to expect (slow decay, 7.4 V, no load, measured in D.4): about 1.37 wheel rpm per %, 37 rpm at 30 %, 105 rpm at 80 %. From standstill, 5 % doesn't start the wheel; 10 % does.

### SPEED: run at a wheel speed

`SPEED <-100..100> [seconds]: speed-controlled run at wheel rpm, default 5 s, max 30, then ramps to a stop`

"Closed loop": the node measures the wheel speed every 10 ms and adjusts the duty to hold the speed you asked for (the *setpoint*), also when the wheel is loaded. The number is wheel rpm, not %; positive is the same direction as positive `DUTY`. The fastest the bench wheel can go within the 80 % cap is about 105 rpm, so stay below about 95 to leave the controller room to correct.

```
>>> SPEED 60 6
<<< OK speed=60 rpm for 6 s
>>> SPEED -60 6
<<< OK speed=-60 rpm for 6 s
>>> SPEED 150
<<< ERR usage: SPEED <-100..100> [1..30 s]
```

What it did on the bench (7.4 V, wheel free): 90 % of 60 rpm after 120 ms, a peak of about 65 rpm, then 59.9 to 60.6 rpm (100 ms averages). A finger lightly on the wheel for 5 s slowed it to about 53 rpm; the duty rose by about 13 % and the speed was back at 60 while the finger was still there. When the finger lifted, the speed overshot to 66 rpm for about a second.

Sending `SPEED` while a `SPEED` run is going changes the setpoint and restarts the timer, without a jump in the duty. A step from 30 to 80 rpm this way peaked at 82 and settled at 80:

```
>>> SPEED 30 6
<<< OK speed=30 rpm for 6 s
>>> SPEED 80 3
<<< OK speed=80 rpm for 3 s
```

When the time is up the duty ramps down to 0 as after `DUTY`; the controller doesn't fight the ramp. `SPEED 0` doesn't hold the wheel still: it ramps the duty towards 0 and the wheel coasts (from a standstill it ends at once). Use `STOP` to stop.

### SET and GAINS: the speed controller's settings

`SET KP|KI|FF <value>: change a speed controller gain, also mid-run (see GAINS)`

`GAINS: speed controller gains: kp, ki (permille duty per rpm, per rpm*s), ff (permille per rpm)`

A *gain* is a multiplier that decides how hard the controller reacts. Every 10 ms the controller computes the duty in ‰ from three parts:

| Part | Gain | What it does |
|---|---|---|
| feedforward | `ff` | the duty the wheel normally needs for that speed, from the measured duty-to-speed line: `ff` × (setpoint + 4.2 rpm of friction) |
| proportional | `kp` | `kp` × the speed error now (setpoint − measured): pushes harder the further off the speed is |
| integral | `ki` | adds up `ki` × error × time: keeps rising while an error stays, so a steady load (a finger, a slope) is eventually fully made up |

The integral is limited and stops growing while the duty is already at the cap and the error pushes further (*anti-windup*), so after a blocked wheel is released it doesn't race off with a huge stored push.

```
>>> GAINS
<<< OK kp=6.000 ki=20.000 ff=7.300
>>> SET KP 2.5
<<< OK KP=2.500
>>> SET KP 6
<<< OK KP=6.000
>>> SET KI 600
<<< ERR KI must be 0..500.000
>>> SET XX 1
<<< ERR usage: SET KP|KI|FF <value>
```

Ranges: `KP` 0..50, `KI` 0..500, `FF` 0..20; decimals are allowed. The values at boot (kp 6, ki 20, ff 7.3) passed the bench check, so changing them is for experiments; they aren't saved and a reset brings the boot values back. Rough guide: more `kp` reacts faster but can oscillate; more `ki` removes a load error sooner but overshoots more after a step or when the load goes away; `ff` 0 makes the controller do all the work from the error alone.

### STOP

`STOP: stop the motor at once (coast) and clear a stall fault`

```
>>> STOP
<<< OK stopped
```

No ramp: the driver switches off and the wheel coasts to a stop. Safe to send at any time, also when nothing is running.

### ENC: encoder position

`ENC [ZERO]: encoder position (counts) and invalid transitions; ZERO clears both first`

```
>>> ENC ZERO
<<< OK pos=0 invalid=0
>>> DUTY 20 2
<<< OK duty=20 for 2 s
>>> ENC
<<< OK pos=3320 invalid=0
```

This was the direction check from item E: +20 % for 2 s turned the wheel about 0.77 of a turn (one wheel turn ≈ 4320 counts) with the count going up; −20 % turns it back and counts down. (Before item E the sign was the other way round: the D.4 check gave −3304 for the same run.) `invalid` counts encoder steps that skipped a state, for example two edges too close for the interrupt to see apart; it should stay 0.

### TELEM: live data

`TELEM ON|OFF: stream telemetry as CSV lines starting with T,`

```
>>> TELEM ON
<<< OK telem on
```

One row every 10 ms. Real rows from `DUTY 20 2` and from `SPEED 60 6`:

```
T,10811964,1,0,200,17,23611
T,10827744,3,60000,470,44,61111
```

The first is time 10811964 ms, mode 1 (`DUTY` run), no setpoint, duty 200 ‰ (20 %), 17 counts in the last 10 ms, 23.611 rpm. The second is mode 3 (`SPEED` run), setpoint 60.000 rpm, duty 470 ‰, 44 counts, 61.111 rpm. The columns are explained in `README.md` (Telemetry). `send_cmd.py` hides these rows; `tools/plot_run.py` records and plots them. Send `TELEM OFF` when done.

### DECAY: what the motor does between pulses

`DECAY SLOW|FAST: PWM decay mode for the next runs (slow at boot); only while not running`

```
>>> DECAY FAST
<<< OK decay=fast
>>> DUTY 30 3
<<< OK duty=30 for 3 s
>>> DECAY SLOW
<<< ERR motor running, change decay when idle
```

Slow decay is the default and the one measured to work well: the speed follows the duty in a straight line down to a few %. Fast decay is much weaker at the same duty (93 rpm at 80 %, 9 rpm at 50 %, and below about 45 % it doesn't turn), so from standstill it usually trips the stall check: after `DECAY FAST`, expect to need `STOP`. Details and the full curves: `README.md` (Decay mode).

### PROBE: interrupt timing on the logic analyzer

`PROBE ON|OFF: GPIO1 high while the encoder interrupt handler runs, for the logic analyzer`

```
>>> PROBE ON
<<< OK probe on
>>> PROBE OFF
<<< OK probe off
```

Measurement only; off at boot. Connect the analyzer as D0 = encoder A, D1 = encoder B, D2 = GPIO1, GND to the C3's GND. How to read the capture: `README.md` (Interrupt probe).

## A typical bench session

```sh
tools/.venv/bin/python tools/send_cmd.py STATUS "ENC ZERO" "DUTY 40 5"
# wait for the run to end (5 s plus the ramp), then:
tools/.venv/bin/python tools/send_cmd.py ENC STATUS
```

If `STATUS` shows `drive=stall`, something stopped the wheel: free it and send `STOP` before the next run. To record and plot a run instead: `tools/plot_run.py --seconds 8 --label test --cmd "SPEED 60 5"` (see `docs/dev-setup.md`, Bench tools).
