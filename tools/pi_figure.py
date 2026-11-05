#!/usr/bin/env python3
"""Draw the README's PI speed-control figure from two recorded bench runs.

The runs are telemetry CSVs recorded with plot_run.py on 2026-11-01 (Phase 1 E,
spare motor, wheel free, PSU 7.4 V) and kept in docs/data/. Each row is one
10 ms control period. One encoder count per period is 1.389 rpm, so the raw
speed steps in 1.389 rpm; the figure also shows a 50 ms moving average.

    tools/.venv/bin/python tools/pi_figure.py   # writes docs/images/pi-speed-control.png
"""

import csv
import pathlib

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA = REPO / "docs" / "data"
OUT = REPO / "docs" / "images" / "pi-speed-control.png"
SPEED_MODE = 3  # telemetry mode of a SPEED run
AVERAGE = 5     # periods in the moving average (50 ms)

RUNS = [
    ("20261101-214633-e-step-30-80.csv", "Setpoint step 30 → 80 rpm", None),
    ("20261101-214750-e-drag60.csv", "60 rpm, finger pressed on the wheel", (2.8, 4.4)),
]


def load(name: str):
    with (DATA / name).open() as f:
        rows = [r for r in csv.DictReader(f) if int(r["mode"]) == SPEED_MODE]
    t0 = int(rows[0]["t_ms"])
    t = [(int(r["t_ms"]) - t0) / 1000 for r in rows]
    setpoint = [int(r["setpoint_milli"]) / 1000 for r in rows]
    rpm = [int(r["rpm_milli"]) / 1000 for r in rows]
    duty = [int(r["duty_permille"]) / 10 for r in rows]
    return t, setpoint, rpm, duty


def moving_average(xs: list[float], n: int) -> list[float]:
    out, total = [], 0.0
    for i, x in enumerate(xs):
        total += x
        if i >= n:
            total -= xs[i - n]
        out.append(total / min(i + 1, n))
    return out


def main() -> None:
    fig, axes = plt.subplots(2, len(RUNS), sharex="col", figsize=(11, 5.2),
                             gridspec_kw={"height_ratios": [2, 1]})
    for col, (name, title, load_span) in enumerate(RUNS):
        t, setpoint, rpm, duty = load(name)
        top, bottom = axes[0][col], axes[1][col]
        top.plot(t, rpm, color="0.75", linewidth=0.8, label="measured (10 ms)")
        top.plot(t, moving_average(rpm, AVERAGE), color="tab:blue", linewidth=1.6,
                 label="measured (50 ms average)")
        top.plot(t, setpoint, color="tab:orange", linewidth=1.4, linestyle="--", label="setpoint")
        bottom.plot(t, duty, color="tab:green", linewidth=1.2)
        if load_span:
            for ax in (top, bottom):
                ax.axvspan(*load_span, color="tab:red", alpha=0.08)
            top.text(sum(load_span) / 2, 2, "finger load", ha="center", color="tab:red")
        top.set_title(title)
        top.set_ylim(0, 95)
        bottom.set_ylim(0, 85)
        bottom.set_xlabel("time [s]")
        for ax in (top, bottom):
            ax.grid(True, alpha=0.3)
    axes[0][0].set_ylabel("wheel speed [rpm]")
    axes[1][0].set_ylabel("PWM duty [%]")
    axes[0][0].legend(loc="lower right", fontsize=8)
    fig.suptitle("PI speed control on the bench (ESP32-C3, 100 Hz loop, duty capped at 80 %)")
    fig.tight_layout()
    fig.savefig(OUT, dpi=110)
    print(f"wrote {OUT.relative_to(REPO)}")


if __name__ == "__main__":
    main()
