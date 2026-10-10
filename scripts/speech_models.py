"""Pack ESP-SR models and include them in PlatformIO CLI uploads.

PlatformIO's ESP-IDF builder does not import component-specific IDF flash
arguments. Use ESP-SR's packer and derive the model offset from our CSV.
"""
import csv
import importlib.util
from pathlib import Path
import subprocess

Import("env")  # noqa: F821
project = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$BUILD_DIR"))
component = project / "managed_components" / "espressif__esp-sr"
model_file = build / "srmodels" / "srmodels.bin"
partition_csv = project / env.GetProjectOption("board_build.partitions")
with partition_csv.open() as stream:
    rows = csv.reader(line for line in stream if not line.lstrip().startswith("#"))
    model = next(row for row in rows if row and row[0].strip() == "model")
offset, size = int(model[3].strip(), 0), int(model[4].strip(), 0)

def pack_models(source, target, env):
    subprocess.run([
        env.subst("$PYTHONEXE"), str(component / "model" / "movemodel.py"),
        "-d1", str(project / ("sdkconfig." + env.subst("$PIOENV"))),
        "-d2", str(component), "-d3", str(build),
    ], check=True)
    if env.subst("$PIOENV") == "codex-monitor":
        spec = importlib.util.spec_from_file_location("monitor_model_bundle", project / "apps/codex-monitor/tools/model_bundle.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        model_file.write_bytes(module.canonicalize(model_file.read_bytes()))
    if not model_file.is_file() or not 0 < model_file.stat().st_size <= size:
        raise RuntimeError("ESP-SR model image missing or larger than model partition")

node = env.Command(str(model_file),
                   [str(project / ("sdkconfig." + env.subst("$PIOENV"))),
                    str(component / "model" / "movemodel.py"), str(partition_csv)],
                   pack_models)
if env.subst("$PIOENV") == "codex-monitor":
    env.Depends(node, str(project / "apps/codex-monitor/tools/model_bundle.py"))
env.AlwaysBuild(node)
env.Alias("buildprog", node)
env.Depends("upload", node)
env.Append(FLASH_EXTRA_IMAGES=[(hex(offset), str(model_file))])

# Post scripts run after the platform has expanded FLASH_EXTRA_IMAGES into
# esptool flags; update both representations so CLI uploads include models.
if env.get("UPLOAD_PROTOCOL") != "esptool":
    raise RuntimeError("ESP-SR model uploads require upload_protocol = esptool")
env.Append(UPLOADERFLAGS=[hex(offset), str(model_file)])
