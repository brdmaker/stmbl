# vis3d: a native 3D view of the arm for LinuxCNC screens (README.md).
#
#   make            libvis3d.so, vis3d_render
#   make check      tests: the model loader, its kinematics against
#                   shared/robot, and drawing in GLES 3, GLES 2, GL 3.3 and
#                   GL 2.1 contexts (offscreen, EGL; Mesa's llvmpipe will do)
#   make shots      screenshots of models/arm6.vis into shots/
#   make qt-check   the QtVCP widget in PyQt5 on a virtual X server
#                   (PYTHON=python3.x with PyQt5; Xvfb)
#
# libvis3d.so links no GL library: the host's context provides the entry
# points. vis3d_render needs EGL and zlib.

CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -fPIC
ROBOT    = ../../shared/robot

LIB_SRCS = model.c render.c
LIB_HDRS = vis3d.h vis3d_int.h vmath.h shaders.h

all: libvis3d.so vis3d_render

libvis3d.so: $(LIB_SRCS) $(LIB_HDRS)
	$(CC) $(CFLAGS) -shared -o $@ $(LIB_SRCS) -ldl -lm

vis3d_render: vis3d_render.c libvis3d.so
	$(CC) $(CFLAGS) -o $@ vis3d_render.c -L. -lvis3d -Wl,-rpath,'$$ORIGIN' -lEGL -lz -lm

tests/test_vis3d: tests/test_vis3d.c $(LIB_SRCS) $(LIB_HDRS) $(ROBOT)/robot.c $(ROBOT)/robot.h
	$(CC) $(CFLAGS) -I. -I$(ROBOT) -o $@ tests/test_vis3d.c $(LIB_SRCS) $(ROBOT)/robot.c -ldl -lm

check: all tests/test_vis3d
	./tests/test_vis3d
	python3 tests/render_tests.py

shots: vis3d_render
	@mkdir -p shots
	./vis3d_render models/arm6.vis -o shots/arm6_home.png -q 0,0,0,0,-90,0
	./vis3d_render models/arm6.vis -o shots/arm6_pick.png -q -25,30,-10,0,-60,20 --path 55,20,15,30,-75,-40 --guard 1,0.18,1
	./vis3d_render models/arm6.vis -o shots/arm6_stop.png -q -10,45,10,0,-55,0 --guard 3,0.15,0 --cam 20,15,3.0 --tint 3,1,0.36,0.33,0.8

qt-check: libvis3d.so
	./tests/qt_smoke.sh

clean:
	rm -f libvis3d.so vis3d_render tests/test_vis3d

.PHONY: all check shots qt-check clean
