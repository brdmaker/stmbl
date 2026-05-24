# Native (x86) build of the stmbl control core for simulation and automated
# testing. Compiles the portable HAL engine and control components with the
# host compiler, replacing the STM32 low-level layer with host/main.c.
#
# Usage:
#   make            build ./stmbl_host
#   make test       build and run the bundled example scripts
#   make clean

PYTHON ?= python3
CC     ?= cc
ROOT    = ..
GEN     = gen

# Portable control components: the V4 SHARED_COMPS set from ../base.mak, minus
# 'term' (USB console). Everything here compiles unchanged on the host.
COMPS = \
  sim svm vel rev hal_test ypid fault pid spid pe \
  pmsm_limits pmsm_ttc dc_limits dc_ttc acim_ttc uvw fanuc fb_switch reslimit \
  iit vel_int linrev psi stp uf2 ramp scale idx_home move not and or jog \
  velbuf avg mux veltopos debounce pos_filter rl mad sensorless field gain \
  rlpsij veltime mpid fmove home en th id motsim

COMP_SRCS = $(addprefix $(ROOT)/shared/comps/,$(addsuffix .c,$(COMPS)))
COMP_HDRS = $(addprefix $(GEN)/,$(addsuffix _comp.h,$(COMPS)))

CORE_SRCS = $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/angle.c
HOST_SRCS = main.c
ALL_SRCS  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS) $(GEN)/hal_tbl.c

# Sources scanned by create_cmd.py for COMMAND() registrations.
CMD_SCAN  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS)

INCLUDES = -I$(GEN) -I$(ROOT)/inc -I$(ROOT)/shared
CFLAGS  ?= -O1 -g
# -Wno-format: the firmware's printf formats target a 32-bit-long ABI (ARM);
# they are harmless on the host and only affect a few diagnostic prints.
CFLAGS  += -std=gnu11 -Wall -Wno-format -fno-strict-aliasing -fwrapv \
           -include host_compat.h $(INCLUDES)
LDLIBS   = -lm

stmbl_host: $(GEN)/commandslist.h $(COMP_HDRS) $(GEN)/hal_tbl.c $(ALL_SRCS)
	$(CC) $(CFLAGS) $(ALL_SRCS) -o $@ $(LDLIBS)

$(GEN)/%_comp.h: $(ROOT)/shared/comps/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<

$(GEN)/hal_tbl.c: $(COMP_SRCS) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_hal_tbl.py $@ $(COMP_SRCS)

$(GEN)/commandslist.h: $(CMD_SCAN) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_cmd.py $@ $(CMD_SCAN)

$(GEN):
	@mkdir -p $(GEN)

test: stmbl_host
	@for f in examples/*.hal; do echo "=== $$f ==="; ./stmbl_host $$f; done

clean:
	rm -rf $(GEN) stmbl_host

.PHONY: test clean
