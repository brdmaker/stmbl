# powerquota: power quotas (fuses) of a machine's stmbl drives, as a
# LinuxCNC realtime module (uspace). The model (quota.c) is tested on the
# host: make -C ../../host check (test_quota).
#
#   make [MODINC=...]    powerquota.so
#   make install         into LinuxCNC's module directory
#
# MODINC: LinuxCNC's Makefile.modinc: ~/linuxcnc-2.9 (run in place) if
# there, else the installed one (linuxcnc-uspace-dev).

MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
QCFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE

obj-m += powerquota.o
powerquota-objs := powerquota_rt.o quota.o
include $(MODINC)

all: powerquota.so

powerquota_rt.o: powerquota.c quota.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -c $<
quota.o: quota.c quota.h
	$(CC) $(QCFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
