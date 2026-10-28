# the test stand: every part as STL (stl/, each as printed), the pictures (img/), the collision check
#   make            the STLs and the pictures (OpenSCAD 2021; a minute or two)
#   make check      the moving parts against the rest at the travel's corners
OPENSCAD ?= openscad
PARTS = base beam table x_carriage z_slide x_motor_bracket z_motor_bracket gib_y gib_x gib_z pinion snap_pin \
        switch_holder bolt_25 bolt_45 set_screw gib_screw_14 gib_screw_25 gib_screw_50 thumb_screw paper_clip
VIEWS = iso:-420,-560,430,10,0,110 front:0,-700,130,0,0,120 side:700,0,150,0,0,120 back:420,560,420,10,0,110

all: $(PARTS:%=stl/%.stl) img

stl/%.stl: teststand.scad
	@mkdir -p stl
	$(OPENSCAD) --export-format binstl -o $@ -D 'part="$*"' $< 2>&1 | grep -iE "^error|warning" || true

img: teststand.scad
	@mkdir -p img
	for v in $(VIEWS); do n=$${v%%:*}; c=$${v#*:}; \
	  xvfb-run -a $(OPENSCAD) -o img/$$n.png --imgsize=1200,1000 --camera=$$c --projection=o --viewall --autocenter --colorscheme=Tomorrow $< >/dev/null 2>&1; done
	xvfb-run -a $(OPENSCAD) -o img/corner.png -D X=-35 -D Y=40 -D Z=-40 --imgsize=1200,1000 --camera=-420,-560,430,10,0,110 --projection=o --viewall --autocenter --colorscheme=Tomorrow $< >/dev/null 2>&1

check:
	python3 check.py

.PHONY: all img check
