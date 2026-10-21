# CAN RC Car — Parts List

Sep 27, 2026 · @Shani Rubovitch

## Ordered (AliExpress), arrived 2026-10-10

One cart, about ₪210 in total. The MPU6050 in the same cart is for a separate STM32 project.

| Part | Qty | Seller | Notes |
| --- | --- | --- | --- |
| TT encoder motor, all-metal (black) | 5 (4 + spare) | Nulllab Choice Store | 3–12 V, 0.82 A stall (listing; measured R ≈ 3 Ω means ≈ 2.8 A at 8.4 V), 1:90, Hall A/B encoder; encoder on 3.3 V |
| DRV8833 motor driver | 5 (4 + spare) | Realpoy | One per wheel node |
| 2S 18650 holder with PCM | 1 | HuaXinHuiDianZi | 3 A continuous; protects but does not balance |
| 2S charger board, 8.4 V, 2 A version | 2 (1 + spare) | XINZHIGUAN | USB-C input; no balancing |
| MP1584EN buck, 5 V output | 5-pack | Shop1105361843 | Logic rail; measure output before connecting |
| Mini blade fuse holder, 18 AWG | 1 | DQLZV |  |
| Mini blade fuses, 3 A | 10 | Light of Future | Main fuse |
| Mini blade fuses, 5 A | 10 | Light of Future | Fallback if 3 A trips on motor starts |
| KCD11 rocker switch, SPST, 3 A | 10-pack | Javino | Main power switch; needs a 10×15 mm cutout |

## To buy locally

| Part | Qty | Where | Notes |
| --- | --- | --- | --- |
| 18650 Li-ion cells | 2, matched | Vape shop (arrived 2026-10-21: LiitoKala Lii-35A, 3500 mAh, flat top both ends; both 3.59 V on arrival. Discharge rating unverified, see `findings.md`) | Same model, bought together (e.g. Samsung 30Q, Sony VTC6, LG HG2, Molicel P28A); check flat or button top fits the holder |
| 5 V USB-C charger, ≥ 2 A | 1 | Probably already owned | Powers the charger board |

## Ordered (wiring, option D), 2026-10-16

Not arrived yet (checked 2026-10-20). These are the only missing parts (the 18650 cells arrived 2026-10-21).

| Part | Qty | Notes |
| --- | --- | --- |
| WAGO 221 assortment box, 5-port (221-415) and 3-port (221-413) | 1 box | Central motor-power split (2× 5-port) and one + / − pair per side pod (4× 3-port); 1 more 3-port already owned |
| Solid-core wire, 22 AWG, assorted colors | 1 set | Breadboard and perfboard links, signal wiring. Not for motor leads: solid core fatigues and breaks under vibration |
| Silicone stranded wire, 20 AWG, red + black | 1 set | Battery trunk (planned 20 AWG) and motor power branches (planned 22 AWG; 20 AWG is thicker, so it covers them too) |
| JST-XH 4-pin pigtail pairs (male + female) | enough for 2 + spares | Bus cable: 5 V, GND, CANH, CANL. 2 pairs needed (gateway → left pod → right pod) |

## Already owned

| Part | Qty | Notes |
| --- | --- | --- |
| ESP32-C3 Super Mini | 4 + spares | One per wheel node |
| CAN transceiver adapters | 5 | 4 wheel nodes + ESP32-CAM; keep 120 Ω only on the two end nodes |
| ESP32-CAM + expansion board | 1 | Camera and WiFi gateway |
| 2-deck acrylic 4WD chassis kit + wheels | 1 | Chosen 2026-10-15, mounted flipped (motors underneath); the first chassis is dropped. Check the new metal shafts fit the wheels |
| Single-color LEDs | assorted | Corner lights, driven by each wheel node |
| Basic stash: resistors, capacitors, wire, connectors | assorted | Includes LED resistors, 470 µF bulk caps (≥ 16 V), 100 nF motor caps |
| Multimeter | 1 | Buck output, cell voltages, current draw |
| Logic analyzer | 1 | CAN debugging with PulseView |
| Half-size breadboards | 2+ | One per side pod, bench phases |
| Raspberry Pi 3B+ | 1 | Optional bench CAN sniffer |
