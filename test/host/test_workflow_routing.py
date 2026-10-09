"""Check representative change routing against the actual workflow path filters."""
from fnmatch import fnmatchcase
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


def triggered(workflow, path):
    text = (ROOT / '.github/workflows' / workflow).read_text()
    block = text.split('  push:\n', 1)[1].split('  pull_request:', 1)[0]
    patterns = re.findall(r"^      - '([^']+)'$", block, re.MULTILINE)
    selected = False
    for pattern in patterns:
        negative = pattern.startswith('!')
        pattern = pattern.removeprefix('!')
        alternatives = [pattern]
        # GitHub **/ also matches zero directories (root Markdown, app READMEs).
        while '**/' in alternatives[-1]:
            alternatives.append(alternatives[-1].replace('**/', '', 1))
        if any(fnmatchcase(path, item) for item in alternatives):
            selected = not negative
    return selected


class WorkflowRoutingTest(unittest.TestCase):
    def test_docs_and_configuration_never_start_firmware(self):
        for path in ['README.md', 'docs/monitor-setup.md', 'docs/images/preview.png',
                     'apps/codex-monitor/README.md', 'lib/ui/README.md',
                     'web-flasher/ble-client.js', 'web-flasher/styles.css',
                     'site/src/pages/monitor.astro', '.github/workflows/pages.yml',
                     'scripts/fetch_firmware_catalog.py']:
            with self.subTest(path=path):
                self.assertTrue(triggered('pages.yml', path))
                self.assertFalse(triggered('firmware.yml', path))

    def test_firmware_sources_and_toolchain_start_builds(self):
        for path in ['apps/codex-monitor/src/main.cpp', 'lib/fnk0104b/src/audio_input.cpp',
                     'apps/codex-monitor/sdkconfig.defaults', 'dependencies.monitor.lock',
                     'platformio.ini', 'CMakeLists.txt', 'components/arduino/CMakeLists.txt',
                     'scripts/speech_models.py', '.github/workflows/firmware.yml']:
            with self.subTest(path=path):
                self.assertTrue(triggered('firmware.yml', path))

    def test_bridge_and_test_only_changes_run_checks_without_firmware(self):
        for path in ['monitor-server/server.py', 'test/test_native/test_main.cpp',
                     'test/host/test_firmware_ci.py']:
            with self.subTest(path=path):
                self.assertTrue(triggered('checks.yml', path))
                self.assertFalse(triggered('firmware.yml', path))

    def test_hardware_evidence_does_not_start_host_tests(self):
        self.assertFalse(triggered('checks.yml', 'test/hardware/usb-micro-voice-2026-10-10/runtime.log'))


if __name__ == '__main__':
    unittest.main()
