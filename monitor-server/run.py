#!/usr/bin/env python3
"""Run the LAN bridge using the device's ignored local configuration."""
import json
import os
from pathlib import Path
import re

import server


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
    configure(os.environ, Path(__file__).resolve().parents[1] /
              "apps/codex-monitor/include/monitor_secrets.h")
    server.main()
