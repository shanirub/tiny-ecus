# CAN RC Car: System Diagrams

The target system as planned. Items marked *(TBD)* are decided in the phase named next to them. The roadmap is in `.planning/can-rc-car/task_plan.md`.

## 1. Power

```mermaid
flowchart TD
    subgraph charging ["Charging, not in the driving power path"]
        usbc["USB-C 5 V, ≥ 2 A"] --> charger["2S charger board<br/>8.4 V CC/CV, no balancing"]
    end
    charger -.->|"connection point TBD, Phase 5"| pack
    pack["2S 18650 pack: 2 matched cells, 6.0 V empty … 8.4 V full<br/>holder with PCM: over-/under-voltage and short-circuit cutoff<br/>3 A continuous ⚠"]
    pack -->|"P+"| fuse["fuse, mini blade<br/>3 A, 5 A fallback ⚠"]
    fuse --> rocker["KCD11 SPST rocker<br/>3 A ⚠"]
    rocker --> vbat(["VBAT 6.0–8.4 V"])
    pack -->|"P−"| gnd(["GND: one common ground for every board"])
    vbat --- bulk["470 µF bulk cap to GND"]
    vbat --> divider["resistor divider"] --> adc["ADC on one wheel node<br/>battery monitor, which node TBD, Phase 5"]
    vbat --> buck["MP1584EN buck<br/>one buck assumed, TBD Phase 5"] --> logic(["+5 V logic rail"])
    logic --> c3s["4 × ESP32-C3 Super Mini, 5V pin<br/>onboard LDO → 3V3: CAN transceiver, encoder, LEDs"]
    logic --> cam["ESP32-CAM, 5V pin<br/>onboard LDO → 3V3"]
    vbat --> rail(["motor rail, unregulated VBAT"])
    rail --> dfl["DRV8833 VM, FL"] --> mfl["FL motor ‖ 100 nF"]
    rail --> dfr["DRV8833 VM, FR"] --> mfr["FR motor ‖ 100 nF"]
    rail --> drl["DRV8833 VM, RL"] --> mrl["RL motor ‖ 100 nF"]
    rail --> drr["DRV8833 VM, RR"] --> mrr["RR motor ‖ 100 nF"]
```

⚠ Measured stall current is about 2.8 A per motor at 8.4 V, about 11 A for all four. That is above the PCM, fuse and switch ratings. Current limiting is planned for Phases 1 and 5 (see Risks in `task_plan.md`). Each DRV8833 bridge also cuts out on its own above 2 A minimum (3.3 A typical) and retries every 1.35 ms (TI datasheet).

## 2. Control and data

```mermaid
flowchart TD
    laptop["laptop"] -->|"USB"| mb["ESP32-CAM-MB, USB-UART"]
    mb -->|"UART0, text commands"| gw
    laptop -.->|"WiFi: TCP text commands, Phase 6<br/>video, Phase 7"| gw
    gw["ESP32-CAM gateway<br/>command parser, operator-link watchdog, status aggregation<br/>TWAI on free GPIOs, TBD Phase 2<br/>SN65HVD230, 120 Ω kept: bus end"]
    gw ===|"CANH / CANL"| bus
    subgraph bus ["CAN bus: twisted pair, daisy-chained"]
        direction LR
        fl["FL node<br/>SN65HVD230<br/>120 Ω removed"] === fr["FR node<br/>SN65HVD230<br/>120 Ω removed"] === rl["RL node<br/>SN65HVD230<br/>120 Ω removed"] === rr["RR node<br/>SN65HVD230<br/>120 Ω kept: bus end"]
    end
```

The order along the bus and the wiring layout are tentative: option D, gateway → left pod → right pod, confirmed in Phase 3 (see `docs/design/wiring-options.html`). Exactly two terminators are fitted, one at each physical end of the bus. With the bus unpowered, CANH–CANL measures about 60 Ω.

The frames on the bus (IDs, byte layout, rates) are defined in [`components/tiny_proto/README.md`](../components/tiny_proto/README.md).

## 3. One wheel node (×4, identical firmware, identity in NVS)

```mermaid
flowchart LR
    v5(["+5 V logic"]) --> ldo
    subgraph c3 ["ESP32-C3 Super Mini"]
        ldo["5V → LDO → 3V3"]
        twai["TWAI TX/RX"]
        isr["GPIO interrupts<br/>4× quadrature decode"]
        pwm["LEDC PWM ×2"]
        stby["GPIO: driver standby"]
        ledpin["GPIO"]
        adc["ADC, one node only"]
        usj["USB-Serial-JTAG"]
    end
    ldo -->|"3V3"| xcvr["SN65HVD230"]
    twai <--> xcvr
    xcvr <--> bus(["CAN bus"])
    ldo -->|"3V3 to encoder V"| enc["Hall encoder"]
    enc -->|"A, B"| isr
    pwm -->|"AIN1, AIN2"| drv["DRV8833"]
    stby -->|"STBY"| drv
    drv -->|"AOUT1, AOUT2"| motor["motor + / −"]
    ledpin --> leds["corner LED(s) + resistor"]
    vdiv["VBAT divider"] --> adc
    usj <--> laptop["laptop: flash, commands, telemetry"]
```

Encoder motor connector, from the silkscreen: `+ GND A B V -`, wire colors red, yellow, green, blue, red, black (same on all 5 motors). `+`/`-` are the motor winding, and `V`/`GND` power the Hall encoder. Two wires are red: the outer one is motor `+`, the inner one is encoder `V`. GPIO numbers are in `firmware/wheel_node/main/pins.h`. The node's firmware modules and the motor drive's states and sequences: `firmware/wheel_node/README.md`.
