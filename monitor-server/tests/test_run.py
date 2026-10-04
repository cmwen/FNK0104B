import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import run


class LauncherTests(unittest.TestCase):
    def test_dispatcher_local_defaults_preserve_explicit_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / '.env.dispatcher'
            config.write_text('MONITOR_REPOSITORY_ROOT=/tmp/projects\nMONITOR_ROUTER_MODEL=gpt-6-luna\n')
            env = {'MONITOR_REPOSITORY_ROOT': '/tmp/override'}
            run.configure_dispatcher(env, config)
            self.assertEqual(env['MONITOR_REPOSITORY_ROOT'], '/tmp/override')
            self.assertEqual(env['MONITOR_ROUTER_MODEL'], 'gpt-6-luna')

    def test_matches_device_and_preserves_environment_overrides(self):
        with tempfile.TemporaryDirectory() as directory:
            header = pathlib.Path(directory) / "monitor_secrets.h"
            header.write_text('#define MONITOR_SERVER_HOST "192.168.1.32"\n'
                              '#define MONITOR_SERVER_PORT 8765\n'
                              '#define MONITOR_SERVER_TOKEN "test-key"\n')
            env = {"MONITOR_PORT": "9999", "MONITOR_AGENT_CWD": "/tmp"}
            run.configure(env, header)
            self.assertEqual(env["MONITOR_HOST"], "192.168.1.32")
            self.assertEqual(env["MONITOR_TOKEN"], "test-key")
            self.assertEqual(env["MONITOR_PORT"], "9999")
            self.assertEqual(env["MONITOR_AGENT_CWD"], "/tmp")

    def test_missing_or_empty_config_keeps_loopback_default(self):
        with tempfile.TemporaryDirectory() as directory:
            header = pathlib.Path(directory) / "monitor_secrets.h"
            for source in (None, '#define MONITOR_SERVER_HOST ""\n#define MONITOR_SERVER_TOKEN ""\n'):
                if source is not None:
                    header.write_text(source)
                env = {}
                run.configure(env, header)
                self.assertNotIn("MONITOR_HOST", env)
                self.assertNotIn("MONITOR_TOKEN", env)
