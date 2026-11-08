#!/usr/bin/env python3
"""Send console commands to a node and print each reply.

Each command waits for its one-line reply (OK ... / ERR ...) or a timeout, then
the script exits: there is no open-ended monitor. Opening the port does not
reset the board (see open_port in nodelink.py). Telemetry ("T,") and CAN
monitor ("C,") lines are skipped; IDF log lines that arrive meanwhile are
printed as they are.

Examples:
    tools/.venv/bin/python tools/send_cmd.py STATUS "HELP TELEM"
    tools/.venv/bin/python tools/send_cmd.py --board cam STATUS CAN
"""

import argparse
import sys
import time

from nodelink import BOARDS, DEFAULT_PORTS, LineReader, is_telemetry, open_port, parse_canmon, send


def ask(s, cmd: str, timeout: float) -> bool:
    """Send one command, print what comes back; True if the reply was OK."""
    print(f">>> {cmd}")
    end = time.monotonic() + timeout
    reader = LineReader()
    while time.monotonic() < end:
        for line in reader.feed(s.read(4096)):
            if not line or is_telemetry(line) or parse_canmon(line) is not None:
                continue
            print(f"<<< {line}")
            if line.startswith(("OK", "ERR")):
                return line.startswith("OK")
    print("<<< (no reply)")
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("commands", nargs="+", help="one command per argument (quote it if it has spaces)")
    ap.add_argument("--board", choices=BOARDS, default="c3",
                    help="c3: wheel node (USB-Serial-JTAG), cam: gateway (CH340); sets the default port")
    ap.add_argument("--port", help=f"default {DEFAULT_PORTS['c3']} for c3, {DEFAULT_PORTS['cam']} for cam")
    ap.add_argument("--timeout", type=float, default=1.0, help="seconds to wait for each reply")
    args = ap.parse_args()

    ok = True
    with open_port(args.port or DEFAULT_PORTS[args.board], args.board) as s:
        s.reset_input_buffer()
        for cmd in args.commands:
            send(s, cmd)
            ok = ask(s, cmd, args.timeout) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
