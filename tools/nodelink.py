"""Serial link to a node: opening the port, sending commands, reading lines.

Shared by send_cmd.py, plot_run.py and live.py, so the port setup that avoids
resetting the board exists in one place.
"""

import os
import termios
import time
from typing import NamedTuple

import serial

BOARDS = ("c3", "cam")
DEFAULT_PORTS = {"c3": "/dev/ttyACM0", "cam": "/dev/ttyUSB0"}
CAM_BAUD = 115200
CAM_BOOT_S = 1.0  # after the one reset prepare_cam_port() may cause: boot until the console reads


def open_port(port: str, board: str = "c3") -> serial.Serial:
    """Open a node's console port without resetting the board.

    board: "c3" for the wheel node's USB-Serial-JTAG port, "cam" for the
    gateway's UART0 through the ESP32-CAM-MB's CH340.
    """
    if board == "c3":
        return _open_c3(port)
    if board == "cam":
        return _open_cam(port)
    raise ValueError(f"unknown board {board!r}, expected one of {BOARDS}")


def _open_c3(port: str) -> serial.Serial:
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


def prepare_cam_port(port: str) -> bool:
    """Leave the port's speed setting at B0 with the CH340 running at 115200.
    True if this had to be done, which resets the ESP32 once.

    On the ESP32-CAM-MB, RTS drives the ESP32's EN (reset) and DTR its IO0
    (boot mode), each through its own transistor. Linux raises both lines on
    every open, unless the port's speed setting is B0 ("hang up"); the setting
    is kept between opens until the adapter is unplugged. The CH340 driver
    leaves the chip at its last real baud rate when B0 is set, so the data
    still flows at 115200 (findings.md, 2026-11-08).
    """
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        if attrs[4] == termios.B0 and attrs[5] == termios.B0:
            return False  # prepared earlier: this open left the lines alone
        # This open raised DTR and RTS: the ESP32 is in reset with IO0 low.
        # Give the CH340 the real rate first, since B0 keeps whatever it has
        # (9600 after plugging in, esptool's rate after a flash).
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        # B0 drops DTR and RTS in one USB request, so EN and IO0 rise
        # together and the ESP32 boots normally.
        attrs[4] = attrs[5] = termios.B0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        return True
    finally:
        os.close(fd)


def _open_cam(port: str) -> serial.Serial:
    reset = prepare_cam_port(port)
    s = serial.Serial()
    s.port = port
    s.baudrate = 0  # B0: keeps the lines down; the CH340 stays at CAM_BAUD
    s.timeout = 0.05
    s.dtr = False  # IO0 high
    s.rts = False  # EN high
    s.open()
    if reset:
        time.sleep(CAM_BOOT_S)
    return s


def send(s, line: str) -> None:
    s.write((line + "\n").encode("ascii"))
    s.flush()


class LineReader:
    """Splits the incoming bytes into lines; a line cut off at the end of one
    read is kept until the rest of it arrives."""

    def __init__(self) -> None:
        self._pending = b""

    def feed(self, data: bytes) -> list[str]:
        self._pending += data
        *lines, self._pending = self._pending.split(b"\n")
        return [raw.decode("utf-8", errors="replace").strip() for raw in lines]


def is_telemetry(line: str) -> bool:
    return line.startswith("T,")


class CanFrame(NamedTuple):
    ms: int  # gateway uptime when it took the frame from its queue
    id: int
    data: bytes


def parse_canmon(line: str) -> CanFrame | None:
    """A gateway CANMON line, "C,<ms>,<id hex>,<len>,<data hex>", as a frame;
    None for any other line or a malformed one."""
    if not line.startswith("C,"):
        return None
    fields = line[2:].split(",")
    if len(fields) != 4:
        return None
    ms, can_id, length, data = fields
    try:
        frame = CanFrame(int(ms, 10), int(can_id, 16), bytes.fromhex(data))
        n = int(length, 10)
    except ValueError:
        return None
    if n != len(frame.data) or n > 8 or not 0 <= frame.id <= 0x7FF or frame.ms < 0:
        return None
    return frame


class Telemetry:
    """Turns "T," lines into rows. The node sends a header line after TELEM ON;
    rows before it have no column names and are skipped, and so are rows whose
    length doesn't match it. A new header (after a reset, or from other
    firmware) replaces the old one."""

    def __init__(self) -> None:
        self.header: list[str] | None = None

    def parse(self, line: str) -> list[str] | None:
        """The row's fields for a data row, else None (a header, or a row that
        can't be matched to one)."""
        fields = line[2:].split(",")
        if not fields[0].isdigit():
            self.header = fields
            return None
        if self.header is None or len(fields) != len(self.header):
            return None
        return fields
