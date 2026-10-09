from ota_fixture import write_monitor_images
import csv
import configparser
import importlib.util
import json
from pathlib import Path
import runpy
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/package_web_firmware.py"
spec = importlib.util.spec_from_file_location("firmware_package", SCRIPT)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class MonitorPackageTest(unittest.TestCase):
    def test_ci_builds_all_firmware_without_publishing_standalone_diagnostics(self):
        published = package.firmware_environments()
        self.assertIn("codex-monitor", published)
        self.assertNotIn("codex-hid-diag", published)
        self.assertNotIn("codex-audio-diag", published)
        self.assertNotIn("codex-ble-diag", published)
        self.assertEqual(set(package.NAMES), set(published))

        config = configparser.ConfigParser(interpolation=None)
        config.read(SCRIPT.parents[1] / "platformio.ini")
        expected = {section.removeprefix("env:") for section in config.sections()
                    if section.startswith("env:") and section != "env:native"}
        with patch.object(sys, "path", [str(SCRIPT.parent), *sys.path]), \
             patch("subprocess.run") as run:
            runpy.run_path(str(SCRIPT.with_name("build_firmware.py")), run_name="__main__")
        run.assert_called_once()
        command = run.call_args.args[0]
        self.assertEqual(["pio", "run"], command[:2])
        self.assertEqual(["-e"] * len(expected), command[2::2])
        self.assertEqual(expected, set(command[3::2]))
        self.assertEqual(len(expected), len(command[3::2]))
        self.assertTrue(run.call_args.kwargs["check"])

    def test_unclassified_firmware_still_requires_catalog_labels(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "platformio.ini").write_text("[env:new-app]\n")
            with patch.object(package, "ROOT", root):
                with self.assertRaisesRegex(ValueError, "Add catalog labels for environments: new-app"):
                    package.firmware_environments()

    def test_monitor_preserves_arduino_nvs_boundary(self):
        root = SCRIPT.parents[1]
        config = configparser.ConfigParser(interpolation=None)
        config.read(root / "platformio.ini")
        path = root / config["env:codex-monitor"]["board_build.partitions"]
        rows = list(csv.reader(line for line in path.read_text().splitlines()
                               if line and not line.startswith("#")))
        nvs = next(row for row in rows if row[0] == "nvs")
        # Match the existing Arduino app3M_fat9M_16MB NVS region, not the
        # larger diagnostic NVS which absorbs Arduino's old OTA data sector.
        self.assertEqual((0x9000, 0x5000), (int(nvs[3], 0), int(nvs[4], 0)))
        app_offset, _, model_offset, _ = package.monitor_partitions()
        self.assertGreaterEqual(app_offset, 0x9000 + 0x5000)
        self.assertGreaterEqual(model_offset, app_offset)

    def test_models_and_offsets_follow_partition_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            site = root / "site"
            site.mkdir()
            (site / "ble-client.bundle.js").write_text("test")
            (site / "index.html").write_text("Astro guide")
            (site / "setup.html").write_text("Installer")
            (site / "firmware/codex-monitor").mkdir(parents=True)
            (site / "firmware/index.html").write_text("Firmware catalog")
            (site / "firmware/codex-monitor/index.html").write_text("Monitor guide")
            (site / "docs/images").mkdir(parents=True)
            (site / "docs/images/preview.png").write_bytes(b"preview")
            (root / "docs/images").mkdir(parents=True)
            (root / "platformio.ini").write_text(
                "[env:codex-monitor]\nboard_build.partitions = partitions.csv\n")
            (root / "partitions.csv").write_text(
                "ota_0,app,ota_0,0x10000,0x400000\nota_1,app,ota_1,0x410000,0x400000\nmodel,data,spiffs,0x810000,0x7f0000\n")
            build = root / "build"
            for name in ("bootloader.bin", "partitions.bin", "firmware.bin", "srmodels/srmodels.bin"):
                image = build / "codex-monitor" / name
                image.parent.mkdir(parents=True, exist_ok=True)
                image.write_bytes(b"test")
            write_monitor_images(build / "codex-monitor")
            core = root / "core"
            boot_app = core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
            boot_app.parent.mkdir(parents=True)
            boot_app.write_bytes(b"arduino")
            output = root / "output"
            with patch.object(package, "ROOT", root), patch.object(package, "SITE", site), \
                 patch.object(package, "BUILD", build), \
                 patch.object(package, "firmware_environments", return_value=["codex-monitor"]), \
                 patch.dict(package.os.environ, {"PLATFORMIO_CORE_DIR": str(core)}):
                self.assertEqual(1, package.package(output, "test"))
                self.assertEqual("Astro guide", (output / "index.html").read_text())
                self.assertEqual("Installer", (output / "setup.html").read_text())
                self.assertEqual("Firmware catalog", (output / "firmware/index.html").read_text())
                self.assertEqual("Monitor guide", (output / "firmware/codex-monitor/index.html").read_text())
                self.assertEqual(b"preview", (output / "docs/images/preview.png").read_bytes())
                manifest = json.loads((output / "firmware/codex-monitor/manifest.json").read_text())
                self.assertTrue(manifest["new_install_prompt_erase"])
                parts = manifest["builds"][0]["parts"]
                self.assertEqual([0, 0x8000, 0xe000, 0x10000, 0x810000], [part["offset"] for part in parts])
                self.assertNotIn("boot_app0.bin", [part["path"] for part in parts])
                self.assertEqual(b"test", (output / "firmware/codex-monitor/srmodels/srmodels.bin").read_bytes())
                (build / "codex-monitor/srmodels/srmodels.bin").unlink()
                with self.assertRaises(FileNotFoundError):
                    package.package(output, "test")


if __name__ == "__main__":
    unittest.main()
