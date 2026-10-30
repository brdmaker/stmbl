# hwtest/arcmate-sim: LinuxCNC + the simulated ARC Mate.
#
#   make test      run lcnc_test.py as LinuxCNC's display (headless), in a
#                  copy of this config under /tmp; prints TAP
#   make test-grip lcnc_grip_test.py with the electric gripper
#                  (arcmate_gripper.arm, cell_grip.txt, grip.hal)
#   make test-tool lcnc_tool_test.py with the pneumatic tool changer
#                  (arcmate_changer.arm, cell_changer.txt, grip.hal +
#                  changer.hal, armkins, M6 = toolchange.ngc)
#   make test-quota lcnc_quota_test.py with powerquota (powerquota.hal,
#                  quota_test.txt: three phases, two drives each)
#   make soak      soak.ngc SOAK_CYCLES times and error scenarios (lcnc_soak.py)
#   make run       LinuxCNC with AXIS
#
# LINUXCNC: a run-in-place LinuxCNC 2.9 (sourced rip-environment), or
# empty for the installed one. Needs host/arm built and stmbl_arm.so
# installed (make -C ../../host/arm/lcnc all install), and armcollide.so,
# armguard.so, armgrip.so, armkins.so, armtool.so, powerquota.so
# (make -C ../../linuxcnc/<name> all install). Not as root
# (rtapi_app refuses).

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
HERE := $(abspath .)
ARM  := $(abspath ../../host/arm/arcmate/arcmate.arm)
LIB  := $(abspath ../../host/arm/libstmbl_drive.so)
MODEL := $(abspath ../../host/arm/arcmate/model_perturbed.arm)
CELL := $(abspath cell.txt)
ENV  = $(if $(LINUXCNC),. $(LINUXCNC)/scripts/rip-environment >/dev/null &&,)

test:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_test.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|; s|^CELL = .*|CELL = $(CELL)|' arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_test.tap 2>/dev/null; \
	test -f lcnc_test.result && test "$$(cat lcnc_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

GRIP_ARM := $(abspath ../../host/arm/arcmate/arcmate_gripper.arm)
GRIP_CELL := $(abspath cell_grip.txt)
test-grip:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_grip_test.py|; s|^ARM = .*|ARM = $(GRIP_ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|; s|^CELL = .*|CELL = $(GRIP_CELL)|; s|^HALFILE = arcmate.hal|HALFILE = arcmate.hal\nHALFILE = grip.hal|' arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_grip_test.tap 2>/dev/null; \
	test -f lcnc_grip_test.result && test "$$(cat lcnc_grip_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

TOOL_ARM := $(abspath ../../host/arm/arcmate/arcmate_changer.arm)
TOOL_CELL := $(abspath cell_changer.txt)
TOOLS := $(abspath ../../host/arm/arcmate/tools.txt)
test-tool:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_tool_test.py|; s|^ARM = .*|ARM = $(TOOL_ARM)|; s|^LIB = .*|LIB = $(LIB)\nTOOLS = $(TOOLS)|; s|^MODEL = .*|MODEL = $(MODEL)|; s|^CELL = .*|CELL = $(TOOL_CELL)|; s|^HALFILE = genserkins.hal|HALFILE = grip.hal\nHALFILE = changer.hal|; s|^KINEMATICS = genserkins|KINEMATICS = armkins model=$(MODEL) tools=$(TOOLS)|; s|^PARAMETER_FILE = arcmate.var|PARAMETER_FILE = arcmate.var\nREMAP=M6 modalgroup=6 ngc=toolchange\nSUBROUTINE_PATH = .|' arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_tool_test.tap 2>/dev/null; \
	test -f lcnc_tool_test.result && test "$$(cat lcnc_tool_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

test-quota:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_quota_test.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|; s|^CELL = .*|CELL = $(CELL)|; s|^HALFILE = arcmate.hal|HALFILE = arcmate.hal\nHALFILE = powerquota.hal|' arcmate.ini && \
	printf '\n[POWERQUOTA]\nCONFIG = quota_test.txt\n' >> arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_quota_test.tap 2>/dev/null; \
	test -f lcnc_quota_test.result && test "$$(cat lcnc_quota_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

# soak: soak.ngc SOAK_CYCLES times, then error scenarios (lcnc_soak.py)
SOAK_CYCLES ?= 1
soak:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./lcnc_soak.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|; s|^CELL = .*|CELL = $(CELL)|' arcmate.ini && \
	(export SOAK_CYCLES=$(SOAK_CYCLES); $(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cat lcnc_soak.tap 2>/dev/null; \
	test -f lcnc_soak.result && test "$$(cat lcnc_soak.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

# reference poses of genserkins for host/tests/robot (test_robot)
genserkins-ref:
	@d=$$(mktemp -d /tmp/arcmate-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = axis|DISPLAY = ./gen_genserkins_ref.py|; s|^ARM = .*|ARM = $(ARM)|; s|^LIB = .*|LIB = $(LIB)|; s|^MODEL = .*|MODEL = $(MODEL)|; s|^CELL = .*|CELL = $(CELL)|' arcmate.ini && \
	($(ENV) linuxcnc arcmate.ini > linuxcnc.log 2>&1; true) && \
	cp genserkins_ref.txt $(abspath ../../host/tests/robot)/genserkins_ref.txt && \
	echo "wrote host/tests/robot/genserkins_ref.txt"

run:
	@$(ENV) cd $(HERE) && linuxcnc arcmate.ini

# The ARM-6 operator screen (QtVCP, linuxcnc/arm6hmi) with the 3D view
# (linuxcnc/vis3d) on the arm with the tool changer (as test-tool: armkins,
# M6 = toolchange.ngc, the torch and the gripper as tools 1 and 2) and the
# power quota:
#   make hmi        run it (a display needed)
#   make test-hmi   its self-test on a virtual X server (Xvfb): the screen's
#                   own buttons drive LinuxCNC; TAP + a screenshot
VIS3D := $(abspath ../../linuxcnc/vis3d)
HMI   := $(abspath ../../linuxcnc/arm6hmi)
HMI_SETUP = cp $(HERE)/* $$d/ && cd $$d && mkdir -p qtvcp/screens && cp -r $(HMI) qtvcp/screens/arm6 && \
	cp $(HMI)/arm6_postgui.hal $(HMI)/arm6_changer_postgui.hal . && \
	sed -i 's|^DISPLAY = axis|DISPLAY = qtvcp arm6|; s|^ARM = .*|ARM = $(TOOL_ARM)|; s|^LIB = .*|LIB = $(LIB)\nTOOLS = $(TOOLS)|; s|^MODEL = .*|MODEL = $(MODEL)|; s|^CELL = .*|CELL = $(TOOL_CELL)|; s|^HALFILE = genserkins.hal|HALFILE = grip.hal\nHALFILE = changer.hal\nHALFILE = powerquota.hal\nPOSTGUI_HALFILE = arm6_postgui.hal\nPOSTGUI_HALFILE = arm6_changer_postgui.hal|; s|^KINEMATICS = genserkins|KINEMATICS = armkins model=$(MODEL) tools=$(TOOLS)|; s|^PARAMETER_FILE = arcmate.var|PARAMETER_FILE = arcmate.var\nREMAP=M6 modalgroup=6 ngc=toolchange\nSUBROUTINE_PATH = .|' arcmate.ini && \
	printf '\n[POWERQUOTA]\nCONFIG = quota.txt\n\n[ARM6HMI]\nVIS3D = $(VIS3D)\nTOOL_READY = G90 G1 X900 Y300 Z1150 A0 B45 C0 F6000\n' >> arcmate.ini

hmi:
	@$(MAKE) -s -C $(VIS3D)
	@d=$$(mktemp -d /tmp/arcmate-hmi.XXXXXX) && $(HMI_SETUP) && $(ENV) linuxcnc arcmate.ini

test-hmi:
	@$(MAKE) -s -C $(VIS3D)
	@d=$$(mktemp -d /tmp/arcmate-hmi.XXXXXX) && $(HMI_SETUP) && \
	(Xvfb :92 -screen 0 1600x1000x24 >/dev/null 2>&1 & echo $$! > xvfb.pid; sleep 1; \
	 export DISPLAY=:92 ARM6HMI_TEST=$$d/hmi_test.tap; $(ENV) timeout 420 linuxcnc arcmate.ini > linuxcnc.log 2>&1; \
	 kill $$(cat xvfb.pid); true) && \
	cat hmi_test.tap 2>/dev/null; echo "# screenshot: $$d/hmi_test.png"; \
	test -f hmi_test.result && test "$$(cat hmi_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

.PHONY: test test-grip test-tool test-quota run genserkins-ref soak hmi test-hmi
