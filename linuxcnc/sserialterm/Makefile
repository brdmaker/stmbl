# sserialterm: stmbl drive consoles over smart-serial as ptys, a LinuxCNC
# realtime module (uspace only: a thread and ptys). The link layer is
# shared/sscon.c (tested on the host: make -C ../../host check).
#
#   make [MODINC=...]    sserialterm.so
#   make install         into LinuxCNC's module directory

ROOT = ../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
SCFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I$(ROOT)/shared

obj-m += sserialterm.o
sserialterm-objs := sserialterm_rt.o sscon.o
EXTRA_LDFLAGS += -lpthread
include $(MODINC)

all: sserialterm.so

sserialterm_rt.o: sserialterm.c $(ROOT)/shared/sscon.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -D_GNU_SOURCE -I$(ROOT)/shared -c $<
sscon.o: $(ROOT)/shared/sscon.c $(ROOT)/shared/sscon.h
	$(CC) $(SCFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
