#!/usr/bin/env python3
"""Board-side setup with mpremote (pip install -r requirements-dev.txt).

    python scripts/deploy.py --port COM5 secrets --ssid MyWifi --password secret
    python scripts/deploy.py --port COM5 dev on     # stay awake with REPL (creates /nosleep)
    python scripts/deploy.py --port COM5 dev off    # back to the deep-sleep cycle
    python scripts/deploy.py --port COM5 run        # mount python/ and run the app from the PC

The board must be awake (dev mode or just flashed) for mpremote to reach it.
"""

import argparse
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def mpremote(port, *args):
    cmd = [sys.executable, "-m", "mpremote", "connect", port, *args]
    print("$ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", "-p", required=True, help="serial port (COMx / /dev/ttyACMx)")
    sub = parser.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("secrets", help="write secrets.py (Wi-Fi credentials for the daily NTP sync)")
    s.add_argument("--ssid", required=True)
    s.add_argument("--password", required=True)
    d = sub.add_parser("dev", help="toggle dev mode (no deep sleep)")
    d.add_argument("state", choices=("on", "off"))
    sub.add_parser("run", help="run the app from the PC's python/ folder (mpremote mount)")
    args = parser.parse_args()

    if args.cmd == "secrets":
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "secrets.py")
            with open(path, "w", newline="\n") as f:
                f.write("WIFI_SSID = %r\nWIFI_PASSWORD = %r\n" % (args.ssid, args.password))
            mpremote(args.port, "fs", "cp", path, ":secrets.py")
    elif args.cmd == "dev":
        if args.state == "on":
            mpremote(args.port, "fs", "touch", ":nosleep")
        else:
            mpremote(args.port, "fs", "rm", ":nosleep", "+", "reset")
    elif args.cmd == "run":
        mpremote(args.port, "mount", os.path.join(ROOT, "python"), "exec", "import app; app.main()")


if __name__ == "__main__":
    main()
