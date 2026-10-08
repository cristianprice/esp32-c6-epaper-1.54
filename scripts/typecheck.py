#!/usr/bin/env python3
"""Type-check the MicroPython app (python/) with mypy against MicroPython's own stubs.

    python scripts/typecheck.py

Installs the pinned MicroPython 1.29 ESP32 stubs into .typings/ on first run
(pip install --target), then runs mypy with mypy.ini. Needs mypy (requirements-dev.txt).
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TYPINGS = os.path.join(ROOT, ".typings")
STUBS = "micropython-esp32-stubs==1.29.0.post1"  # must match the MicroPython submodule


def main():
    marker = os.path.join(TYPINGS, ".installed")
    if not os.path.isfile(marker) or open(marker).read().strip() != STUBS:
        print("Installing %s into .typings/" % STUBS, flush=True)
        subprocess.run(
            [sys.executable, "-m", "pip", "install", "--quiet", "--upgrade", "--target", TYPINGS, STUBS],
            check=True,
        )
        with open(marker, "w") as f:
            f.write(STUBS + "\n")
    result = subprocess.run([sys.executable, "-m", "mypy", "--config-file", os.path.join(ROOT, "mypy.ini")], cwd=ROOT)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
