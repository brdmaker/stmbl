#!/bin/bash
# Flash the application through the stmbl bootloader's smart-serial loader
# with LinuxCNC's setsserial (docs/sserial_bootloader.md). `make ssburn`
# runs it after `make ssfw_install`.
#
#   tools/ssflash.sh            the drive on USB: sserialboot sent there
#   tools/ssflash.sh --in-loader  the drive is already in the loader (broken
#                               application, or sserialboot sent elsewhere)
#
# Environment: HM2_CARD (hm2_7c81), HM2_DRIVER (the loadrt line's driver and
# parameters; default from hwtest/burnin/stmbl.ini [HOSTMOT2]), HM2_CONFIG
# (the driver's config string, if any), DEV (the STMBL by-id link, else /dev/ttyACM0).
# LinuxCNC and halrun must not be running; the drive must be disabled.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
CLI=$REPO/servoterm-cli/servoterm-cli
# the drive's USB console by its serial number: after a console that stalled
# the drive comes back as ttyACM1 while ttyACM0 is still held (DEBUGNOTES
# 2026-10-29); the first STMBL one, else ttyACM0
DEV=${DEV:-$(ls /dev/serial/by-id/usb-STMicroelectronics_STMBL_Virtual_ComPort_*-if00 2>/dev/null | head -1)}
DEV=${DEV:-/dev/ttyACM0}
CARD=${HM2_CARD:-hm2_7c81}
DRIVER=${HM2_DRIVER:-$(python3 "$REPO/hwtest/hm2_driver.py" | sed "s/^loadrt //")}
IMAGE=stmbl/STMBL_SS.BIN

if pgrep -x milltask >/dev/null || pgrep -x rtapi_app >/dev/null; then
  echo "ssflash: LinuxCNC or a HAL session is running; stop it first (halrun -U)" >&2
  exit 1
fi
[ -f /lib/firmware/$IMAGE ] || { echo "ssflash: no /lib/firmware/$IMAGE (make ssfw_install)" >&2; exit 1; }

if [ "$1" != --in-loader ]; then
  [ -e "$DEV" ] || { echo "ssflash: no $DEV; if the drive is in its loader, use --in-loader" >&2; exit 1; }
  echo "ssflash: sserialboot on $DEV"
  "$CLI" -d "$DEV" -t 0.5 -c sserialboot | grep -E 'fw (ok|err)' || true
  for i in $(seq 50); do [ -e "$DEV" ] || break; sleep 0.1; done
  [ -e "$DEV" ] && { echo "ssflash: the drive did not reset into the loader (enabled?)" >&2; exit 1; }
fi

# the loader erases the application area before it answers; a hostmot2
# started earlier finds nothing. Within 30 s (a broken application goes
# on to DFU without smart-serial traffic).
echo "ssflash: waiting 15 s for the loader"
sleep 15
HAL=$(mktemp /tmp/ssflash.XXXXXX.hal)
LOG=$(mktemp /tmp/ssflash.XXXXXX.log)
{
  echo "loadrt hostmot2 sserial_baudrate=115200"
  echo "loadrt $DRIVER ${HM2_CONFIG:+config=\"$HM2_CONFIG\"}"
  echo "loadrt setsserial cmd=\"flash $CARD.0.sibl.0.0 $IMAGE\""
} > "$HAL"
echo "ssflash: setsserial flash $CARD.0.sibl.0.0 $IMAGE"
halrun -f "$HAL" > "$LOG" 2>&1 || true
halrun -U >/dev/null 2>&1 || true
rm -f "$HAL"
wrote=$(grep -c 'Wrote block' "$LOG" || true)
errors=$(grep -c -E 'Error in sslbp|Synch failed|Flash Failed|Unable to find' "$LOG" || true)
if ! grep -q 'Flash Success' "$LOG" || [ "$wrote" = 0 ] || [ "$errors" != 0 ]; then
  echo "ssflash: FAILED ($wrote blocks written, $errors errors); log: $LOG" >&2
  grep -v 'IO Pin' "$LOG" | grep -E 'found a|Unable|rror|Synch|Size' | head -5 >&2
  echo "ssflash: the drive stays in its loader; run again, or power it to reach DFU (make btburn)" >&2
  exit 1
fi
echo "ssflash: $wrote blocks written; waiting for the application"
for i in $(seq 100); do [ -e "$DEV" ] && break; sleep 0.3; done
sleep 3
"$CLI" -d "$DEV" -t 0.4 -c about 2>/dev/null | grep -m1 'STMBL v' || { echo "ssflash: no answer on $DEV" >&2; exit 1; }
rm -f "$LOG"
