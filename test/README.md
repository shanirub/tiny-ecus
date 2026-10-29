# Tests

How the firmware is tested, why, and what each test file covers.

- [Running the tests](#running-the-tests)
- [Test categories](#test-categories)
- [Strategy](#strategy)
- [Mutation testing](#mutation-testing)
- [What the tests don't cover](#what-the-tests-dont-cover)
- [Test files](#test-files)

## Running the tests

From the repo root:

```sh
cmake -S test/host -B build/host        # add -DSANITIZE=OFF without libasan/libubsan
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python3 test/host/mutate.py             # mutation tests; needs the build above (it fetches Unity)
```

A single suite can be run directly for its full Unity output, e.g. `build/host/test_tiny_qdec`.

The sanitizers need their runtime libraries (Fedora: `sudo dnf install libasan libubsan`); see `docs/dev-setup.md`. `mutate.py` needs neither, because it runs UBSan in trap mode (see [Mutation testing](#mutation-testing)).

### CI

`.github/workflows/build.yml` runs on every push to `main` and on every pull request:

| Job | Checks |
|---|---|
| `build (wheel_node)`, `build (gateway)` | the firmware builds with ESP-IDF v6.1 (in the `espressif/idf` container) |
| `host-tests` | the host tests with AddressSanitizer and UndefinedBehaviorSanitizer, then the mutation tests |

Nothing in CI touches hardware; see [What the tests don't cover](#what-the-tests-dont-cover).

## Test categories

| Category | Answers the question | Where |
|---|---|---|
| Unit tests | Does each hardware-free component do what its header promises? | `test/host/test_*.c` |
| Exhaustive tests | Is every possible input handled right, not only the examples? | `test_every_transition` in `test_tiny_qdec.c` |
| Must-fail tests | Is bad input rejected, and does the component recover afterwards? | all four suites |
| Sanitizers | Does any test, passing or not, touch memory out of bounds or hit undefined behavior? | the `SANITIZE` option in `test/host/CMakeLists.txt` |
| Mutation tests | Would the tests notice a realistic bug? | `test/host/mutate.py` |
| Bench checks | Does it work on the real board, motor and encoder? | the Phase 1 plan, run by hand |

## Strategy

Only code that needs no chip is unit-tested. That code (the parser, the line reader, the decoder and the motor run logic) lives in `components/` with no ESP-IDF dependencies, so `test/host` can compile it with the PC's gcc. Code that uses pins, interrupts or FreeRTOS stays in `firmware/*/main/` and is checked on the bench. The encoder is split along this line into `tiny_qdec` (the decoding, tested here) and `encoder.c` (GPIO, interrupt handler, lock), and the motor into `tiny_motor` (runs, cap, slew, stall, pin mapping) and `drive.c` (PWM, driver pins, the 100 Hz loop).

Each component has one test file, built into its own executable and registered with CTest. The framework is [Unity](https://github.com/ThrowTheSwitch/Unity) v2.7.0, which CMake fetches at a pinned commit.

Unity calls `setUp()` before every test and `tearDown()` after it. Each file's `setUp()` re-initialises the object its tests share, so no test depends on which tests ran before it.

The expected values in the decoder tests come from the state sequence recorded on the logic analyzer (`RING`, 10 → 11 → 01 → 00), not from the decoder's table, so a mistake in the table can't be copied into the tests.

Limits are tested on both sides: a line of exactly `TINY_LINE_MAX` characters is accepted and one more is rejected; 8 words reach the argument check and 9 are refused; `INT32_MAX` + 1 wraps to `INT32_MIN`.

The components and the test files compile with `-Wall -Wextra -Werror`, so any warning fails the build. They also compile with AddressSanitizer (ASan: out-of-bounds and use-after-free memory accesses) and UndefinedBehaviorSanitizer (UBSan: undefined behavior such as signed overflow or an array index past the end). `-fno-sanitize-recover=all` makes the first report stop the test, so a sanitizer can fail a test whose assertions all pass. The sanitizers are on by default and always on in CI.

## Mutation testing

The unit tests check the code. Mutation tests check the **tests**: they plant a realistic bug (a *mutant*) and expect at least one test to fail.

`test/host/mutate.py` holds a hand-picked list of mutants, each an exact text replacement in a component's source. It first builds and runs the unchanged source (it must pass), then each mutant in a temporary copy, with the same test file. The repo is never modified.

| Verdict | Meaning |
|---|---|
| `killed` | a test failed, crashed or timed out (30 s): the tests notice this bug |
| `SURVIVED` | every test still passed: the tests have a blind spot, and the script exits non-zero |
| stale mutant | the source changed and the mutant's text no longer matches exactly once; the script stops, and the mutant's text needs updating |

It compiles with `-fsanitize=undefined -fsanitize-undefined-trap-on-error`. In trap mode, a UBSan check that fails executes an illegal instruction instead of calling the UBSan runtime library. The process dies with SIGILL (signal 4), which counts as killed. Trap mode gives no message saying what went wrong, but the harness only needs killed or survived, and it runs on any machine with gcc.

### Current mutants

`tiny_qdec.c`:

| # | Mutant | Killed by |
|---|---|---|
| 1 | one table entry's −1 becomes +1 | `test_every_transition`, the cycle tests |
| 2 | an INV table entry becomes 0 | `test_every_transition`, `test_invalid_means_both_bits_changed` |
| 3 | `prev` and `now` swapped in the table index | `test_every_transition`, the cycle tests |
| 4 | mask dropped in `tiny_qdec_step` | `test_step_uses_only_low_two_bits` (UBSan: index past the table) |
| 5 | mask dropped in `tiny_qdec_update` | `test_only_low_two_bits_used` |
| 6 | no resync: state only updated on valid steps | the cycle tests |
| 7 | invalid transitions not counted | `test_invalid_counts_and_keeps_position`, `test_resyncs_after_invalid` |
| 8 | an invalid step also moves the position | the same two |
| 9 | plain signed add instead of the wrapping unsigned add | `test_position_wraps`, only through UBSan |
| 10 | `init` ignores the start state | `test_init_starts_at_zero` and others |
| 11 | `init` leaves `invalid` as it was | `test_resyncs_after_invalid` (a leftover from the previous test) |
| 12 | `zero` also resets the state | `test_zero_clears_counts_keeps_state` |

Mutant 9 shows why the sanitizers are part of the tests: without UBSan it survives, because the PC's CPU happens to wrap to exactly the value the test expects.

`tiny_motor.c`:

| # | Mutant | Killed by |
|---|---|---|
| 1 | duty cap not applied | `test_duty_is_capped_both_ways` |
| 2 | a run of 0 steps holds its duty | `test_zero_step_run_ramps_down` |
| 3 | no slew limit: the duty jumps to the target | the ramp tests |
| 4 | a FAULT doesn't refuse new runs | `test_fault_refuses_runs_until_stop` |
| 5 | `stop` leaves the old duty, so the next run starts from it | `test_stop_is_immediate`, and other tests through `setUp` |
| 6 | stall check only above `stall_duty`, not at it | `test_stall_judges_the_previous_period` |
| 7 | stall check ignores the sign of the counts | `test_stall_counts_threshold_both_directions` |
| 8 | movement doesn't reset the stall count | `test_movement_resets_the_stall_count` |
| 9 | the run ends one step late | `test_run_ends_with_ramp_then_idle` |
| 10 | never returns to IDLE after a run | the same, and `test_zero_step_run_ramps_down` |
| 11 | forward and reverse swapped on the pins | the pin tests |
| 12 | fast decay instead of slow on the pins | the pin tests |

### Adding mutants

Add a `(description, original text, replacement)` line to the component's `mutants` list in `mutate.py`. A new component gets its own entry in `TARGETS` (source, include directory, test file). A good mutant is a bug someone could realistically write, in code whose behavior matters. Add one whenever a test is written to guard a specific rule, to prove that the test can fail.

`tiny_qdec` and `tiny_motor` have mutants; `tiny_cmd` has none.

A clean run covers only the listed mutants, so it doesn't prove that the tests are complete. Generated mutation testing (a tool such as Mull, which mutates everywhere automatically) is not set up.

## What the tests don't cover

The host tests prove the logic. Everything that depends on the chip, the wiring or timing is checked on the bench, by hand, as listed in `.planning/can-rc-car/phase-1-one-wheel-node.md`:

| Not covered by host tests | Checked by |
|---|---|
| Firmware code in `firmware/*/main/` (console task, telemetry, `encoder.c`, `drive.c`, `main.c`) | CI only builds it; bench use of the console commands |
| `tiny_common` (boot banner; needs ESP-IDF) | seen in the boot log |
| Encoder wiring, pin numbers, pull-ups, which channel leads | C.3 (10 hand turns ≈ 43 200 counts), D.4 direction convention |
| The ISR keeping up at full speed; edges missed under load | D.4: ISR load on the logic analyzer, invalid count at full speed |
| IRAM placement of the ISR path | checked by hand with `nm`; not automated |
| Concurrency: the ISR and tasks sharing the decoder under the lock | code review; no test can force the interleavings |
| The PWM output and the DRV8833's response: direction, decay mode, that the motor actually stops on `STOP`, a timeout and a stall | D.4 and the PR bench checks, recorded with `tools/plot_run.py` |
| Speed control | item E |

The firmware doesn't run sanitizers. ESP-IDF can enable UBSan for one component on the chip for debugging (`docs/en/api-guides/fatal-errors.rst` in ESP-IDF); this project doesn't use it.

## Test files

### `host/test_tiny_qdec.c`

The quadrature decoder (`components/tiny_qdec`), 15 tests. The helper `turn(n)` feeds `n` single steps around the recorded ring, forwards or backwards. Each test starts at position 0, state 10.

- **Building blocks:**
  - `tiny_qdec_state` packs A as the high bit, B as the low bit
  - all 16 (previous, new) state pairs give the step the ring predicts: 0, +1, −1 or invalid
  - both bits changing is always invalid
  - out-of-range inputs to `tiny_qdec_step` are masked to their low two bits
- **Counting:**
  - one cycle forward is +4 and ends at the start state, backward is −4
  - 4320 forward and back is 0, with no invalids
  - 10 wheel turns (10 × 48 × 90) are 43 200, the C.3 bench expectation
- **Invalid transitions:**
  - an unchanged state counts nothing
  - an invalid jump increments `invalid` and leaves the position
  - the next valid step counts again from the new state (resync)
- **Inputs:** only the low two bits of a state are used.
- **Zero:** clears position and invalid, keeps the state (tested at a state that is neither 0 nor the init state).
- **Wrap:** `INT32_MAX` + 1 becomes `INT32_MIN` and back, without undefined behavior; the difference between two positions stays right across the wrap (what speed estimation relies on).

### `host/test_tiny_motor.c`

The motor run logic (`components/tiny_motor`), 16 tests. A small config (cap 800 ‰, slew 50 ‰ per step, stall at ≥ 300 ‰ with ≤ 1 count for 30 steps) keeps the expected values countable by hand. Each test starts idle.

- **Runs:**
  - idle outputs 0
  - the duty ramps by at most the slew per step, both directions, and is capped both ways
  - a run of N steps holds for N−1 steps, then ramps to 0 and turns idle; a run of 0 steps just ramps down
  - reversing ramps through 0 without ending the run
  - `stop` is immediate, and the next run ramps from 0 again
- **Stall:**
  - trips after exactly the configured number of stalled steps, then stays off
  - movement resets the count; below the stall duty nothing trips
  - the counts are judged against the duty of the period they come from (the previous step), so a ramp doesn't trip early
  - ±1 count is "not moving", ±2 is moving
  - a fault refuses new runs until `stop`
- **Pins (slow decay):** 0 brakes (both high), ±1000 drives one way or the other, part duties PWM the right pin by the right amount, out-of-range duties are clamped.

### `host/test_tiny_cmd.c`

The command parser and dispatcher (`components/tiny_cmd`), 22 tests. It uses a small table of fake commands whose handler records how it was called. `setUp()` fills the reply buffer with `x`, so a missing string terminator shows up.

- **Dispatch:**
  - a handler is called with the right arguments; its reply is prefixed `OK` or `ERR` by its return value; an empty reply gives a bare `OK`
  - command names are case-insensitive; extra spaces and tabs are ignored
  - a blank line is `EMPTY`, with no reply
- **Rejections:** no handler is called for any of these.
  - unknown commands, including prefixes and extensions of real ones (`SPEE`, `SPEEDY`)
  - too few or too many arguments → `ERR usage: …`; optional arguments within their range
  - more than 8 words → `ERR too many words`
- **Reply buffer:** a reply too long for it is truncated and still terminated.
- **`HELP`:** lists all commands; shows one command's usage; unknown command and too many arguments are rejected.
- **`tiny_cmd_equals`:** case-insensitive, whole-word comparison.
- **Number parsing:**
  - integers and floats within the given range are accepted, with signs
  - rejected: empty, a sign alone, trailing junk, decimals for integers, hex, out of range, overflow, `NULL`, `nan`, `inf`, `1e40`
  - a rejected input leaves the output variable unchanged

### `host/test_tiny_line.c`

The line reader (`tiny_line` in `components/tiny_cmd`), 11 tests. The reader turns a byte stream from the USB console into complete lines.

- **Line endings:**
  - a partial line isn't ready yet
  - LF and CR each end a line
  - CR LF counts as one line, not a line plus a blank one
  - LF LF gives a blank line
  - consecutive lines come out separately
- **Editing:**
  - backspace and DEL delete, and deleting more than was typed is harmless
  - control characters (e.g. from arrow keys) are dropped, tab is kept
- **Length:**
  - exactly `TINY_LINE_MAX` (96) characters is accepted
  - a longer line is reported as too long and discarded, and the next line is read normally
  - backspace can't rescue a line that already overflowed

### `host/CMakeLists.txt`

Not a test file. Fetches Unity, builds each component as a library with the strict and sanitizer flags, and builds and registers one test executable per component.

### `host/mutate.py`

Not a test file. The mutation harness, described in [Mutation testing](#mutation-testing).
