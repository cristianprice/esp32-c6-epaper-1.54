#!/usr/bin/env python3
"""Flash build/<board>/firmware.bin with the ESP-IDF's esptool (Windows and Linux).

    python scripts/flash.py --port COM5 [--erase]
    python scripts/flash.py --port /dev/ttyACM0 --idf-path ~/esp/esp-idf

While the clock deep-sleeps its USB port is gone: hold BOOT and plug in USB (or press
PWR to wake it into dev mode) so the board shows up as a serial port.
"""

import argparse
import os
import subprocess
import sys

import idf_env

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    idf_env.add_arguments(parser)
    parser.add_argument("--port", "-p", required=True, help="serial port (COMx / /dev/ttyACMx)")
    parser.add_argument("--board", default="WAVESHARE_C6_EPAPER_154")
    parser.add_argument("--baud", "-b", default="460800")
    parser.add_argument("--erase", action="store_true", help="erase the whole flash first (wipes the filesystem)")
    args = parser.parse_args()

    firmware = os.path.join(ROOT, "build", args.board, "firmware.bin")
    if not os.path.isfile(firmware):
        sys.exit("error: %s not found, run scripts/build.py first" % firmware)
    _, _, env = idf_env.from_args(args)
    esptool = [env["IDF_PYTHON"], "-m", "esptool", "--chip", "esp32c6", "-p", args.port, "-b", args.baud]
    if args.erase:
        subprocess.run(esptool + ["erase_flash"], env=env, check=True)
    subprocess.run(esptool + ["write_flash", "0x0", firmware], env=env, check=True)


if __name__ == "__main__":
    main()
