#!/usr/bin/env python3
"""Check the real Rasta desktop and viewer-driven GDB/MI shutdown under Xvfb."""

import ctypes as c
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time


class client_data(c.Union):
    _fields_ = [("values", c.c_long * 5)]


class client_message(c.Structure):
    _fields_ = [("type", c.c_int), ("serial", c.c_ulong),
                ("send_event", c.c_int), ("display", c.c_void_p),
                ("window", c.c_ulong), ("message_type", c.c_ulong),
                ("format", c.c_int), ("data", client_data)]


class event(c.Union):
    _fields_ = [("client", client_message), ("padding", c.c_long * 24)]


def x_function(library, name, result, *arguments):
    function = getattr(library, name)
    function.restype = result
    function.argtypes = list(arguments)
    return function


def viewer_window(display, xlib):
    root = c.c_ulong(xlib.XDefaultRootWindow(display))
    parent = c.c_ulong()
    children = c.POINTER(c.c_ulong)()
    count = c.c_uint()
    assert xlib.XQueryTree(display, root.value, c.byref(root), c.byref(parent),
                           c.byref(children), c.byref(count))
    found = None
    for window in children[:count.value]:
        title = c.c_void_p()
        if xlib.XFetchName(display, window, c.byref(title)) and title.value:
            name = c.string_at(title).decode()
            xlib.XFree(title)
            if name.startswith("rasta "):
                found = window, name
    if children:
        xlib.XFree(children)
    return found


def descendants(parent):
    """Record owned descendants before shutdown, without matching global names."""
    parents = {}
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            status = (entry / "stat").read_text().rsplit(")", 1)[1].split()
            parents[int(entry.name)] = int(status[1])
        except (FileNotFoundError, ProcessLookupError):
            pass
    owned = {parent}
    while True:
        added = {pid for pid, ppid in parents.items() if ppid in owned} - owned
        if not added:
            return owned - {parent}
        owned.update(added)


def main():
    runner, vision, screenshot = sys.argv[1:]
    xlib = c.CDLL("libX11.so.6")
    pointer, word = c.c_void_p, c.c_ulong
    x_function(xlib, "XOpenDisplay", pointer, c.c_char_p)
    x_function(xlib, "XDefaultRootWindow", word, pointer)
    x_function(xlib, "XQueryTree", c.c_int, pointer, word,
               c.POINTER(word), c.POINTER(word), c.POINTER(c.POINTER(word)),
               c.POINTER(c.c_uint))
    x_function(xlib, "XFetchName", c.c_int, pointer, word, c.POINTER(pointer))
    x_function(xlib, "XFree", c.c_int, pointer)
    x_function(xlib, "XInternAtom", word, pointer, c.c_char_p, c.c_int)
    x_function(xlib, "XSendEvent", c.c_int, pointer, word, c.c_int,
               c.c_long, c.POINTER(event))
    x_function(xlib, "XFlush", c.c_int, pointer)
    display = xlib.XOpenDisplay(None)
    assert display, "Xvfb display is unavailable"
    with subprocess.Popen(["bash", runner, "gdb", "--interpreter=mi2", vision],
                          stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE) as process:
        owned = set()
        try:
            selector = selectors.DefaultSelector()
            selector.register(process.stdout, selectors.EVENT_READ)
            output = b""

            def await_output(marker):
                nonlocal output
                deadline = time.monotonic() + 15
                while marker not in output and time.monotonic() < deadline:
                    for key, _ in selector.select(0.1):
                        chunk = os.read(key.fd, 65536)
                        assert chunk, "GDB/MI closed unexpectedly: " + output.decode()
                        output += chunk
                assert marker in output, "Missing GDB/MI response: " + output.decode()

            await_output(b"(gdb)")
            process.stdin.write(b"-exec-run\n")
            process.stdin.flush()
            await_output(b"*running")
            time.sleep(2)
            owned = descendants(process.pid)
            found = viewer_window(display, xlib)
            assert found, "Rasta viewer did not open"
            window, title = found
            assert "32bpp" in title and "inverse=off" in title, title
            Path(screenshot).parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["import", "-window", str(window), screenshot], check=True)
            pixels = subprocess.check_output(["convert", screenshot, "-depth", "8", "rgb:-"])
            assert len(pixels) == 904 * 900 * 3, "Unexpected viewer dimensions"

            def paper(x, y):
                start = (y * 904 + x) * 3
                return all(channel > 240 for channel in pixels[start:start + 3])

            # Sample AES menu/title/scrollbar paper as well as Native's client.
            for x, y in [(880, 8), (800, 42), (865, 400), (500, 720), (780, 200)]:
                assert paper(x, y), f"Expected white paper at {x},{y}; see {screenshot}"
            for x, y in [(800, 21), (36, 400), (57, 78)]:
                start = (y * 904 + x) * 3
                assert all(channel < 16 for channel in pixels[start:start + 3]), \
                    f"Expected black border at {x},{y}; see {screenshot}"
            close = event()
            close.client.type = 33  # ClientMessage / WM_DELETE_WINDOW
            close.client.display = display
            close.client.window = window
            close.client.message_type = xlib.XInternAtom(display, b"WM_PROTOCOLS", 0)
            close.client.format = 32
            close.client.data.values[0] = xlib.XInternAtom(display, b"WM_DELETE_WINDOW", 0)
            assert xlib.XSendEvent(display, window, 0, 0, c.byref(close))
            xlib.XFlush(display)
            # Keep MI stdin open: its EOF must not be responsible for shutdown.
            assert process.wait(timeout=5) == 0, "Viewer closure failed the session"
            for pid in owned:
                stat = Path(f"/proc/{pid}/stat")
                if stat.exists():
                    state = stat.read_text().rsplit(")", 1)[1].split()[0]
                    assert state == "Z", f"Session process {pid} survived viewer closure"
            assert not viewer_window(display, xlib), "Viewer survived session shutdown"
        finally:
            for pid in descendants(process.pid) | owned:
                try:
                    os.kill(pid, 9)
                except ProcessLookupError:
                    pass
            if process.poll() is None:
                process.kill()
            _, errors = process.communicate(timeout=5)
            if process.returncode:
                sys.stderr.buffer.write(errors)
    completed = subprocess.run(["bash", runner, "bash", "-c", "exit 7"],
                               capture_output=True, timeout=10)
    assert completed.returncode == 7, "Command exit status was lost"
    assert not completed.stdout, "Session logs polluted GDB/MI stdout"
    assert not viewer_window(display, xlib), "Viewer survived command exit"
    print("Rasta desktop polarity, live GDB/MI viewer closure and command exit passed")


if __name__ == "__main__":
    main()
