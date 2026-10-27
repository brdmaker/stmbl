# armguard: collision reaction (stop, back off, estop) and, with the cell
# geometry, collision prevention, as a LinuxCNC realtime module (uspace).
# Built from shared/robot (robot.c, kin.c, geom.c, guard.c).
#
#   make [MODINC=...]    armguard.so
#   make install         into LinuxCNC's module directory

ROOT = ../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
ROBOT_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I$(ROOT)/shared/robot

obj-m += armguard.o
armguard-objs := armguard_rt.o robot.o kin.o geom.o guard.o
include $(MODINC)

all: armguard.so

armguard_rt.o: armguard.c $(ROOT)/shared/robot/guard.h $(ROOT)/shared/robot/geom.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I$(ROOT)/shared/robot -c $<
%.o: $(ROOT)/shared/robot/%.c $(ROOT)/shared/robot/%.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
