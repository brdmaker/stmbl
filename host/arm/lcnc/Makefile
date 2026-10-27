# stmbl_arm: the simulated arm and its drives as a LinuxCNC (uspace) module.
#
#   make [MODINC=...]   build stmbl_arm.so (and ../libstmbl_drive.so)
#   make install        copy it to LinuxCNC's module directory
#
# MODINC: LinuxCNC's Makefile.modinc; the run-in-place build in
# ~/linuxcnc-2.9 (LinuxCNC 2.9, as on the bench) if there, else the
# installed one.

ROOT = ../../..
MODINC ?= $(firstword $(wildcard $(HOME)/linuxcnc-2.9/src/Makefile.modinc /home/user/linuxcnc-2.9/src/Makefile.modinc) /usr/share/linuxcnc/Makefile.modinc)

ARM_CFLAGS = -O2 -g -fPIC -std=gnu11 -Wall -D_GNU_SOURCE -I.. -I../../lcnc -I$(ROOT)/shared/robot

obj-m += stmbl_arm.o
stmbl_arm-objs := stmbl_arm_rt.o armcore.o plant.o robot.o kin.o geom.o gripsim.o changersim.o tool.o
EXTRA_LDFLAGS += -ldl
include $(MODINC)

all: stmbl_arm.so ../libstmbl_drive.so

../libstmbl_drive.so:
	$(MAKE) -C .. libstmbl_drive.so

stmbl_arm_rt.o: stmbl_arm.c ../armcore.h ../plant.h ../changersim.h ../../lcnc/stmbl_sim_api.h
	$(CC) -o $@ $(EXTRA_CFLAGS) -I.. -I../../lcnc -I$(ROOT)/shared/robot -c $<
armcore.o: ../armcore.c ../armcore.h ../plant.h ../gripsim.h ../changersim.h ../../lcnc/stmbl_sim_api.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@
plant.o: ../plant.c ../plant.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@
robot.o: $(ROOT)/shared/robot/robot.c $(ROOT)/shared/robot/robot.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@
kin.o: $(ROOT)/shared/robot/kin.c $(ROOT)/shared/robot/kin.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@
geom.o: $(ROOT)/shared/robot/geom.c $(ROOT)/shared/robot/geom.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@
gripsim.o: ../gripsim.c ../gripsim.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@
changersim.o: ../changersim.c ../changersim.h $(ROOT)/shared/robot/tool.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@
tool.o: $(ROOT)/shared/robot/tool.c $(ROOT)/shared/robot/tool.h
	$(CC) $(ARM_CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.tmp *.ver

.PHONY: all clean
