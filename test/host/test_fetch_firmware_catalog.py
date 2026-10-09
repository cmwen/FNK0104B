import importlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
fetcher = importlib.import_module('fetch_firmware_catalog')


class FirmwareSourceTest(unittest.TestCase):
    def setUp(self):
        self.run = {'id': 123, 'head_branch': 'main', 'conclusion': 'success',
                    'event': 'push', 'path': '.github/workflows/firmware.yml',
                    'html_url': 'https://github.com/owner/repo/actions/runs/123',
                    'head_sha': 'verified-revision'}

    def artifact(self, name, expired=False):
        return {'name': name, 'expired': expired}

    def test_only_successful_trusted_main_firmware_runs(self):
        for field, value in [('head_branch', 'feature'), ('conclusion', 'failure'),
                             ('event', 'pull_request'), ('path', '.github/workflows/pages.yml')]:
            run = {**self.run, field: value}
            self.assertIsNone(fetcher.select_artifacts(run, [self.artifact('firmware-catalog')], ['hello']))

    def test_aggregate_and_legacy_complete_catalogs(self):
        self.assertEqual(['firmware-catalog'], fetcher.select_artifacts(
            self.run, [self.artifact('firmware-catalog')], ['hello', 'display']))
        self.run['path'] = '.github/workflows/build.yml'
        names = ['firmware-hello', 'firmware-display']
        self.assertEqual(names, fetcher.select_artifacts(self.run, [self.artifact(n) for n in names], ['hello', 'display']))

    def test_partial_and_expired_catalogs_are_rejected(self):
        for artifacts in [[self.artifact('firmware-catalog', True)],
                          [self.artifact('firmware-hello')],
                          [self.artifact('firmware-hello'), self.artifact('firmware-display', True)]]:
            self.assertIsNone(fetcher.select_artifacts(self.run, artifacts, ['hello', 'display']))

    def test_download_verifies_every_environment_without_building(self):
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(fetcher, 'firmware_environments', return_value=['hello', 'display']), \
             patch.object(fetcher, 'api', side_effect=[{'workflow_runs': [self.run]}, {'artifacts': [self.artifact('firmware-catalog')]}]), \
             patch.object(fetcher.subprocess, 'run') as command, \
             patch.object(fetcher, 'verify') as verify:
            output = Path(directory) / 'firmware'
            self.assertEqual(123, fetcher.fetch('owner/repo', output))
            self.assertEqual(1, command.call_count)
            self.assertEqual(['gh', 'run', 'download'], command.call_args.args[0][:3])
            self.assertEqual(['hello', 'display'], [call.args[1] for call in verify.call_args_list])

    def test_missing_source_has_explicit_recovery_and_no_build_fallback(self):
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(fetcher, 'api', return_value={'workflow_runs': []}), \
             patch.object(fetcher.subprocess, 'run') as command:
            with self.assertRaisesRegex(RuntimeError, 'Run Build firmware on main once'):
                fetcher.fetch('owner/repo', Path(directory) / 'firmware')
            command.assert_not_called()

    def test_corrupt_image_fails_before_publication(self):
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(fetcher, 'firmware_environments', return_value=['hello']), \
             patch.object(fetcher, 'api', side_effect=[{'workflow_runs': [self.run]}, {'artifacts': [self.artifact('firmware-catalog')]}]), \
             patch.object(fetcher.subprocess, 'run'), \
             patch.object(fetcher, 'verify', side_effect=ValueError('corrupt image')):
            with self.assertRaisesRegex(ValueError, 'corrupt image'):
                fetcher.fetch('owner/repo', Path(directory) / 'firmware')


    def test_published_fallback_retains_provenance_and_verifies_images(self):
        names = fetcher.image_names('hello')
        payload = b'verified firmware'
        metadata = {'schema': 1, 'environment': 'hello', 'version': 'original-commit',
                    'fingerprint': 'original-inputs',
                    'images': {name: hashlib.sha256(payload).hexdigest() for name in names}}
        def response(url, **kwargs):
            return io.BytesIO(json.dumps(metadata).encode() if url.endswith('metadata.json') else payload)
        with tempfile.TemporaryDirectory() as directory, patch.object(fetcher, 'urlopen', side_effect=response):
            output = Path(directory)
            fetcher.download_published('https://example.com/board/', output, ['hello'])
            self.assertEqual('original-commit', fetcher.verify(output, 'hello')['version'])

    def test_corrupt_published_image_and_unsafe_image_paths_are_rejected(self):
        names = fetcher.image_names('hello')
        metadata = {'schema': 1, 'environment': 'hello', 'version': 'original',
                    'fingerprint': 'inputs', 'images': {name: 'wrong-hash' for name in names}}
        def response(url, **kwargs):
            return io.BytesIO(json.dumps(metadata).encode() if url.endswith('metadata.json') else b'corrupt')
        with tempfile.TemporaryDirectory() as directory, patch.object(fetcher, 'urlopen', side_effect=response):
            with self.assertRaisesRegex(ValueError, 'Missing or corrupt'):
                fetcher.download_published('https://example.com/', Path(directory), ['hello'])
            metadata['images']['../../unexpected'] = 'hash'
            with self.assertRaisesRegex(ValueError, 'image list mismatch'):
                fetcher.download_published('https://example.com/', Path(directory), ['hello'])

    def test_expired_artifacts_use_published_fallback_without_building(self):
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(fetcher, 'api', return_value={'workflow_runs': []}), \
             patch.object(fetcher, 'download_published') as published, \
             patch.object(fetcher.subprocess, 'run') as command:
            output = Path(directory)
            fetcher.fetch('owner/repo', output, 'https://example.com/')
            published.assert_called_once_with('https://example.com/', output, fetcher.firmware_environments())
            command.assert_not_called()


    def test_initial_missing_catalog_defers_only_for_active_main_firmware(self):
        pending = {**self.run, 'conclusion': None, 'status': 'in_progress'}
        with patch.object(fetcher, 'fetch', side_effect=fetcher.CatalogUnavailable('missing')), \
             patch.object(fetcher, 'api', return_value={'workflow_runs': [pending]}), \
             patch.dict(fetcher.os.environ, {'GITHUB_STEP_SUMMARY': ''}):
            self.assertFalse(fetcher.prepare('owner/repo', Path('unused'), defer_if_building=True))

    def test_missing_catalog_without_active_build_still_fails(self):
        with patch.object(fetcher, 'fetch', side_effect=fetcher.CatalogUnavailable('missing')), \
             patch.object(fetcher, 'api', return_value={'workflow_runs': []}):
            with self.assertRaises(fetcher.CatalogUnavailable):
                fetcher.prepare('owner/repo', Path('unused'), defer_if_building=True)

    def test_corrupt_catalog_never_defers_even_with_active_build(self):
        with patch.object(fetcher, 'fetch', side_effect=ValueError('corrupt')), \
             patch.object(fetcher, 'active_firmware_run') as active:
            with self.assertRaisesRegex(ValueError, 'corrupt'):
                fetcher.prepare('owner/repo', Path('unused'), defer_if_building=True)
            active.assert_not_called()

    def test_untrusted_or_finished_build_cannot_defer_publication(self):
        for field, value in [('head_branch', 'feature'), ('event', 'pull_request'),
                             ('path', '.github/workflows/pages.yml'), ('status', 'completed')]:
            run = {**self.run, 'status': 'in_progress', field: value}
            with self.subTest(field=field), patch.object(fetcher, 'api', return_value={'workflow_runs': [run]}):
                self.assertIsNone(fetcher.active_firmware_run('owner/repo'))


if __name__ == '__main__':
    unittest.main()
