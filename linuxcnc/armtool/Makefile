# armtool: the tool changer monitor as a LinuxCNC
# realtime module (uspace). Built from shared/robot (robot.c, kin.c,
# tool.c).
#
#   make [MODINC=...]    armtool.so
#   make install         into LinuxCNC's module directory

ROOT = ../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
ROBOT_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I$(ROOT)/shared/robot
RB = $(ROOT)/shared/robot

obj-m += armtool.o
armtool-objs := armtool_rt.o robot.o kin.o tool.o
include $(MODINC)

all: armtool.so

armtool_rt.o: armtool.c $(RB)/kin.h $(RB)/tool.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I$(RB) -c $<
robot.o: $(RB)/robot.c $(RB)/robot.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@
kin.o: $(RB)/kin.c $(RB)/kin.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@
tool.o: $(RB)/tool.c $(RB)/tool.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
