# webui/mill: the browser console and its backend, millweb (README.md)
#
#   make test        millweb against a stand-in LinuxCNC (~3 s)
#   make test-live   against real LinuxCNC (test/sim) and a headless browser
#                    (~1 min; a non-root user; LINUXCNC=, node with playwright)
#   make sim         LinuxCNC on test/sim with millweb: http://localhost:8080/
#   make demo        the console alone (simulated): http://localhost:8000/
#
# A LinuxCNC run-in-place tree: LINUXCNC=/path/to/linuxcnc (its scripts/rip-environment);
# an installed LinuxCNC: LINUXCNC= (empty).

LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
RIP = $(if $(LINUXCNC),export PATH=$(LINUXCNC)/pybin:$$PATH; . $(LINUXCNC)/scripts/rip-environment >/dev/null;,)
NODE_PATH ?= $(firstword $(wildcard /opt/node22/lib/node_modules /usr/lib/node_modules /usr/local/lib/node_modules))

test:
	cd server && python3 -m unittest discover -s tests

test-live:
	LINUXCNC=$(LINUXCNC) NODE_PATH=$(NODE_PATH) python3 test/run_live.py --browser

sim:
	@bash -c '$(RIP) cd test/sim && linuxcnc mill.ini'

demo:
	python3 -m http.server -d . 8000

.PHONY: test test-live sim demo
