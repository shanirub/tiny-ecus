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
| Must-fail tests | Is bad input rejected, and does the component recover afterwards? | all three suites |
| Sanitizers | Does any test, passing or not, touch memory out of bounds or hit undefined behavior? | the `SANITIZE` option in `test/host/CMakeLists.txt` |
| Mutation tests | Would the tests notice a realistic bug? | `test/host/mutate.py` |
| Bench checks | Does it work on the real board, motor and encoder? | the Phase 1 plan, run by hand |

An example of a failure each category catches:

- **Unit tests.** A change makes `ENC ZERO` also forget the encoder state → `test_zero_clears_counts_keeps_state` fails.
- **Exhaustive tests.** A typo in one of the decoder table's 16 entries → `test_every_transition` fails and names the pair, e.g. `prev=0 now=1`.
- **Must-fail tests.** An overlong console line overflows the buffer, or locks the line reader up → `test_too_long_line_is_discarded_then_recovers` fails.
- **Sanitizers.** The position is updated with a plain signed `+`. On the PC the result *looks* right (the CPU wraps), so no assertion fails, but the overflow is undefined behavior in C → UBSan stops the test.
- **Mutation tests.** A test that passes for the wrong reason. The first run found one: the zero test happened to call `zero` at state 0, where keeping and resetting the state look the same.
- **Bench checks.** The A and B wires are swapped, so the count runs backwards. No host test can see this.

## Strategy

- **Hardware-free logic lives in components, and only that is unit-tested.** Code that needs no chip (the parser, the line reader, the decoder) is a component in `components/` with no ESP-IDF dependencies, so `test/host` can compile it with the PC's gcc. Code that touches pins, interrupts or FreeRTOS stays in `firmware/*/main/` and is checked on the bench. The encoder is split along that line: `tiny_qdec` (decoding, tested here) and `encoder.c` (GPIO, ISR, lock).
- **One framework, one executable per component.** [Unity](https://github.com/ThrowTheSwitch/Unity) v2.7.0, fetched by CMake and pinned to a commit. Each `test_<component>.c` is its own executable with its own `main()`, registered with `ctest`.
- **Every test starts from a known state.** Unity calls `setUp()` before each test and `tearDown()` after it. `setUp()` re-initialises the shared object (the decoder, the line reader, the fake handler's records), so no test depends on the order or on what the previous one left behind. The isolation also makes bugs in the init functions visible: a broken `tiny_qdec_init` shows up as leftovers in the next test.
- **Expected values don't come from the code under test.** The decoder tests derive every expected step from the forward sequence as recorded on the logic analyzer (`RING`, 10 → 11 → 01 → 00), not from the decoder's table. A typo in the table can't be copied into the tests.
- **Boundaries are tested on both sides.** E.g. a line of exactly `TINY_LINE_MAX` characters is accepted and one more is rejected; 8 words reach the argument check and 9 are refused; `INT32_MAX` + 1 wraps.
- **Strict compilation.** Our code is built with `-Wall -Wextra -Werror`: any warning fails the build.
- **Sanitizers on by default, always in CI.** AddressSanitizer (ASan) reports out-of-bounds and use-after-free memory accesses. UndefinedBehaviorSanitizer (UBSan) reports undefined behavior such as signed overflow or a bad array index. `-fno-sanitize-recover=all` makes every report stop the test, so CI fails.

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

All in `tiny_qdec.c`; all 12 killed.

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

The first run (2026-10-28) had two survivors, mutants 4 and 12. Both were gaps in the tests, not bugs in the decoder, and both are now covered.

### Adding mutants

Add a `(description, original text, replacement)` line to the component's `mutants` list in `mutate.py`. A new component gets its own entry in `TARGETS` (source, include directory, test file). A good mutant is a bug someone could realistically write, in code whose behavior matters. Add one whenever a test is written to guard a specific rule, to prove that the test can fail.

`tiny_cmd` has no mutants yet.

A clean run covers only the listed mutants, so it doesn't prove that the tests are complete. Generated mutation testing (a tool such as Mull, which mutates everywhere automatically) is not set up.

## What the tests don't cover

The host tests prove the logic. Everything that depends on the chip, the wiring or timing is checked on the bench, by hand, as listed in `.planning/can-rc-car/phase-1-one-wheel-node.md`:

| Not covered by host tests | Checked by |
|---|---|
| Firmware code in `firmware/*/main/` (console task, telemetry, `encoder.c`, `main.c`) | CI only builds it; bench use of the console commands |
| `tiny_common` (boot banner; needs ESP-IDF) | seen in the boot log |
| Encoder wiring, pin numbers, pull-ups, which channel leads | C.3 (10 hand turns ≈ 43 200 counts), D.4 direction convention |
| The ISR keeping up at full speed; edges missed under load | D.4: ISR load on the logic analyzer, invalid count at full speed |
| IRAM placement of the ISR path | checked once with `nm` (PR #5); not automated |
| Concurrency: the ISR and tasks sharing the decoder under the lock | code review; no test can force the interleavings |
| Motor drive, protections, speed control | items D and E, recorded with `tools/plot_run.py` |

The firmware doesn't run sanitizers. ESP-IDF can enable UBSan for one component on the chip for debugging (`docs/en/api-guides/fatal-errors.rst` in ESP-IDF); it isn't used so far.

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
