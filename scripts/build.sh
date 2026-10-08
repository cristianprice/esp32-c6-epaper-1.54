#!/usr/bin/env sh
# Linux/macOS wrapper: ./scripts/build.sh [--idf-path DIR] [options]  (see scripts/build.py)
exec python3 "$(dirname "$0")/build.py" "$@"
