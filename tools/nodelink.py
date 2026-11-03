"""Serial link to a wheel node: opening the port, sending commands, reading lines.

Shared by send_cmd.py, plot_run.py and live.py, so the port setup that avoids
resetting the board exists in one place.
"""

import serial


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
