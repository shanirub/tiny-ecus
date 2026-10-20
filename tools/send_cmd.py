#!/usr/bin/env python3
"""Send console commands to a wheel node and print each reply.

Each command waits for its one-line reply (OK ... / ERR ...) or a timeout, then
the script exits: there is no open-ended monitor. Opening the port does not
reset the board (see open_port in plot_run.py). Telemetry lines ("T,") are
skipped; IDF log lines that arrive meanwhile are printed as they are.

Example:
    tools/.venv/bin/python tools/send_cmd.py STATUS "HELP TELEM"
"""

import argparse
import sys
import time

from plot_run import open_port, send


def ask(s, cmd: str, timeout: float) -> bool:
    """Send one command, print what comes back; True if the reply was OK."""
    print(f">>> {cmd}")
    end = time.monotonic() + timeout
    pending = b""
    while time.monotonic() < end:
        pending += s.read(4096)
        *lines, pending = pending.split(b"\n")
        for raw in lines:
            line = raw.decode("utf-8", errors="replace").strip()
            if not line or line.startswith("T,"):
                continue
            print(f"<<< {line}")
            if line.startswith(("OK", "ERR")):
                return line.startswith("OK")
    print("<<< (no reply)")
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("commands", nargs="+", help="one command per argument (quote it if it has spaces)")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--timeout", type=float, default=1.0, help="seconds to wait for each reply")
    args = ap.parse_args()

    ok = True
    with open_port(args.port) as s:
        s.reset_input_buffer()
        for cmd in args.commands:
            send(s, cmd)
            ok = ask(s, cmd, args.timeout) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
