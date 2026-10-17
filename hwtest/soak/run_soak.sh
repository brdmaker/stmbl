#!/bin/bash
# Long-term reliability run, simulated or on the real drive.
#
#   run_soak.sh sim [soak.py options]       LinuxCNC + stmbl_sim (hwtest/burnin-sim)
#   run_soak.sh hw  [soak.py options]       LinuxCNC + the real drive (hwtest/burnin)
#   run_soak.sh console [soak.py options]   no LinuxCNC: health/console checks
#                                           of an idle drive on /dev/ttyACM0
#
# Common options: --hours H, --cycles N, --on-fault stop|continue,
# --console-every N (long-line check, firmware >= v0.9.18-243). See
# soak.py --help. Results: hwtest/soak/runs/<target>-<time>/summary.md.
# Stop cleanly with: touch <run dir>/STOP
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
TARGET=${1:?usage: run_soak.sh sim|hw|console [options]}
shift
RUN=$HERE/runs/$TARGET-$(date +%Y%m%d-%H%M%S)
mkdir -p "$RUN"

case $TARGET in
  console)
    exec python3 "$HERE/soak.py" --target hw --mode console --out "$RUN" "$@"
    ;;
  sim)  SRC=$REPO/hwtest/burnin-sim; PORT=$RUN/stmbl-sim ;;
  hw)   SRC=$REPO/hwtest/burnin;     PORT=/dev/ttyACM0 ;;
  *)    echo "unknown target $TARGET" >&2; exit 1 ;;
esac

if pgrep -x milltask >/dev/null || pgrep -x rtapi_app >/dev/null; then
  echo "LinuxCNC (or a HAL session) is already running; stop it first (halrun -U)" >&2
  exit 1
fi
if [ "$TARGET" = sim ] && [ ! -f /usr/lib/linuxcnc/modules/stmbl_sim.so ]; then
  echo "stmbl_sim module not installed: make -C $REPO/host/lcnc && sudo make -C $REPO/host/lcnc install" >&2
  exit 1
fi

# A copy of the machine config with the soak runner as its display program.
# For sim, the copy also holds the simulated drive's flash (sim.conf).
mkdir -p "$RUN/lcnc"
cp "$SRC"/* "$RUN/lcnc/" 2>/dev/null || true
INI=$RUN/lcnc/stmbl.ini
cat > "$RUN/lcnc/display.sh" <<EOS
#!/bin/sh
# as a GUI would: the POSTGUI_HALFILEs (e.g. phasing_ferror.hal)
cd "$RUN/lcnc"
# (after the task has made its ini.* pins, as AXIS does once it is connected)
i=0; while ! halcmd getp ini.0.ferror >/dev/null 2>&1 && [ \$i -lt 150 ]; do sleep 0.2; i=\$((i+1)); done
for f in \$(sed -n 's/^POSTGUI_HALFILE *= *//p' "$INI"); do halcmd -i "$INI" -f "\$f" || exit 1; done
exec python3 "$HERE/soak.py" > "$RUN/runner.log" 2>&1
EOS
chmod +x "$RUN/lcnc/display.sh"
sed -i "s|^DISPLAY *=.*|DISPLAY = $RUN/lcnc/display.sh|" "$INI"
[ "$TARGET" = sim ] && sed -i "s|^TTY *=.*|TTY = $PORT|" "$INI"

export SOAK_ARGS="--target $TARGET --port $PORT --out $RUN $*"
echo "soak run: $RUN"
echo "stop with: touch $RUN/STOP"
cd "$RUN/lcnc"
# No X display: LinuxCNC's error dialogs would pop up on the desktop.
env -u DISPLAY linuxcnc "$INI" </dev/null >"$RUN/linuxcnc.log" 2>&1 || true
find "$HOME" -maxdepth 1 -name "linuxcnc_*.txt" -newer "$RUN/lcnc/stmbl.ini" -exec cp {} "$RUN/" \; || true
echo "done: $RUN/summary.md"
cat "$RUN/summary.md" 2>/dev/null | head -20
