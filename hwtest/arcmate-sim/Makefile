# hwtest/arcmate-sim: LinuxCNC + the simulated ARC Mate.
#
#   make test      run lcnc_test.py as LinuxCNC's display (headless), in a
#                  copy of this config under /tmp; prints TAP
#   make run       LinuxCNC with AXIS
#
# LINUXCNC: a run-in-place LinuxCNC 2.9 (sourced rip-environment), or
# empty for the installed one. Needs host/arm built and stmbl_arm.so
# installed (make -C ../../host/arm/lcnc all install). Not as root
# (rtapi_app refuses).

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
HERE := $(abspath .)
ARM  := $(abspath ../../host/arm/arcmate/arcmate.arm)
LIB  := $(abspath ../../host/arm/libstmbl_drive.so)
ENV  = $(if $(LINUXCNC),. $(LINUXCNC)/scripts/rip-environment >/dev/null &&,)

test:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_test.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|' arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_test.tap 2>/dev/null; \
	test -f lcnc_test.result && test "$$(cat lcnc_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

run:
	@$(ENV) cd $(HERE) && linuxcnc arcmate.ini

.PHONY: test run
