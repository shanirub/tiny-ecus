# CAN RC Car: System Diagrams

The target system as planned. Items marked *(TBD)* are decided in the phase named next to them. The roadmap is in `.planning/can-rc-car/task_plan.md`.

## 1. Power

```
 Charging (not in the driving power path)
   USB-C 5 V, ≥ 2 A ──► 2S charger board (8.4 V CC/CV, no balancing) ──► pack
                                                     (connection point TBD, Phase 5)

 ┌───────────────────────────────────────────┐
 │ 2S 18650 pack: 2 matched cells            │   6.0 V empty … 8.4 V full
 │ Holder with PCM: over-/under-voltage and  │
 │ short-circuit cutoff, 3 A continuous  ⚠   │
 └──────┬─────────────────────────────┬──────┘
        │ P+                          │ P−
   ┌────┴─────┐                       │
   │ Fuse     │ mini blade, 3 A       │
   │          │ (5 A fallback)  ⚠     │
   └────┬─────┘                       │
   ┌────┴─────┐                       │
   │ KCD11    │ SPST rocker, 3 A  ⚠   │
   └────┬─────┘                       │
        │                             │
        ▼ VBAT 6.0–8.4 V              ▼ GND: one common ground for every board
        │
        ├── 470 µF bulk cap ── GND
        ├── resistor divider ──► ADC on one wheel node (battery monitor; which node TBD, Phase 5)
        │
        ├──► MP1584EN buck ──► +5 V logic rail (one buck assumed; TBD, Phase 5)
        │                        ├──► 4 × ESP32-C3 Super Mini, 5V pin
        │                        │      └─ onboard LDO → 3V3: CAN transceiver, encoder, LEDs
        │                        └──► ESP32-CAM, 5V pin (onboard LDO → 3V3)
        │
        └──► motor rail (unregulated VBAT)
               ├──► DRV8833 VM (FL) ──► FL motor ‖ 100 nF
               ├──► DRV8833 VM (FR) ──► FR motor ‖ 100 nF
               ├──► DRV8833 VM (RL) ──► RL motor ‖ 100 nF
               └──► DRV8833 VM (RR) ──► RR motor ‖ 100 nF
```

⚠ Measured stall current is about 2.8 A per motor at 8.4 V, about 11 A for all four. That is above the PCM, fuse and switch ratings. Current limiting is planned for Phases 1 and 5 (see Risks in `task_plan.md`).

## 2. Control and data

```
 laptop ──USB──► ESP32-CAM-MB (USB-UART) ──UART0, text commands──┐
 laptop ~~WiFi~~ TCP text commands (Phase 6), video (Phase 7) ~~~~┤
                                                                   ▼
                                                ┌────────────────────────────────┐
                                                │ ESP32-CAM gateway              │
                                                │ command parser, operator-link  │
                                                │ watchdog, status aggregation   │
                                                │ TWAI on free GPIOs (TBD, Ph. 2)│
                                                └───────────────┬────────────────┘
                                                          SN65HVD230 [120 Ω kept]
                                                                │
 ═══════════════════════ CANH / CANL twisted pair, daisy-chained ══════════════════════
        │                     │                     │                     │
   SN65HVD230            SN65HVD230            SN65HVD230            SN65HVD230
   [120 Ω removed]       [120 Ω removed]       [120 Ω removed]       [120 Ω kept: bus end]
        │                     │                     │                     │
     FL node               FR node               RL node               RR node
                                        (wiring layout: option D, gateway → left pod → right pod, tentative; confirmed in Phase 3. See docs/design/wiring-options.html)
```

Exactly two terminators are fitted, one at each physical end of the bus. With the bus unpowered, CANH–CANL measures about 60 Ω.

## 3. One wheel node (×4, identical firmware, identity in NVS)

```
                 ┌──────────── ESP32-C3 Super Mini ────────────┐
 +5 V logic ────►│ 5V ─► LDO ─► 3V3                            ├──► 3V3 to transceiver and encoder V
                 │                                             │
                 │ LEDC PWM ×2                                 ├──► DRV8833 AIN1/AIN2
                 │                                             │      AOUT1/AOUT2 ──► motor + / −
                 │ GPIO interrupts (4× quadrature decode)      │◄─── encoder A, B
                 │ TWAI TX/RX                                  │◄──► SN65HVD230 ◄──► CAN bus
                 │ GPIO                                        ├──► corner LED(s) + resistor
                 │ ADC (one node only)                         │◄─── VBAT divider
                 │ USB-Serial-JTAG                             │◄──► laptop: flash, monitor,
                 └─────────────────────────────────────────────┘      Phase 1 commands
```

Encoder motor connector, from the silkscreen: `+ GND A B V -`, wire colors red, yellow, green, blue, red, black (same on all 5 motors). `+`/`-` are the motor winding, and `V`/`GND` power the Hall encoder. Two wires are red: the outer one is motor `+`, the inner one is encoder `V`. GPIO numbers are assigned in Phase 1.
