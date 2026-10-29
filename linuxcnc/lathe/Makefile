# lathe: a lathe's turret (index-homed, reverse-locked, on a stepper) and its stmbl spindle, as a LinuxCNC
# realtime module (uspace). The logic (lathe_logic.c) is tested on the host: make -C ../../host check
# (test_lathe).
#
#   make [MODINC=...]    lathe.so
#   make install         into LinuxCNC's module directory
#
# MODINC: LinuxCNC's Makefile.modinc: ~/linuxcnc-2.9 (run in place) if there, else the installed one
# (linuxcnc-uspace-dev).

MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
LCFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE

obj-m += lathe.o
lathe-objs := lathe_rt.o lathe_logic.o
include $(MODINC)

all: lathe.so

lathe_rt.o: lathe.c lathe_logic.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -c $<
lathe_logic.o: lathe_logic.c lathe_logic.h
	$(CC) $(LCFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
