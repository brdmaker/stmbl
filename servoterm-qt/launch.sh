#!/usr/bin/env bash
# Launch ServoTerm connected to the stmbl_host simulator.
#
# Usage:
#   ./launch.sh                       # plain simulator, no script
#   ./launch.sh examples/motor.hal    # load a HAL script at startup
#   ./launch.sh --help                # show servoterm options
#
# The script auto-builds both stmbl_host (in ../host/) and servoterm
# (in ./build/) if the binaries are missing or sources are newer.

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOST_DIR="$SCRIPT_DIR/../host"
BUILD_DIR="$SCRIPT_DIR/build"

# ---- build stmbl_host if needed -----------------------------------------
if [ ! -x "$HOST_DIR/stmbl_host" ] || \
   find "$HOST_DIR" -name "*.c" -newer "$HOST_DIR/stmbl_host" 2>/dev/null | grep -q .; then
  echo "[launch] building stmbl_host..."
  make -C "$HOST_DIR" stmbl_host
fi

# ---- build servoterm if needed -------------------------------------------
if [ ! -x "$BUILD_DIR/servoterm" ] || \
   find "$SCRIPT_DIR/src" -name "*.cpp" -newer "$BUILD_DIR/servoterm" 2>/dev/null | grep -q .; then
  echo "[launch] building servoterm-qt..."
  cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake
  cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

# ---- launch --------------------------------------------------------------
exec "$BUILD_DIR/servoterm" --sim "$HOST_DIR/stmbl_host" "$@"
