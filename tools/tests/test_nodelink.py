import os
import termios

import pytest

from nodelink import CanFrame, LineReader, Telemetry, is_telemetry, open_port, parse_canmon, prepare_cam_port, send

from conftest import FakePort


def test_reader_splits_lines_and_keeps_a_cut_one():
    r = LineReader()
    assert r.feed(b"OK one\r\nT,1,2") == ["OK one"]
    assert r.feed(b",3\nERR two\n") == ["T,1,2,3", "ERR two"]
    assert r.feed(b"") == []


def test_reader_survives_bad_bytes():
    assert LineReader().feed(b"\xff\xfeOK\n") == ["��OK"]


def test_is_telemetry():
    assert is_telemetry("T,1,2")
    assert not is_telemetry("OK telem on")
    assert not is_telemetry("")


def test_rows_need_the_header_first():
    t = Telemetry()
    assert t.parse("T,100,1,0") is None  # no header yet
    assert t.parse("T,t_ms,mode,duty_permille") is None
    assert t.header == ["t_ms", "mode", "duty_permille"]
    assert t.parse("T,110,1,200") == ["110", "1", "200"]


def test_rows_of_the_wrong_length_are_skipped():
    t = Telemetry()
    t.parse("T,t_ms,mode")
    assert t.parse("T,110,1,200") is None
    assert t.parse("T,110") is None


def test_a_new_header_replaces_the_old_one():
    t = Telemetry()
    t.parse("T,t_ms,mode")
    t.parse("T,t_ms,mode,setpoint_milli")
    assert t.header == ["t_ms", "mode", "setpoint_milli"]
    assert t.parse("T,1,3,60000") == ["1", "3", "60000"]


def test_send_adds_the_newline():
    p = FakePort()
    send(p, "STATUS")
    assert p.written == ["STATUS\n"]


def test_canmon_line_parses():
    assert parse_canmon("C,1234,180,7,F401320301A500") == CanFrame(1234, 0x180, bytes.fromhex("F401320301A500"))


def test_canmon_frame_without_data():
    assert parse_canmon("C,5,7FF,0,") == CanFrame(5, 0x7FF, b"")


@pytest.mark.parametrize("line", [
    "OK canmon on",
    "T,1,2,3",
    "C,1234,180,7",                  # a field missing
    "C,1234,180,3,F40132,00",        # one too many
    "C,1234,180,2,F40132",           # len doesn't match the data
    "C,1234,800,0,",                 # not an 11-bit ID
    "C,1234,180,9,000000000000000000",
    "C,x,180,1,00",
    "C,1234,180,1,0",                # half a byte
    "C,-1,180,0,",
])
def test_other_and_malformed_lines_are_not_frames(line):
    assert parse_canmon(line) is None


def test_unknown_board_is_refused():
    with pytest.raises(ValueError):
        open_port("/dev/null", "esp32")


def _speed(name):
    fd = os.open(name, os.O_RDWR | os.O_NOCTTY)
    try:
        a = termios.tcgetattr(fd)
        return a[4], a[5]
    finally:
        os.close(fd)


def test_cam_port_is_left_at_b0_once():
    # A pty keeps its termios between opens while the master is open, as the
    # CH340 port does while plugged in.
    master, slave = os.openpty()
    try:
        name = os.ttyname(slave)
        a = termios.tcgetattr(slave)
        a[4] = a[5] = termios.B9600  # as after plugging in
        termios.tcsetattr(slave, termios.TCSANOW, a)

        assert prepare_cam_port(name) is True
        assert _speed(name) == (termios.B0, termios.B0)
        assert prepare_cam_port(name) is False  # already prepared: no reset
    finally:
        os.close(slave)
        os.close(master)
