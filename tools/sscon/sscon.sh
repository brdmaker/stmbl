#!/bin/bash
# The drive's console over smart-serial on a pty, without LinuxCNC:
#   tools/sscon/sscon.sh [PTY]     (default /tmp/stmbl-con), stop with halrun -U
#   servoterm-cli -d /tmp/stmbl-con ...
# The drive's config must load sserial, and it must run before this starts
# (hm2 finds the drive's smart-serial remote when it loads).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TTY=${1:-/tmp/stmbl-con}
if pgrep -x rtapi_app >/dev/null || pgrep -x milltask >/dev/null; then
  echo "LinuxCNC (or a HAL session) is already running; stop it first (halrun -U)" >&2
  exit 1
fi
INI=$(mktemp /tmp/sscon-XXXX.ini)
printf '[SSCON]\nTTY = %s\n' "$TTY" > "$INI"
# halrun -I keeps the session up while its stdin stays open
(sleep infinity | halrun -I -i "$INI" -f "$HERE/sscon.hal" >/tmp/sscon.log 2>&1 &)
for i in $(seq 50); do [ -e "$TTY" ] && break; sleep 0.2; done
[ -e "$TTY" ] || { echo "no $TTY: see /tmp/sscon.log" >&2; exit 1; }
# A drive reset ends the link (hm2 stops the port): restart it while the
# HAL session runs. port_state 3 is running.
P=hm2_7c81.0.sserial.port-0
(while s=$(halcmd getp $P.port_state 2>/dev/null); do
   if [ "$s" != 3 ]; then
     halcmd setp $P.run 0; sleep 0.3; halcmd setp $P.run 1; sleep 1.5
     echo "$(date +%T) port restarted (state was $s)" >> /tmp/sscon.log
   fi
   sleep 0.2
 done &) 
echo "console on $TTY (halrun -U stops it)"
