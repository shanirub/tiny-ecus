# Development Setup

How to build and flash tiny-ecus on a new Linux machine.

## Toolchain: ESP-IDF v6.1

The project is pinned to **ESP-IDF v6.1**, allowing 6.1.x patch releases. Building with any other version fails on purpose: each project's `main/idf_component.yml` declares the `idf` version it needs.

### 1. Prerequisites (Fedora)

```sh
sudo dnf install git wget flex bison gperf python3 cmake ninja-build ccache dfu-util libusbx
```

Other distributions: see `docs/en/get-started/linux-macos-setup-legacy.rst` in the ESP-IDF tree.

### 2. Install

One directory per IDF version, so several versions can coexist:

```sh
git clone -b v6.1 --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v6.1
cd ~/esp/esp-idf-v6.1
./install.sh esp32,esp32c3
```

The shallow clone is enough for building a pinned release. The tools (compilers, Python venv) go in `~/.espressif`, which is shared between IDF versions.

Espressif's default installer since v6.0 is EIM, the ESP-IDF Installation Manager (`sudo dnf install eim` after adding Espressif's repo). Either method works. This guide uses the classic `install.sh` route.

### 3. Activate in each shell

Add an alias to `~/.zshrc` (or `~/.bashrc`):

```sh
alias idf61='. ~/esp/esp-idf-v6.1/export.sh'
```

Then run `idf61` in any new terminal before using `idf.py`. Check it with `idf.py --version`, which should print `ESP-IDF v6.1`.

If `idf61` fails with "Checking python dependencies ... FAILED" and the diagnostics name a Python env for another IDF version (for example `idf6.2_py3.14_env`), the shell already had a different IDF activated (an IDE extension can do this in its terminals). Clear it and activate again:

```sh
unset IDF_PATH IDF_PYTHON_ENV_PATH
idf61
```

### Updating to a patch release

Install the new tag next to the old one, for example `~/esp/esp-idf-v6.1.1`, and point the alias at it. The version constraint accepts any 6.1.x.

## Serial port access

Your user must be in the `dialout` group:

```sh
sudo usermod -aG dialout $USER   # then log out and back in
```

| Board | Connection | Typical port |
|---|---|---|
| ESP32-C3 Super Mini (wheel node) | Native USB (USB-Serial-JTAG) | `/dev/ttyACM*` |
| ESP32-CAM on its expansion board (gateway) | USB-UART bridge on the expansion board | `/dev/ttyUSB*` |

## Build, flash, monitor

Each firmware is a separate ESP-IDF project with its chip target fixed in `sdkconfig.defaults`, so there's no need to run `idf.py set-target`.

```sh
idf61
cd firmware/wheel_node          # or firmware/gateway
idf.py build
idf.py -p /dev/ttyACM0 flash monitor   # Ctrl+] exits the monitor
```

Both boards blink their onboard LED at 1 Hz. The gateway still logs a heartbeat every 5 s; the wheel node instead has a text console on the same USB port (see Bench tools). On the ESP32-CAM the blinking red LED faces the expansion board, so only its glow is visible at the edge between the boards.

Both boards normally enter the bootloader on their own (auto-reset over DTR/RTS). If flashing fails to connect:
- **ESP32-C3:** hold BOOT, tap RESET, release BOOT, then flash again.
- **ESP32-CAM:** hold the IO0 button on the expansion board while flashing starts.

## Host unit tests

Hardware-free code (the `tiny_cmd` parser and the `tiny_qdec` encoder decoder) is also compiled and tested on the PC with CMake, CTest and Unity. CI runs the same tests on every push.

```sh
cmake -S test/host -B build/host        # add -DSANITIZE=OFF if the line below fails
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

By default the tests build with AddressSanitizer and UndefinedBehaviorSanitizer, which abort on memory errors and undefined behavior. They need the runtime libraries: on Fedora `sudo dnf install libasan libubsan`. Without them, configure with `-DSANITIZE=OFF`; CI always runs them on.

## Bench tools

Python scripts in `tools/` talk to the wheel node's console over USB. Set up their venv once (about 440 MB, most of it PyQt6, which `plot_run.py` needs to open a plot window):

```sh
python3 -m venv tools/.venv
tools/.venv/bin/pip install -r tools/requirements.txt
```

- `tools/send_cmd.py STATUS "HELP TELEM"`: sends each command and prints its one-line reply (`OK ...` or `ERR ...`). Exits non-zero if any reply was `ERR` or missing.
- `tools/plot_run.py --seconds 5 --label test`: switches telemetry on, records it for a fixed time, then saves a CSV and a PNG under `logs/` and opens a plot window. `--cmd "..."` sends extra commands at the start; `--no-show` skips the window.

Both open the port in a way that doesn't reset the C3 (DTR high, RTS low; see `open_port()` in `plot_run.py`). `idf.py monitor` does reset it, which is useful to see the boot log. Only one program can hold the port at a time.

## Git workflow

- Firmware, CI and build changes: work on a branch, open a pull request to `main` and wait for the CI build to pass. The user reviews and approves every PR and does the squash-merge; Claude opens PRs but never merges them (2026-10-22).
- Planning files, bench logs and docs: commit straight to `main`.
