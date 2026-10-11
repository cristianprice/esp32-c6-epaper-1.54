#!/usr/bin/env python3
"""Type-check the MicroPython app (python/) with mypy against MicroPython's own stubs.

    python scripts/typecheck.py

Installs the pinned MicroPython 1.29 ESP32 stubs into .typings/ on first run
(see scripts/install.py), then runs mypy with mypy.ini. Needs mypy (requirements-dev.txt).
"""

import os
import subprocess
import sys

import install


def main():
    install.ensure_stubs()
    result = subprocess.run(
        [sys.executable, "-m", "mypy", "--config-file", os.path.join(install.ROOT, "mypy.ini")], cwd=install.ROOT
    )
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
