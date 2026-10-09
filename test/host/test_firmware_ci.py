import hashlib
import importlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
ci = importlib.import_module('firmware_ci')
package = importlib.import_module('package_web_firmware')


class FirmwareInputsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.write('platformio.ini', '''[common]
framework = arduino
[env:hello]
framework = ${common.framework}
build_src_filter = +<01-hello/src/>
[env:hello-debug]
extends = env:hello
extra_scripts = post:scripts/debug.py
[env:display]
framework = ${common.framework}
build_src_filter = +<02-display/src/>
''')
        self.write('apps/01-hello/src/main.cpp', '#include <board/board.hpp>\n')
        self.write('apps/02-display/src/main.cpp', '#include <board/board.hpp>\n#include <ui/theme.hpp>\n')
        self.write('lib/board/src/board/board.hpp', '#include <logic/logic.hpp>\n')
        self.write('lib/board/src/board.cpp', '#include <board/board.hpp>\n')
        self.write('lib/logic/src/logic/logic.hpp', 'constexpr int value = 1;\n')
        self.write('lib/ui/src/ui/theme.hpp', 'constexpr int color = 1;\n')
        self.write('scripts/debug.py', 'print("debug")\n')

    def write(self, path, value):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(value)

    def hashes(self):
        files = [str(path.relative_to(self.root)) for path in self.root.rglob('*') if path.is_file()]
        return {env: ci.fingerprint(self.root, ci.inputs(self.root, env, files))
                for env in ('hello', 'hello-debug', 'display')}

    def changed(self, before):
        return {env for env, digest in self.hashes().items() if digest != before[env]}

    def test_app_change_and_inherited_environment(self):
        before = self.hashes()
        self.write('apps/01-hello/src/main.cpp', '#include <board/board.hpp>\nint x = 1;')
        self.assertEqual({'hello', 'hello-debug'}, self.changed(before))

    def test_shared_library_change_reaches_transitive_consumers(self):
        before = self.hashes()
        self.write('lib/logic/src/logic/logic.hpp', 'constexpr int value = 2;')
        self.assertEqual({'hello', 'hello-debug', 'display'}, self.changed(before))

    def test_ui_change_does_not_rebuild_hello(self):
        before = self.hashes()
        self.write('lib/ui/src/ui/theme.hpp', 'constexpr int color = 2;')
        self.assertEqual({'display'}, self.changed(before))

    def test_docs_tests_and_site_changes_do_not_invalidate_firmware(self):
        before = self.hashes()
        for path in ('docs/test.md', 'apps/01-hello/README.md', 'lib/ui/README.md',
                     'site/src/pages/index.astro', 'test/test_native/test_main.cpp'):
            self.write(path, 'changed')
        self.assertEqual(set(), self.changed(before))

    def test_pages_workflow_and_catalog_fetch_do_not_invalidate_firmware(self):
        before = self.hashes()
        self.write('.github/workflows/pages.yml', 'new publishing workflow')
        self.write('scripts/fetch_firmware_catalog.py', 'new download logic')
        self.assertEqual(set(), self.changed(before))
        self.write('.github/workflows/firmware.yml', 'new build workflow')
        self.assertEqual({'hello', 'hello-debug', 'display'}, self.changed(before))

    def test_new_and_deleted_source_files_change_fingerprint(self):
        before = self.hashes()
        self.write('apps/02-display/src/new.cpp', 'int x = 1;')
        self.assertEqual({'display'}, self.changed(before))
        before = self.hashes()
        (self.root / 'apps/02-display/src/main.cpp').unlink()
        self.assertEqual({'display'}, self.changed(before))

    def test_toolchain_change_invalidates_all_environments(self):
        before = self.hashes()
        with (self.root / 'platformio.ini').open('a') as stream:
            stream.write('\n; toolchain revision\n')
        self.assertEqual({'hello', 'hello-debug', 'display'}, self.changed(before))

    def test_build_script_only_invalidates_its_environment(self):
        before = self.hashes()
        self.write('scripts/debug.py', 'print("new debug")')
        self.assertEqual({'hello-debug'}, self.changed(before))

    def test_actual_monitor_sources_do_not_include_other_app_mains(self):
        files = ci.tracked_files(ROOT)
        monitor = ci.inputs(ROOT, 'codex-monitor', files)
        audio = ci.inputs(ROOT, 'codex-audio-diag', files)
        self.assertIn('apps/codex-monitor/src/main.cpp', monitor)
        self.assertNotIn('apps/codex-monitor/src/main.cpp', audio)
        self.assertIn('apps/codex-monitor/sdkconfig.defaults', audio)
        self.assertIn('lib/codex_hid/src/backend.cpp', monitor)
        self.assertIn('dependencies.monitor.lock', monitor)
        self.assertNotIn('dependencies.lock', monitor)
        self.assertNotIn('apps/01-hello/src/main.cpp', monitor)
        self.assertNotIn('lib/ui/src/ui/micro_layout.hpp', ci.inputs(ROOT, 'hello', files))


class FirmwareBundleTest(unittest.TestCase):
    def test_cached_images_retain_build_revision_and_need_no_platformio_install(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build, bundles, site = root / 'build', root / 'bundles', root / 'site'
            for name in ci.image_names('hello'):
                if name == 'boot_app0.bin':
                    source = root / 'core/packages/framework-arduinoespressif32/tools/partitions' / name
                else:
                    source = build / 'hello' / name
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(name.encode())
            site.mkdir()
            (site / 'ble-client.bundle.js').write_text('bundle')
            (root / 'docs/images').mkdir(parents=True)
            with patch.object(ci, 'BUILD', build), patch.dict(ci.os.environ, {'PLATFORMIO_CORE_DIR': str(root / 'core')}):
                ci.collect(bundles, 'hello', 'inputs-v1', 'original-commit')
            metadata = ci.verify(bundles, 'hello', 'inputs-v1')
            self.assertIn('boot_app0.bin', metadata['images'])
            with patch.object(package, 'SITE', site), patch.object(package, 'ROOT', root), \
                 patch.object(package, 'firmware_environments', return_value=['hello']), \
                 patch.dict(package.os.environ, {'PLATFORMIO_CORE_DIR': str(root / 'missing-core')}):
                output = root / 'output'
                self.assertEqual(1, package.package(output, 'new-site-commit', bundles))
                manifest = json.loads((output / 'firmware/hello/manifest.json').read_text())
                self.assertEqual('original-commit', manifest['version'])
                self.assertEqual('new-site-commit', json.loads((output / 'firmware/catalog.json').read_text())['version'])
                (bundles / 'hello/firmware.bin').write_bytes(b'corrupt')
                with self.assertRaisesRegex(ValueError, 'Missing or corrupt'):
                    package.package(output, 'new-site-commit', bundles)

    def test_complete_catalog_reuses_every_environment_and_rejects_missing_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build, bundles, site = root / 'build', root / 'bundles', root / 'site'
            environments = package.firmware_environments()
            for env in environments:
                for name in ci.image_names(env):
                    if name == 'boot_app0.bin':
                        source = root / 'core/packages/framework-arduinoespressif32/tools/partitions' / name
                    else:
                        source = build / env / name
                    source.parent.mkdir(parents=True, exist_ok=True)
                    source.write_bytes(b'image')
            site.mkdir()
            (site / 'ble-client.bundle.js').write_text('bundle')
            (root / 'docs/images').mkdir(parents=True)
            (root / 'platformio.ini').write_bytes((ROOT / 'platformio.ini').read_bytes())
            partition = root / 'apps/codex-monitor/partitions.csv'
            partition.parent.mkdir(parents=True)
            partition.write_bytes((ROOT / 'apps/codex-monitor/partitions.csv').read_bytes())
            with patch.object(ci, 'BUILD', build), patch.dict(ci.os.environ, {'PLATFORMIO_CORE_DIR': str(root / 'core')}):
                for env in [*package.BUILD_ONLY_ENVIRONMENTS, *environments]:
                    ci.collect(bundles, env, 'fingerprint', f'original-{env}')
            with patch.object(package, 'SITE', site), patch.object(package, 'ROOT', root):
                output = root / 'output'
                self.assertEqual(len(environments), package.package(output, 'new-site', bundles))
                # Packaging newer docs must preserve the previous firmware layout.
                old_metadata = json.loads((bundles / 'codex-monitor/metadata.json').read_text())
                old_model_offset = next(part['offset'] for part in old_metadata['layout']['parts'] if part['path'] == 'srmodels/srmodels.bin')
                with patch.object(package, 'monitor_partitions', return_value=(0x10000, 4 * 1024 * 1024, 0x500000, 4 * 1024 * 1024)):
                    package.package(output, 'newer-site', bundles)
                manifest = json.loads((output / 'firmware/codex-monitor/manifest.json').read_text())
                self.assertEqual(old_model_offset, manifest['builds'][0]['parts'][-1]['offset'])
                self.assertEqual(old_metadata, json.loads((output / 'firmware/codex-monitor/metadata.json').read_text()))
                catalog = json.loads((output / 'firmware/catalog.json').read_text())
                self.assertEqual(set(environments), {item['id'] for item in catalog['builds']})
                for env in environments:
                    manifest = json.loads((output / f'firmware/{env}/manifest.json').read_text())
                    self.assertEqual(f'original-{env}', manifest['version'])
                (bundles / 'hello/metadata.json').unlink()
                with self.assertRaises(FileNotFoundError):
                    package.package(output, 'new-site', bundles)

    def test_wrong_fingerprint_or_missing_image_cannot_be_reused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'hello'
            target.mkdir()
            metadata = {'schema': ci.SCHEMA, 'environment': 'hello', 'version': 'commit',
                        'fingerprint': 'old', 'images': {}}
            (target / 'metadata.json').write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError, 'fingerprint mismatch'):
                ci.verify(root, 'hello', 'new')
            with self.assertRaisesRegex(ValueError, 'image list mismatch'):
                ci.verify(root, 'hello', 'old')

    def test_success_marker_caches_build_only_diagnostic(self):
        with tempfile.TemporaryDirectory() as directory:
            ci.collect(Path(directory), 'hello-debug', 'inputs', 'commit')
            metadata = ci.verify(Path(directory), 'hello-debug', 'inputs')
            self.assertEqual({}, metadata['images'])


if __name__ == '__main__':
    unittest.main()
