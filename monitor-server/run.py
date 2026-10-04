#!/usr/bin/env python3
"""Run the LAN bridge using the device's ignored local configuration."""
import json
import os
from pathlib import Path
import re

import server


def configure_dispatcher(environ, path):
    """Load optional ignored local defaults; explicit environment values win."""
    allowed = {"MONITOR_REPOSITORY_ROOT", "MONITOR_ROUTER_MODEL", "MONITOR_ROUTER_EFFORT",
               "MONITOR_WORKER_MODEL", "MONITOR_REPOSITORY_SCAN_DEPTH"}
    if not path.exists():
        return
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, separator, value = line.partition("=")
        if not separator or name.strip() not in allowed:
            raise ValueError("Invalid dispatcher setting in " + str(path))
        value = value.strip()
        if value.startswith('"'):
            value = json.loads(value)
        environ.setdefault(name.strip(), value)


def configure(environ, config_path):
    """Read literal firmware defines without executing header contents."""
    source = config_path.read_text() if config_path.exists() else ""
    for define, variable in (("MONITOR_SERVER_HOST", "MONITOR_HOST"),
                             ("MONITOR_SERVER_TOKEN", "MONITOR_TOKEN")):
        match = re.search(r'^\s*#define\s+' + define + r'\s+("(?:[^"\\]|\\.)*")\s*(?://.*)?$', source, re.M)
        if match:
            value = json.loads(match.group(1))
            if value:
                environ.setdefault(variable, value)
    match = re.search(r'^\s*#define\s+MONITOR_SERVER_PORT\s+(\d+)\s*(?://.*)?$', source, re.M)
    if match:
        environ.setdefault("MONITOR_PORT", match.group(1))
    environ.setdefault("MONITOR_AGENT_CWD", str(Path(__file__).resolve().parents[1]))


if __name__ == "__main__":
    configure_dispatcher(os.environ, Path(__file__).resolve().parents[1] / ".env.dispatcher")
    configure(os.environ, Path(__file__).resolve().parents[1] /
              "apps/codex-monitor/include/monitor_secrets.h")
    server.main()
