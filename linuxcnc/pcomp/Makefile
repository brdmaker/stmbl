# pcomp: physics-based command conditioning for a portal machine, as a
# LinuxCNC realtime module (uspace). Built from shared/pcomp/pcomp.c.
#
#   make [MODINC=...]    pcomp.so
#   make install         into LinuxCNC's module directory

ROOT = ../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
PC_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I$(ROOT)/shared/pcomp

obj-m += pcomp.o
pcomp-objs := pcomp_rt.o pcomp_core.o
include $(MODINC)

all: pcomp.so

pcomp_rt.o: pcomp_hal.c $(ROOT)/shared/pcomp/pcomp.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I$(ROOT)/shared/pcomp -c $<
pcomp_core.o: $(ROOT)/shared/pcomp/pcomp.c $(ROOT)/shared/pcomp/pcomp.h
	$(CC) $(PC_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
