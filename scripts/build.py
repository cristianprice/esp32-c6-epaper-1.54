#!/usr/bin/env python3
"""Build the MicroPython firmware for the Waveshare ESP32-C6-ePaper-1.54.

Works on Windows and Linux with a locally installed ESP-IDF v5.5.x (no Docker):

    python scripts/build.py --idf-path C:\\esp\\Espressif\\frameworks\\esp-idf-v5.5.5
    python scripts/build.py                      # uses $IDF_PATH
    python scripts/build.py --test --clean       # run host tests, clean build

Steps: submodules -> ESP-IDF environment -> mpy-cross -> (assets, tests) ->
idf.py build of MicroPython's esp32 port with our board + `bsp` C module + frozen
Python app -> combined image build/<board>/firmware.bin (flash at 0x0).
"""

import argparse
import os
import shutil
import subprocess
import sys
import time

import idf_env
import install

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MPY_DIR = os.path.join(ROOT, "third_party", "micropython")
PORT_DIR = os.path.join(MPY_DIR, "ports", "esp32")
BOARDS_DIR = os.path.join(ROOT, "firmware", "boards")
USER_C_MODULES = os.path.join(ROOT, "firmware", "modules", "micropython.cmake")
DEFAULT_BOARD = "WAVESHARE_C6_EPAPER_154"
WINDOWS_PATCH = os.path.join(ROOT, "patches", "micropython-windows-build.patch")


def log(msg):
    print("\n==> " + msg, flush=True)


def run(cmd, **kw):
    print("$ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, **kw)


def apply_windows_patch():
    """MicroPython's esp32 build is Linux/WSL-only upstream: its qstr generation uses
    command lines over the Windows 32K limit and POSIX tools. The patch switches those
    steps to response files and Python. Applied on Windows only; Linux builds use
    MicroPython unmodified."""
    if not idf_env.IS_WINDOWS:
        return
    log("MicroPython Windows build patch")
    applied = subprocess.run(["git", "-C", MPY_DIR, "apply", "--reverse", "--check", WINDOWS_PATCH],
                             capture_output=True).returncode == 0
    if applied:
        print("already applied")
    else:
        run(["git", "-C", MPY_DIR, "apply", WINDOWS_PATCH])


def find_mpy_cross():
    """Prebuilt mpy-cross from PyPI (pip install mpy-cross==<micropython version>)."""
    path = os.environ.get("MICROPY_MPYCROSS")
    if path:
        return path
    try:
        import mpy_cross

        if os.path.isfile(mpy_cross.mpy_cross):
            return mpy_cross.mpy_cross
    except ImportError:
        pass
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    idf_env.add_arguments(parser)
    parser.add_argument("--board", default=DEFAULT_BOARD, help="board directory under firmware/boards")
    parser.add_argument("--build-dir", help="build directory (default: build/<board>)")
    parser.add_argument("--clean", action="store_true", help="delete the build directory first")
    parser.add_argument("--test", action="store_true", help="run the host tests before building")
    parser.add_argument("--typecheck", action="store_true", help="type-check python/ (mypy) before building")
    parser.add_argument("--regen-assets", action="store_true", help="regenerate icons, fonts and the face mockup")
    parser.add_argument("--no-submodules", action="store_true", help="don't init/update git submodules")
    parser.add_argument("--verbose", action="store_true", help="verbose idf.py output")
    args = parser.parse_args()

    board_dir = os.path.join(BOARDS_DIR, args.board)
    if not os.path.isfile(os.path.join(board_dir, "mpconfigboard.cmake")):
        sys.exit("error: unknown board %s (no %s/mpconfigboard.cmake)" % (args.board, board_dir))
    build_dir = os.path.abspath(args.build_dir or os.path.join(ROOT, "build", args.board))
    start = time.time()

    if not args.no_submodules:
        log("Submodules")
        install.ensure_submodules()
    apply_windows_patch()

    log("ESP-IDF")
    idf_path, idf_version, env = idf_env.from_args(args)
    print("ESP-IDF %s at %s" % (idf_version, idf_path))
    print("Python env: %s" % env["IDF_PYTHON"])

    log("mpy-cross")
    mpy_cross = find_mpy_cross()
    if mpy_cross:
        env["MICROPY_MPYCROSS"] = mpy_cross
        print(mpy_cross)
    else:
        print("prebuilt mpy-cross not found (pip install -r requirements-dev.txt);\n"
              "MicroPython will build it from source, which needs make and a host C compiler")

    if args.regen_assets:
        log("Assets")
        for tool in ("make_icons.py", "make_fonts.py", "make_mockup.py"):
            run([sys.executable, os.path.join(ROOT, "tools", tool)])

    if args.typecheck:
        log("Type check")
        run([sys.executable, os.path.join(ROOT, "scripts", "typecheck.py")])

    if args.test:
        log("Host tests")
        run([sys.executable, os.path.join(ROOT, "tests", "host", "test_app.py")])

    if args.clean and os.path.isdir(build_dir):
        log("Clean " + build_dir)
        shutil.rmtree(build_dir)

    log("Build %s" % args.board)
    idf_py = [env["IDF_PYTHON"], os.path.join(idf_path, "tools", "idf.py")]
    cmake_path = lambda p: p.replace("\\", "/")  # CMake treats backslashes as escapes
    cmd = idf_py + [
        "-C", PORT_DIR,
        "-B", build_dir,
        "-D", "MICROPY_BOARD=%s" % args.board,
        "-D", "MICROPY_BOARD_DIR=%s" % cmake_path(board_dir),
        "-D", "USER_C_MODULES=%s" % cmake_path(USER_C_MODULES),
    ]
    if args.verbose:
        cmd.append("--verbose")
    try:
        run(cmd + ["build"], env=env)
    except subprocess.CalledProcessError:
        sys.exit("error: build failed (see https://github.com/micropython/micropython/wiki/Build-Troubleshooting)")

    log("Firmware image")
    firmware = os.path.join(build_dir, "firmware.bin")
    run([env["IDF_PYTHON"], os.path.join(PORT_DIR, "makeimg.py"),
         os.path.join(build_dir, "sdkconfig"),
         os.path.join(build_dir, "bootloader", "bootloader.bin"),
         os.path.join(build_dir, "partition_table", "partition-table.bin"),
         os.path.join(build_dir, "micropython.bin"),
         firmware,
         os.path.join(build_dir, "micropython.uf2")], env=env, cwd=PORT_DIR)

    log("Done in %.0f s" % (time.time() - start))
    print("firmware: %s" % firmware)
    print("flash:    python scripts/flash.py --port <PORT>   (COMx on Windows, /dev/ttyACM0 on Linux)")


if __name__ == "__main__":
    main()
