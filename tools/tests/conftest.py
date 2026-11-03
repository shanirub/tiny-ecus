import pathlib
import sys
import threading
import time

import pytest
import serial

# The tools are scripts in tools/, not a package.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))


class FakePort:
    """Stands in for the node's serial port: tests push the bytes the node
    would send and read back what the tool wrote."""

    def __init__(self) -> None:
        self._in = bytearray()
        self._lock = threading.Lock()
        self.written: list[str] = []
        self.closed = False
        self.broken = False

    # Node side, for the tests.
    def push(self, text: str) -> None:
        with self._lock:
            self._in += text.encode()

    def sent(self) -> list[str]:
        return [line for chunk in self.written for line in chunk.splitlines()]

    # pyserial side, for the tool.
    def read(self, size: int) -> bytes:
        if self.broken:
            raise serial.SerialException("device disconnected")
        with self._lock:
            data = bytes(self._in[:size])
            del self._in[:size]
        if not data:
            time.sleep(0.01)  # like the real port's read timeout
        return data

    def write(self, data: bytes) -> None:
        if self.broken:
            raise serial.SerialException("device disconnected")
        self.written.append(data.decode())

    def flush(self) -> None:
        pass

    def close(self) -> None:
        self.closed = True


class FakeNode:
    """Hands out FakePorts like open_port() does; while `unplugged`, opening
    fails as it does with the cable out."""

    def __init__(self) -> None:
        self.ports: list[FakePort] = []
        self.unplugged = False

    def open(self, name: str) -> FakePort:
        if self.unplugged:
            raise serial.SerialException(f"could not open port {name}")
        self.ports.append(FakePort())
        return self.ports[-1]

    @property
    def port(self) -> FakePort:
        return self.ports[-1]


@pytest.fixture
def node() -> FakeNode:
    return FakeNode()
