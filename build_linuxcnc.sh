#!/bin/bash
# How linuxcnc-2.9-uspace.tar.xz was built (Ubuntu 24.04 cloud container,
# x86_64, 2026-10-15): LinuxCNC branch 2.9 (2.9.10), uspace, run-in-place
# at /home/user/linuxcnc-2.9, Python 3.12 (the system Boost.Python's).
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -q --no-install-recommends autoconf automake libtool pkg-config intltool \
  bwidget libboost-python-dev libepoxy-dev libgl-dev libglu1-mesa-dev libgtk-3-dev libcap-dev \
  libmodbus-dev libgpiod-dev libreadline-dev libtirpc-dev libusb-1.0-0-dev libxmu-dev procps \
  psmisc python3-dev python3-pybind11 python3-tk python3-xlib tcl tcl8.6-dev tclx tk8.6-dev \
  libudev-dev yapps2 python3-yapps asciidoctor groff libfmt-dev libedit-dev libxinerama-dev \
  librsvg2-bin libxaw7-dev gettext
cd /home/user && git clone --depth 1 -b 2.9 https://github.com/LinuxCNC/linuxcnc.git linuxcnc-2.9
# man pages need a2x (asciidoc + docbook); not needed for simulation
mkdir -p /tmp/stub && cat > /tmp/stub/a2x <<'A2X'
#!/bin/sh
dest=.; src=
while [ $# -gt 0 ]; do case "$1" in --destination-dir) dest=$2; shift;; -*) ;; *) src=$1;; esac; shift; done
b=$(basename "$src" .adoc); mkdir -p "$dest"; : > "$dest/$b"
A2X
chmod +x /tmp/stub/a2x
cd linuxcnc-2.9/src
./autogen.sh
PYTHON_BIN=python3.12 ./configure --with-realtime=uspace --disable-check-runtime-deps \
  --enable-build-documentation=no --disable-gtk2
PATH=/tmp/stub:$PATH make -j"$(nproc)"
cd /home/user && tar -cJf linuxcnc-2.9-uspace.tar.xz linuxcnc-2.9/bin linuxcnc-2.9/lib \
  linuxcnc-2.9/rtlib linuxcnc-2.9/scripts linuxcnc-2.9/tcl linuxcnc-2.9/share \
  linuxcnc-2.9/include linuxcnc-2.9/src/Makefile.modinc linuxcnc-2.9/src/config.h \
  linuxcnc-2.9/nc_files linuxcnc-2.9/configs/common linuxcnc-2.9/VERSION
