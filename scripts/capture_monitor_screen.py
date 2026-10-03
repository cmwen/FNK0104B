#!/usr/bin/env python3
"""Capture the monitor's actual LCD through PlatformIO's serial terminal."""
import argparse
import os
from pathlib import Path
import pty
import re
import select
import struct
import subprocess
import time
import zlib

WIDTH, HEIGHT = 320, 240
ROW = re.compile(r"^monitor_screenshot row=(\d+) data=([0-9a-f]+)$")


def decode_rows(lines):
    rows = {}
    started = False
    for line in lines:
        line = line.strip()
        if line == "monitor_screenshot begin width=320 height=240 format=rgb888":
            started = True
            rows.clear()
        elif started and line == "monitor_screenshot end":
            if set(rows) != set(range(HEIGHT)):
                raise ValueError("Incomplete screenshot: missing LCD rows")
            return b"".join(rows[y] for y in range(HEIGHT))
        elif started and line.startswith("monitor_screenshot error="):
            raise ValueError(line)
        elif started and (match := ROW.fullmatch(line)):
            y, data = int(match[1]), bytes.fromhex(match[2])
            if y >= HEIGHT or len(data) != WIDTH * 3 or y in rows:
                raise ValueError("Invalid or duplicate screenshot row")
            # TFT readback returns six significant bits per RGB channel.
            rows[y] = bytes(value | (value >> 6) for value in data)
    raise ValueError("No complete screenshot received")


def save_png(path, pixels):
    if len(pixels) != WIDTH * HEIGHT * 3:
        raise ValueError("Invalid framebuffer size")
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    raw = b"".join(b"\0" + pixels[y * WIDTH * 3:(y + 1) * WIDTH * 3] for y in range(HEIGHT))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def capture(port, timeout):
    master, slave = pty.openpty()
    process = subprocess.Popen(["pio", "device", "monitor", "-p", port, "-b", "115200",
                                "--dtr", "0", "--rts", "0"],
                               stdin=slave, stdout=slave, stderr=slave)
    os.close(slave)
    lines, pending = [], b""
    start = time.monotonic()
    terminal_at = None
    sent = False
    try:
        while time.monotonic() - start < timeout:
            if process.poll() is not None:
                raise RuntimeError("PlatformIO terminal exited before capture")
            if select.select([master], [], [], 0.25)[0]:
                pending += os.read(master, 65536)
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    text = line.decode("ascii", errors="replace").strip()
                    lines.append(text)
                    if "--- Terminal on" in text:
                        terminal_at = time.monotonic()
                    if not sent and text.startswith("monitor_status integration=connected"):
                        os.write(master, b"screenshot\n")
                        sent = True
                    if text == "monitor_screenshot end":
                        return decode_rows(lines)
                    if text.startswith("monitor_screenshot error="):
                        raise RuntimeError(text)
            if not sent and terminal_at and time.monotonic() - terminal_at >= 20:
                os.write(master, b"screenshot\n")
                sent = True
        raise TimeoutError("Timed out waiting for the LCD screenshot")
    finally:
        os.write(master, b"\x03")
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=5)
        os.close(master)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()
    pixels = capture(args.port, args.timeout)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    save_png(args.output, pixels)
    print(f"Captured actual LCD: {args.output} ({WIDTH}x{HEIGHT})")


if __name__ == "__main__":
    main()
