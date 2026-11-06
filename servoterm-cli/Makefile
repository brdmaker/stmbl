# servoterm-cli: command-line client for a real stmbl drive over USB serial,
# with the setup wizard (setup/, --wizard).
#
# Usage:
#   make            build ./servoterm-cli
#   make check      the setup wizard's unit tests (setup/tests)
#   make clean

CC     ?= cc
CFLAGS ?= -O1 -g
CFLAGS += -std=gnu11 -Wall -I../shared
# the firmware version this wizard was built with ("about" prints the drive's)
SETUP_GIT := $(shell git describe --always --dirty 2>/dev/null)
CFLAGS += -DSETUP_GIT='"$(SETUP_GIT)"'
LDLIBS  = -lm

SETUP_SRCS = setup/su_state.c setup/su_link.c setup/su_conf.c setup/su_version.c setup/su_motor.c \
             setup/su_feedback.c setup/su_wiring.c setup/su_steps.c setup/su_cli_ui.c
SETUP_HDRS = $(wildcard setup/*.h) ../shared/setup_api.h

all: servoterm-cli

servoterm-cli: servoterm-cli.c $(SETUP_SRCS) $(SETUP_HDRS)
	$(CC) $(CFLAGS) servoterm-cli.c $(SETUP_SRCS) -o $@ $(LDLIBS)

setup/tests/test_setup: setup/tests/test_setup.c $(SETUP_SRCS) $(SETUP_HDRS)
	$(CC) $(CFLAGS) -Isetup $< $(SETUP_SRCS) -o $@ $(LDLIBS)

check: setup/tests/test_setup
	./setup/tests/test_setup

clean:
	rm -f servoterm-cli setup/tests/test_setup

.PHONY: all check clean
