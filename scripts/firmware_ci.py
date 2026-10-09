#!/usr/bin/env python3
"""Fingerprint firmware inputs and cache verified, portable release images."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from package_web_firmware import (
    ROOT, BUILD, BUILD_ONLY_ENVIRONMENTS, PARTS, firmware_environments, monitor_partitions,
)

SCHEMA = 1
# IDF compiles these libraries explicitly, independently of include discovery.
IDF_APPS = {
    "speech-diag": ("19-speech-diag", ("fnk0104b", "speech"), "19-speech-diag"),
    "recorder": ("20-recorder", ("fnk0104b", "speech", "recorder"), "20-recorder"),
    "recorder-io-diag": ("21-recorder-io-diag", ("fnk0104b",), "20-recorder"),
    "codex-monitor": ("codex-monitor", ("fnk0104b", "codex_hid", "speech", "locallink", "ui"), "codex-monitor"),
    "codex-audio-diag": ("25-codex-audio-diag", ("fnk0104b", "codex_hid", "speech", "locallink", "ui"), "codex-monitor"),
}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', re.MULTILINE)
REFERENCE = re.compile(r'\$\{([^}]+)\}')


def tracked_files(root):
    result = subprocess.run(["git", "ls-files", "-z"], cwd=root, check=True, capture_output=True)
    return set(result.stdout.decode().split("\0")) - {""}


def option(config, section, name, seen=()):
    identity = (section, name)
    if identity in seen:
        raise ValueError(f"Cyclic PlatformIO option: {identity}")
    if config.has_option(section, name):
        value = config[section][name]
    elif config.has_option(section, "extends"):
        parents = config[section]["extends"].replace(",", " ").split()
        values = [option(config, parent, name, (*seen, identity)) for parent in parents]
        value = next((value for value in values if value), "")
    else:
        return ""
    def replace(match):
        target, field = match[1].rsplit(".", 1)
        return option(config, target, field, (*seen, identity))
    return REFERENCE.sub(replace, value)


def inputs(root, environment, files=None):
    files = tracked_files(root) if files is None else set(files)
    files = {path for path in files if not path.endswith(".md")}
    config = configparser.ConfigParser(interpolation=None)
    config.read(root / "platformio.ini")
    section = f"env:{environment}"
    if not config.has_section(section):
        raise ValueError(f"Unknown environment: {environment}")
    selected = {"platformio.ini", ".github/workflows/firmware.yml", "scripts/firmware_ci.py", "scripts/package_web_firmware.py"}
    settings = "\n".join(option(config, section, name) for name in (
        "framework", "build_flags", "extra_scripts", "board_build.partitions", "build_src_filter"))
    # Include local scripts/headers/partition files referred to by build options.
    selected.update(path for path in files if path in settings)
    libraries = set()
    if "espidf" in option(config, section, "framework"):
        if environment not in IDF_APPS:
            raise ValueError(f"Declare explicit IDF dependencies for {environment}")
        app, libs, defaults = IDF_APPS[environment]
        libraries.update(libs)
        selected.update(path for path in files if path.startswith(f"apps/{app}/"))
        selected.update({"CMakeLists.txt", "apps/CMakeLists.txt", "apps/idf_component.yml",
                         f"apps/{defaults}/sdkconfig.defaults",
                         "apps/19-speech-diag/compile_req.yml"})
        monitor = environment in {"codex-monitor", "codex-audio-diag"}
        selected.add("dependencies.monitor.lock" if monitor else "dependencies.lock")
        if monitor:
            selected.update(path for path in files if path.startswith("components/"))
            selected.add("scripts/idf_linker_preprocessor.cmake")
    else:
        filters = re.findall(r'\+<([^>]+)>', option(config, section, "build_src_filter"))
        if not filters or any("*" in item or ".." in item for item in filters):
            raise ValueError(f"Declare source dependencies for {environment}'s filter")
        selected.update(path for path in files if any(path.startswith(f"apps/{item}") for item in filters))

    # Index repository headers, then follow include references transitively.
    # Whole libraries are inputs because PlatformIO compiles their source files.
    headers = {}
    for path in files:
        if path.startswith("lib/") and "/src/" in path and path.endswith((".h", ".hpp")):
            library = path.split("/")[1]
            for name in (path.split("/src/", 1)[1], Path(path).name):
                headers.setdefault(name, set()).add(library)
    pending = set(selected)
    scanned = set()
    while pending or libraries:
        for library in libraries:
            pending.update(path for path in files if path.startswith(f"lib/{library}/"))
        libraries.clear()
        for path in sorted(pending - scanned):
            selected.add(path)
            scanned.add(path)
            if path.endswith((".cpp", ".c", ".h", ".hpp")) and (root / path).is_file():
                for name in INCLUDE.findall((root / path).read_text()):
                    libraries.update(headers.get(name, set()))
        pending.clear()
        # Already selected libraries need not be scanned again.
        libraries = {lib for lib in libraries if any(
            path.startswith(f"lib/{lib}/") and path not in scanned for path in files)}
    # Hash absent declared inputs too, so deletion invalidates the cache.
    return sorted(selected)


def fingerprint(root, paths):
    digest = hashlib.sha256(f"firmware-ci-{SCHEMA}\0".encode())
    for path in paths:
        digest.update(path.encode() + b"\0")
        file = root / path
        digest.update(hashlib.sha256(file.read_bytes()).digest() if file.is_file() else b"MISSING")
    return digest.hexdigest()


def image_names(environment):
    if environment in BUILD_ONLY_ENVIRONMENTS:
        return ()
    if environment == "codex-monitor":
        return ("bootloader.bin", "partitions.bin", "firmware.bin", "srmodels/srmodels.bin", "ota_data_initial.bin")
    return tuple(name for name, _ in PARTS)


def image_layout(environment):
    parts = PARTS
    app_limit = 3 * 1024 * 1024
    model_limit = None
    if environment == 'codex-monitor':
        app_offset, app_limit, model_offset, model_limit = monitor_partitions()
        parts = (("bootloader.bin", 0), ("partitions.bin", 0x8000),
                 ("ota_data_initial.bin", 0xe000), ("firmware.bin", app_offset), ("srmodels/srmodels.bin", model_offset))
    return {"parts": [{"path": name, "offset": offset} for name, offset in parts],
            "app_limit": app_limit, "model_limit": model_limit}


def verify(directory, environment, expected_fingerprint=None):
    target = directory / environment
    metadata = json.loads((target / "metadata.json").read_text())
    if metadata.get("schema") != SCHEMA or metadata.get("environment") != environment:
        raise ValueError("Firmware bundle identity mismatch")
    if not metadata.get("version") or not metadata.get("fingerprint"):
        raise ValueError("Firmware bundle lacks provenance")
    if expected_fingerprint and metadata["fingerprint"] != expected_fingerprint:
        raise ValueError("Firmware bundle input fingerprint mismatch")
    expected_images = set(image_names(environment))
    # Accept already-published pre-OTA monitor catalogs until the next build.
    if environment == "codex-monitor" and "ota_data_initial.bin" not in metadata["images"]:
        expected_images.remove("ota_data_initial.bin")
    if set(metadata["images"]) != expected_images:
        raise ValueError("Firmware bundle image list mismatch")
    layout = metadata.get("layout")
    if layout is not None:
        if {part["path"] for part in layout["parts"]} != expected_images:
            raise ValueError("Firmware bundle flash layout mismatch")
        if any(type(part["offset"]) is not int or not 0 <= part["offset"] < 16 * 1024 * 1024
               for part in layout["parts"]):
            raise ValueError("Firmware bundle flash offset invalid")
        if type(layout["app_limit"]) is not int or layout["app_limit"] <= 0:
            raise ValueError("Firmware bundle app limit invalid")
        if environment == "codex-monitor" and (type(layout["model_limit"]) is not int or layout["model_limit"] <= 0):
            raise ValueError("Firmware bundle model limit invalid")
    for name, expected in metadata["images"].items():
        image = target / name
        if not image.is_file() or not image.stat().st_size or hashlib.sha256(image.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Missing or corrupt firmware image: {image}")
    return metadata


def collect(directory, environment, digest, version):
    target = directory / environment
    target.mkdir(parents=True, exist_ok=True)
    images = {}
    for name in image_names(environment):
        source = BUILD / environment / name
        if name == "boot_app0.bin":
            core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
            source = core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
        if not source.is_file() or not source.stat().st_size:
            raise FileNotFoundError(f"Build {environment} before collecting: {source}")
        destination = target / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        images[name] = hashlib.sha256(destination.read_bytes()).hexdigest()
    metadata = {"schema": SCHEMA, "environment": environment, "fingerprint": digest,
                "version": version, "images": images,
                "layout": image_layout(environment) if images else None}
    (target / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    verify(directory, environment, digest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("plan", "verify", "collect"))
    parser.add_argument("--environment")
    parser.add_argument("--directory", type=Path, default=Path(".ci/firmware"))
    parser.add_argument("--fingerprint")
    parser.add_argument("--version", default=os.environ.get("GITHUB_SHA", "local")[:12])
    args = parser.parse_args()
    environments = [*BUILD_ONLY_ENVIRONMENTS, *firmware_environments()]
    if args.action == "plan":
        files = tracked_files(ROOT)
        print(json.dumps({"include": [{"environment": env, "download_group": ("monitor" if env in {"codex-monitor", "codex-audio-diag"} else "idf" if env in IDF_APPS else "arduino"),
                                       "fingerprint": fingerprint(ROOT, inputs(ROOT, env, files))}
                                      for env in environments]}, separators=(",", ":")))
    else:
        if args.environment not in environments:
            parser.error("Select a known firmware environment")
        if args.action == "verify":
            try:
                verify(args.directory, args.environment, args.fingerprint)
            except (OSError, ValueError, KeyError, TypeError) as error:
                parser.exit(1, f"Rebuild {args.environment}: {error}\n")
        else:
            if not args.fingerprint:
                parser.error("collect requires --fingerprint")
            collect(args.directory, args.environment, args.fingerprint, args.version)


if __name__ == "__main__":
    main()
