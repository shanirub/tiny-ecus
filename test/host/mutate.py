#!/usr/bin/env python3
"""Mutation tests: check that the host tests catch deliberate bugs.

Each mutant is one small text change to a component's source. The script
builds the unchanged source first (it must pass), then each mutant with the
same test file, and runs it. A mutant is *killed* if the tests fail, crash or
time out, and *survives* if they still pass. Any survivor means the tests have
a blind spot, and the script exits non-zero.

The mutants are matched as exact text. If the source changes so that one no
longer matches exactly once, the script stops with "stale mutant": update its
text here along with the source.

Needs gcc and the Unity sources, which the host test build fetches:
    cmake -S test/host -B build/host && cmake --build build/host
    python3 test/host/mutate.py

UndefinedBehaviorSanitizer runs in trap mode (no runtime library needed), so
undefined behavior such as signed overflow or an out-of-bounds table read
stops the test.
"""

import argparse
import pathlib
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]

TARGETS = {
    "tiny_qdec": {
        "source": "components/tiny_qdec/tiny_qdec.c",
        "include": "components/tiny_qdec/include",
        "test": "test/host/test_tiny_qdec.c",
        # (what the bug is, original text, replacement)
        "mutants": [
            ("one table entry's -1 becomes +1 (prev 00, now 01)",
             "0,  -1,  +1,  INV,  // prev 00",
             "0,  +1,  +1,  INV,  // prev 00"),
            ("an INV table entry becomes 0",
             "INV, +1,  -1,   0,",
             "0, +1,  -1,   0,"),
            ("prev and now swapped in the table index",
             "((prev & 3u) << 2) | (now & 3u)",
             "((now & 3u) << 2) | (prev & 3u)"),
            ("mask dropped in tiny_qdec_step",
             "s_step[((prev & 3u) << 2) | (now & 3u)]",
             "s_step[(prev << 2) | now]"),
            ("mask dropped in tiny_qdec_update",
             "    now &= 3u;\n",
             ""),
            ("no resync: state only updated on valid steps",
             "    q->state = now;\n}",
             "}"),
            ("invalid not counted",
             "q->invalid++;",
             ";"),
            ("an invalid step also moves the position",
             "q->invalid++;",
             "q->invalid++;\n        q->position += step;"),
            ("plain signed add (overflow is undefined)",
             "(int32_t)((uint32_t)q->position + (uint32_t)step)",
             "q->position + step"),
            ("init ignores the start state",
             "q->state = state & 3u;",
             "q->state = 0;"),
            ("init leaves invalid as it was",
             "    q->invalid = 0;\n    q->state = state & 3u;",
             "    q->state = state & 3u;"),
            ("zero also resets the state",
             "    q->invalid = 0;\n}",
             "    q->invalid = 0;\n    q->state = 0;\n}"),
        ],
    },
    "tiny_motor": {
        "source": "components/tiny_motor/tiny_motor.c",
        "include": "components/tiny_motor/include",
        "test": "test/host/test_tiny_motor.c",
        "mutants": [
            ("duty cap not applied",
             "steps > 0 ? clamp(duty, -cfg->cap, cfg->cap) : 0",
             "steps > 0 ? duty : 0"),
            ("a run of 0 steps holds its duty",
             "steps > 0 ? clamp(duty, -cfg->cap, cfg->cap) : 0",
             "clamp(duty, -cfg->cap, cfg->cap)"),
            ("no slew limit: duty jumps to the target",
             "m->duty += clamp(m->target - m->duty, -cfg->slew, cfg->slew);",
             "m->duty = m->target;"),
            ("a FAULT doesn't refuse new runs",
             "    if (m->mode == TINY_MOTOR_FAULT) {\n        return false;\n    }\n",
             ""),
            ("stop leaves the old duty, so the next run starts from it",
             "    m->target = 0;\n    m->duty = 0;\n    m->steps_left = 0;\n    m->stall_steps = 0;\n}",
             "    m->target = 0;\n    m->steps_left = 0;\n    m->stall_steps = 0;\n}"),
            ("stall check only above stall_duty, not at it",
             "abs32(m->duty) >= cfg->stall_duty",
             "abs32(m->duty) > cfg->stall_duty"),
            ("stall check ignores the sign of counts",
             "abs32(counts) <= cfg->stall_counts",
             "counts <= cfg->stall_counts"),
            ("movement doesn't reset the stall count",
             "    } else {\n        m->stall_steps = 0;\n    }",
             "    }"),
            ("the run ends one step late",
             "--m->steps_left == 0",
             "m->steps_left-- == 0"),
            ("never returns to IDLE after a run",
             "        m->mode = TINY_MOTOR_IDLE;\n    }\n    return m->duty;",
             "    }\n    return m->duty;"),
            ("forward and reverse swapped on the pins",
             "*in1 = duty < 0 ? brake : full;",
             "*in1 = duty > 0 ? brake : full;"),
            ("fast decay instead of slow: the pulse is the drive, not the brake",
             "uint32_t brake = full - (uint32_t)((uint64_t)full * (uint32_t)abs32(duty) / TINY_MOTOR_DUTY_MAX);",
             "uint32_t brake = (uint32_t)((uint64_t)full * (uint32_t)abs32(duty) / TINY_MOTOR_DUTY_MAX);"),
        ],
    },
}

CFLAGS = ["-std=c11", "-O1", "-g",
          "-fsanitize=undefined", "-fsanitize-undefined-trap-on-error"]
TIMEOUT_S = 30


def build_and_run(target, code, unity, workdir):
    """Return (passed, detail) for the tests built against `code`."""
    src = workdir / pathlib.Path(target["source"]).name
    src.write_text(code)
    exe = workdir / "test"
    cmd = ["gcc", *CFLAGS,
           "-I", str(REPO / target["include"]), "-I", str(unity),
           str(src), str(REPO / target["test"]), str(unity / "unity.c"),
           "-o", str(exe)]
    build = subprocess.run(cmd, capture_output=True, text=True)
    if build.returncode != 0:
        sys.exit(f"build failed:\n{build.stderr}")
    try:
        run = subprocess.run([str(exe)], capture_output=True, text=True,
                             timeout=TIMEOUT_S)
    except subprocess.TimeoutExpired:
        return False, "timed out"
    if run.returncode < 0:
        return False, f"crashed (signal {-run.returncode})"
    fails = [l.split(":", 2)[-1] for l in run.stdout.splitlines() if ":FAIL" in l]
    return run.returncode == 0, "; ".join(fails[:2])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--unity", type=pathlib.Path,
                    default=REPO / "build/host/_deps/unity-src/src",
                    help="Unity's src directory (default: %(default)s)")
    args = ap.parse_args()
    if not (args.unity / "unity.c").is_file():
        sys.exit(f"no unity.c in {args.unity}; build test/host first")

    survivors = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, target in TARGETS.items():
            code = (REPO / target["source"]).read_text()
            passed, detail = build_and_run(target, code, args.unity, pathlib.Path(tmp))
            if not passed:
                sys.exit(f"{name}: tests fail on the unchanged source ({detail})")
            print(f"{name}: unchanged source passes")
            for i, (what, old, new) in enumerate(target["mutants"], 1):
                if code.count(old) != 1:
                    sys.exit(f"{name} mutant {i} ({what}): stale mutant, "
                             f"its text occurs {code.count(old)} times")
                passed, detail = build_and_run(target, code.replace(old, new),
                                               args.unity, pathlib.Path(tmp))
                status = "SURVIVED" if passed else "killed"
                survivors += passed
                print(f"  {i:2}. {status:8} {what}" + (f"  [{detail}]" if detail else ""))
    if survivors:
        sys.exit(f"{survivors} mutant(s) survived: the tests miss these bugs")
    print("all mutants killed")


if __name__ == "__main__":
    main()
