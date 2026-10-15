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

# Config templates for 'link', as in the firmware, plus the sim-only board.
TEMPLATES = $(wildcard $(ROOT)/conf/template/*.txt) sim_board.txt

CORE_SRCS = $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/angle.c \
            $(ROOT)/shared/config.c $(ROOT)/shared/ringbuf.c
# Everything but main.c also goes into the stmbl_sim LinuxCNC module (lcnc/).
DRIVE_SRCS = drive.c cdc_host.c pty.c
HOST_SRCS = main.c $(DRIVE_SRCS)
ALL_SRCS  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS) $(GEN)/hal_tbl.c $(GEN)/conf_templates.c

# Sources scanned by create_cmd.py for COMMAND() registrations.
CMD_SCAN  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS)

# shim/ comes first so host/shim/usbd_cdc_if.h shadows inc/usbd_cdc_if.h.
INCLUDES = -Ishim -I. -I$(GEN) -I$(ROOT)/inc -I$(ROOT)/shared
CFLAGS  ?= -O1 -g
# -Wno-format: the firmware's printf formats target a 32-bit-long ABI (ARM);
# they are harmless on the host and only affect a few diagnostic prints.
CFLAGS  += -std=gnu11 -Wall -Wno-format -fno-strict-aliasing -fwrapv -D_GNU_SOURCE \
           -include host_compat.h $(INCLUDES)
LDLIBS   = -lm -lpthread

all: stmbl_host servoterm test_robot

# shared/robot (arm dynamics) and its tests (tests/robot/test_robot.c)
ROBOT_SRCS = $(ROOT)/shared/robot/robot.c $(ROOT)/shared/robot/kin.c $(ROOT)/shared/robot/geom.c
test_robot: tests/robot/test_robot.c $(ROBOT_SRCS) $(ROOT)/shared/robot/robot.h $(ROOT)/shared/robot/kin.h $(ROOT)/shared/robot/geom.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -I$(ROOT)/shared/robot -o $@ tests/robot/test_robot.c $(ROBOT_SRCS) -lm

stmbl_host: $(GEN)/commandslist.h $(COMP_HDRS) $(ALL_SRCS) *.h hw/*.c
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

# Self-checking suite (tests/*.test, see tests/run_tests.py); fails on any
# unmet expectation.
check: stmbl_host test_robot
	@./test_robot tests/robot/rtb_ref.txt tests/robot/genserkins_ref.txt arm/arcmate/arcmate.arm
	@$(PYTHON) tests/robot/test_step2cell.py
	@$(MAKE) -s -C arm all
	@$(PYTHON) tests/run_tests.py tests/*.test arm/tests/*.test

# Soak: every motor in tests/motors.py, tuned with id_pid, through a motion
# profile in closed loop (tests/gen_soak_tests.py), and the simulated arm:
# movement patterns and error scenarios (arm/tests/gen_arm_soak.py).
# Generated into gen/soak and gen/arm_soak.
# make soak SOAK_CYCLES=20 for a longer run.
SOAK_CYCLES ?= 1
soak: stmbl_host
	@$(PYTHON) tests/gen_soak_tests.py --cycles $(SOAK_CYCLES) --out $(GEN)/soak
	@$(PYTHON) tests/run_tests.py $(GEN)/soak/*.test
	@$(MAKE) -s -C arm all
	@$(PYTHON) arm/tests/gen_arm_soak.py --cycles $(SOAK_CYCLES) --out $(GEN)/arm_soak
	@$(PYTHON) tests/run_tests.py $(GEN)/arm_soak/*.test

test: all check
	@for f in examples/*.hal; do echo "=== $$f ==="; ./stmbl_host $$f; done
	@for f in examples/*.servoterm; do echo "=== $$f ==="; ./servoterm $$f; done
	@for f in examples/drive_*.txt; do echo "=== $$f ==="; \
	  ./stmbl_host --drive $(ROOT)/hwtest/burnin/ECMA-C30604FS/stmbl.conf $$f | grep -v "^OK \|^not found"; done

clean:
	rm -rf $(GEN) stmbl_host servoterm test_robot
	$(MAKE) -C arm clean

.PHONY: all check soak test clean
