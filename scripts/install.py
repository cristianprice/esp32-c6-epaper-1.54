#!/usr/bin/env python3
"""Fetch everything the project needs that isn't stored in git (Windows and Linux).

    python scripts/install.py                 # pip tools + MicroPython submodule + type stubs
    python scripts/install.py --reference     # also clone the Waveshare example repo (BSP reference)

- host tools from requirements-dev.txt (pip)
- third_party/micropython: git submodule pinned to v1.29.0, plus the MicroPython
  submodules the esp32 port needs (shallow clones)
- .typings/: MicroPython 1.29 ESP32 stubs for scripts/typecheck.py
- third_party/waveshare (optional): Waveshare examples/schematic at a pinned commit

The ESP-IDF itself is installed separately (v5.5.x) and passed to scripts/build.py.
build.py and typecheck.py call the relevant steps themselves when something is missing.
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MPY_DIR = os.path.join(ROOT, "third_party", "micropython")
MPY_SUBMODULES = ("lib/micropython-lib", "lib/berkeley-db-1.xx")
TYPINGS = os.path.join(ROOT, ".typings")
STUBS = "micropython-esp32-stubs==1.29.0.post1"  # must match the MicroPython submodule
WAVESHARE_URL = "https://github.com/waveshareteam/ESP32-C6-ePaper-1.54.git"
WAVESHARE_DIR = os.path.join(ROOT, "third_party", "waveshare")
WAVESHARE_COMMIT = "c4c47b6a8001f9daa50b38912393c158371e03be"  # the BSP version the port is based on


# Deep checkout paths exceed MAX_PATH (260) inside .git/modules on Windows.
GIT = ["git", "-c", "core.longpaths=true"] if os.name == "nt" else ["git"]


def run(cmd, **kw):
    print("$ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, **kw)


def ensure_submodules():
    """MicroPython checkout at the commit recorded in this repo, plus its needed submodules."""
    if not os.path.isfile(os.path.join(MPY_DIR, "py", "mpconfig.h")):
        run(GIT + ["-C", ROOT, "submodule", "update", "--init", "--depth", "1", "third_party/micropython"])
    missing = [m for m in MPY_SUBMODULES if not os.listdir(os.path.join(MPY_DIR, m))]
    if missing:
        run(GIT + ["-C", MPY_DIR, "submodule", "update", "--init", "--depth", "1", *missing])


def ensure_stubs():
    """MicroPython type stubs in .typings/ (pip --target), reinstalled if the pin changes."""
    marker = os.path.join(TYPINGS, ".installed")
    if os.path.isfile(marker):
        with open(marker) as f:
            if f.read().strip() == STUBS:
                return
    run([sys.executable, "-m", "pip", "install", "--quiet", "--upgrade", "--target", TYPINGS, STUBS])
    with open(marker, "w") as f:
        f.write(STUBS + "\n")


def ensure_reference():
    """Waveshare example repo (schematic, original BSP, factory firmware) - reference only."""
    if os.path.isdir(os.path.join(WAVESHARE_DIR, ".git")):
        return
    run(GIT + ["init", "-q", WAVESHARE_DIR])
    run(GIT + ["-C", WAVESHARE_DIR, "fetch", "--depth", "1", WAVESHARE_URL, WAVESHARE_COMMIT])
    run(GIT + ["-C", WAVESHARE_DIR, "checkout", "-q", "FETCH_HEAD"])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--no-pip", action="store_true", help="skip pip install -r requirements-dev.txt")
    parser.add_argument("--reference", action="store_true", help="also clone the Waveshare example repo")
    args = parser.parse_args()

    if not args.no_pip:
        print("\n==> Host tools", flush=True)
        run([sys.executable, "-m", "pip", "install", "-r", os.path.join(ROOT, "requirements-dev.txt")])
    print("\n==> MicroPython", flush=True)
    ensure_submodules()
    print("\n==> Type stubs", flush=True)
    ensure_stubs()
    if args.reference:
        print("\n==> Waveshare reference", flush=True)
        ensure_reference()
    print("\nDone. Next: python scripts/build.py --idf-path <ESP-IDF v5.5.x>")


if __name__ == "__main__":
    main()
