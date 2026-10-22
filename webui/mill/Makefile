# webui/mill: the browser console and its backend, millweb (README.md)
#
#   make test           millweb against a stand-in LinuxCNC (~5 s)
#   make test-gui-sim   the console's every page and control, simulated, in headless Chromium (~4 min)
#   make test-gui-tablet every page at iPad and Android tablet sizes, by touch (~2 min)
#   make test-backend   the protocol case by case against real LinuxCNC (test/sim; ~7 min)
#   make test-gui-live  the console on real LinuxCNC through its own controls (~3 min)
#   make test-live      the end-to-end scenario: protocol, then the page (~1 min)
#   make test-drive     the Drives tab: backend and page with a simulated stmbl drive on X (~1 min)
#   make test-stress    large programs (a million lines ...) through every layer (~10 min)
#   make check-all      all of them
#
# The heavy suites from another computer (a PC with Chromium against a small board):
#   on the board:  make serve-test [PORT=8080] [STRESS=1]   the test machine on the network
#   on the PC:     make test-gui-live REMOTE=board:8080     make test-stress REMOTE=board:8080
# (the board's own suites stay light there: make test test-backend)
#   make sim            LinuxCNC on test/sim with millweb: http://localhost:8080/
#   make demo           the console alone (simulated): http://localhost:8000/
#
# Against real LinuxCNC: a non-root user (rtapi_app refuses root). A run-in-place
# tree: LINUXCNC=/path/to/linuxcnc (its scripts/rip-environment); an installed
# LinuxCNC: LINUXCNC= (empty). The GUI suites need node with playwright.
# K=word runs only the cases whose name has it; SHOTS=dir keeps screenshots of failures.

LINUXCNC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9 /home/user/linuxcnc-2.9))
RIP = $(if $(LINUXCNC),export PATH=$(LINUXCNC)/pybin:$$PATH; . $(LINUXCNC)/scripts/rip-environment >/dev/null;,)
NODE_PATH ?= $(firstword $(wildcard /opt/node22/lib/node_modules /usr/lib/node_modules /usr/local/lib/node_modules))
export LINUXCNC NODE_PATH
SEL = $(if $(K),-k "$(K)",) $(if $(SHOTS),--shots $(SHOTS),)

test:
	cd server && python3 -m unittest discover -s tests

test-gui-sim:
	node test/gui/sim_suite.js $(SEL)

test-gui-tablet:
	node test/gui/tablet_suite.js $(SEL)

test-backend:
	python3 test/backend_suite.py $(if $(K),-k "$(K)",)

test-gui-live:
ifdef REMOTE
	MILLWEB_REMOTE=$(REMOTE) node test/gui/live_suite.js $(SEL)
else
	python3 test/harness.py -- node test/gui/live_suite.js $(SEL)
endif

# the Debug page's drives against LinuxCNC with a simulated stmbl drive on X (harness.py --drive)
test-drive:
	python3 test/backend_suite.py -k drive
	python3 test/harness.py --drive -- node test/gui/live_suite.js -k drive -k connects

test-live:
	python3 test/run_live.py --browser

test-stress:
	python3 test/stress_suite.py $(if $(K),-k "$(K)",) $(if $(REMOTE),--remote $(REMOTE),)

PORT ?= 8080
serve-test:
	python3 test/harness.py --serve --port $(PORT) $(if $(STRESS),--stress-files,)

check-all: test test-gui-sim test-gui-tablet test-backend test-gui-live

sim:
	@bash -c '$(RIP) cd test/sim && linuxcnc mill.ini'

demo:
	python3 -m http.server -d . 8000

.PHONY: test test-gui-sim test-backend test-gui-live test-live test-stress serve-test check-all sim demo
