#!/usr/bin/env python3
"""Live view of a wheel node: telemetry plot, status line and a command line.

Interactive, runs until you quit it (Ctrl+Q). For scripted, time-bounded use
there are send_cmd.py and plot_run.py; only one program can hold the port.

Keys: Esc sends STOP at any time, Ctrl+Q quits (sending STOP and TELEM OFF
first), Up/Down walk the command history, Ctrl+L clears the console, F1 help.

Example:
    tools/.venv/bin/python tools/live.py
    tools/.venv/bin/python tools/live.py --port /dev/ttyACM1
"""

import argparse
import collections
import datetime
import queue
import signal
import sys
import threading
import time

import plotext
import serial
from rich.text import Text
from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.widgets import Footer, Input, RichLog, Static

from nodelink import LineReader, Telemetry, is_telemetry, open_port, send

WINDOW_S = 10.0     # plot span
REFRESH_S = 0.1     # screen updates; the node sends a row every 10 ms
POLL_S = 2.0        # hidden STATUS and ENC, for version, drops and invalid
STALE_S = 0.5       # no row for this long: telemetry is off or the link is gone
RETRY_S = 0.5       # reopening a lost port

MODES = {"0": "idle", "1": "DUTY run", "2": "STALL", "3": "SPEED run"}

HELP = (
    "keys: Esc STOP · Ctrl+Q quit (sends STOP, TELEM OFF) · Up/Down history · Ctrl+L clear · F1 this help\n"
    "type any node command and press Enter; HELP lists them, HELP <command> explains one"
)

LINK_UP = True
LINK_DOWN = False

# plotext caps plots at the terminal size it detects, which inside Textual (or
# a headless test) isn't the widget's size; the widget sizes the plot itself.
plotext.terminal.limit(False, False)


class Link:
    """Owns the port. A background thread reads lines into a queue and reopens
    the port when it's lost (cable pulled, board reset); the UI thread writes.
    The queue carries text lines, plus LINK_UP / LINK_DOWN when that changes."""

    def __init__(self, port_name: str, opener=open_port) -> None:
        self.port_name = port_name
        self.lines: queue.SimpleQueue = queue.SimpleQueue()
        self._opener = opener
        self._port = None
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, name="link", daemon=True)

    def start(self) -> None:
        self._thread.start()

    @property
    def up(self) -> bool:
        return self._port is not None

    def send(self, line: str) -> bool:
        with self._lock:
            if self._port is None:
                return False
            try:
                send(self._port, line)
                return True
            except (serial.SerialException, OSError):
                self._drop()
                return False

    def close(self) -> None:
        self._stop.set()
        self._thread.join(timeout=1.0)
        with self._lock:
            if self._port is not None:
                self._port.close()
                self._port = None

    def _drop(self) -> None:
        # Called with the lock held.
        if self._port is not None:
            try:
                self._port.close()
            except (serial.SerialException, OSError):
                pass
            self._port = None
            self.lines.put(LINK_DOWN)

    def _run(self) -> None:
        reader = LineReader()
        while not self._stop.is_set():
            port = self._port
            if port is None:
                try:
                    port = self._opener(self.port_name)
                except (serial.SerialException, OSError):
                    self._stop.wait(RETRY_S)
                    continue
                with self._lock:
                    self._port = port
                reader = LineReader()
                self.lines.put(LINK_UP)
            try:
                data = port.read(4096)
            except (serial.SerialException, OSError, TypeError, AttributeError):
                # TypeError/AttributeError: pyserial reading a port that the
                # UI thread closed meanwhile.
                with self._lock:
                    if self._port is port:
                        self._drop()
                continue
            for line in reader.feed(data):
                if line:
                    self.lines.put(line)


class Plot(Static):
    """A plotext plot rendered as text into a Textual widget."""

    def show(self, t: list[float], series: list[tuple[list[float], bool]],
             ylim: tuple[float, float], ystep: float) -> None:
        width, height = self.content_size
        if width < 20 or height < 5:
            return
        f = plotext.figure
        f.clear()
        f.theme("dusk")
        for values, thin in series:
            # A reference (the setpoint) in thin braille dots, drawn first; the
            # measured line in half blocks over it, so where the two agree the
            # measurement shows, and the setpoint shows wherever they differ.
            f.draw(f.signal(t, values, marker="braille" if thin else "hd").lines())
        xs = [-WINDOW_S + 2 * i for i in range(int(WINDOW_S / 2) + 1)]
        f.ruler("x").lim(-WINDOW_S, 0)
        f.ruler("x").ticks(xs, [f"{x:g} s" if x == 0 else f"{x:g}" for x in xs])
        lo, hi = ylim
        ys = [ystep * k for k in range(int(lo // ystep) + 1, int(hi // ystep) + 1)]
        f.ruler("y").lim(lo, hi)
        f.ruler("y").ticks(ys, [f"{y:g}" for y in ys])
        f.plot_size(width, height)
        self.update(Text.from_ansi(f.build().string()))


def speed_limits(values: list[float]) -> tuple[float, float]:
    """Fixed -10..100 rpm, so the picture doesn't jump, widened only when the
    data leaves it (a reverse run)."""
    lo = -10.0 if not values or min(values) >= -10 else min(-100.0, min(values) - 10)
    hi = 100.0 if not values or max(values) <= 100 else max(values) + 10
    return lo, hi


class LiveApp(App):
    TITLE = "tiny-ecus live"
    CSS = """
    Screen { layout: vertical; }
    #top { height: 1; background: $panel; padding: 0 1; }
    #status { height: 1; padding: 0 1; background: $boost; }
    #status.stall { background: $error; color: $text; text-style: bold; }
    #status.idle { color: $text-muted; }
    Plot { border: round $primary; border-title-color: $text; }
    #speed { height: 2fr; min-height: 8; }
    #duty { height: 1fr; min-height: 6; }
    #console { height: 1fr; min-height: 4; border: round $primary-darken-2; }
    #cmd { dock: bottom; margin-bottom: 1; }
    """
    BINDINGS = [
        Binding("escape", "stop", "STOP", priority=True),
        Binding("ctrl+q", "quit", "quit (STOP, TELEM OFF)", priority=True),
        Binding("ctrl+c", "quit", show=False, priority=True),
        Binding("ctrl+l", "clear", "clear console"),
        Binding("f1", "help", "help"),
        Binding("up", "history(-1)", show=False),
        Binding("down", "history(1)", show=False),
    ]

    def __init__(self, link: Link) -> None:
        super().__init__()
        self.link = link
        self.telemetry = Telemetry()
        self.rows: collections.deque[dict[str, str]] = collections.deque()
        self.last_row_at = 0.0
        self.dirty = False
        self.ever_up = False
        self.node = {"ver": "?", "drops": "?", "drive": "?", "invalid": "?"}
        self.hidden = {"STATUS": 0, "ENC": 0}
        self.history: list[str] = []
        self.history_pos = 0
        self._shut = False

    # --- layout ---

    def compose(self) -> ComposeResult:
        yield Static(id="top")
        yield Static(id="status")
        speed = Plot(id="speed")
        speed.border_title = "speed (rpm, thick) · setpoint (thin) · last 10 s"
        yield speed
        duty = Plot(id="duty")
        duty.border_title = "duty (%)"
        yield duty
        console = RichLog(id="console", wrap=True, markup=False)
        console.border_title = "console"
        yield console
        yield Input(id="cmd", placeholder="command, Enter sends · Up/Down history · Esc STOP")
        yield Footer()

    def on_mount(self) -> None:
        self.link.start()
        self.set_interval(REFRESH_S, self.tick)
        self.set_interval(POLL_S, self.poll)
        self.query_one("#cmd", Input).focus()
        self.refresh_top()
        self.refresh_status()

    # --- incoming ---

    def tick(self) -> None:
        while True:
            try:
                item = self.link.lines.get_nowait()
            except queue.Empty:
                break
            if item is LINK_UP:
                self.on_link_up()
            elif item is LINK_DOWN:
                self.on_link_down()
            else:
                self.on_line(item)
        self.refresh_top()
        self.refresh_status()
        if self.dirty:
            self.dirty = False
            self.refresh_plots()

    def on_link_up(self) -> None:
        if not self.ever_up:
            self.ever_up = True
            self.log_line("link up; sending TELEM ON")
            self.send_logged("TELEM ON")
        else:
            self.log_line("link back. If the node was reset, telemetry is off: send TELEM ON")
        self.poll()

    def on_link_down(self) -> None:
        self.hidden = {"STATUS": 0, "ENC": 0}
        self.log_line("link lost; retrying")

    def on_line(self, line: str) -> None:
        if is_telemetry(line):
            header = self.telemetry.header
            fields = self.telemetry.parse(line)
            if self.telemetry.header is not header:
                self.rows.clear()  # new columns: old rows don't fit them
            if fields is not None:
                self.add_row(dict(zip(self.telemetry.header, fields)))
            return
        hide = self.parse_reply(line)
        if not hide:
            self.log_line(f"<<< {line}")

    def add_row(self, row: dict[str, str]) -> None:
        self.rows.append(row)
        newest = int(row["t_ms"])
        while self.rows and newest - int(self.rows[0]["t_ms"]) > WINDOW_S * 1000:
            self.rows.popleft()
        self.last_row_at = time.monotonic()
        self.dirty = True

    def parse_reply(self, line: str) -> bool:
        """Picks up node state from STATUS and ENC replies; True if this reply
        answers a hidden poll and stays out of the console."""
        kind = None
        if line.startswith("OK up_ms="):
            kind = "STATUS"
        elif line.startswith("OK pos="):
            kind = "ENC"
        if kind is None:
            return False
        for word in line[3:].split():
            key, _, value = word.partition("=")
            if key in ("ver", "drops", "drive", "invalid"):
                self.node[key] = value
        if self.hidden[kind] > 0:
            self.hidden[kind] -= 1
            return True
        return False

    # --- outgoing ---

    def poll(self) -> None:
        for cmd in ("STATUS", "ENC"):
            if self.link.send(cmd):
                self.hidden[cmd] += 1

    def send_logged(self, cmd: str) -> None:
        if self.link.send(cmd):
            self.log_line(f">>> {cmd}")
        else:
            self.log_line(f"not sent, link down: {cmd}")

    def on_input_submitted(self, event: Input.Submitted) -> None:
        cmd = event.value.strip()
        event.input.value = ""
        if not cmd:
            return
        self.history.append(cmd)
        self.history_pos = len(self.history)
        self.send_logged(cmd)

    def action_stop(self) -> None:
        self.send_logged("STOP")

    def action_history(self, step: int) -> None:
        if not self.history:
            return
        self.history_pos = max(0, min(len(self.history), self.history_pos + step))
        box = self.query_one("#cmd", Input)
        box.value = self.history[self.history_pos] if self.history_pos < len(self.history) else ""
        box.cursor_position = len(box.value)

    def action_clear(self) -> None:
        self.query_one("#console", RichLog).clear()

    def action_help(self) -> None:
        self.log_line(HELP)

    async def action_quit(self) -> None:
        self.shutdown_link()
        self.exit()

    def shutdown_link(self) -> None:
        """STOP and TELEM OFF, then release the port. Safe to call twice."""
        if self._shut:
            return
        self._shut = True
        self.link.send("STOP")
        self.link.send("TELEM OFF")
        self.link.close()

    # --- drawing ---

    def log_line(self, text: str) -> None:
        stamp = datetime.datetime.now().strftime("%H:%M:%S")
        self.query_one("#console", RichLog).write(f"{stamp}  {text}")

    def rate(self) -> int:
        if not self.rows:
            return 0
        newest = int(self.rows[-1]["t_ms"])
        return sum(1 for r in self.rows if newest - int(r["t_ms"]) < 1000)

    def fresh(self) -> bool:
        return self.rows and time.monotonic() - self.last_row_at < STALE_S

    def refresh_top(self) -> None:
        link = "● link ok" if self.link.up else "○ link lost, retrying…"
        rate = f"{self.rate()} rows/s" if self.fresh() else "no telemetry"
        self.query_one("#top", Static).update(
            f"tiny-ecus live · {self.link.port_name} · fw {self.node['ver']} · {rate}   {link}")

    def refresh_status(self) -> None:
        bar = self.query_one("#status", Static)
        bar.remove_class("stall", "idle")
        tail = f"drops {self.node['drops']}   enc invalid {self.node['invalid']}"
        if not self.fresh():
            bar.add_class("idle")
            bar.update(f"telemetry off (TELEM ON shows it)   drive {self.node['drive']}   {tail}")
            return
        row = self.rows[-1]
        mode = row.get("mode", "?")
        if mode == "2":
            bar.add_class("stall")
            bar.update("STALL: driver off, latched. Press Esc or type STOP to clear")
            return
        parts = [MODES.get(mode, f"mode {mode}")]
        known = {"t_ms", "mode"}
        if "setpoint_milli" in row:
            parts.append(f"setpoint {int(row['setpoint_milli']) / 1000:6.1f} rpm")
            known.add("setpoint_milli")
        if "rpm_milli" in row:
            parts.append(f"speed {int(row['rpm_milli']) / 1000:6.1f} rpm")
            known.add("rpm_milli")
        if "duty_permille" in row:
            parts.append(f"duty {int(row['duty_permille']) / 10:5.1f} %")
            known.add("duty_permille")
        if "counts" in row:
            parts.append(f"counts {row['counts']}")
            known.add("counts")
        # Columns this tool doesn't know yet (newer firmware) show raw.
        parts += [f"{k}={v}" for k, v in row.items() if k not in known]
        bar.update("   ".join(parts) + "   " + tail)

    def refresh_plots(self) -> None:
        if not self.rows:
            return
        newest = int(self.rows[-1]["t_ms"])
        t = [(int(r["t_ms"]) - newest) / 1000 for r in self.rows]

        def col(name: str, scale: float) -> list[float] | None:
            if name not in self.rows[-1]:
                return None
            return [int(r[name]) / scale for r in self.rows]

        speed = col("rpm_milli", 1000)
        setpoint = col("setpoint_milli", 1000)
        duty = col("duty_permille", 10)
        if speed is not None:
            series = [(speed, False)]
            if setpoint is not None:
                series.insert(0, (setpoint, True))
            self.query_one("#speed", Plot).show(t, series, speed_limits(speed + (setpoint or [])), 20)
        if duty is not None:
            self.query_one("#duty", Plot).show(t, [(duty, False)], (-105, 105), 100)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", default="/dev/ttyACM0")
    args = ap.parse_args()

    app = LiveApp(Link(args.port))
    # A closed terminal (SIGHUP) or a kill (SIGTERM) still sends STOP and TELEM OFF.
    for sig in (signal.SIGHUP, signal.SIGTERM):
        signal.signal(sig, lambda *_: app.exit())
    try:
        app.run()
    finally:
        app.shutdown_link()
    return 0


if __name__ == "__main__":
    sys.exit(main())
