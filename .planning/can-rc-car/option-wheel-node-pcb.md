# Option: custom carrier PCB for the wheel nodes

Status: **open option, not decided** (discussed with the user 2026-11-14). Nothing here changes the roadmap until the user decides; if adopted, it gets its own planning pass and the plan changes listed at the end. Prices and sources: `findings.md`, "Wheel-node PCB: cost and lead time (2026-11-14)".

## Glossary
- **Carrier board:** a PCB that holds existing modules in sockets (female pin headers) and connects them with copper traces instead of jumper wires.
- **Footprint:** the pattern of pads and holes a part solders onto. For AliExpress modules it must be drawn from caliper measurements of *our* modules.
- **Trace / pour / via:** a copper line; a filled copper area (for high current or ground); a plated hole joining the two copper layers.
- **Copper weight:** copper thickness, the same for the whole board, chosen when ordering: 1 oz = 35 µm (standard), 2 oz = 70 µm.
- **Schematic / layout:** the circuit drawing (what connects to what) / the physical placement of parts and routing of traces.
- **DRC / ERC:** design rule check (traces too close or too thin) / electrical rule check (unconnected pins, outputs shorted together); both run by KiCad.
- **Gerbers:** the manufacturing files a fab house (PCB manufacturer, e.g. JLCPCB) reads.
- **Rev (revision):** one ordered version of the board. Rev 1 usually has a mistake; plan for a rev 2.

## Settled so far
- **Carrier board, not bare chips** (user, 2026-11-14). The C3 Super Mini, DRV8833 module and CAN adapter plug into female headers; no SMD chip soldering, no own driver or transceiver circuits.
- Mounting: the user has spacers (received) to fix the boards to the chassis, and plans to put the second chassis on top as a third deck for batteries, gateway, etc.

## Open questions
1. **One node per board (4 boards + spare) or one pod board with two nodes (option D)?** Claude leans single-node: one motor's current per board (3 A, not 6 A), a broken node is swapped as one board, matches "one ECU per wheel", each board near its wheel. Cost: the bus daisy-chains through four boards (more connectors in the chain), and the option D harness (gateway → left pod → right pod, decided 2026-10-15) changes. A pod board keeps that harness as planned.
2. **When:** for Phase 3's four-node bench harness (four breadboard nodes is where wiring errors pile up), or only for Phase 5 in place of perfboard.
3. **Spacer thread size** (M2 / M2.5 / M3): sets the mounting-hole diameter. Hole positions should line up with existing slots in the acrylic so no drilling is needed.
4. **Perfboard order: on hold** until 1–2 are decided. If PCBs are chosen, no perfboards are needed.

## What one node board carries
- Modules (socketed): C3 Super Mini, DRV8833 module, CAN adapter.
- Connectors (board-mount JST-XH; the pigtails are already owned): bus in + bus out, 4-pin (5 V, GND, CANH, CANL; two in parallel so the bus chains through); motor, 2-pin; encoder (the motor's cable, check its connector); VM power in, 2-pin.
- Passives: 470 µF bulk cap and 100 nF next to the driver; corner LED(s) and resistors; battery-divider footprint on every board, populated on one (Phase 5, GPIO0).
- Test pads for the logic analyzer: CAN TX/RX, encoder A/B.
- Mounting holes for the spacers.
- Pins from `firmware/wheel_node/main/pins.h`, including the reserved ones (GPIO3/4 CAN, GPIO1 corner LED, GPIO0 battery ADC), so the board doesn't need a rev when Phases 4–5 add them.
- Size guess: modules about 18–23 mm a side each (from memory of typical listings, to be measured), so roughly 50 × 40 mm with connectors, vs ≈ 82 × 55 mm for the half breadboard.
- Termination: the CAN adapters carry a fixed 120 Ω (Phase 0 E.2), so the board changes nothing there; the Phase 3 item (lift the resistor on the non-end adapters) still applies.

## Motor current on the board
Trace width is set per trace, so power traces are wide and signal traces (≈ 0.25 mm) thin on the same board. IPC-2221 estimate for outer-layer traces (a rule of thumb, not a datasheet; Claude computed it 2026-11-14):

| Current | Temp rise | 1 oz width | 2 oz width |
|---|---|---|---|
| 3 A (one motor stalled) | 10 °C | 1.4 mm | 0.7 mm |
| 3 A | 20 °C | 0.9 mm | 0.45 mm |
| 6 A (two motors, pod board) | 10 °C | 3.6 mm | 1.8 mm |

A 1.5–2 mm motor trace on 1 oz copper fits a small board. Stall lasts briefly anyway: 80 % duty cap and the 300 ms stall trip (Phase 1 D.2). A trace has about a tenth of the cross-section of 20 AWG wire but sheds heat much better, being flat on the board.

## Cost and lead time (estimates, details in `findings.md`)
- Boards: 5 two-layer boards ≤ 100 × 100 mm, 1 oz, green, listed at $2 by JLCPCB (promo still running today: unverified). 2 oz costs extra (amount unknown).
- Shipping to Israel: calculated at checkout; guess $10–30. Import VAT: probably under the personal-import threshold (unverified).
- Headers and JST-XH sockets: a few dollars.
- Total ≈ $15–40 per rev, mostly shipping; ×2 with a rev 2. A real quote takes 2 minutes once a board file exists.
- Arrival: a few days to fabricate plus shipping, roughly 1–3 weeks.

## KiCad work (first board, estimate)
| Step | Estimate |
|---|---|
| Install (`sudo dnf install kicad`; not installed as of 2026-11-14) + KiCad's "Getting Started" tutorial | 3–6 h |
| Measure the three modules with calipers, draw their footprints (the error-prone step) | 2–4 h |
| Schematic (≈ 20 connections, from `pins.h`) | 1–2 h |
| Layout: placement, routing, wide motor traces, mounting holes | 3–6 h |
| DRC, Gerbers, order | 1–2 h |
| **Total** | **≈ 12–20 h**, over several evenings; a rev 2 takes a fraction |

Claude can: check the schematic's connections against `pins.h`, check footprint dimensions against the caliper readings, run DRC/ERC with `kicad-cli` and read out the results. Placing and routing is done by the user in the GUI (the main learning part).

## Risks
- AliExpress module pinouts and spacing vary by seller (DRV8833 modules especially): every footprint from our own modules, measured.
- Rev 1 will probably have a mistake: use it as the bench board, rev 2 for the car.
- The pin map isn't final until Phases 4–5 implement GPIO1 and GPIO0; routed now as reserved.
- Socketed modules add height; check it against the deck spacing.

## Order if adopted
1. Measure the modules and the spacer thread; Phase 0 F.5 fit check for where the boards sit.
2. Decide single-node or pod board.
3. Design while Phase 2 E/F runs on the breadboard. The KiCad files (schematic, footprints, board) go on their own branch (e.g. `hw/wheel-node-pcb`) with a PR, like firmware; this option note itself stays on `main` (planning files, user 2026-11-14).
4. Boards in hand for Phase 3's four-node bench (or Phase 5, per question 2).

## Plan changes if adopted
- Phase 5 "move to soldered wiring … each side pod on a perfboard": replaced by the PCBs.
- Decisions Made, "soldered perfboard per pod from Phase 5" and the option D layout row: revised (pod vs single-node, bus chain).
- Roadmap "Future (not committed): … custom PCBs": moved into the phase that uses them.
- `docs/can-rc-car-parts-list-final.md`: add the boards, headers and JST-XH sockets.
