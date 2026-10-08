"""Locate a local ESP-IDF install and produce its environment (Windows and Linux, no Docker).

The IDF is given with --idf-path or the IDF_PATH environment variable. Its environment is
obtained from ESP-IDF itself (`tools/idf_tools.py export --format key-value`, run with the
IDF's own Python virtualenv), so no shell-specific export script has to be sourced.
"""

import glob
import os
import re
import subprocess
import sys

IS_WINDOWS = os.name == "nt"
SUPPORTED_IDF = ((5, 5),)  # major.minor accepted (MicroPython v1.29 supports IDF 5.3 - 5.5)


class IdfError(Exception):
    pass


def resolve_idf_path(cli_path):
    path = cli_path or os.environ.get("IDF_PATH")
    if not path:
        raise IdfError(
            "ESP-IDF not specified: pass --idf-path <dir> or set IDF_PATH\n"
            "  e.g. Windows: --idf-path C:\\esp\\Espressif\\frameworks\\esp-idf-v5.5.5\n"
            "       Linux:   --idf-path ~/esp/esp-idf"
        )
    path = os.path.abspath(os.path.expanduser(path))
    if not os.path.isfile(os.path.join(path, "tools", "idf.py")):
        raise IdfError("not an ESP-IDF directory (tools/idf.py missing): " + path)
    return path


def idf_version(idf_path):
    """(major, minor, patch, text) from version.txt, git describe or version.cmake."""
    text = None
    version_txt = os.path.join(idf_path, "version.txt")
    if os.path.isfile(version_txt):
        with open(version_txt) as f:
            text = f.read().strip()
    if not text:
        try:
            text = subprocess.check_output(
                ["git", "-C", idf_path, "describe", "--tags"], stderr=subprocess.DEVNULL, text=True
            ).strip()
        except (OSError, subprocess.CalledProcessError):
            text = None
    if not text:
        with open(os.path.join(idf_path, "tools", "cmake", "version.cmake")) as f:
            parts = dict(re.findall(r"set\(IDF_VERSION_(\w+) (\d+)\)", f.read()))
        text = "v%s.%s.%s" % (parts["MAJOR"], parts["MINOR"], parts["PATCH"])
    m = re.match(r"v?(\d+)\.(\d+)(?:\.(\d+))?", text)
    if not m:
        raise IdfError("can't parse ESP-IDF version: " + text)
    return int(m.group(1)), int(m.group(2)), int(m.group(3) or 0), text


def check_version(idf_path, allow_any=False):
    major, minor, patch, text = idf_version(idf_path)
    if (major, minor) not in SUPPORTED_IDF and not allow_any:
        raise IdfError(
            "ESP-IDF %s at %s is not supported; use v5.5.x (or pass --allow-any-idf)" % (text, idf_path)
        )
    return text


def _tools_path():
    return os.environ.get("IDF_TOOLS_PATH") or os.path.join(os.path.expanduser("~"), ".espressif")


def _venv_python(env_dir):
    exe = os.path.join(env_dir, "Scripts", "python.exe") if IS_WINDOWS else os.path.join(env_dir, "bin", "python")
    return exe if os.path.isfile(exe) else None


def find_idf_python_env(idf_path):
    """Directory of the IDF virtualenv created by the ESP-IDF installer.

    Looked up in IDF_PYTHON_ENV_PATH, else in $IDF_TOOLS_PATH/python_env (the tools
    the IDF was installed with), else ~/.espressif/python_env."""
    env_dir = os.environ.get("IDF_PYTHON_ENV_PATH")
    if env_dir and _venv_python(env_dir):
        return env_dir
    major, minor, _, _ = idf_version(idf_path)
    host = "py%d.%d" % sys.version_info[:2]
    for tools in dict.fromkeys([_tools_path(), os.path.join(os.path.expanduser("~"), ".espressif")]):
        pattern = os.path.join(tools, "python_env", "idf%d.%d_py*_env" % (major, minor))
        envs = [e for e in sorted(glob.glob(pattern), reverse=True) if _venv_python(e)]
        if envs:
            envs.sort(key=lambda e: host not in os.path.basename(e))  # prefer this Python's version
            return envs[0]
    raise IdfError(
        "no ESP-IDF Python environment found for v%d.%d under %s/python_env\n"
        "  run the IDF installer (install.sh / install.ps1) or set IDF_PYTHON_ENV_PATH"
        % (major, minor, _tools_path())
    )


def idf_environment(idf_path):
    """Environment dict with the IDF tools, IDF Python env and IDF_PATH set up."""
    base = dict(os.environ)
    base.pop("MSYSTEM", None)  # ESP-IDF refuses to run under MSYS/Git Bash markers
    base["IDF_PATH"] = idf_path
    env_dir = find_idf_python_env(idf_path)
    base["IDF_PYTHON_ENV_PATH"] = env_dir  # idf.py must use exactly this env
    python = _venv_python(env_dir)
    out = subprocess.run(
        [python, os.path.join(idf_path, "tools", "idf_tools.py"), "export", "--format", "key-value"],
        env=base, capture_output=True, text=True,
    )
    if out.returncode != 0:
        raise IdfError("idf_tools.py export failed:\n" + out.stderr)
    env = dict(base)
    for line in out.stdout.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key == "PATH":
            value = value.replace("%PATH%", base.get("PATH", "")).replace("$PATH", base.get("PATH", ""))
        env[key] = value
    env["IDF_PYTHON"] = python
    return env


def add_arguments(parser):
    parser.add_argument("--idf-path", help="ESP-IDF v5.5.x directory (default: $IDF_PATH)")
    parser.add_argument("--allow-any-idf", action="store_true", help="skip the ESP-IDF version check")


def from_args(args):
    """(idf_path, version_text, env) or exit with a clear message."""
    try:
        idf_path = resolve_idf_path(args.idf_path)
        version = check_version(idf_path, args.allow_any_idf)
        return idf_path, version, idf_environment(idf_path)
    except IdfError as e:
        sys.exit("error: %s" % e)
