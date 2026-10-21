# linuxcnc/dynwiz: measure a gantry's dynamics through LinuxCNC (README.md)
#
#   make test       the identification math on synthetic signals (~5 s)
#   make test-sim   the whole wizard on the simulated machines
#                   (host/portal/machine, machine/retrofit) against what
#                   their files say (~1 min; needs host/portal built)
#   make check      both
#
# Through LinuxCNC itself: make -C ../../hwtest/portal-sim dynwiz

PYTHON ?= python3

test:
	$(PYTHON) -m unittest tests/test_dynident.py

test-sim:
	$(MAKE) -s -C ../../host/portal
	$(PYTHON) -m unittest tests/test_machines.py

check: test test-sim

.PHONY: test test-sim check
