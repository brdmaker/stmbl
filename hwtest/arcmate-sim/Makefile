# hwtest/arcmate-sim: LinuxCNC + the simulated ARC Mate.
#
#   make test      run lcnc_test.py as LinuxCNC's display (headless), in a
#                  copy of this config under /tmp; prints TAP
#   make soak      soak.ngc SOAK_CYCLES times and error scenarios (lcnc_soak.py)
#   make run       LinuxCNC with AXIS
#
# LINUXCNC: a run-in-place LinuxCNC 2.9 (sourced rip-environment), or
# empty for the installed one. Needs host/arm built and stmbl_arm.so
# installed (make -C ../../host/arm/lcnc all install), and armcollide.so
# (make -C ../../linuxcnc/armcollide all install). Not as root
# (rtapi_app refuses).

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
HERE := $(abspath .)
ARM  := $(abspath ../../host/arm/arcmate/arcmate.arm)
LIB  := $(abspath ../../host/arm/libstmbl_drive.so)
MODEL := $(abspath ../../host/arm/arcmate/model_perturbed.arm)
ENV  = $(if $(LINUXCNC),. $(LINUXCNC)/scripts/rip-environment >/dev/null &&,)

test:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_test.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|' arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_test.tap 2>/dev/null; \
	test -f lcnc_test.result && test "$$(cat lcnc_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

# soak: soak.ngc SOAK_CYCLES times, then error scenarios (lcnc_soak.py)
SOAK_CYCLES ?= 1
soak:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_soak.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|' arcmate.ini && \
	(export SOAK_CYCLES=$(SOAK_CYCLES); $(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_soak.tap 2>/dev/null; \
	test -f lcnc_soak.result && test "$$(cat lcnc_soak.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

# reference poses of genserkins for host/tests/robot (test_robot)
genserkins-ref:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./gen_genserkins_ref.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|' arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cp genserkins_ref.txt $(abspath ../../host/tests/robot)/genserkins_ref.txt && \
	echo "wrote host/tests/robot/genserkins_ref.txt"

run:
	@$(ENV) cd $(HERE) && linuxcnc arcmate.ini

.PHONY: test run genserkins-ref soak
