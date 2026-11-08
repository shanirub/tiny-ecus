# Requirements and traceability

What the firmware must do, and for each requirement: where it is implemented, how it is verified, and where the evidence is. The table is the project's traceability matrix: from a requirement you find its tests, and from a test you find which requirement it protects.

**How this was written.** The requirements were derived on 2026-11-06 from the phase plans' "done when" criteria and the checks already run (`.planning/can-rc-car/`), so they come after the work for Phases 1 and 2 B/D rather than before it. From Phase 2 C on, each new plan item adds its rows here when it is planned, and its status is updated when its PR is merged.

## Verification methods

| Code | Method | Here |
|---|---|---|
| **T** | Test: a scripted check with a pass/fail result | host unit tests (CI), mutation tests (CI), bench runs with written pass criteria |
| **A** | Analysis: shown by calculation or by reasoning from measurements | timing margins, model vs bench |
| **I** | Inspection: shown by reading the design or the code | reviews |
| **D** | Demonstration: shown by operating the system, with no numeric criterion | |

Status: ✅ verified · ⚠️ partly verified (the limit is stated) · ⏳ planned, not verified yet

## Wheel node: motor drive

| ID | Requirement | Implemented in | Verified by | Method | Evidence | Status |
|---|---|---|---|---|---|---|
| MOT-01 | The motor duty shall never exceed the duty cap, in either direction | `tiny_motor` | `test_duty_is_capped_both_ways`; mutant "duty cap not applied" | T | CI | ✅ |
| MOT-02 | Duty changes shall be limited to the slew rate, also through a reversal | `tiny_motor` | `test_ramps_up_by_slew_then_holds`, `test_reverse_ramps_down_by_slew`, `test_reversing_ramps_through_zero`; mutant "no slew limit" | T | CI | ✅ |
| MOT-03 | A timed run shall end by itself: ramp to 0, then idle | `tiny_motor` | `test_run_ends_with_ramp_then_idle`, `test_zero_step_run_ramps_down` | T | CI | ✅ |
| MOT-04 | A stalled wheel shall cut the drive within 1 s and stay off until `STOP` | `tiny_motor`, `drive.c` | 6 stall host tests (`test_stall_trips_after_stall_steps` …); bench D.2 check 6 and D.3 stall run (cut at 450 ms, `DUTY` refused until `STOP`) | T | CI; `logs/20261031-223625-d3-stall.*`; [progress log](../.planning/can-rc-car/progress.md) | ✅ |

## Wheel node: encoder and speed

| ID | Requirement | Implemented in | Verified by | Method | Evidence | Status |
|---|---|---|---|---|---|---|
| ENC-01 | Every A/B transition shall be decoded as +1, −1, 0 or invalid, with invalid ones counted and the position kept | `tiny_qdec` | `test_every_transition` (all 16, expected values taken from a logic-analyzer recording, not from the code) and 14 more host tests; mutation tests | T | CI | ✅ |
| ENC-02 | No encoder counts shall be lost at the duty cap | `encoder.c`, `tiny_qdec` | Shortest edge gap 14.4 µs vs ≈ 9.8 µs worst handler time (1.5×); `invalid` = 0 over 30 s at ±80 % | A + T | [encoder timing margin](design/encoder-timing-margin.md) | ✅ (≈ 1.15× extrapolated to 100 % duty) |
| SPD-01 | The wheel shall hold a commanded speed under finger-drag load | `tiny_pi`, `drive.c` | `test_integral_holds_speed_under_load`, `test_integral_frozen_while_saturated`; Phase 1 exit test on the bench | T + A | [PI speed control](design/pi-speed-control.md), `docs/data/` | ✅ |

## CAN bus

| ID | Requirement | Implemented in | Verified by | Method | Evidence | Status |
|---|---|---|---|---|---|---|
| CAN-01 | The gateway and a wheel node shall exchange frames at 500 kbit/s with none lost | `tiny_can` | B.3: both directions for 5 min, 0 counter gaps, TEC/REC 0/0, ACK on every frame | T | `logs/20261105-173833-b3-*.txt`; [findings](../.planning/can-rc-car/findings.md) | ✅ |
| CAN-02 | A wheel node shall not drive the bus while it resets | pin choice (C3 GPIO4), transceiver | B.4: logic-analyzer capture of the C3's TX across a reset | T | `logs/20261105-174418-b4-c3-boot.sr`; findings | ⚠️ one capture, chip reset only; power-on not tested |
| CAN-03 | After bus-off a node shall recover by itself about 1 s later, and count it | `tiny_can` (`recover_task`) | Review against the IDF 6.1 driver source; F step 11 (short CANH to CANL) on the bench | I + T | [`tiny_can` diagrams](../components/tiny_can/README.md) | ⏳ reviewed, bench test in F |
| PRO-01 | The frame layout shall have one source (the DBC), and the C code and docs shall match it | `tiny_proto` | `test_generated_c_is_up_to_date`, `test_frame_table_matches_dbc`, `test_example_matches_dbc` | T | CI (`tools-tests`) | ✅ |
| PRO-02 | Pack and unpack shall round-trip every field, including its limits and the fault bits | `tiny_proto` | `test_setpoint_round_trip_limits`, `test_status_round_trip_extremes`, `test_fault_bits_positions`, `test_reserved_fault_bits_ignored` | T | CI (`host-tests`) | ✅ |

## Gateway and fail-safe

| ID | Requirement | Implemented in | Verified by | Method | Evidence | Status |
|---|---|---|---|---|---|---|
| GW-01 | Opening or closing the gateway's console port shall neither reset it nor leave it in download mode | `tools/nodelink.py` (`prepare_cam_port`, `open_port(..., "cam")`) | `test_cam_port_is_left_at_b0_once`; bench: 10 open/close cycles, uptime = wall time ±1 ms, every reply arrived | T | CI (`tools-tests`); progress log 2026-11-08 | ⚠️ the first open after a flash or replug resets it once (by design); replug not tried |
| GW-02 | The gateway shall answer `HELP`, `STATUS`, `CAN` and `CANMON ON\|OFF` with one `OK`/`ERR` line each, and with the monitor on print each received frame as a `C,` line | `tiny_console`, `firmware/gateway` | Bench run through `send_cmd.py`; `test_canmon_line_parses`, `test_other_and_malformed_lines_are_not_frames` | T | progress log 2026-11-08; CI (`tools-tests`) | ✅ |
| SAFE-01 | A wheel shall stop within 100 ms plus the ramp after the last valid setpoint | Phase 2 E | Exit test: pull CANH/CANL during `DRIVE`, analyzer measures last setpoint → PWM 0 | T | — | ⏳ E/F |
| SAFE-02 | After a setpoint timeout the wheel shall stay stopped until a 0 setpoint re-arms it | Phase 2 E | `fault_timeout` shown until a 0 setpoint | T | — | ⏳ E |
| SAFE-03 | CAN traffic shall not make the encoder lose counts | Phase 2 E | E.5: `0x7FF` flood at ≈ 3000 frames/s at 80 % duty; `invalid` stays 0, counts match a run without CAN | T | — | ⏳ E |

## Bench operator tool

| ID | Requirement | Implemented in | Verified by | Method | Evidence | Status |
|---|---|---|---|---|---|---|
| OPS-01 | The live tool shall send `STOP` on one key, whatever is typed, and on quit | `tools/live.py` | `test_escape_sends_stop_even_while_typing`, `test_quit_sends_stop_and_telem_off_then_closes`; bench run 2026-11-03 | T | CI (`tools-tests`); progress log | ✅ |

## Reading the gaps

The ⚠️ and ⏳ rows are the open work, kept here on purpose: SAFE-01…03 are Phase 2's exit criterion, and GW-01's one reset after a flash or replug is accepted. What the host tests cannot reach at all (wiring, timing, the real motor) is listed in [`test/README.md`](../test/README.md#what-the-tests-dont-cover).
