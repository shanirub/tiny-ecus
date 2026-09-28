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
- **Motors:** TT encoder motor, 3–12 V, 0.82 A stall, 1:90 gearing, Hall A/B encoder powered from 3.3 V.
- **Drivers:** DRV8833, one per wheel node. Its motor supply maximum is 10.8 V (from memory, unverified), and 2S full charge is 8.4 V, so it should be within range.
- **Power:** 2S 18650 with PCM (3 A continuous, no balancing), 8.4 V charger board, MP1584EN buck to 5 V, 3 A mini blade main fuse with 5 A as fallback, KCD11 switch.
- **Nodes:** four ESP32-C3 Super Mini, five CAN transceiver adapters (SN65HVD230), one ESP32-CAM with expansion board.
- **Tools:** multimeter, logic analyzer (PulseView), Raspberry Pi 3B+ as an optional sniffer.

Technical notes (from general knowledge, verify when relevant):
- The ESP32-C3 has native USB-Serial-JTAG. On the ESP32-CAM, UART0 goes through the expansion board's USB-UART and is shared with boot logs.
- TWAI pins route through the GPIO matrix, so any free GPIO can be used. On the ESP32-CAM, candidate free pins are the SD-card pins (GPIO 2, 4, 12, 13, 14, 15) when no SD card is used. GPIO12 and GPIO2 are strapping pins, and GPIO4 drives the flash LED.

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
