#!/usr/bin/env python3
"""Optional hidapi smoke probe. Close Desktop first to avoid two RPC writers.

Install the `hidapi` Python distribution in a local virtual environment.
This script never types keys or changes persistent device settings.
"""
import argparse
import json
import time

VID, PID, REPORT_ID = 0x303A, 0x8360, 6


def reports(message):
    data = json.dumps(message, separators=(",", ":")).encode() + b"\r\n"
    for offset in range(0, len(data), 61):
        chunk = data[offset:offset + 61]
        yield bytes([REPORT_ID, 2, len(chunk)]) + chunk + bytes(61 - len(chunk))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--serial", help="select one device when multiple are present")
    parser.add_argument("--send-status", action="store_true", help="send a synthetic slot-0 lighting status")
    parser.add_argument("--listen", type=float, default=5, help="listen duration in seconds")
    args = parser.parse_args()
    import hid  # provided by the hidapi distribution, not the unrelated hid package
    devices = [d for d in hid.enumerate(VID, PID)
               if d.get("usage_page") in (0, 0xFF00) and
               (not args.serial or d.get("serial_number") == args.serial)]
    if args.list:
        for d in devices:
            print(json.dumps({k: d.get(k) for k in ("product_string", "usage_page", "usage", "interface_number")}))
        return
    if len(devices) != 1:
        raise SystemExit(f"Expected one vendor interface, found {len(devices)}; use --list / --serial")
    device = hid.device()
    device.open_path(devices[0]["path"])
    try:
        for message in ({"m": "device.status", "id": 1}, {"m": "sys.version", "id": 2}):
            for report in reports(message):
                if device.write(report) != len(report):
                    raise RuntimeError("short HID write")
        if args.send_status:
            for report in reports({"m": "v.oai.thstatus", "p": [{"id": 0, "c": 65280, "b": 0.5, "e": 1, "s": 0}]}):
                if device.write(report) != len(report):
                    raise RuntimeError("short HID write")
        buffer = bytearray()
        deadline = time.monotonic() + max(0, args.listen)
        while time.monotonic() < deadline:
            packet = bytes(device.read(64, timeout_ms=100))
            if not packet:
                continue
            if len(packet) == 64 and packet[0] == REPORT_ID:
                packet = packet[1:]
            if len(packet) < 2 or packet[0] != 2 or packet[1] > 61 or packet[1] > len(packet) - 2:
                print("invalid report")
                buffer.clear()
                continue
            buffer.extend(packet[2:2 + packet[1]])
            if len(buffer) > 4096:
                raise RuntimeError("oversized response")
            while b"\n" in buffer:
                line, _, tail = buffer.partition(b"\n")
                buffer = bytearray(tail)
                if line.strip():
                    print(json.dumps(json.loads(line)))
        print("Probe complete. Host traffic alone does not prove Desktop discovery.")
    finally:
        device.close()


if __name__ == "__main__":
    main()
