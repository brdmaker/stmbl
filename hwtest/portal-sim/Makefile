# hwtest/portal-sim: LinuxCNC drives the simulated portal (host/portal):
# a gantry (trivkins XYY) on three simulated stmbl drives with linear
# motors, the machine's mechanics (frame on its feet, racking bridge,
# flexible head) and a shared DC bus in the servo thread.
#
#   make test     the benchmark path at 12 and 36 m/min, 0.5 g (~3 min)
#   make bench    the speed sweep: accelerations x feeds (bench.py, ~30 min);
#                 results/results.md
#   make run      LinuxCNC with AXIS
#
# Needs host/arm/libstmbl_drive.so and stmbl_portal.so installed:
#   make -C ../../host/portal/lcnc all install
# Not as root (rtapi_app refuses). bench.py needs numpy (system python3);
# LinuxCNC's python is put on PATH for the display (portal_run.py).

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
PY ?= $(firstword $(wildcard /usr/local/bin/python3 /usr/bin/python3))

test:
	@LINUXCNC=$(LINUXCNC) $(PY) bench.py --quick --out $$(mktemp -d /tmp/portal-test.XXXXXX)

bench:
	@LINUXCNC=$(LINUXCNC) $(PY) bench.py --out results

run:
	@. $(LINUXCNC)/scripts/rip-environment >/dev/null && linuxcnc portal.ini

.PHONY: test bench run
