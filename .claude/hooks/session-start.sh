#!/bin/bash
# SessionStart hook for stmbl: build the native (x86) host simulator so the
# control components can be run and tested in Claude Code on the web sessions.
# The host build only needs gcc/make/python3, which the base image provides,
# so there is nothing to install.
set -euo pipefail

# Only run in the remote (web) environment; local machines manage their own tools.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

# Sanity-build the native host simulator (fast, ~5s).
make -C "$CLAUDE_PROJECT_DIR/host" >/dev/null

echo "stmbl env ready: host simulator built"
