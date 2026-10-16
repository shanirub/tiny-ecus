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

If flashing fails to connect:
- **ESP32-C3:** hold BOOT, tap RESET, release BOOT, then flash again.
- **ESP32-CAM:** hold the IO0 button on the expansion board while flashing starts.

## Git workflow

- Firmware, CI and build changes: work on a branch, open a pull request to `main`, wait for the CI build to pass, then squash-merge.
- Planning files, bench logs and docs: commit straight to `main`.
