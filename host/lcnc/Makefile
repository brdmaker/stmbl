# stmbl_sim: the simulated drive as a LinuxCNC (uspace) realtime module.
#
#   make              build stmbl_sim.so
#   sudo make install copy it to LinuxCNC's module directory
#   make clean
#
# The drive (host/drive.c, shared/, the stand-ins in host/hw/) is built into
# stmbl_core.o, and every symbol but the stmbl_sim_* API is made local: the
# drive's HAL (hal_init, hal_start, ...) shares names with LinuxCNC's.

PYTHON ?= python3
ROOT    = ../..
HOST    = ..
GEN     = gen
OBJ     = obj

# Same component set as host/Makefile.
include comps.mak

COMP_SRCS = $(addprefix $(ROOT)/shared/comps/,$(addsuffix .c,$(COMPS))) \
            $(addprefix $(HOST)/hw/,$(addsuffix .c,$(HW_COMPS))) $(ROOT)/src/comps/conf.c
COMP_HDRS = $(addprefix $(GEN)/,$(addsuffix _comp.h,$(COMPS) $(HW_COMPS) conf))
TEMPLATES = $(wildcard $(ROOT)/conf/template/*.txt) $(HOST)/sim_board.txt
CORE_SRCS = $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/angle.c \
            $(ROOT)/shared/config.c $(ROOT)/shared/ringbuf.c $(ROOT)/shared/sscon.c \
            $(ROOT)/shared/fwstage.c $(ROOT)/shared/bip_loop.c \
            $(ROOT)/linuxcnc/stmblcan/mcp2515.c $(ROOT)/linuxcnc/stmblcan/stmblcan_bus.c \
            $(ROOT)/linuxcnc/stmblcan/candesc.c
DRIVE_SRCS = $(addprefix $(HOST)/,drive.c cdc_host.c pty.c fwup_host.c can_host.c mcp2515_sim.c) sim_api.c
SRCS = $(DRIVE_SRCS) $(CORE_SRCS) $(COMP_SRCS) $(GEN)/hal_tbl.c $(GEN)/conf_templates.c
OBJS = $(addprefix $(OBJ)/,$(notdir $(SRCS:.c=.o)))

INCLUDES = -I$(HOST)/shim -I$(HOST) -I. -I$(GEN) -I$(ROOT)/inc -I$(ROOT)/shared -I$(ROOT)/linuxcnc/stmblcan
CORE_CFLAGS = -O2 -g -fPIC -DSSCON_TXSIZE=32768 -std=gnu11 -Wall -Wno-format -fno-strict-aliasing -fwrapv \
              -D_GNU_SOURCE -include host_compat.h $(INCLUDES)

vpath %.c $(HOST) $(HOST)/hw $(ROOT)/shared $(ROOT)/shared/comps $(ROOT)/src/comps $(GEN) .

# --- LinuxCNC module rules (provides %.so, install) ---------------------------
obj-m += stmbl_sim.o
stmbl_sim-objs := stmbl_sim_rt.o stmbl_core.o
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
include $(MODINC)

all: stmbl_sim.so

stmbl_sim_rt.o: stmbl_sim.c stmbl_sim_api.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -c $<

stmbl_core.o: $(OBJS)
	ld -r -o $@.tmp $(OBJS)
	objcopy --keep-global-symbol=stmbl_sim_open --keep-global-symbol=stmbl_sim_close \
	        --keep-global-symbol=stmbl_sim_update $@.tmp $@
	rm -f $@.tmp

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
	rm -rf $(GEN) $(OBJ) *.o *.so *.tmp *.sym *.ver

.PHONY: all clean
