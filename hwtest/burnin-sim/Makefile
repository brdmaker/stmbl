# hwtest/burnin-sim: LinuxCNC + the simulated drive (stmbl_sim).
#
#   make test-quota  pq_test.py as LinuxCNC's display (headless) with
#                    powerquota.hal: the drive's power over smart-serial,
#                    a quota below what the moves draw (quota_test.txt), the
#                    load of sim_quota_test.txt; in a copy under /tmp;
#                    prints TAP
#
# LINUXCNC: a run-in-place LinuxCNC 2.9 (sourced rip-environment), or
# empty for the installed one. Needs stmbl_sim.so (make -C ../../host/lcnc
# all install) and powerquota.so (make -C ../../linuxcnc/powerquota all
# install), sserialterm.so (make -C ../../linuxcnc/sserialterm all install)
# for test-console, servoterm-cli (built here). Not as root (rtapi_app
# refuses).

SHELL = /bin/bash
LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
HERE := $(abspath .)
ENV  = $(if $(LINUXCNC),. $(LINUXCNC)/scripts/rip-environment >/dev/null &&,)

test-quota:
	@d=$$(mktemp -d /tmp/burnin-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = .*|DISPLAY = ./pq_test.py|; s|^HALFILE = stmbl_sim.hal|HALFILE = stmbl_sim.hal\nHALFILE = powerquota.hal|; s|^TTY = .*|TTY = '"$$d"'/stmbl-sim|; s|^FERROR = .*|FERROR = 5|; s|^MIN_FERROR = .*|MIN_FERROR = 5|' stmbl.ini && \
	printf '\n[POWERQUOTA]\nCONFIG = quota_test.txt\n' >> stmbl.ini && cat sim_quota_test.txt >> sim.txt && \
	($(ENV) linuxcnc stmbl.ini > linuxcnc.log 2>&1; true) && \
	cat pq_test.tap 2>/dev/null; \
	test -f pq_test.result && test "$$(cat pq_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

CLI := $(abspath ../../servoterm-cli/servoterm-cli)
test-console:
	@$(MAKE) -s -C ../../servoterm-cli
	@d=$$(mktemp -d /tmp/burnin-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = .*|DISPLAY = ./con_test.py|; s|^HALFILE = stmbl_sim.hal|HALFILE = stmbl_sim.hal\nHALFILE = console.hal|; s|^TTY = .*|TTY = '"$$d"'/stmbl-sim|' stmbl.ini && \
	printf '\n[SSERIALTERM]\nTTY = '"$$d"'/drive-con\n' >> stmbl.ini && \
	(export SERVOTERM_CLI=$(CLI); $(ENV) linuxcnc stmbl.ini > linuxcnc.log 2>&1; true) && \
	cat con_test.tap 2>/dev/null; \
	test -f con_test.result && test "$$(cat con_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

# can.hal for stmbl_sim.hal: the drive links can (sim.conf with "link can"),
# LinuxCNC's end is stmblcan.so (spi=sim; make -C ../../linuxcnc/stmblcan
# all install), the console over CAN through sserialterm, joint 0 homed
# on the encoder index (index-only: HOME_SEARCH_VEL 0, latch at 5 mm/s).
test-can:
	@$(MAKE) -s -C ../../servoterm-cli
	@d=$$(mktemp -d /tmp/burnin-sim.XXXXXX) && cp $(HERE)/* $$d/ && cd $$d && \
	sed -i 's|^DISPLAY = .*|DISPLAY = ./can_test.py|; s|^HALFILE = stmbl_sim.hal|HALFILE = can.hal|; s|^TTY = .*|TTY = '"$$d"'/stmbl-sim|' stmbl.ini && \
	sed -i '0,/^#HOME_SEARCH_VEL.*/s//HOME_SEARCH_VEL = 0/; 0,/^#HOME_LATCH_VEL.*/s//HOME_LATCH_VEL = 5/; 0,/^#HOME_USE_INDEX.*/s//HOME_USE_INDEX = YES/' stmbl.ini && \
	sed -i 's|^link sserial|link can|' sim.conf && printf 'can0.vel_scale = 600 # > 500 mm/s rapids; stmblcan reads it (discover)\n' >> sim.conf && \
	printf '\n[SSERIALTERM]\nTTY = '"$$d"'/drive-con\n' >> stmbl.ini && \
	(export SERVOTERM_CLI=$(CLI); $(ENV) linuxcnc stmbl.ini > linuxcnc.log 2>&1; true) && \
	cat can_test.tap 2>/dev/null; \
	test -f can_test.result && test "$$(cat can_test.result)" = 0 || \
	{ echo "# failed: log in $$d/linuxcnc.log"; exit 1; }

.PHONY: test-quota test-console test-can
