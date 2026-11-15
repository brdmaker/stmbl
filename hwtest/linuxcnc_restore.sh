#!/bin/bash
# Restore the prebuilt LinuxCNC 2.9.10 (uspace, run-in-place, x86_64) for the simulations
# (hwtest/*-sim, host/*/lcnc) in a fresh cloud container, then build and install stmbl's
# LinuxCNC module for the simulated arm. The binaries and their restore.sh live on the orphan
# branches linuxcnc-bin-2.9.10-<date> of brdmaker/linuxcnc (the newest is used; the
# branch's README says how it was built). Idempotent; ~1-2 min, mostly apt.
#
#   hwtest/linuxcnc_restore.sh   -> /home/user/linuxcnc-2.9.10, /home/user/linuxcnc-2.9 -> it
#   LCNC_QT=1 hwtest/linuxcnc_restore.sh   also PyQt5 for QtVCP screens
#
# Then as the non-root user it creates (rtapi_app refuses root):
#   su lcnc -c "export PATH=/home/user/linuxcnc-2.9/pybin:\$PATH; make -C hwtest/burnin-sim test"
set -euo pipefail
REPO_URL=${LINUXCNC_BIN_REPO:-https://github.com/brdmaker/linuxcnc}
STMBL=$(cd "$(dirname "$0")/.." && pwd)
BRANCH=$(git ls-remote --heads "$REPO_URL" 'linuxcnc-bin-2.9.10-*' | sed 's#.*refs/heads/##' | sort | tail -1)
[ -n "$BRANCH" ] || { echo "no linuxcnc-bin-2.9.10-* branch in $REPO_URL" >&2; exit 1; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
git -C "$tmp" init -q
git -C "$tmp" fetch -q --depth=1 "$REPO_URL" "refs/heads/$BRANCH"
git -C "$tmp" show FETCH_HEAD:restore.sh > "$tmp/restore.sh"
bash "$tmp/restore.sh" "$BRANCH"

# the simulated arm's LinuxCNC module (hwtest/arcmate-sim in brdmot)
LCNC=/home/user/linuxcnc-2.9.10
make -s -C "$STMBL/host/arm" all >/dev/null
make -s -C "$STMBL/host/arm/lcnc" "MODINC=$LCNC/src/Makefile.modinc" all install >/dev/null
[ "$(id -u)" = 0 ] && chown -R lcnc:lcnc "$LCNC/rtlib"
echo "stmbl_arm installed in $LCNC/rtlib"
