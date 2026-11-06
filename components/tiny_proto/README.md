# tiny_proto: the CAN frames

The frames the gateway and the wheel nodes exchange over CAN, and the C code that packs and unpacks them.

**[`tiny_proto.dbc`](tiny_proto.dbc) is authoritative.** `tiny_proto.c` and `include/tiny_proto.h` are generated from it by `cantools` and committed, so the firmware build needs no Python. The tables below explain the same frames for people; a test (`tools/tests/test_tiny_proto_docs.py`) fails if they disagree with the DBC, and another fails if the generated files are out of date.

Phase 2 has two frames for one wheel node (node number 0). The four-wheel spec is a Phase 3 item.

## Conventions

- **11-bit IDs**, function code + node number: setpoints `0x100 + n`, status `0x180 + n`. IDs below `0x100` are reserved for a future emergency stop. A lower ID wins arbitration, so this order is also the priority order.
- **Little-endian** ("Intel" in the DBC): a 2-byte value is sent low byte first.
- **Raw value × scale = physical value.** `target_rpm` raw 550 means 55.0 rpm. The C structs hold raw values.
- **Cyclic:** both frames are sent every 20 ms, whether or not a value changed. The wheel node stops the motor if no valid setpoint arrives for 100 ms (`phase-2-can-bring-up.md`, item E).
- **DLC check:** the generated unpack functions only reject frames that are too *short*. A receiver must also check that the DLC equals the frame's `*_LENGTH`, and drop the frame otherwise.

### Setpoint: gateway → wheel node, ID `0x100`, DLC 3, every 20 ms
| Byte | Field | Type | Scale / unit | Range used | Meaning |
|---|---|---|---|---|---|
| 0–1 | `target_rpm` | int16, little-endian | 0.1 rpm | −100.0 … +100.0 | Wanted wheel speed; sign = direction; 0 = stop |
| 2 | `counter` | uint8 | — | 0 … 255, wraps | Alive counter, +1 per frame |

### Status: wheel node → gateway, ID `0x180`, DLC 7, every 20 ms
| Byte | Field | Type | Scale / unit | Range used | Meaning |
|---|---|---|---|---|---|
| 0–1 | `rpm` | int16, little-endian | 0.1 rpm | full int16 | Measured wheel speed |
| 2 | `duty` | int8 | 1 % | −100 … +100 | Duty being applied |
| 3 | `mode` | uint8 | — | 0 … 2 | 0 = idle, 1 = run, 2 = fault |
| 4 | `faults` | uint8 bit field | — | — | bit 0 = `fault_stall` (stall latched), bit 1 = `fault_timeout` (setpoint timeout), bits 2–7 = 0 (ignored on receive) |
| 5 | `counter` | uint8 | — | 0 … 255, wraps | The node's alive counter |
| 6 | `setpoint_seen` | uint8 | — | 0 … 255 | `counter` of the last setpoint received |

### Who uses what
- The **setpoint stream** and its 100 ms timeout are the fail-safe: they tell the node the gateway is alive and commanding it. There is no separate heartbeat frame.
- The **status stream** tells the gateway the node is alive.
- **`setpoint_seen`** only diagnoses lost setpoints: if status frames arrive but `setpoint_seen` is stuck while the gateway's counter moves on, the setpoints aren't reaching the node (wrong ID, filter, wiring).

Bus load: about 100 frames/s, under 3 % at 500 kbit/s.

## Examples
Physical values in, bytes on the wire out (`ID [DLC] data`).

| Frame | Physical values | Bytes |
|---|---|---|
| `SETPOINT_0` | `target_rpm=55.0 counter=53` | `0x100 [3] 26 02 35` |
| `SETPOINT_0` | `target_rpm=-55.0 counter=53` | `0x100 [3] DA FD 35` |
| `STATUS_0` | `rpm=54.3 duty=41 mode=1 fault_stall=0 fault_timeout=0 counter=122 setpoint_seen=53` | `0x180 [7] 1F 02 29 01 00 7A 35` |

Worked through: 55.0 rpm is raw 550 = `0x0226`, sent low byte first as `26 02`. −550 in two's complement is `0xFDDA`, sent as `DA FD`. In the status frame, 543 = `0x021F`, 41 = `0x29`, 122 = `0x7A`, 53 = `0x35`.

## Reading the DBC
A signal line such as

```
 SG_ target_rpm : 0|16@1- (0.1,0) [-100|100] "rpm" WHEEL0
```

says: start bit 0, 16 bits long, `@1` little-endian (`@0` would be big-endian), `-` signed (`+` unsigned), physical = raw × 0.1 + 0, range −100 … 100, unit rpm, received by `WHEEL0`. `BO_ 256 SETPOINT_0: 3 GATEWAY` declares a frame: ID 256 (`0x100`), its name, 3 bytes, sent by `GATEWAY`. `VAL_` names the values of `mode`.

## Changing a frame
1. Edit `tiny_proto.dbc`.
2. Regenerate: `python3 components/tiny_proto/generate.py` (needs `cantools` from `tools/requirements-test.txt`).
3. Update the tables and examples above, and the C tests in `test/host/test_tiny_proto.c`.
4. Run the host tests and `python -m pytest tools/tests`. CI runs both, including the check that the generated files match the DBC.

## The generated API
For each frame `X` (`setpoint_0`, `status_0`): `struct tiny_proto_X_t` with the raw fields, `tiny_proto_X_pack(dst, &msg, size)` (returns the frame length, or `-EINVAL` if `size` is too small) and `tiny_proto_X_unpack(&msg, src, size)` (returns 0, or `-EINVAL`), plus `tiny_proto_X_<signal>_is_in_range(raw)` for the DBC's range. Constants: `TINY_PROTO_X_FRAME_ID`, `TINY_PROTO_X_LENGTH`, and the `mode` names `TINY_PROTO_STATUS_0_MODE_{IDLE,RUN,FAULT}_CHOICE`. Generated with `--no-floating-point-numbers`; the only floating-point code left is the unused `*_is_in_phys_range(double)` helpers, which the linker drops.
