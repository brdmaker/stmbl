#!/bin/sh
# Hand the MCP2515s from the kernel's mcp251x driver (SocketCAN: can0..2,
# candump) to spidev for stmblcan, and back. The Pi's device tree overlay
# stays as it is (the MCP2515s on spi0, chip selects 0..2).
#
#   sudo ./spi_takeover.sh take   [spi0.0 spi0.1 spi0.2]   before LinuxCNC
#   sudo ./spi_takeover.sh give   [spi0.0 spi0.1 spi0.2]   after it
#   ./spi_takeover.sh status
#
# take: the can interfaces down, mcp251x unbound, spidev bound through
# driver_override (Documentation/spi/spidev.rst): /dev/spidev0.0 .. 0.2.
set -e
cmd=${1:-status}
shift || true
devs=${*:-spi0.0 spi0.1 spi0.2}
sysdev=/sys/bus/spi/devices

drv() {
  l=$(readlink "$sysdev/$1/driver" 2>/dev/null) || true
  if [ -n "$l" ]; then basename "$l"; else echo none; fi
}

case "$cmd" in
take)
  modprobe spidev 2>/dev/null || true
  for d in $devs; do
    [ -e "$sysdev/$d" ] || { echo "$d: no such SPI device (overlay loaded?)" >&2; exit 1; }
    if [ "$(drv "$d")" = mcp251x ]; then
      for n in "$sysdev/$d"/net/*; do [ -e "$n" ] && ip link set "$(basename "$n")" down || true; done
      echo "$d" > /sys/bus/spi/drivers/mcp251x/unbind
    fi
    echo spidev > "$sysdev/$d/driver_override"
    [ "$(drv "$d")" = spidev ] || echo "$d" > /sys/bus/spi/drivers/spidev/bind
    echo "$d: spidev (/dev/spidev$(echo "$d" | sed 's/spi//'))"
  done
  ;;
give)
  for d in $devs; do
    [ "$(drv "$d")" = spidev ] && echo "$d" > /sys/bus/spi/drivers/spidev/unbind
    echo > "$sysdev/$d/driver_override"
    echo "$d" > /sys/bus/spi/drivers/mcp251x/bind
    echo "$d: mcp251x"
  done
  ;;
status)
  for d in $devs; do echo "$d: $(drv "$d")"; done
  ;;
*)
  echo "usage: $0 take|give|status [spi0.0 ...]" >&2
  exit 2
  ;;
esac
