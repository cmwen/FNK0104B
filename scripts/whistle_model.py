"""Add the verified depth-4 model to full PlatformIO Whistle uploads only."""
import importlib.util
from pathlib import Path

Import('env')  # noqa: F821
project = Path(env.subst('$PROJECT_DIR'))
spec = importlib.util.spec_from_file_location('prepare_whistle_model', project / 'scripts/prepare_whistle_model.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
image = module.DEFAULT_DIRECTORY / 'whistle_d4.cact'


def validate_model(source, target, env):
    try:
        module.verify(module.DEFAULT_DIRECTORY)
    except (OSError, ValueError, KeyError) as error:
        raise RuntimeError('Prepare model before upload: python3 scripts/prepare_whistle_model.py') from error


env.AddPreAction('upload', validate_model)
env.Append(FLASH_EXTRA_IMAGES=[('0x210000', str(image))])
env.Append(UPLOADERFLAGS=['0x210000', str(image)])
