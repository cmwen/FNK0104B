"""Invalidate cached PlatformIO CMake metadata when the monitor version changes.

PlatformIO's reconfigure check ignores CMAKE_CONFIGURE_DEPENDS headers. Removing
only its generated Ninja entry point asks the normal builder to reconfigure.
"""
import json
from pathlib import Path
import re

Import("env")  # noqa: F821

project = Path(env.subst("$PROJECT_DIR"))
build = Path(env.subst("$BUILD_DIR"))
header = project / "apps/codex-monitor/include/monitor_ota.hpp"
match = re.search(r'^constexpr char version\[\] = "([0-9]+\.[0-9]+\.[0-9]+)";', header.read_text(), re.M)
if not match:
    raise RuntimeError("Monitor version must be numeric major.minor.patch")
description = build / "project_description.json"
if description.is_file():
    cached = json.loads(description.read_text()).get("project_version")
    if cached != match[1]:
        (build / "build.ninja").unlink(missing_ok=True)
