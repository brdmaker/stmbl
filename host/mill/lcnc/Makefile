# stmbl_mill: the simulated fixed-bridge mill (the Scripta CN 720) and its
# drives as a LinuxCNC (uspace) module.
#
#   make [MODINC=...]   build stmbl_mill.so (and ../../arm/libstmbl_drive.so)
#   make install        copy it to LinuxCNC's module directory
#
# MODINC: LinuxCNC's Makefile.modinc; the run-in-place build in
# ~/linuxcnc-2.9 if there, else the installed one.

MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)

MC_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -Wno-format-truncation -D_GNU_SOURCE -I.. -I../../lcnc

obj-m += stmbl_mill.o
stmbl_mill-objs := stmbl_mill_rt.o millcore.o mplant.o
EXTRA_LDFLAGS += -ldl
include $(MODINC)

all: stmbl_mill.so ../../arm/libstmbl_drive.so

../../arm/libstmbl_drive.so:
	$(MAKE) -C ../../arm libstmbl_drive.so

stmbl_mill_rt.o: stmbl_mill.c ../millcore.h ../mplant.h ../../lcnc/stmbl_sim_api.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I.. -I../../lcnc -c $<
millcore.o: ../millcore.c ../millcore.h ../mplant.h ../../lcnc/stmbl_sim_api.h
	$(CC) $(MC_CFLAGS) -c $< -o $@
mplant.o: ../mplant.c ../mplant.h
	$(CC) $(MC_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
