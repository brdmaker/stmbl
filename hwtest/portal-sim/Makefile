# hwtest/portal-sim: LinuxCNC drives the simulated portal (host/portal):
# a gantry (trivkins XYY) on three simulated stmbl drives with linear
# motors, the machine's mechanics (frame on its feet, racking bridge,
# flexible head) and a shared DC bus in the servo thread.
#
#   make test     the benchmark path at 12 and 36 m/min, 0.5 g (~3 min)
#   make bench    the speed sweep: accelerations x feeds (bench.py, ~30 min);
#                 results/results.md
#   make test-pcomp / bench-pcomp   the same with linuxcnc/pcomp's
#                 conditioning between motion and the drives
#                 (portal_pcomp.hal, the model in [PCOMP])
#   make dynwiz   linuxcnc/dynwiz measures the machine through LinuxCNC
#                 (dynwiz_display.py, with pcomp: portal_pcomp.hal;
#                 DYNWIZ_STEPS, default run; ~4 min):
#                 the log and the session in a /tmp directory
#   make run      LinuxCNC with AXIS
#
# Needs host/arm/libstmbl_drive.so and stmbl_portal.so installed:
#   make -C ../../host/portal/lcnc all install
# (and for pcomp: make -C ../../linuxcnc/pcomp all install)
# Not as root (rtapi_app refuses). bench.py needs numpy (system python3);
# LinuxCNC's python is put on PATH for the display (portal_run.py).

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
PY ?= $(firstword $(wildcard /usr/local/bin/python3 /usr/bin/python3))

test:
	@LINUXCNC=$(LINUXCNC) $(PY) bench.py --quick --out $$(mktemp -d /tmp/portal-test.XXXXXX)

bench:
	@LINUXCNC=$(LINUXCNC) $(PY) bench.py --out results

test-pcomp:
	@LINUXCNC=$(LINUXCNC) $(PY) bench.py --pcomp --quick --out $$(mktemp -d /tmp/portal-test.XXXXXX)

bench-pcomp:
	@LINUXCNC=$(LINUXCNC) $(PY) bench.py --pcomp --accel 2500,5000,10000,20000 --feeds 24000,72000 --out results-pcomp

DYNWIZ_STEPS ?= run
dynwiz:
	@d=$$(mktemp -d /tmp/portal-dynwiz.XXXXXX); cp portal.ini portal.hal portal_pcomp.hal tool.tbl portal.var $$d/; \
	sed -i "s#^DISPLAY = axis#DISPLAY = $(CURDIR)/dynwiz_display.py#; s#^HALFILE = portal.hal#HALFILE = portal_pcomp.hal#; s#= \.\./\.\./#= $(abspath ../..)/#" $$d/portal.ini; \
	export PATH=$(LINUXCNC)/pybin:$$PATH; . $(LINUXCNC)/scripts/rip-environment >/dev/null; \
	cd $$d && DYNWIZ_OUT=$$d DYNWIZ_STEPS="$(DYNWIZ_STEPS)" timeout 900 linuxcnc portal.ini > linuxcnc.log 2>&1; \
	cat $$d/dynwiz.log; echo "(in $$d)"

run:
	@. $(LINUXCNC)/scripts/rip-environment >/dev/null && linuxcnc portal.ini

.PHONY: test bench test-pcomp bench-pcomp dynwiz run
