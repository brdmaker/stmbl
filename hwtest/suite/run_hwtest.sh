#!/bin/bash
# Hardware test suite: the drive alone over USB, then under LinuxCNC.
#
#   run_hwtest.sh hw  [hwtest.py options]   the real drive: /dev/ttyACM0,
#                                           LinuxCNC on hwtest/burnin + hm2
#   run_hwtest.sh sim [hwtest.py options]   the simulated drive: stmbl_host
#                                           --pty, then LinuxCNC + stmbl_sim
#   --phase-only drive|lcnc                 run one phase
#
# Options for hwtest.py: --only NAMES, --dc-volt V, --dumps N, ... (see
# hwtest.py --help). Results: hwtest/suite/runs/<target>-<time>/summary.md.
# On hw the motor moves (X +-300 mm at up to 500 mm/s): the machine must be
# safe to move. LinuxCNC and halrun must not be running.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
TARGET=${1:?usage: run_hwtest.sh hw|sim [--phase-only drive|lcnc] [options]}
shift
PHASES="drive lcnc"
if [ "$1" = --phase-only ]; then PHASES=$2; shift 2; fi
RUN=$HERE/runs/$TARGET-$(date +%Y%m%d-%H%M%S)
mkdir -p "$RUN"
echo "hardware test run: $RUN"

if pgrep -x milltask >/dev/null || pgrep -x rtapi_app >/dev/null; then
  echo "LinuxCNC (or a HAL session) is already running; stop it first (halrun -U)" >&2
  exit 1
fi
make -s -C "$REPO/servoterm-cli"
for m in sserialterm $([ "$TARGET" = sim ] && echo stmbl_sim); do
  [ -f /usr/lib/linuxcnc/modules/$m.so ] || { echo "LinuxCNC module $m not installed (see hwtest/suite/README.md)" >&2; exit 1; }
done

case $TARGET in
  hw)  SRC=$REPO/hwtest/burnin;     USB=/dev/ttyACM0; PREFIX=hm2_7c81.0.stbl.0.0. ;;
  sim) SRC=$REPO/hwtest/burnin-sim; USB=$RUN/drive-usb; PREFIX=stmbl-sim.0. ;;
  *)   echo "unknown target $TARGET" >&2; exit 1 ;;
esac
CON=$RUN/drive-con
ARGS="--target $TARGET --out $RUN --usb $USB --con $CON --prefix $PREFIX $*"
rc=0

if [[ " $PHASES " == *" drive "* ]]; then
  echo "== drive phase (USB console, no LinuxCNC)"
  SIMPID=
  if [ "$TARGET" = sim ]; then
    # the simulated drive alone, booted from a copy of its flash
    cp "$SRC/sim.conf" "$RUN/sim-drive.conf"
    "$REPO/host/stmbl_host" --drive "$RUN/sim-drive.conf" --sim "$SRC/sim.txt" --pty "$USB" \
      >"$RUN/stmbl_host.log" 2>&1 &
    SIMPID=$!
    for i in $(seq 50); do [ -e "$USB" ] && break; sleep 0.1; done
    sleep 1
  fi
  python3 "$HERE/hwtest.py" --phase drive $ARGS || rc=1
  [ -n "$SIMPID" ] && kill $SIMPID 2>/dev/null || true
fi

if [[ " $PHASES " == *" lcnc "* ]]; then
  echo "== LinuxCNC phase"
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
exec python3 "$HERE/hwtest.py" --phase lcnc $ARGS > "$RUN/lcnc-phase.log" 2>&1
EOS
  chmod +x "$RUN/lcnc/display.sh"
  sed -i "s|^DISPLAY *=.*|DISPLAY = $RUN/lcnc/display.sh|" "$INI"
  if [ "$TARGET" = sim ]; then
    sed -i "s|^HALFILE = stmbl_sim.hal|HALFILE = stmbl_sim.hal\nHALFILE = console.hal|; s|^TTY *=.*|TTY = $USB|" "$INI"
    [ -f "$RUN/sim-drive.conf" ] && cp "$RUN/sim-drive.conf" "$RUN/lcnc/sim.conf"
  else
    cp "$HERE/hw_console.hal" "$RUN/lcnc/"
    sed -i "s|^HALFILE = stmbl.hal|HALFILE = stmbl.hal\nHALFILE = hw_console.hal|" "$INI"
  fi
  printf '\n[SSERIALTERM]\nTTY = %s\n' "$CON" >> "$INI"
  # No X display: LinuxCNC's error dialogs would pop up on the desktop.
  (cd "$RUN/lcnc" && env -u DISPLAY linuxcnc "$INI" </dev/null >"$RUN/linuxcnc.log" 2>&1) || true
  cat "$RUN/lcnc-phase.log" 2>/dev/null | grep -E '^(ok|fail|skip) ' || true
  grep -q '"status": "fail"' "$RUN/results-lcnc.json" 2>/dev/null && rc=1
  [ -f "$RUN/results-lcnc.json" ] || { echo "LinuxCNC phase produced no results: $RUN/linuxcnc.log" >&2; rc=1; }
fi

if [[ " $PHASES " == *" lcnc "* ]] && [ "$TARGET" = hw ]; then
  echo "== after LinuxCNC (USB console)"
  python3 "$HERE/hwtest.py" --phase post $ARGS || rc=1
fi

python3 "$HERE/hwtest.py" --phase report --out "$RUN" || rc=1
echo "summary: $RUN/summary.md"
exit $rc
