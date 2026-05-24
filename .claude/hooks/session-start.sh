#!/bin/bash
# SessionStart hook for stmbl: install the build environment so the firmware
# can be cross-compiled (arm-none-eabi) and the native host simulator
# (host/) can be built and tested in Claude Code on the web sessions.
set -euo pipefail

# Only run in the remote (web) environment; local machines manage their own tools.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

# Install the ARM cross toolchain if missing. Update against the official
# Ubuntu archive only, since third-party PPAs in this image are not reachable.
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  export DEBIAN_FRONTEND=noninteractive
  sudo apt-get update \
    -o Dir::Etc::SourceList="/etc/apt/sources.list.d/ubuntu.sources" \
    -o Dir::Etc::SourceParts="-" >/dev/null
  sudo apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi >/dev/null
fi

# Sanity-build the native host simulator (fast, ~5s). This is the target used
# for automated testing of the control components.
make -C "$CLAUDE_PROJECT_DIR/host" >/dev/null

echo "stmbl env ready: $(arm-none-eabi-gcc -dumpversion) arm-gcc, host simulator built"
