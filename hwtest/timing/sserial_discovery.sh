#!/bin/bash
# Smart-serial discovery, repeated: load hostmot2 and the bench's SPI driver
# N times (default 12) and count the times the drive (stbl.0.0) is found.
# Nothing is enabled, no motor moves. LinuxCNC and halrun must not run.
#   hwtest/timing/sserial_discovery.sh [N]
# Set the drive's sserial0.reply_mode first (servoterm-cli -c ...): mode 1
# found it 8 of 24 times before the link-timeout fix (DEBUGNOTES 2026-11-01
# "sserial0.reply_mode"), 24 of 24 after; mode 0 24 of 24 either way.
N=${1:-12}
T=$(mktemp -d /tmp/sserial-disc.XXXX)
cat > $T/disc.hal <<H
loadrt hostmot2
loadrt hm2_spix config=" sserial_port_0=00000000" spiclk_rate=25000 spiclk_rate_rd=25000
H
ok=0
for i in $(seq $N); do
  halrun -U >/dev/null 2>&1; sleep 3  # the drive notices the link went away
  (sleep 100000 | halrun -I -f $T/disc.hal > $T/disc-$i.log 2>&1 &)
  f=0
  for k in $(seq 40); do
    halcmd show pin 2>/dev/null | grep -q 'stbl.0.0.pos_fb' && { f=1; break; }
    grep -q 'Timeout trying' $T/disc-$i.log && break
    sleep 0.25
  done
  ok=$((ok + f)); echo -n "$f"
done
halrun -U >/dev/null 2>&1
echo "  found $ok/$N (logs: $T)"
[ $ok = $N ]
