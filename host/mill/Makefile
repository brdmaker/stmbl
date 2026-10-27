# Three simulated drives (X, Y, Z ball-screw axes) on a simulated
# fixed-bridge mill (see README.md).
#
#   make            mill_sim (and ../arm/libstmbl_drive.so)
#   make check      tests/*.test (with ../tests/run_tests.py)

PYTHON ?= python3
HOST    = ..

all: mill_sim

$(HOST)/arm/libstmbl_drive.so:
	$(MAKE) -C $(HOST)/arm libstmbl_drive.so

mill_sim: mill_sim.c mplant.c mplant.h $(HOST)/lcnc/stmbl_sim_api.h $(HOST)/arm/libstmbl_drive.so
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -Wno-format-truncation -D_GNU_SOURCE -I$(HOST)/lcnc -o $@ mill_sim.c mplant.c -ldl -lm

check: all
	@$(PYTHON) $(HOST)/tests/run_tests.py tests/*.test

clean:
	rm -f mill_sim

.PHONY: all check clean
