import importlib.util
import json
from pathlib import Path
import shutil
import runpy
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('whistle_model', ROOT / 'scripts/prepare_whistle_model.py')
model = importlib.util.module_from_spec(spec)
spec.loader.exec_module(model)


class WhistleTests(unittest.TestCase):
    def test_full_upload_includes_verified_model(self):
        class Environment:
            def __init__(self):
                self.actions = {}
                self.values = {}

            def subst(self, value):
                return value.replace('$PROJECT_DIR', str(ROOT))

            def AddPreAction(self, target, callback):
                self.actions[target] = callback

            def Append(self, **kwargs):
                self.values.update(kwargs)

        env = Environment()
        script = runpy.run_path(str(ROOT / 'scripts/whistle_model.py'),
                               init_globals={'Import': lambda _: None, 'env': env})
        image = str(model.DEFAULT_DIRECTORY / 'whistle_d4.cact')
        self.assertEqual([('0x210000', image)], env.values['FLASH_EXTRA_IMAGES'])
        self.assertEqual(['0x210000', image], env.values['UPLOADERFLAGS'])
        self.assertEqual({'upload'}, set(env.actions))
        with tempfile.TemporaryDirectory() as directory:
            script['module'].DEFAULT_DIRECTORY = Path(directory)
            with self.assertRaisesRegex(RuntimeError, 'Prepare model before upload'):
                env.actions['upload'](None, None, env)

    def test_upload_rejects_missing_or_modified_model(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            with self.assertRaises(OSError):
                model.verify(folder)
            image = folder / 'whistle_d4.cact'
            image.write_bytes(b'wrong model')
            (folder / 'manifest.json').write_text('{}')
            with self.assertRaises((KeyError, ValueError)):
                model.verify(folder)
            # Even a plausible manifest and correct image size cannot bless
            # changed weights; the expected SHA is pinned in the source.
            with image.open('wb') as stream:
                stream.truncate(model.MODEL_SIZE)
            (folder / 'manifest.json').write_text(json.dumps({
                'source_revision': model.REVISION, 'source_sha256': model.SOURCE_SHA256,
                'depth': 4, 'sha256': model.IMAGE_SHA256,
            }))
            with self.assertRaises(ValueError):
                model.verify(folder)

    @unittest.skipUnless(shutil.which('cc'), 'C compiler unavailable')
    def test_engine_speech_gate_and_optional_model_memory(self):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / 'probe'
            subprocess.run([
                'cc', '-O2', '-I', str(ROOT / 'lib/whistle/include'),
                '-include', str(ROOT / 'test/host/whistle/alloc.h'),
                '-DND_ALLOC(n)=whistle_test_alloc(n)', '-DND_FREE(p)=whistle_test_free(p)',
                *map(str, sorted((ROOT / 'lib/whistle/src').glob('*.c'))),
                str(ROOT / 'test/host/whistle/probe.c'), '-lm', '-o', str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True, timeout=30)
            image = model.DEFAULT_DIRECTORY / 'whistle_d4.cact'
            if image.is_file():
                model.verify(image.parent)
                subprocess.run([str(executable), str(image)], check=True, timeout=120)


if __name__ == '__main__':
    unittest.main()
