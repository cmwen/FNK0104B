"""Select PlatformIO's standalone Xtensa GDB for the Arduino debug build.

The bundled GDB in the Arduino ESP32-S3 compiler package links to Python 2.7,
which is not available on this Ubuntu release. The standalone package is pinned
by [env:hello-debug] in platformio.ini.
"""

from os.path import join

Import("env")  # noqa: F821 - provided by PlatformIO/SCons

package_dir = env.PioPlatform().get_package_dir("tool-xtensa-esp-elf-gdb")
if not package_dir:
    raise RuntimeError("Install the pinned tool-xtensa-esp-elf-gdb package")

env.Replace(GDB=join(package_dir, "bin", "xtensa-esp32s3-elf-gdb"))
