# hwtest/mill-sim: LinuxCNC drives the simulated Scripta CN 720 (host/mill):
# a fixed bridge and a moving table (trivkins XYZ, one joint per axis) on
# three simulated stmbl drives with ball screws and SEW CMP40M servos, the
# machine's mechanics (base on its feet, swaying bridge, the spindle tip's
# modes, Z on its brake) in the servo thread.
#
#   make test       circles, a square and a line at 1 and 2 kHz on the
#                   resolvers (~1 min), checked against bounds (mill_test.py)
#   make test-all   the same on the resolvers, blended scales and scales
#                   (cn720, cn720_blend, cn720_scales; ~3 min): results.md
#   make run        LinuxCNC with AXIS
#
# Needs host/arm/libstmbl_drive.so and stmbl_mill.so installed:
#   make -C ../../host/mill/lcnc all install
# Not as root (rtapi_app refuses); mill_test.py needs numpy.

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
PY ?= $(firstword $(wildcard /usr/local/bin/python3 /usr/bin/python3))

test:
	@LINUXCNC=$(LINUXCNC) $(PY) mill_test.py --servo 1,0.5 --machine cn720

test-all:
	@LINUXCNC=$(LINUXCNC) $(PY) mill_test.py --servo 1,0.5 --machine cn720,cn720_blend,cn720_scales --out . --no-check

run:
	@. $(LINUXCNC)/scripts/rip-environment >/dev/null && linuxcnc mill.ini

.PHONY: test test-all run
