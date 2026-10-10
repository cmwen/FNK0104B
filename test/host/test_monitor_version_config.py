import json
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / 'apps/codex-monitor/tools/version_config.py'


class VersionConfigTest(unittest.TestCase):
    def run_hook(self, cached):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            header = project / 'apps/codex-monitor/include/monitor_ota.hpp'
            header.parent.mkdir(parents=True)
            header.write_text('constexpr char version[] = "0.7.1";\n')
            build = project / 'build'
            build.mkdir()
            (build / 'project_description.json').write_text(json.dumps({'project_version': cached}))
            ninja = build / 'build.ninja'
            ninja.write_text('generated build metadata')
            class Environment:
                def subst(self, name):
                    return str(project if name == '$PROJECT_DIR' else build)
            exec(compile(SCRIPT.read_text(), str(SCRIPT), 'exec'),
                 {'env': Environment(), 'Import': lambda _: None})
            return ninja.exists()

    def test_version_change_requires_cmake_reconfigure(self):
        self.assertFalse(self.run_hook('0.7.0'))

    def test_current_version_keeps_cached_configuration(self):
        self.assertTrue(self.run_hook('0.7.1'))
