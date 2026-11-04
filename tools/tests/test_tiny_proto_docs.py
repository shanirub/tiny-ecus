"""tiny_proto: the README's frame tables and examples, and the generated C,
checked against the DBC (the single source of the frame layout)."""

import importlib.util
import pathlib
import re

import cantools
import pytest

PROTO = pathlib.Path(__file__).resolve().parents[2] / "components" / "tiny_proto"
README = (PROTO / "README.md").read_text()
DB = cantools.database.load_file(str(PROTO / "tiny_proto.dbc"))

FRAME_HEADING = re.compile(r"^### .*ID `(0x[0-9A-Fa-f]+)`, DLC (\d+)", re.MULTILINE)


def table_after(pos: int) -> list[list[str]]:
    """The rows (cells stripped) of the first Markdown table after `pos`,
    without the header and separator lines."""
    rows = []
    for line in README[pos:].splitlines():
        if line.startswith("|"):
            rows.append([c.strip() for c in line.strip().strip("|").split("|")])
        elif rows:
            break
    return rows[2:]


def frame_tables():
    for m in FRAME_HEADING.finditer(README):
        yield int(m.group(1), 16), int(m.group(2)), table_after(m.end())


def byte_span(cell: str) -> tuple[int, int]:
    first, _, last = cell.replace("–", "-").partition("-")
    return int(first), int(last or first)


def test_readme_documents_every_frame():
    documented = {frame_id for frame_id, _, _ in frame_tables()}
    assert documented == {msg.frame_id for msg in DB.messages}


@pytest.mark.parametrize("frame_id,dlc,rows", list(frame_tables()),
                         ids=lambda v: hex(v) if isinstance(v, int) and v > 8 else None)
def test_frame_table_matches_dbc(frame_id, dlc, rows):
    msg = DB.get_message_by_frame_id(frame_id)
    assert msg.length == dlc
    covered = set()
    for byte_cell, field, typ, scale, _range, meaning in rows:
        first, last = byte_span(byte_cell)
        bits = re.findall(r"bit (\d+) = `(\w+)`", meaning)
        if "bit field" in typ:
            assert first == last, f"{field}: a bit field is one byte"
            assert bits, f"{field}: name each bit's signal as bit N = `name`"
            for bit, name in bits:
                sig = msg.get_signal_by_name(name)
                assert (sig.start, sig.length) == (first * 8 + int(bit), 1), name
                covered.add(name)
            continue
        sig = msg.get_signal_by_name(field.strip("`"))
        assert sig.start == first * 8, f"{sig.name}: start byte"
        assert sig.length == (last - first + 1) * 8, f"{sig.name}: length"
        assert sig.byte_order == "little_endian", sig.name
        assert sig.is_signed == typ.startswith("int"), f"{sig.name}: signedness"
        number = re.match(r"[\d.]+", scale)
        assert sig.scale == (float(number.group()) if number else 1), f"{sig.name}: scale"
        assert sig.offset == 0, sig.name
        covered.add(sig.name)
    assert covered == {s.name for s in msg.signals}, "every signal has a row"


EXAMPLE_ROW = re.compile(
    r"^\| `(\w+)` \| `([^`]+)` \| `(0x[0-9A-Fa-f]+) \[(\d)\] ([0-9A-Fa-f ]+)` \|$", re.MULTILINE)
EXAMPLES = EXAMPLE_ROW.findall(README)


def test_readme_has_examples():
    assert {name for name, *_ in EXAMPLES} == {msg.name for msg in DB.messages}


@pytest.mark.parametrize("name,values,frame_id,dlc,data", EXAMPLES)
def test_example_matches_dbc(name, values, frame_id, dlc, data):
    msg = DB.get_message_by_name(name)
    assert msg.frame_id == int(frame_id, 16)
    assert msg.length == int(dlc)
    phys = {k: float(v) for k, v in (kv.split("=") for kv in values.split())}
    expected = bytes.fromhex(data)
    assert msg.encode(phys) == expected
    decoded = msg.decode(expected, decode_choices=False)
    assert decoded == pytest.approx(phys)


def test_generated_c_is_up_to_date():
    spec = importlib.util.spec_from_file_location("tiny_proto_generate", PROTO / "generate.py")
    gen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gen)
    for name, text in gen.generate().items():
        assert gen.OUTPUTS[name].read_text() == text, f"{name}: run components/tiny_proto/generate.py"
