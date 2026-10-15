#!/bin/bash
# Restore the prebuilt LinuxCNC 2.9 (uspace, run-in-place) used by
# hwtest/arcmate-sim in a fresh cloud container, instead of building it
# (~25 min). The archive lives on the orphan branch linuxcnc-bin (see its
# README: how it was built). Idempotent; ~1 min (mostly apt).
#
#   hwtest/arcmate-sim/linuxcnc_restore.sh     -> /home/user/linuxcnc-2.9
#
# Then, as a non-root user (rtapi_app refuses root; this creates "lcnc"):
#   su lcnc -c "export PATH=/home/user/linuxcnc-2.9/pybin:\$PATH; \
#               make -C hwtest/arcmate-sim test"
# The paths inside the build are absolute: it must live at /home/user.
set -euo pipefail
LCNC=/home/user/linuxcnc-2.9
REPO=$(cd "$(dirname "$0")/../.." && pwd)

if [ ! -x "$LCNC/bin/rtapi_app" ]; then
  tmp=$(mktemp -d)
  git -C "$REPO" fetch -q origin linuxcnc-bin
  git -C "$REPO" show FETCH_HEAD:linuxcnc-2.9-uspace.tar.xz > "$tmp/l.tar.xz"
  tar -C /home/user -xJf "$tmp/l.tar.xz"
  rm -rf "$tmp"
fi

# runtime libraries (the build's ldd closure) and Tcl/Tk/Python 3.12
PKGS="python3.12 libpython3.12t64 tcl8.6 tk8.6 tclx bwidget libboost-python1.83.0 \
  libmodbus5 libusb-1.0-0 libtirpc3t64 libgpiod2t64 libedit2 libgtk-3-0t64 libxmu6 \
  libxinerama1 libepoxy0 libgl1 libglu1-mesa procps psmisc"
missing=$(for p in $PKGS; do dpkg -s "$p" >/dev/null 2>&1 || echo "$p"; done)
if [ -n "$missing" ]; then
  DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends $missing >/dev/null
fi

# LinuxCNC's Python modules are built for 3.12; the image's python3 may not be
mkdir -p "$LCNC/pybin"
ln -sf /usr/bin/python3.12 "$LCNC/pybin/python3"
ln -sf /usr/bin/python3.12 "$LCNC/pybin/python"

# rtapi_app refuses to run as root
if [ "$(id -u)" = 0 ] && ! id lcnc >/dev/null 2>&1; then
  useradd -m -s /bin/bash lcnc
fi

# the simulated arm's LinuxCNC module
make -s -C "$REPO/host/arm" all >/dev/null
make -s -C "$REPO/host/arm/lcnc" MODINC="$LCNC/src/Makefile.modinc" all install >/dev/null
# the collision detector
make -s -C "$REPO/linuxcnc/armcollide" MODINC="$LCNC/src/Makefile.modinc" all install >/dev/null
make -s -C "$REPO/linuxcnc/armguard" MODINC="$LCNC/src/Makefile.modinc" all install >/dev/null
make -s -C "$REPO/linuxcnc/armgrip" MODINC="$LCNC/src/Makefile.modinc" all install >/dev/null
make -s -C "$REPO/linuxcnc/armkins" MODINC="$LCNC/src/Makefile.modinc" all install >/dev/null
make -s -C "$REPO/linuxcnc/armtool" MODINC="$LCNC/src/Makefile.modinc" all install >/dev/null
make -s -C "$REPO/linuxcnc/powerquota" MODINC="$LCNC/src/Makefile.modinc" all install >/dev/null
echo "LinuxCNC $(cat "$LCNC/VERSION") ready at $LCNC"
