# Native (x86) build of the stmbl control core for simulation and automated
# testing. Compiles the portable HAL engine and control components with the
# host compiler, replacing the STM32 low-level layer with host/main.c.
#
# Usage:
#   make               build ./stmbl_host and ./servoterm
#   make test          run unit tests + all HAL scripts (examples/ and tests/scripts/)
#   make SANITIZE=1 test   same with ASAN + UBSan
#   make clean

PYTHON ?= python3
CC     ?= cc
ROOT    = ..
GEN     = gen

# Portable control components: the V4 SHARED_COMPS set from ../base.mak. 'term'
# (the USB console / scope channel) is included via the host CDC shim in shim/.
COMPS = \
  sim svm vel rev hal_test ypid fault pid spid pe \
  pmsm_limits pmsm_ttc dc_limits dc_ttc acim_ttc uvw fanuc fb_switch reslimit \
  iit vel_int linrev psi stp uf2 ramp scale idx_home move not and or jog \
  velbuf avg mux veltopos debounce pos_filter rl mad sensorless field gain \
  rlpsij veltime mpid fmove home en th motsim term \
  idm idpmsm ids iddc

COMP_SRCS = $(addprefix $(ROOT)/shared/comps/,$(addsuffix .c,$(COMPS)))
COMP_HDRS = $(addprefix $(GEN)/,$(addsuffix _comp.h,$(COMPS)))

# Host-only HAL components (live in host/, not shared/comps/).
HOST_COMP_SRCS = sserial_host.c
HOST_COMP_HDRS = $(GEN)/sserial_host_comp.h

CORE_SRCS = $(ROOT)/shared/hal.c $(ROOT)/shared/commands.c $(ROOT)/shared/angle.c \
            $(ROOT)/shared/crc8.c
HOST_SRCS = main.c cdc_host.c
ALL_SRCS  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS) $(HOST_COMP_SRCS) $(GEN)/hal_tbl.c

# Sources scanned by create_cmd.py for COMMAND() registrations.
CMD_SCAN  = $(HOST_SRCS) $(CORE_SRCS) $(COMP_SRCS) $(HOST_COMP_SRCS)

# Sources shared by all unit-test binaries: HAL + comps without main.c.
# tests/stubs.c supplies the host symbols that main.c would otherwise provide.
TEST_SRCS = $(CORE_SRCS) $(COMP_SRCS) $(HOST_COMP_SRCS) $(GEN)/hal_tbl.c cdc_host.c
TEST_BINS = tests/test_hal tests/test_pins tests/test_comps tests/test_sserial tests/test_sserial_motor

# shim/ comes first so host/shim/usbd_cdc_if.h shadows inc/usbd_cdc_if.h.
INCLUDES = -Ishim -I$(GEN) -I$(ROOT)/inc -I$(ROOT)/shared
CFLAGS  ?= -O1 -g
# -Wno-format: the firmware's printf formats target a 32-bit-long ABI (ARM);
# they are harmless on the host and only affect a few diagnostic prints.
CFLAGS  += -std=gnu11 -Wall -Wno-format -fno-strict-aliasing -fwrapv \
           -include host_compat.h $(INCLUDES)
LDLIBS   = -lm

# SANITIZE=1: AddressSanitizer + UndefinedBehaviorSanitizer.
# Drops to -O0 for accurate stack frames and line attribution.
# Usage: make SANITIZE=1
ifeq ($(SANITIZE),1)
  CFLAGS  += -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address,undefined
endif

all: stmbl_host servoterm

stmbl_host: $(GEN)/commandslist.h $(COMP_HDRS) $(HOST_COMP_HDRS) $(GEN)/hal_tbl.c $(ALL_SRCS)
	$(CC) $(CFLAGS) $(ALL_SRCS) -o $@ $(LDFLAGS) $(LDLIBS)

# CLI servoterm: standalone, no firmware sources, no special includes.
servoterm: servoterm.c
	$(CC) -O1 -g -std=gnu11 -Wall servoterm.c -o $@

$(GEN)/%_comp.h: $(ROOT)/shared/comps/%.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ $<

$(GEN)/hal_tbl.c: $(COMP_SRCS) $(HOST_COMP_SRCS) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_hal_tbl.py $@ $(COMP_SRCS) $(HOST_COMP_SRCS)

$(GEN)/sserial_host_comp.h: sserial_host.c | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_comp_h.py $@ sserial_host.c

$(GEN)/commandslist.h: $(CMD_SCAN) | $(GEN)
	@$(PYTHON) $(ROOT)/tools/create_cmd.py $@ $(CMD_SCAN)

$(GEN):
	@mkdir -p $(GEN)

tests/test_hal: $(GEN)/commandslist.h $(COMP_HDRS) $(GEN)/hal_tbl.c \
    tests/stubs.c tests/test_hal.c tests/runner.h
	@mkdir -p tests
	$(CC) $(CFLAGS) -Itests $(TEST_SRCS) tests/stubs.c tests/test_hal.c \
	    -o $@ $(LDFLAGS) $(LDLIBS)

tests/test_pins: $(GEN)/commandslist.h $(COMP_HDRS) $(GEN)/hal_tbl.c \
    tests/stubs.c tests/test_pins.c tests/runner.h
	@mkdir -p tests
	$(CC) $(CFLAGS) -Itests $(TEST_SRCS) tests/stubs.c tests/test_pins.c \
	    -o $@ $(LDFLAGS) $(LDLIBS)

tests/test_comps: $(GEN)/commandslist.h $(COMP_HDRS) $(GEN)/hal_tbl.c \
    tests/stubs.c tests/test_comps.c tests/runner.h
	@mkdir -p tests
	$(CC) $(CFLAGS) -Itests $(TEST_SRCS) tests/stubs.c tests/test_comps.c \
	    -o $@ $(LDFLAGS) $(LDLIBS)

tests/test_sserial: stmbl_host lbp_client.c lbp_client.h \
    tests/test_sserial.c tests/runner.h
	@mkdir -p tests
	$(CC) $(CFLAGS) -I. -Itests lbp_client.c $(ROOT)/shared/crc8.c \
	    tests/test_sserial.c -o $@ $(LDFLAGS) $(LDLIBS)

tests/test_sserial_motor: stmbl_host lbp_client.c lbp_client.h \
    tests/test_sserial_motor.c tests/runner.h
	@mkdir -p tests
	$(CC) $(CFLAGS) -I. -Itests lbp_client.c $(ROOT)/shared/crc8.c \
	    tests/test_sserial_motor.c -o $@ $(LDFLAGS) $(LDLIBS)

test: all $(TEST_BINS)
	@echo "=== unit: test_hal ===" && ./tests/test_hal
	@echo "=== unit: test_pins ===" && ./tests/test_pins
	@echo "=== unit: test_comps ===" && ./tests/test_comps
	@echo "=== unit: test_sserial ===" && ./tests/test_sserial
	@echo "=== unit: test_sserial_motor ===" && ./tests/test_sserial_motor
	@for f in examples/*.hal; do echo "=== $$f ==="; ./stmbl_host $$f; done
	@for f in examples/*.servoterm; do echo "=== $$f ==="; ./servoterm $$f; done
	@for f in tests/scripts/*.hal; do echo "=== $$f ==="; ./stmbl_host $$f; done

clean:
	rm -rf $(GEN) stmbl_host servoterm $(TEST_BINS)

.PHONY: all test clean
