#!/bin/bash
# Restore the prebuilt LinuxCNC 2.9.10 (uspace, run-in-place, x86_64) used by stmbl's LinuxCNC
# tests (hwtest/burnin-sim; brdmot's hwtest/*-sim; host/lcnc's stmbl_sim) in a fresh cloud
# container, instead of building it (~25 min), then build and install stmbl's LinuxCNC modules
# in it. The binaries and their restore.sh live on the orphan branches linuxcnc-bin-2.9.10-<date>
# of brdmaker/linuxcnc (the newest is used; the branch's README says how it was built; stmbl's
# own orphan branch linuxcnc-bin is no longer used). Idempotent; ~1-2 min, mostly apt.
#
#   hwtest/linuxcnc_restore.sh   -> /home/user/linuxcnc-2.9.10, /home/user/linuxcnc-2.9 -> it,
#                                   stmbl's modules installed in it
#   LCNC_QT=1 hwtest/linuxcnc_restore.sh   also PyQt5 for QtVCP screens
#
# Then as the non-root user it creates (rtapi_app refuses root):
#   su lcnc -c "export PATH=/home/user/linuxcnc-2.9/pybin:\$PATH; make -C hwtest/burnin-sim test"
set -euo pipefail
REPO_URL=${LINUXCNC_BIN_REPO:-https://github.com/brdmaker/linuxcnc}
REPO=$(cd "$(dirname "$0")/.." && pwd)
BRANCH=$(git ls-remote --heads "$REPO_URL" 'linuxcnc-bin-2.9.10-*' | sed 's#.*refs/heads/##' | sort | tail -1)
[ -n "$BRANCH" ] || { echo "no linuxcnc-bin-2.9.10-* branch in $REPO_URL" >&2; exit 1; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
git -C "$tmp" init -q
git -C "$tmp" fetch -q --depth=1 "$REPO_URL" "refs/heads/$BRANCH"
git -C "$tmp" show FETCH_HEAD:restore.sh > "$tmp/restore.sh"
bash "$tmp/restore.sh" "$BRANCH"

# stmbl's LinuxCNC modules: the simulated drive (stmbl_sim), the simulated arm, the consoles
LCNC=/home/user/linuxcnc-2.9.10
M="MODINC=$LCNC/src/Makefile.modinc"
make -s -C "$REPO/host/lcnc" "$M" all install >/dev/null
make -s -C "$REPO/host/arm" all >/dev/null
make -s -C "$REPO/host/arm/lcnc" "$M" all install >/dev/null
make -s -C "$REPO/linuxcnc/sserialterm" "$M" all install >/dev/null
make -s -C "$REPO/linuxcnc/stmblcan" "$M" all install >/dev/null
if [ "$(id -u)" = 0 ]; then chown -R lcnc:lcnc "$LCNC/rtlib"; fi
echo "LinuxCNC ($BRANCH) ready at $LCNC (stmbl's modules installed)"
