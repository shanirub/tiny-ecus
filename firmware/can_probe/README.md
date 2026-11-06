# can_probe

A CAN bring-up app for both boards (Phase 2 item B): it sends a counter frame every 100 ms, prints every frame it receives, and once a second prints its CAN status. It runs on the gateway board (ESP32-CAM, `esp32`) and on a wheel node board (ESP32-C3, `esp32c3`) from the same source, using `components/tiny_can`. The pins are chosen by the target in `main/main.c`:

| Board | Target | TX | RX | Counter frame ID |
|---|---|---|---|---|
| ESP32-CAM gateway | `esp32` | GPIO14 | GPIO13 | `0x7E0` |
| ESP32-C3 wheel node | `esp32c3` | GPIO4 | GPIO3 | `0x7E1` |

The bus runs at 500 kbit/s. Wiring: `docs/images/phase2-a-wiring.png`.

There is no console yet: the mode is chosen when building (Kconfig, menu `can_probe`). A console comes with `tiny_console` (Phase 2 item C).

**Flashing it replaces the wheel node firmware**, so nothing drives the DRV8833's pins on the C3 board: keep the motor supply off while `can_probe` runs there.

## Build and flash

`can_probe` has no target in `sdkconfig.defaults`, so each chip gets its own build directory and `sdkconfig`. From this directory, with ESP-IDF v6.1 active:

```sh
# t is esp32 (gateway, /dev/ttyUSB0) or esp32c3 (wheel node, /dev/ttyACM0)
idf.py -B build-$t -D SDKCONFIG=build-$t/sdkconfig -D IDF_TARGET=$t build
idf.py -B build-$t -p /dev/ttyACM0 flash
```

**Self-test build:** add `-D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.selftest"` and use `build-$t-selftest` as the directory. In self-test mode a frame needs no acknowledgement and the node receives its own frames, so one node can be checked with nothing else on the bus.

Kconfig options (`idf.py -B build-$t menuconfig`, menu `can_probe`):

| Option | Default | Meaning |
|---|---|---|
| `CAN_PROBE_SELF_TEST` | n | self-test mode (set by `sdkconfig.selftest`) |
| `CAN_PROBE_TX_ID` | `0x7E0` on `esp32`, `0x7E1` otherwise | ID of the counter frame; 0 sends nothing |
| `CAN_PROBE_PERIOD_MS` | 100 | counter frame period |
| `CAN_PROBE_FLOOD_FPS` | 0 | extra 8-byte frames per second on ID `0x7FF`, for the E.5 stress check; 4000 fills the bus |

## Output

One line per event on the console (USB-Serial-JTAG on the C3, UART0 on the gateway), after the usual boot log:

```
R,7E0,4,02000000
S,state=active,tec=0,rec=0,tx_ok=3821,tx_failed=0,tx_rejected=0,rx=3764,rx_dropped=0,bus_errors=0,bus_off=0,gaps=0,flood_rx=0,flood_rejected=0
G,7E0,55,0
```

| Line | Meaning |
|---|---|
| `R,<id>,<len>,<data hex>` | a received frame (frames on the flood ID `0x7FF` are only counted) |
| `G,<id>,<expected>,<got>` | a counter frame from that ID arrived out of order: a frame was lost, or the sender restarted |
| `S,...` | once a second: error state (`active`, `warning`, `passive`, `bus-off`), TEC and REC (transmit and receive error counters), frames sent / failed / rejected because the TX queue was full, frames received / dropped because the RX queue was full, bus errors, bus-off events, counter gaps, flood frames received / rejected |

The counter frame is 4 bytes, the counter as a little-endian `uint32`, so `02000000` is 2. The counters in `S` lines count from boot, so a bench run is judged by how they change during it: a flash or reset of the other board makes this one's frames go unacknowledged for a moment, which adds bus errors and TEC (see `.planning/can-rc-car/findings.md`, "CAN bring-up on the bench").

## Reading the output

Never run `idf.py monitor`; read for a fixed time instead. On the C3, `tools/nodelink.py`'s `open_port()` opens the port without resetting it. On the ESP32-CAM-MB, opening the port always restarts the gateway (the reset is in the USB-serial adapter's wiring); open it once with DTR and RTS low and keep it open for the whole run. A proper fix for the gateway is part of item C.
