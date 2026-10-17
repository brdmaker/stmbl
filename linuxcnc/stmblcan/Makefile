# stmblcan: stmbl drives over CAN, MCP2515s on SPI from LinuxCNC's servo
# thread (Raspberry Pi; uspace). The driver and bus logic are tested on the
# host without hardware: make -C ../../host check (test_stmblcan).
#
#   make [MODINC=...]    stmblcan.so
#   make install         into LinuxCNC's module directory

ROOT = ../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
SCFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I$(ROOT)/shared -I.

obj-m += stmblcan.o
stmblcan-objs := stmblcan_rt.o mcp2515.o stmblcan_bus.o sscon.o
include $(MODINC)

all: stmblcan.so

stmblcan_rt.o: stmblcan.c stmblcan_bus.h mcp2515.h $(ROOT)/shared/canproto.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -D_GNU_SOURCE -I$(ROOT)/shared -I. -c $<
mcp2515.o: mcp2515.c mcp2515.h $(ROOT)/shared/canproto.h
	$(CC) $(SCFLAGS) -c $< -o $@
stmblcan_bus.o: stmblcan_bus.c stmblcan_bus.h mcp2515.h $(ROOT)/shared/canproto.h
	$(CC) $(SCFLAGS) -c $< -o $@
sscon.o: $(ROOT)/shared/sscon.c $(ROOT)/shared/sscon.h
	$(CC) $(SCFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
