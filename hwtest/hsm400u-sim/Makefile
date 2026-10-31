# hwtest/hsm400u-sim: LinuxCNC drives the simulated Mikron HSM 400U retrofit
# (host/mill, machine/hsm400u*.mill): five simulated stmbl drives, the
# machine's Siemens servos on stmbl's bus, the A/C trunnion through
# xyzac-trt-kins (world mode = the tool centre point).
#
#   make test       three five-axis programs at 1 and 2 kHz on the motors'
#                   EnDat (~2 min), checked against bounds (hsm_test.py)
#   make test-all   the same with the glass scales blended in too: results.md
#   make run        LinuxCNC with AXIS
#
# Needs host/arm/libstmbl_drive.so and stmbl_mill.so installed:
#   make -C ../../host/mill/lcnc all install
# Not as root (rtapi_app refuses); hsm_test.py needs numpy.

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
PY ?= $(firstword $(wildcard /usr/local/bin/python3 /usr/bin/python3))

test:
	@LINUXCNC=$(LINUXCNC) $(PY) hsm_test.py --servo 1,0.5 --machine hsm400u

test-all:
	@LINUXCNC=$(LINUXCNC) $(PY) hsm_test.py --servo 1,0.5 --machine hsm400u,hsm400u_blend --out .

run:
	@. $(LINUXCNC)/scripts/rip-environment >/dev/null && linuxcnc hsm.ini

.PHONY: test test-all run
