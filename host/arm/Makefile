# Six simulated drives coupled to a simulated arm (see README.md).
#
#   make            libstmbl_drive.so and arm_sim
#   make check      tests/*.test (with ../tests/run_tests.py)
#   make clean
#
# libstmbl_drive.so is the drive (host/drive.c, shared/, the stand-ins in
# host/hw/) with every symbol but the stmbl_sim_* API made local, as for the
# LinuxCNC module (../lcnc). arm_sim loads it once per drive with dlmopen,
# so each copy has its own globals: the firmware's HAL is one global set.

PYTHON ?= python3
ROOT    = ../..
HOST    = ..
GEN     = gen
OBJ     = obj

include $(HOST)/lcnc/comps.mak

COMP_SRCS = $(addprefix $(ROOT)/shared/comps/,$(addsuffix .c,$(COMPS))) \
            $(addprefix $(HOST)/hw/,$(addsuffix .c,$(HW_COMPS))) $(ROOT)/src/comps/conf.c
COMP_HDRS = $(addprefix $(GEN)/,$(addsuffix _comp.h,$(COMPS) $(HW_COMPS) conf))
TEMPLATES = $(wildcard $(ROOT)/conf/template/*.txt) $(HOST)/sim_board.txt
CORE_SRCS = $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/angle.c \
            $(ROOT)/shared/config.c $(ROOT)/shared/ringbuf.c
DRIVE_SRCS = $(addprefix $(HOST)/,drive.c cdc_host.c pty.c) $(HOST)/lcnc/sim_api.c
SRCS = $(DRIVE_SRCS) $(CORE_SRCS) $(COMP_SRCS) $(GEN)/hal_tbl.c $(GEN)/conf_templates.c
OBJS = $(addprefix $(OBJ)/,$(notdir $(SRCS:.c=.o)))

INCLUDES = -I$(HOST)/shim -I$(HOST) -I$(HOST)/lcnc -I$(GEN) -I$(ROOT)/inc -I$(ROOT)/shared
CORE_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -Wno-format -fno-strict-aliasing -fwrapv \
              -D_GNU_SOURCE -include host_compat.h $(INCLUDES)
API = stmbl_sim_open stmbl_sim_close stmbl_sim_update stmbl_sim_boot stmbl_sim_tick \
      stmbl_sim_pin stmbl_sim_cmd stmbl_sim_time

vpath %.c $(HOST) $(HOST)/hw $(HOST)/lcnc $(ROOT)/shared $(ROOT)/shared/comps $(ROOT)/src/comps $(GEN)

all: libstmbl_drive.so arm_sim

libstmbl_drive.so: $(OBJS)
	ld -r -o drive_core.tmp $(OBJS)
	objcopy $(addprefix --keep-global-symbol=,$(API)) drive_core.tmp drive_core.o
	$(CC) -shared -o $@ drive_core.o -lm -lpthread
	rm -f drive_core.tmp

ARM_SRCS = arm_sim.c armcore.c plant.c $(ROOT)/shared/robot/robot.c $(ROOT)/shared/robot/collide.c $(ROOT)/shared/robot/kin.c $(ROOT)/shared/robot/geom.c $(ROOT)/shared/robot/guard.c
arm_sim: $(ARM_SRCS) armcore.h plant.h $(ROOT)/shared/robot/robot.h $(ROOT)/shared/robot/collide.h $(ROOT)/shared/robot/geom.h $(HOST)/lcnc/stmbl_sim_api.h
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -Wno-format-truncation -D_GNU_SOURCE -I$(ROOT)/shared/robot -I$(HOST)/lcnc \
	      -o $@ $(ARM_SRCS) -ldl -lm

check: all
	@$(PYTHON) $(HOST)/tests/run_tests.py tests/*.test

$(OBJ)/%.o: %.c $(GEN)/commandslist.h $(COMP_HDRS) | $(OBJ)
	$(CC) $(CORE_CFLAGS) -c $< -o $@

$(GEN)/%_comp.h: $(ROOT)/shared/comps/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<
$(GEN)/%_comp.h: $(HOST)/hw/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<
$(GEN)/conf_comp.h: $(ROOT)/src/comps/conf.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<
$(GEN)/conf_templates.c: $(TEMPLATES) $(ROOT)/tools/create_config.py | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_config.py $@ $(TEMPLATES)
$(GEN)/hal_tbl.c: $(COMP_SRCS) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_hal_tbl.py $@ $(COMP_SRCS)
$(GEN)/commandslist.h: $(DRIVE_SRCS) $(CORE_SRCS) $(COMP_SRCS) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_cmd.py $@ $(DRIVE_SRCS) $(CORE_SRCS) $(COMP_SRCS)

$(GEN) $(OBJ):
	@mkdir -p $@

clean:
	rm -rf $(GEN) $(OBJ) *.o *.so *.tmp arm_sim

.PHONY: all check clean
