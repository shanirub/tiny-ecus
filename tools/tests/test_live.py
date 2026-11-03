import asyncio

from textual.widgets import Input, RichLog, Static

import live

HEADER = "T,t_ms,mode,setpoint_milli,duty_permille,counts,rpm_milli\n"


def row(t_ms, mode=3, setpoint=60000, duty=470, counts=43, rpm=59722):
    return f"T,{t_ms},{mode},{setpoint},{duty},{counts},{rpm}\n"


def run(node, scenario, size=(110, 40)):
    """Runs the app headless with `scenario(app, pilot)`, then quits it."""
    async def main():
        app = live.LiveApp(live.Link("/dev/fake", opener=node.open))
        async with app.run_test(size=size) as pilot:
            await pilot.pause(0.3)  # link up, first ticks
            await scenario(app, pilot)
            await app.action_quit()
    asyncio.run(main())


def console(app) -> str:
    return "\n".join(line.text for line in app.query_one("#console", RichLog).lines)


def text(app, selector) -> str:
    return str(app.query_one(selector, Static).render())


def test_link_up_sends_telem_on_and_polls(node):
    async def scenario(app, pilot):
        sent = node.port.sent()
        assert sent[0] == "TELEM ON"
        assert "STATUS" in sent and "ENC" in sent
    run(node, scenario)


def test_rows_show_in_status_line_and_plot(node):
    async def scenario(app, pilot):
        node.port.push(HEADER + "".join(row(1000 + 10 * i) for i in range(50)))
        await pilot.pause(0.3)
        status = text(app, "#status")
        assert "SPEED run" in status
        assert "setpoint   60.0 rpm" in status
        assert "speed   59.7 rpm" in status
        assert "duty  47.0 %" in status
        assert "rows/s" in text(app, "#top")
        assert "─" in text(app, "#speed")  # the plot's frame was drawn
    run(node, scenario)


def test_unknown_columns_show_raw(node):
    async def scenario(app, pilot):
        node.port.push("T,t_ms,mode,rpm_milli,temp_c\nT,1000,1,5000,31\n")
        await pilot.pause(0.3)
        assert "temp_c=31" in text(app, "#status")
    run(node, scenario)


def test_stall_row_turns_status_red(node):
    async def scenario(app, pilot):
        node.port.push(HEADER + row(1000, mode=2, duty=0, counts=0, rpm=0))
        await pilot.pause(0.3)
        assert app.query_one("#status").has_class("stall")
        assert "STALL" in text(app, "#status")
    run(node, scenario)


def test_replies_and_log_lines_go_to_console(node):
    async def scenario(app, pilot):
        node.port.push("OK speed=60 rpm for 6 s\nI (123) boot: something\n")
        await pilot.pause(0.3)
        assert "<<< OK speed=60 rpm for 6 s" in console(app)
        assert "<<< I (123) boot: something" in console(app)
    run(node, scenario)


def test_poll_replies_stay_out_of_console_but_fill_the_status(node):
    async def scenario(app, pilot):
        node.port.push("OK up_ms=5 ver=abc1234 heap=1 telem=on drops=3 drive=idle duty=0 decay=slow probe=off\n"
                       "OK pos=10 invalid=2\n")
        await pilot.pause(0.3)
        assert "up_ms" not in console(app)
        assert "pos=10" not in console(app)
        assert "fw abc1234" in text(app, "#top")
        assert "drops 3" in text(app, "#status")
        assert "enc invalid 2" in text(app, "#status")
        # A STATUS the user asks for is shown.
        await pilot.press(*"STATUS", "enter")
        node.port.push("OK up_ms=6 ver=abc1234 heap=1 telem=on drops=3 drive=idle duty=0 decay=slow probe=off\n")
        await pilot.pause(0.3)
        assert "<<< OK up_ms=6" in console(app)
    run(node, scenario)


def test_enter_sends_the_command_and_up_recalls_it(node):
    async def scenario(app, pilot):
        await pilot.press(*"SPEED 60 6", "enter")
        await pilot.pause(0.1)
        assert "SPEED 60 6" in node.port.sent()
        assert ">>> SPEED 60 6" in console(app)
        box = app.query_one("#cmd", Input)
        assert box.value == ""
        await pilot.press("up")
        assert box.value == "SPEED 60 6"
        await pilot.press("down")
        assert box.value == ""
    run(node, scenario)


def test_escape_sends_stop_even_while_typing(node):
    async def scenario(app, pilot):
        await pilot.press(*"SPEED 9")
        await pilot.press("escape")
        await pilot.pause(0.1)
        assert node.port.sent()[-1] == "STOP"
    run(node, scenario)


def test_quit_sends_stop_and_telem_off_then_closes(node):
    async def main():
        app = live.LiveApp(live.Link("/dev/fake", opener=node.open))
        async with app.run_test() as pilot:
            await pilot.pause(0.3)
            await pilot.press("ctrl+q")
            await pilot.pause(0.1)
    asyncio.run(main())
    assert node.port.sent()[-2:] == ["STOP", "TELEM OFF"]
    assert node.port.closed


def test_lost_link_is_shown_and_reopened(node):
    async def scenario(app, pilot):
        node.unplugged = True
        node.port.broken = True
        await pilot.pause(0.3)
        assert "link lost" in text(app, "#top")
        assert "link lost; retrying" in console(app)
        await pilot.press(*"STOP", "enter")
        await pilot.pause(0.1)
        assert "not sent, link down: STOP" in console(app)
        node.unplugged = False
        await pilot.pause(1.0)
        assert len(node.ports) == 2
        assert "link ok" in text(app, "#top")
        assert "send TELEM ON" in console(app)
        assert "TELEM ON" not in node.port.sent()  # nothing that changes the node after a reconnect
    run(node, scenario)


def test_speed_limits():
    assert live.speed_limits([]) == (-10, 100)
    assert live.speed_limits([0, 60, 80]) == (-10, 100)
    assert live.speed_limits([0, 105]) == (-10, 115)
    assert live.speed_limits([-60, 0]) == (-100, 100)
