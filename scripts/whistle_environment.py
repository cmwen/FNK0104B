"""Select an isolated IDF component and manifest for standalone Whistle."""
Import('env')  # noqa: F821
from pathlib import Path
env.Replace(PROJECT_SRC_DIR=str(Path(env.subst('$PROJECT_DIR')) / 'apps/29-whistle'))
