# Wheel node commands

How to drive the motor and read the encoder from a PC over the USB cable. What happens inside the firmware for each command is in `README.md`; this page is about using them.

Every example below is real: it was sent to the bench node on 2026-11-01 (firmware of PR #8) and the reply is copied as it came back.

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
- **Stall trip:** if the duty is 30 % or more and the encoder sees almost no movement for 300 ms, the driver switches off and stays off (`drive=stall`) until `STOP`. Every new `DUTY` is refused until then.
- **`STOP` always works** and switches the driver off at once.

When a script runs the motor, it should check for a stall and send `STOP` itself; otherwise the next runs are refused.

## The commands

`HELP` lists them; `HELP <command>` shows one command's usage and description:

```
>>> HELP
<<< OK HELP STATUS TELEM ENC DUTY STOP DECAY PROBE
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
| `drive` | `idle`, `run` or `stall` (stall trip latched, `STOP` clears it) |
| `duty` | output duty now, % (during a ramp it's between the start and the target) |
| `decay`, `probe` | see `DECAY` and `PROBE` below |

After a stall trip in fast decay:

```
<<< OK up_ms=448473 ver=eb7d80c-dirty heap=317844 telem=on drops=0 drive=stall duty=0 decay=fast probe=off
```

### DUTY: run the motor

`DUTY <-100..100> [seconds]: open-loop run at a duty % (cap 80), default 5 s, max 30, then ramps to a stop`

"Open loop" means the node applies this duty and doesn't correct the speed. The duty is the share of each PWM period the motor is driven. Positive duty turns the wheel clockwise seen from its outer side, and the encoder counts down; negative is the other way.

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
<<< OK pos=-3304 invalid=0
>>> ENC ZERO
<<< OK pos=0 invalid=0
>>> DUTY -20 2
<<< OK duty=-20 for 2 s
>>> ENC
<<< OK pos=3284 invalid=0
```

This was the direction check from D.4: +20 % for 2 s turned the wheel about 0.76 of a turn (one wheel turn ≈ 4320 counts) with the count going down, and −20 % turned it back. `invalid` counts encoder steps that skipped a state, for example two edges too close for the interrupt to see apart; it should stay 0.

### TELEM: live data

`TELEM ON|OFF: stream telemetry as CSV lines starting with T,`

```
>>> TELEM ON
<<< OK telem on
```

One row every 10 ms. Real rows at slow decay 80 %, while the wheel was still speeding up:

```
T,4196874,1,800,-56,-77778
T,4196884,1,800,-60,-83333
```

That's time 4196874 ms, mode 1 (run), duty 800 ‰ (80 %), −60 counts in the last 10 ms, −83.333 rpm. The columns are explained in `README.md` (Telemetry). `send_cmd.py` hides these rows; `tools/plot_run.py` records and plots them. Send `TELEM OFF` when done.

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

If `STATUS` shows `drive=stall`, something stopped the wheel: free it and send `STOP` before the next run. To record and plot a run instead: `tools/plot_run.py --seconds 8 --label test --cmd "DUTY 40 5"` (see `docs/dev-setup.md`, Bench tools).
