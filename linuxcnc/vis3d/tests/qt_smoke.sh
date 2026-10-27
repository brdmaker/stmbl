#!/bin/sh
# the widget smoke test on a virtual X server (Mesa's GLX)
cd "$(dirname "$0")/.." || exit 1
PY=${PYTHON:-python3}
if [ -n "$DISPLAY" ]; then exec $PY tests/qt_smoke.py; fi
Xvfb :91 -screen 0 1280x800x24 >/dev/null 2>&1 &
X=$!
sleep 1
DISPLAY=:91 $PY tests/qt_smoke.py
rc=$?
kill $X
exit $rc
