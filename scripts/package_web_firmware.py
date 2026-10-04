#!/usr/bin/env python3
"""Copy PlatformIO builds into a GitHub Pages ESP Web Tools site."""

import argparse
import configparser
import csv
import json
import os
from pathlib import Path
import shutil


ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "site" / "dist"
BUILD = ROOT / ".pio" / "build"
OUTPUT_MARKER = ".fnk0104b-pages"

# These are the image offsets used by the pinned Arduino ESP32 PlatformIO
# builder for ESP32-S3. Keep this list aligned with platformio.ini.
PARTS = (
    ("bootloader.bin", 0x0000),
    ("partitions.bin", 0x8000),
    ("boot_app0.bin", 0xE000),
    ("firmware.bin", 0x10000),
)

NAMES = {
    "hid-diag": ("USB HID diagnostic", "Composite USB keyboard and serial diagnostic. Types only when commanded over serial; switches USB from Serial/JTAG to TinyUSB."),
    "keyboard": ("Emoji and numpad keyboard", "USB touchscreen number pad and OS-specific emoji shortcuts. Windows emoji search, Mac Character Viewer, and Linux GTK Unicode entry. Uses TinyUSB HID plus serial."),
    "hello": ("Hello", "USB serial diagnostic. Reports the chip, flash, PSRAM, and free heap; it does not initialize the display."),
    "display": ("Display", "Cycles red, green, blue, white, and black screens to check the display panel."),
    "avatar-diag": ("Avatar animation", "Shows four generated pixel avatars with animated agent states and reports rendering speed over USB serial."),
    "touch": ("Touch", "Reads screen taps and prints their coordinates over USB serial."),
    "calculator": ("Calculator", "Touchscreen calculator with decimal input, sign toggle, backspace, clear, and chained basic arithmetic."),
    "wifi": ("Wi-Fi", "Shows a setup QR code for Espressif's phone app to provision 2.4 GHz Wi-Fi. Credentials are saved on the board."),
    "wifi-ble": ("Wi-Fi over BLE", "Use this page and the code shown on the board to securely set up 2.4 GHz Wi-Fi over Bluetooth."),
    "connectivity": ("Connectivity", "Touchscreen tools to scan for and join Wi-Fi networks, scan BLE devices, or advertise a BLE diagnostic device."),
    "nvs": ("NVS", "Placeholder only: reports startup information but does not test nonvolatile storage yet."),
    "sd": ("SD card diagnostic", "Lists files from the FNK0104B four-bit SDIO card over serial without formatting it."),
    "file-manager": ("SD file manager", "Touchscreen SD browser with storage capacity and read-only text and hex file viewing."),
    "mqtt": ("MQTT", "Placeholder only: does not connect to an MQTT broker yet."),
    "ota": ("OTA", "User-confirmed HTTPS firmware update from a GitHub Release, with touchscreen and serial controls."),
    "codex-monitor": ("Codex monitor", "Shows Codex limits and agents, wakes on Hi ESP for local commands, and submits VAD-ended voice to a local bridge. Includes secure Web BLE Wi-Fi setup; preserves Arduino Wi-Fi storage and replaces OTA/FATFS data."),
    "audio-diag": ("Audio", "Captures one second of onboard microphone audio at a time and reports signal detection; it never prints or saves samples."),
    "speaker-diag": ("Speaker keyboard", "Plays notes from a touchscreen keyboard with an adjustable speaker volume."),
    "locallink": ("LocalLink", "Records speech with the onboard microphone, discovers a Speech Recognition service on your LAN, and shows the returned transcript. Requires Wi-Fi and a compatible service."),
    "screen-timeout": ("Screen timeout", "Turns the display backlight off after 60 seconds of inactivity and wakes it on touch."),
    "button-diag": ("Button", "Checks the verified expansion GPIO14 button input over USB serial."),
}


def firmware_environments():
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "platformio.ini")
    # Standalone IDF diagnostics are built separately in CI. The monitor
    # uses its own speech image list below; other entries use Arduino images.
    environments = [
        section.removeprefix("env:")
        for section in config.sections()
        if section.startswith("env:")
        and section not in {"env:native", "env:hello-debug", "env:speech-diag", "env:recorder", "env:recorder-io-diag"}
    ]
    unknown = set(environments) - NAMES.keys()
    if unknown:
        raise ValueError(f"Add catalog labels for environments: {', '.join(sorted(unknown))}")
    return environments


def monitor_partitions():
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "platformio.ini")
    path = ROOT / config["env:codex-monitor"]["board_build.partitions"]
    with path.open() as stream:
        rows = list(csv.reader(line for line in stream if not line.lstrip().startswith("#")))
    app = next(row for row in rows if row and row[1].strip() == "app")
    model = next(row for row in rows if row and row[0].strip() == "model")
    return int(app[3], 0), int(app[4], 0), int(model[3], 0), int(model[4], 0)


def package(output: Path, version: str):
    if not (SITE / "ble-client.bundle.js").is_file():
        raise FileNotFoundError("Build web-flasher and the Astro site before packaging")
    if output.exists():
        if not output.is_dir() or not (output / OUTPUT_MARKER).is_file():
            raise ValueError(f"Refusing to replace a directory not made by this packager: {output}")
        shutil.rmtree(output)
    shutil.copytree(
        SITE,
        output,
        ignore=shutil.ignore_patterns(
            "firmware", "node_modules", "package.json", "package-lock.json",
            "ble-client.js", "test",
        ),
    )
    # Canonical documentation images are also served by the Pages app.
    shutil.copytree(ROOT / "docs" / "images", output / "docs" / "images", dirs_exist_ok=True)
    (output / OUTPUT_MARKER).touch()
    (output / ".nojekyll").touch()
    firmware_root = output / "firmware"
    firmware_root.mkdir()
    boot_app0 = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")) / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"
    if not boot_app0.is_file():
        raise FileNotFoundError(f"PlatformIO boot_app0 image is missing: {boot_app0}")

    catalog = {"version": version, "builds": []}
    for environment in firmware_environments():
        image_dir = BUILD / environment
        target = firmware_root / environment
        target.mkdir()
        parts = []
        image_parts = PARTS
        app_limit = 3 * 1024 * 1024
        if environment == "codex-monitor":
            app_offset, app_limit, model_offset, model_limit = monitor_partitions()
            image_parts = (("bootloader.bin", 0), ("partitions.bin", 0x8000),
                           ("firmware.bin", app_offset), ("srmodels/srmodels.bin", model_offset))
            if (image_dir / "srmodels/srmodels.bin").stat().st_size > model_limit:
                raise ValueError("Monitor models exceed their partition")
        for name, offset in image_parts:
            source = boot_app0 if name == "boot_app0.bin" else image_dir / name
            if not source.is_file() or not source.stat().st_size:
                raise FileNotFoundError(f"Build {environment} before packaging: {source}")
            (target / name).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target / name)
            parts.append({"path": name, "offset": offset})
        if (target / "firmware.bin").stat().st_size > app_limit:
            raise ValueError(f"{environment} exceeds the configured app partition")
        manifest = {
            "name": f"FNK0104B {NAMES[environment][0]}",
            "version": version,
            "new_install_prompt_erase": True,
            "builds": [{"chipFamily": "ESP32-S3", "parts": parts}],
        }
        (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        catalog["builds"].append({
            "id": environment,
            "name": NAMES[environment][0],
            "description": NAMES[environment][1],
            "manifest": f"firmware/{environment}/manifest.json",
        })
    (firmware_root / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")
    return len(catalog["builds"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", default=os.environ.get("GITHUB_SHA", "local")[:12])
    args = parser.parse_args()
    count = package(args.output.resolve(), args.version)
    print(f"Packaged {count} firmware builds in {args.output}")
