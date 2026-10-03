"""Use a local linker preprocessor with PlatformIO 7.0.1 / IDF 5.5.5.

PlatformIO's IDF-6 builder invokes a helper absent in IDF 5.5. Redirect only
the two generated linker-script actions; never modify installed packages.
"""
from pathlib import Path
import subprocess

Import("env")  # noqa: F821

framework = Path(env.PioPlatform().get_package_dir("framework-espidf"))
missing = framework / "tools/cmake/linker_script_preprocessor.cmake"
helper = Path(env.subst("$PROJECT_DIR")) / "scripts/idf_linker_preprocessor.cmake"

def redirect(value):
    if isinstance(value, str):
        return value.replace(str(missing), str(helper))
    if isinstance(value, list):
        return [redirect(item) for item in value]
    return value

if not missing.is_file():
    for name in ("memory.ld", "esp-idf/esp_system/ld/sections.ld.in"):
        node = env.File(env.subst("$BUILD_DIR") + "/" + name)
        replaced = False
        for action in node.get_executor().get_action_list():
            if hasattr(action, "cmd_list") and str(missing) in str(action.cmd_list):
                action.cmd_list = redirect(action.cmd_list)
                replaced = True
        if not replaced:
            raise RuntimeError("PlatformIO linker action changed: " + name)
        env.Depends(node, str(helper))

# Arduino's component graph includes these SDK certificate assets even with
# its cloud libraries disabled. PlatformIO does not import their CMake
# target_add_binary_data generators. Reproduce IDF's TEXT embedding action;
# this does not enable or start any of those services.
cmake = Path(env.PioPlatform().get_package_dir("tool-cmake")) / "bin/cmake"
embed_script = framework / "tools/cmake/scripts/data_file_embed_asm.cmake"

def embed_text(source, target, env):
    subprocess.run([str(cmake), "-DDATA_FILE=" + str(source[0]),
                    "-DSOURCE_FILE=" + str(target[0]), "-DFILE_TYPE=TEXT",
                    "-P", str(embed_script)], check=True)

for component, names in (
    ("esp_insights", ("https_server.crt",)),
    ("esp_rainmaker", ("rmaker_mqtt_server.crt", "rmaker_claim_service_server.crt", "rmaker_ota_server.crt")),
):
    for name in names:
        source = Path(env.subst("$PROJECT_DIR")) / ("managed_components/espressif__" + component) / "server_certs" / name
        node = env.Command(env.subst("$BUILD_DIR") + "/" + name + ".S", str(source), embed_text)
        env.Depends(node, str(embed_script))
