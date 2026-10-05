#!/usr/bin/env python3
"""Build every firmware environment, plus the symbol-rich hello diagnostic."""

import subprocess

from package_web_firmware import BUILD_ONLY_ENVIRONMENTS, firmware_environments


if __name__ == "__main__":
    environments = [*BUILD_ONLY_ENVIRONMENTS, *firmware_environments()]
    command = ["pio", "run"]
    for environment in environments:
        command.extend(("-e", environment))
    subprocess.run(command, check=True)
