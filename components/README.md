# Components

ESP-IDF components used by the firmware projects in `firmware/`. A component is a directory with its own `CMakeLists.txt` (calling `idf_component_register`), sources and an `include/` directory for its public header.

## What goes here

Code goes into a component, rather than into a project's `main/`, if either of these is true:

- **Both firmware projects need it.** It lives once here instead of being copied into `firmware/wheel_node` and `firmware/gateway`.
- **It needs no hardware.** A component with no ESP-IDF dependencies (no `REQUIRES` in its `CMakeLists.txt`, no ESP-IDF includes) also builds with the PC's gcc, so `test/host` can unit-test it. Code that touches pins, interrupts or FreeRTOS stays in `main/`, and the hardware-free part of it is split out into a component. For example, `encoder.c` in the wheel node handles the GPIO, the interrupt handler and the lock, and calls `tiny_qdec` for the decoding.

## How a project uses them

Each project's top-level `CMakeLists.txt` adds this directory with `set(EXTRA_COMPONENT_DIRS ".../../components")`. That makes the components available but doesn't link them. A project's `main/CMakeLists.txt` lists the components it uses under `PRIV_REQUIRES`. Both projects build with `MINIMAL_BUILD`, so a component that no one requires isn't compiled.

## The components

| Component | What it does | Used by | Host-tested |
|---|---|---|---|
| `tiny_common` | Logs the boot banner (role, app version, chip, ESP-IDF version) | wheel node, gateway | no: it needs ESP-IDF |
| `tiny_cmd` | Text command parser and dispatcher (`tiny_cmd`), and the line reader that assembles console input into lines (`tiny_line`) | wheel node | yes |
| `tiny_qdec` | Quadrature decoder: turns the encoder's A/B levels into a signed position and counts invalid transitions | wheel node | yes, plus mutation tests |

`tiny_qdec` has a `linker.lf` (an ESP-IDF linker fragment) that places its code in IRAM and its table in DRAM. The encoder interrupt handler calls it and is registered as IRAM-safe, so it keeps running while the flash is busy. Any function it calls must then not be in flash.

## More

- **API:** each component's header in `include/` describes its functions, types and conventions.
- **Tests:** `test/README.md` lists what the host tests cover and what they don't.
- **Adding a component:** create the directory with `CMakeLists.txt`, the source and `include/`, then add it to `PRIV_REQUIRES` of the project that uses it. If it's hardware-free, also add it to `test/host/CMakeLists.txt` with a `test_<component>.c`.
