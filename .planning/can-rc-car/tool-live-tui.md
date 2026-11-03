# Live bench tool (TUI): plan

Status: **approved by the user 2026-11-03**. Requested 2026-11-01: a tool in `tools/` that shows the wheel node's telemetry live and has a command line, because one program has to own the serial port for both. Own branch (`tools/live-tui`) and PR.

A *TUI* (text user interface) is a full-screen program in the terminal, with panels and keyboard input, like `htop`. It needs no window system and runs over SSH.

## What it is for, and what it isn't

**For the user at the bench.** Type commands, see the replies, and watch speed, setpoint and duty move as it happens. Today that takes either `send_cmd.py` (no live data) or `plot_run.py` (data, but only after a fixed-length run).

**Not for Claude.** The TUI is interactive and runs until you quit it, which is exactly what the "no never-ending serial terminal" rule excludes. `send_cmd.py` and `plot_run.py` stay unchanged as the scripted, time-bounded tools. Only one program can hold the port, so the TUI must be closed before Claude runs a script, and the other way round.

## Screen

```
 tiny-ecus live · /dev/ttyACM0 · fw b37cf25 · 100 rows/s                                 ● link ok
 ─────────────────────────────────────────────────────────────────────────────────────────────────
  SPEED run   setpoint  80.0 rpm   speed  79.6 rpm   duty  61.7 %   counts 57   drops 0   enc invalid 0
 ┌ speed (rpm) ── setpoint · · · ── last 10 s ──────────────────────────────────────────────────────┐
 │ 100 ┤                                                                                            │
 │     │                                                ⡠⠊⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉⠉   │
 │  80 ┤· · · · · · · · · · · · · · · · · · · · · · · ·⡇· · · · · · · · · · · · · · · · · · · · ·   │
 │  60 ┤                                               ⡇                                            │
 │  40 ┤          ⡠⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠜                                             │
 │  30 ┤· · · · ·⡇· · · · · · · · · · · · · · · · · · · ·                                           │
 │   0 ┤⣀⣀⣀⣀⣀⣀⣀⣀⠇                                                                                   │
 │     └┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────┘
 │     -10            -8            -6            -4            -2             0  s                 │
 ├ duty (%) ───────────────────────────────────────────────────────────────────────────────────────┤
 │  80 ┤                                                ⢠⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒   │
 │  40 ┤         ⣠⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠤⠜                                              │
 │   0 ┤⣀⣀⣀⣀⣀⣀⣀⣀⠇                                                                                   │
 │ -80 ┤                                                                                            │
 └──────────────────────────────────────────────────────────────────────────────────────────────────┘
 ┌ console ─────────────────────────────────────────────────────────────────────────────────────────┐
 │ 14:46:31  >>> SPEED 30 6                                                                         │
 │ 14:46:31  <<< OK speed=30 rpm for 6 s                                                            │
 │ 14:46:34  >>> SPEED 80 3                                                                         │
 │ 14:46:34  <<< OK speed=80 rpm for 3 s                                                            │
 └──────────────────────────────────────────────────────────────────────────────────────────────────┘
 > SET KI 25█                                                                     ↑↓ history
 Esc STOP   Ctrl+L clear console   F1 help   Ctrl+Q quit (sends STOP, TELEM OFF)
```

Other states change only the status line: a stall turns it red (`STALL — driver off, latched · press Esc or type STOP to clear`) until cleared; a lost port shows `○ link lost, retrying…` in the top bar and freezes the plots.

- **Status line:** the newest telemetry row as values with units: mode as a word, setpoint, speed, duty, plus `drops` and the firmware version from `STATUS`. A stall (mode 2) shows in red until it clears.
- **Plot:** the last 10 s of speed with the setpoint over it, and duty below on its own scale. Redrawn about 10 times a second. The node sends 100 rows a second, so redrawing on every row would only cost CPU.
- **Console:** every command sent and every line that isn't telemetry: `OK`/`ERR` replies, and ESP-IDF log lines (after a reset, for example). It scrolls back.
- **Input line:** sends one command on Enter, with history on the up and down arrows. It doesn't check commands itself; the node's `ERR usage: ...` already does that.

The columns come from the header line the node sends after `TELEM ON`, as in `plot_run.py`. So a new telemetry column in Phase 2 doesn't break the tool: it shows up in the status line as raw `name=value`, until the tool is taught its unit.

## Safety

- **Esc sends `STOP` at once**, whatever is typed in the input line. One key, always the same, never needs focus.
- **On quit**, including Ctrl+C or a closed terminal, the tool sends `STOP` and `TELEM OFF` before it releases the port, as the scripts do.
- **No automatic `STOP` on a stall row.** That rule is for Claude's unattended scripts. At the bench you are watching. The stall already has the driver off and latched, and an automatic `STOP` would clear the latch before you see it. The red status shows it, and you clear it with Esc or `STOP`.
- **No reset on open.** The port is opened the way `plot_run.py` does it: DTR high, RTS low. Opening it the wrong way resets the C3, as seen 2026-10-20.
- **Port lost** (cable pulled, board reset by its button): the status line shows "link lost", and the tool reopens the port by itself once it's back. It doesn't crash. Nothing is sent automatically after a reconnect. A reset node boots idle with telemetry off, so the tool shows a hint to send `TELEM ON`.

## Recording: later, not in this PR

Left out by user decision (2026-11-03): a nice-to-have, and `plot_run.py` already records. When wanted: Ctrl+R toggles writing the rows to `logs/<time>-tui.csv`, in `plot_run.py`'s format, flushed every batch and closed on quit; `REC` in the top bar.

## Structure

- `tools/live.py`: the TUI.
- `tools/nodelink.py` (new): opening the port (`open_port`), sending a line, and splitting the incoming bytes into telemetry rows and other lines. These are moved out of `plot_run.py`, which then imports them, so the port setup that avoids the reset exists in one place. The alternative is to import from `plot_run.py` as it is. That needs no change to it, but every start of the TUI would also load matplotlib, about 1 s. Decision for the user, see below.
- Reading the port runs in a background thread. It hands complete lines to the screen in batches, so a slow redraw never makes it lose bytes. If the screen falls behind, it keeps only the newest rows and counts the rest. Unlike the node's `drops`, that loss would be in the PC tool, not on the USB link.

## Library

**Textual 8.2.x** for the screen, and **plotext 6.1.x** for the plot, used directly in a small widget of our own (user decision, 2026-11-03). `textual-plotext` was dropped: its last release was 1.0.1 on 2024-11-30, it caps plotext below 6 (6.0 came out 2026-08-23), and it was never tested against Textual 8. Exact versions are pinned in `tools/requirements.txt`, each a release at least two weeks old.

Checked 2026-11-03 in a scratch venv (Textual 8.2.8, plotext 6.1.0, Python 3.14), with a headless prototype fed from `logs/20261101-214633-e-step-30-80.csv`:

- **plotext 6 has a new API.** The 5.x module functions (`plt.plot`, `plt.build`) are gone. Now `plotext.figure.signal(x, y).lines().label(...)`, `figure.draw(signal)`, `figure.ruler("y").lim(lo, hi)`, `figure.plot_size(w, h)`, `figure.theme(...)`, and `figure.build().string()` returns the plot as text with ANSI colors. Most examples online are for 5.x and no longer apply.
- **It works inside Textual.** `rich.text.Text.from_ansi(...)` in a `Static` widget renders the plot correctly. Seen in a headless screenshot.
- **It is fast enough.** One render of 1000 points at 78 × 14 cells takes about 13 ms, so 10 redraws a second cost about 13 % of one core. 5 Hz is the fallback if that's too much.
- **To fix in the real tool:**
  - plotext's default white background: use its `dusk` theme (user choice, 2026-11-03: dark blue frame, teal plot area, warm lines);
  - the legend covering part of the plot: show the series names in the panel title instead;
  - the setpoint line hidden under the speed line: draw the setpoint last;
  - the coarse block marker: use `braille`, 2 × 4 dots per cell;
  - redraw on terminal resize.

Textual's own widgets have no line plot: in 8.2.8 the only chart is `Sparkline`, one series and no axes. That's why plotext is needed for the speed-with-setpoint view.

## Tests

- **Without the board:** pytest tests that run the app headless against a fake port. pyserial's `loop://` port gives back what is written to it, so a test can feed it header and rows and check the screen. They cover:
  - parsing rows by header (including a changed header and a half line at the end of a read);
  - non-telemetry lines going to the console;
  - Esc sending `STOP`;
  - quit sending `STOP` and `TELEM OFF`;
  - the "link lost" state.
- **CI:** a new pytest job for `tools/`, next to the C host tests. It installs only what the tests need (pyserial, Textual, plotext, pytest), not PyQt6 or matplotlib's GUI backend.
- **On the bench (Claude, time-bounded):** the same headless mode against the real board, with a hard timeout. It sends `TELEM ON` and `SPEED 60 5`, checks for about 100 rows a second and a status line near 60 rpm, presses Esc mid-run, and checks `drive=idle`. Then it quits and checks that `send_cmd.py STATUS` can open the port, and that `up_ms` kept counting (no reset). PSU 7.4 V / 1.5 A, wheel free, as always.
- **On the bench (the user):** use it by hand for a few minutes and judge the layout and readability. That's the real acceptance test, because readability is the point of the tool.

## Docs

`tools/README.md` is new in this PR: the three tools (`send_cmd.py`, `plot_run.py`, the TUI), venv setup, and the TUI's keys and screen. The "Bench tools" section of `docs/dev-setup.md` moves there and leaves a one-line link (move, not copy).

## Decisions for the user

1. ~~Library~~: decided 2026-11-03, Textual + plotext directly (verified, see Library).
2. ~~Shared serial code~~: decided 2026-11-03, `tools/nodelink.py`, imported by `plot_run.py` and the TUI.
3. ~~CI~~: decided 2026-11-03, a pytest job for `tools/` in this PR.
4. ~~Docs~~: decided 2026-11-03, `tools/README.md` is created in this PR. The "Bench tools" section of `docs/dev-setup.md` moves there and leaves a one-line link, so that part of the later Mermaid + `tools/README.md` task is done here.
5. ~~Recording~~: decided 2026-11-03, not in this PR. It's a nice-to-have; `plot_run.py` covers recording. About 40 lines (Ctrl+R toggles a CSV in `logs/`, same format as `plot_run.py`), to be added later if wanted.

## Out of scope

- Several nodes at once, or the gateway: that's Phase 2 and later. The header-driven columns keep the door open.
- Changing gains or sending commands through buttons: the input line covers it.
- Firmware changes: none planned.
