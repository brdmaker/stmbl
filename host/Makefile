# Native (x86) build of the stmbl control core for simulation and automated
# testing. Compiles the portable HAL engine and control components with the
# host compiler, replacing the STM32 low-level layer with host/main.c.
#
# Usage:
#   make               build ./stmbl_host and ./servoterm
#   make test          build and run the bundled example scripts
#   make clean

PYTHON ?= python3
CC     ?= cc
ROOT    = ..
GEN     = gen

include lcnc/comps.mak

COMP_SRCS = $(addprefix $(ROOT)/shared/comps/,$(addsuffix .c,$(COMPS))) \
            $(addprefix hw/,$(addsuffix .c,$(HW_COMPS))) $(ROOT)/src/comps/conf.c
COMP_HDRS = $(addprefix $(GEN)/,$(addsuffix _comp.h,$(COMPS) $(HW_COMPS) conf))
# The vibration sensor (docs/vibsensor.md): its processing and a simulated
# IIS3DWB on a machine. In stmbl_host only, not in the drive's LinuxCNC module.
COMP_SRCS += $(ROOT)/shared/comps/vib.c hw/vibsim.c
COMP_HDRS += $(GEN)/vib_comp.h $(GEN)/vibsim_comp.h

# Config templates for 'link', as in the firmware, plus the sim-only board.
TEMPLATES = $(wildcard $(ROOT)/conf/template/*.txt) sim_board.txt

CORE_SRCS = $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/angle.c \
            $(ROOT)/shared/config.c $(ROOT)/shared/ringbuf.c $(ROOT)/shared/sscon.c \
            $(ROOT)/shared/fwstage.c $(ROOT)/shared/bip_loop.c \
            $(ROOT)/linuxcnc/stmblcan/mcp2515.c $(ROOT)/linuxcnc/stmblcan/stmblcan_bus.c \
            $(ROOT)/linuxcnc/stmblcan/candesc.c $(ROOT)/shared/vibdsp.c
# Everything but main.c also goes into the stmbl_sim LinuxCNC module (lcnc/).
DRIVE_SRCS = drive.c cdc_host.c pty.c fwup_host.c can_host.c mcp2515_sim.c
HOST_SRCS = main.c $(DRIVE_SRCS)
ALL_SRCS  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS) $(GEN)/hal_tbl.c $(GEN)/conf_templates.c

# Sources scanned by create_cmd.py for COMMAND() registrations.
CMD_SCAN  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS)

# shim/ comes first so host/shim/usbd_cdc_if.h shadows inc/usbd_cdc_if.h.
INCLUDES = -Ishim -I. -I$(GEN) -I$(ROOT)/inc -I$(ROOT)/shared -I$(ROOT)/linuxcnc/stmblcan
CFLAGS  ?= -O1 -g
# -Wno-format: the firmware's printf formats target a 32-bit-long ABI (ARM);
# they are harmless on the host and only affect a few diagnostic prints.
CFLAGS  += -DSSCON_TXSIZE=32768 -std=gnu11 -Wall -Wno-format -fno-strict-aliasing -fwrapv -D_GNU_SOURCE \
           -include host_compat.h $(INCLUDES)
LDLIBS   = -lm -lpthread

all: stmbl_host servoterm

stmbl_host: $(GEN)/commandslist.h $(COMP_HDRS) $(ALL_SRCS) *.h hw/*.c $(ROOT)/shared/*.h
	$(CC) $(CFLAGS) $(ALL_SRCS) -o $@ $(LDLIBS)

# CLI servoterm: standalone, no firmware sources, no special includes.
servoterm: servoterm.c
	$(CC) -O1 -g -std=gnu11 -Wall servoterm.c -o $@

$(GEN)/%_comp.h: $(ROOT)/shared/comps/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<

$(GEN)/%_comp.h: hw/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<

$(GEN)/conf_comp.h: $(ROOT)/src/comps/conf.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<

$(GEN)/conf_templates.c: $(TEMPLATES) $(ROOT)/tools/create_config.py | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_config.py $@ $(TEMPLATES)

$(GEN)/hal_tbl.c: $(COMP_SRCS) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_hal_tbl.py $@ $(COMP_SRCS)

$(GEN)/commandslist.h: $(CMD_SCAN) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_cmd.py $@ $(CMD_SCAN)

$(GEN):
	@mkdir -p $(GEN)

# linuxcnc/powerquota's fuse models and feed scale (unit tests)
QUOTA_DIR = ../linuxcnc/powerquota
test_quota: tests/test_quota.c $(QUOTA_DIR)/quota.c $(QUOTA_DIR)/quota.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I$(QUOTA_DIR) -o $@ tests/test_quota.c $(QUOTA_DIR)/quota.c -lm

# shared/sserial_q.h: noise-shaped process data (unit tests)
test_ssq: tests/test_ssq.c ../shared/sserial_q.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I../shared -o $@ tests/test_ssq.c -lm

# shared/sscon.c: the console over smart-serial (unit tests)
test_cur_offset: tests/test_cur_offset.c ../shared/cur_offset.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I../shared -o $@ tests/test_cur_offset.c -lm

test_sscon: tests/test_sscon.c ../shared/sscon.c ../shared/sscon.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I../shared -o $@ tests/test_sscon.c ../shared/sscon.c

# the smart-serial bootloader: shared/lbp_dev.c, lbp_flash.c, fwstage.c
test_bootloader: tests/test_bootloader.c ../shared/lbp_flash.c ../shared/lbp_flash.h ../shared/fwstage.c ../shared/fwstage.h ../shared/lbp_dev.c ../shared/lbp_dev.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I../shared -o $@ tests/test_bootloader.c ../shared/lbp_flash.c ../shared/fwstage.c ../shared/lbp_dev.c ../shared/crc8.c

# brdstepper's two-coil current loop (shared/bip_loop.c)
test_bip: tests/test_bip.c ../shared/bip_loop.c ../shared/bip_loop.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I../shared -o $@ tests/test_bip.c ../shared/bip_loop.c -lm

# linuxcnc/stmblcan: MCP2515 driver and bus logic against simulated chips
STMBLCAN = ../linuxcnc/stmblcan
test_stmblcan: tests/test_stmblcan.c mcp2515_sim.c mcp2515_sim.h $(STMBLCAN)/mcp2515.c $(STMBLCAN)/mcp2515.h $(STMBLCAN)/stmblcan_bus.c $(STMBLCAN)/stmblcan_bus.h $(STMBLCAN)/candesc.c $(STMBLCAN)/candesc.h ../shared/canproto.h ../shared/sscon.c
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I../shared -I$(STMBLCAN) -I. -o $@ tests/test_stmblcan.c mcp2515_sim.c $(STMBLCAN)/mcp2515.c $(STMBLCAN)/stmblcan_bus.c $(STMBLCAN)/candesc.c ../shared/sscon.c -lm

# shared/vibproto.h: the vibration sensors' measurement bus (docs/vibsensor.md)
test_vibproto: tests/test_vibproto.c ../shared/vibproto.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I../shared -o $@ tests/test_vibproto.c -lm

# Self-checking suite (tests/*.test, see tests/run_tests.py); fails on any
# unmet expectation.
check: stmbl_host test_quota test_ssq test_sscon test_cur_offset test_bootloader test_bip test_stmblcan test_vibproto
	@./test_ssq
	@./test_sscon
	@./test_cur_offset
	@./test_bootloader
	@./test_bip
	@./test_stmblcan
	@./test_vibproto
	@$(PYTHON) ../tools/vibmon/test_vibaddr.py | tail -1
	@$(PYTHON) tests/test_mkssfw.py
	@$(PYTHON) tests/test_scope.py
	@$(PYTHON) tests/check_sserial_table.py
	@$(PYTHON) tests/test_comp_names.py
	@$(PYTHON) tests/test_stmblconf.py
	@./test_quota
	@$(PYTHON) tests/run_tests.py tests/*.test tests/ebrake/*.test tests/bus/*.test tests/vib/*.test

# Soak: every motor in tests/motors.py, tuned with id_pid, through a motion
# profile in closed loop (tests/gen_soak_tests.py). Generated into gen/soak.
# make soak SOAK_CYCLES=20 for a longer run.
SOAK_CYCLES ?= 1
soak: stmbl_host
	@$(PYTHON) tests/gen_soak_tests.py --cycles $(SOAK_CYCLES) --out $(GEN)/soak
	@$(PYTHON) tests/run_tests.py $(GEN)/soak/*.test

# Emergency braking (docs/emergency_stop.md): the hand-written tests in
# tests/ebrake (also part of check), then every motor in tests/motors.py
# through the stops (tests/gen_ebrake_tests.py), generated into gen/ebrake.
ebrake: stmbl_host
	@$(PYTHON) tests/gen_ebrake_tests.py --out $(GEN)/ebrake
	@$(PYTHON) tests/run_tests.py tests/ebrake/*.test $(GEN)/ebrake/*.test

test: all check
	@for f in examples/*.hal; do echo "=== $$f ==="; ./stmbl_host $$f; done
	@for f in examples/*.servoterm; do echo "=== $$f ==="; ./servoterm $$f; done
	@for f in examples/drive_*.txt; do echo "=== $$f ==="; \
	  ./stmbl_host --drive $(ROOT)/hwtest/burnin/ECMA-C30604FS/stmbl.conf $$f | grep -v "^OK \|^not found"; done

clean:
	rm -rf $(GEN) stmbl_host servoterm

.PHONY: ebrake all check soak test clean
