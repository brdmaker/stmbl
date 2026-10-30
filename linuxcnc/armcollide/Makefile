# armcollide: model-based collision detection as a LinuxCNC realtime
# module (uspace). Built from shared/robot (robot.c, collide.c).
#
#   make [MODINC=...]    armcollide.so
#   make install         into LinuxCNC's module directory
#
# MODINC: LinuxCNC's Makefile.modinc: ~/linuxcnc-2.9 (run in place) if
# there, else the installed one (linuxcnc-uspace-dev).

ROOT = ../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)
ROBOT_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I$(ROOT)/shared/robot

obj-m += armcollide.o
armcollide-objs := armcollide_rt.o robot.o collide.o
include $(MODINC)

all: armcollide.so

armcollide_rt.o: armcollide.c $(ROOT)/shared/robot/collide.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I$(ROOT)/shared/robot -c $<
robot.o: $(ROOT)/shared/robot/robot.c $(ROOT)/shared/robot/robot.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@
collide.o: $(ROOT)/shared/robot/collide.c $(ROOT)/shared/robot/collide.h
	$(CC) $(ROBOT_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
