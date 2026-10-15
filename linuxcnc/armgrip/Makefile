# armgrip: the electric gripper and payload check as a LinuxCNC realtime
# module (uspace). Built from shared/robot (robot.c, kin.c, collide.c,
# gripper.c).
#
#   make [MODINC=...]    armgrip.so
#   make install         into LinuxCNC's module directory

ROOT = ../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
ROBOT_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I$(ROOT)/shared/robot
RB = $(ROOT)/shared/robot

obj-m += armgrip.o
armgrip-objs := armgrip_rt.o robot.o kin.o collide.o gripper.o
include $(MODINC)

all: armgrip.so

armgrip_rt.o: armgrip.c $(RB)/gripper.h $(RB)/collide.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I$(RB) -c $<
robot.o: $(RB)/robot.c $(RB)/robot.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@
kin.o: $(RB)/kin.c $(RB)/kin.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@
collide.o: $(RB)/collide.c $(RB)/collide.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@
gripper.o: $(RB)/gripper.c $(RB)/gripper.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
