from nodelink import LineReader, Telemetry, is_telemetry, send

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
