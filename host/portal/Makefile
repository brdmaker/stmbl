# Three simulated drives (Y1, Y2, X linear motors) on a simulated portal
# machine (see README.md).
#
#   make            portal_sim (and ../arm/libstmbl_drive.so)
#   make check      tests/*.test (with ../tests/run_tests.py)

PYTHON ?= python3
HOST    = ..

all: portal_sim

$(HOST)/arm/libstmbl_drive.so:
	$(MAKE) -C $(HOST)/arm libstmbl_drive.so

portal_sim: portal_sim.c portalcore.c pplant.c ../../shared/pcomp/pcomp.c portalcore.h pplant.h ../../shared/pcomp/pcomp.h $(HOST)/lcnc/stmbl_sim_api.h $(HOST)/arm/libstmbl_drive.so
	$(CC) -O2 -g -std=gnu11 -Wall -Wextra -Wno-format-truncation -D_GNU_SOURCE -I$(HOST)/lcnc -I../../shared/pcomp -o $@ portal_sim.c portalcore.c pplant.c ../../shared/pcomp/pcomp.c -ldl -lm

check: all
	@$(PYTHON) $(HOST)/tests/run_tests.py tests/*.test

clean:
	rm -f portal_sim

.PHONY: all check clean
