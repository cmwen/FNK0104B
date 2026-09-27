#!/usr/bin/env python3
"""Copy PlatformIO builds into a GitHub Pages ESP Web Tools site."""

import argparse
import configparser
import json
import os
from pathlib import Path
import shutil


ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "web-flasher"
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
    "hello": ("Hello", "USB serial and board status diagnostic"),
    "display": ("Display", "Display diagnostic"),
    "touch": ("Touch", "Touch diagnostic"),
    "calculator": ("Calculator", "Display and touch calculator"),
    "wifi": ("Wi-Fi", "Wi-Fi diagnostic"),
    "connectivity": ("Connectivity", "Connectivity UI"),
    "nvs": ("NVS", "Nonvolatile storage diagnostic"),
    "sd": ("SD card", "SD card diagnostic"),
    "mqtt": ("MQTT", "MQTT app"),
    "ota": ("OTA", "OTA app"),
    "codex-monitor": ("Codex monitor", "Codex monitor app"),
    "audio-diag": ("Audio", "Microphone diagnostic"),
    "locallink": ("LocalLink", "Local network speech UI"),
}


def firmware_environments():
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "platformio.ini")
    environments = [
        section.removeprefix("env:")
        for section in config.sections()
        if section.startswith("env:")
        and section not in {"env:native", "env:hello-debug"}
    ]
    unknown = set(environments) - NAMES.keys()
    if unknown:
        raise ValueError(f"Add catalog labels for environments: {', '.join(sorted(unknown))}")
    return environments


def package(output: Path, version: str):
    if output.exists():
        if not output.is_dir() or not (output / OUTPUT_MARKER).is_file():
            raise ValueError(f"Refusing to replace a directory not made by this packager: {output}")
        shutil.rmtree(output)
    shutil.copytree(SITE, output, ignore=shutil.ignore_patterns("firmware"))
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
        for name, offset in PARTS:
            source = boot_app0 if name == "boot_app0.bin" else image_dir / name
            if not source.is_file() or not source.stat().st_size:
                raise FileNotFoundError(f"Build {environment} before packaging: {source}")
            shutil.copyfile(source, target / name)
            parts.append({"path": name, "offset": offset})
        if (target / "firmware.bin").stat().st_size > 3 * 1024 * 1024:
            raise ValueError(f"{environment} exceeds the configured 3 MiB app partition")
        manifest = {
            "name": f"FNK0104B {NAMES[environment][0]}",
            "version": version,
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
