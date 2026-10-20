#!/usr/bin/env python3
"""Record one telemetry run from a wheel node and plot it.

Sends TELEM ON plus the given commands, records the "T," CSV lines for a fixed
number of seconds, sends TELEM OFF, saves the CSV (and a PNG) under logs/, and
plots it. The capture always ends on its own: there is no open-ended monitor.

Setup (once):
    python3 -m venv tools/.venv
    tools/.venv/bin/pip install -r tools/requirements.txt
Example:
    tools/.venv/bin/python tools/plot_run.py --seconds 5 --label sine-check
    tools/.venv/bin/python tools/plot_run.py --cmd "DUTY 30" --seconds 6
"""

import argparse
import csv
import datetime
import pathlib
import sys
import time

import serial

MAX_SECONDS = 60
REPO = pathlib.Path(__file__).resolve().parent.parent


def open_port(port: str) -> serial.Serial:
    # The ESP32-C3's USB-Serial-JTAG port resets the chip when RTS is high and
    # DTR low. Linux raises both on open, and pyserial then applies DTR before
    # RTS, so asking for both low passes through exactly that state and resets
    # the board (seen 2026-10-20). Keeping DTR high and dropping only RTS never
    # does: verified on the bench, no reset on open or close.
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200  # ignored by USB-Serial-JTAG, needed by pyserial
    s.timeout = 0.05
    s.dtr = True
    s.rts = False
    s.open()
    return s


def send(s: serial.Serial, line: str) -> None:
    s.write((line + "\n").encode("ascii"))
    s.flush()


def capture(s: serial.Serial, commands: list[str], seconds: float):
    """Return (header, rows, other_lines) from a time-bounded capture."""
    header: list[str] | None = None
    rows: list[list[str]] = []
    other: list[str] = []
    pending = b""

    def handle(raw: bytes) -> None:
        nonlocal header
        line = raw.decode("utf-8", errors="replace").strip()
        if not line.startswith("T,"):
            if line:
                other.append(line)
            return
        fields = line[2:].split(",")
        if header is None:
            if not fields[0].isdigit():
                header = fields  # the first T, line after TELEM ON is the header
            return  # samples before the header have no column names
        if not fields[0].isdigit():
            return  # a repeated header
        if len(fields) == len(header):
            rows.append(fields)

    def read_for(duration: float) -> None:
        nonlocal pending
        end = time.monotonic() + duration
        while time.monotonic() < end:
            pending += s.read(4096)
            *lines, pending = pending.split(b"\n")
            for raw in lines:
                handle(raw)

    s.reset_input_buffer()
    send(s, "TELEM ON")
    for cmd in commands:
        send(s, cmd)
    try:
        read_for(seconds)
    finally:
        send(s, "TELEM OFF")
    read_for(0.3)  # collect the last replies
    return header, rows, other


def save_csv(path: pathlib.Path, header: list[str], rows: list[list[str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def plot(path_png: pathlib.Path, title: str, header: list[str], rows: list[list[str]],
         show: bool) -> None:
    import matplotlib
    if not show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    t0 = int(rows[0][0])
    t = [(int(r[0]) - t0) / 1000.0 for r in rows]
    cols = header[1:]
    fig, axes = plt.subplots(len(cols), 1, sharex=True, squeeze=False,
                             figsize=(10, 2.5 * len(cols)))
    for i, name in enumerate(cols):
        ax = axes[i][0]
        ax.plot(t, [float(r[i + 1]) for r in rows], linewidth=1)
        ax.set_ylabel(name)
        ax.grid(True, alpha=0.3)
    axes[-1][0].set_xlabel("time [s]")
    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(path_png, dpi=120)
    if show:
        plt.show()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--seconds", type=float, default=5.0,
                    help=f"capture length (max {MAX_SECONDS})")
    ap.add_argument("--cmd", action="append", default=[],
                    help="command to send after TELEM ON (repeatable)")
    ap.add_argument("--label", default="", help="added to the file name")
    ap.add_argument("--out", type=pathlib.Path, default=REPO / "logs")
    ap.add_argument("--no-show", action="store_true", help="save the PNG without opening a window")
    args = ap.parse_args()

    if not 0 < args.seconds <= MAX_SECONDS:
        ap.error(f"--seconds must be in (0, {MAX_SECONDS}]")

    with open_port(args.port) as s:
        header, rows, other = capture(s, args.cmd, args.seconds)

    for line in other:
        print(f"  {line}")
    if header is None or not rows:
        print("no telemetry received", file=sys.stderr)
        return 1

    span = (int(rows[-1][0]) - int(rows[0][0])) / 1000.0
    rate = (len(rows) - 1) / span if span > 0 else 0.0
    print(f"{len(rows)} rows over {span:.2f} s ({rate:.1f} Hz), columns: {','.join(header)}")

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    name = f"{stamp}-{args.label}" if args.label else stamp
    csv_path = args.out / f"{name}.csv"
    save_csv(csv_path, header, rows)
    title = name + (f"  ({'; '.join(args.cmd)})" if args.cmd else "")
    plot(csv_path.with_suffix(".png"), title, header, rows, show=not args.no_show)
    print(f"saved {csv_path} and .png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
