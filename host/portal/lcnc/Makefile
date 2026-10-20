# stmbl_portal: the simulated portal and its drives as a LinuxCNC (uspace)
# module.
#
#   make [MODINC=...]   build stmbl_portal.so (and ../../arm/libstmbl_drive.so)
#   make install        copy it to LinuxCNC's module directory
#
# MODINC: LinuxCNC's Makefile.modinc; the run-in-place build in
# ~/linuxcnc-2.9 if there, else the installed one.

MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)

PT_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -Wno-format-truncation -D_GNU_SOURCE -I.. -I../../lcnc

obj-m += stmbl_portal.o
stmbl_portal-objs := stmbl_portal_rt.o portalcore.o pplant.o
EXTRA_LDFLAGS += -ldl
include $(MODINC)

all: stmbl_portal.so ../../arm/libstmbl_drive.so

../../arm/libstmbl_drive.so:
	$(MAKE) -C ../../arm libstmbl_drive.so

stmbl_portal_rt.o: stmbl_portal.c ../portalcore.h ../pplant.h ../../lcnc/stmbl_sim_api.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I.. -I../../lcnc -c $<
portalcore.o: ../portalcore.c ../portalcore.h ../pplant.h ../../lcnc/stmbl_sim_api.h
	$(CC) $(PT_CFLAGS) -c $< -o $@
pplant.o: ../pplant.c ../pplant.h
	$(CC) $(PT_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
