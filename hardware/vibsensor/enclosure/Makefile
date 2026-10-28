# The vibration sensor's enclosure concept "Shard" (OpenSCAD; xvfb-run for
# the renders on a machine without a display).
#   make          STLs (base, cover) and renders
SCAD = openscad
XVFB = $(shell command -v xvfb-run >/dev/null && echo xvfb-run -a)
VIEW = --imgsize=1400,1000 --colorscheme=Tomorrow

all: stl render

stl: stl/shard-base.stl stl/shard-cover.stl

stl/shard-%.stl: shard.scad
	@mkdir -p stl
	$(SCAD) -D 'part="$*"' -o $@ shard.scad

render: shard.scad
	@mkdir -p render
	$(XVFB) $(SCAD) -D 'part="assembly"' -o render/front.png $(VIEW) --camera=-2,0,6,62,0,38,150 shard.scad
	$(XVFB) $(SCAD) -D 'part="assembly"' -o render/rear.png $(VIEW) --camera=-2,0,6,58,0,212,150 shard.scad
	$(XVFB) $(SCAD) -D 'part="assembly"' -o render/top.png $(VIEW) --camera=-2,0,6,0,0,0,150 shard.scad
	$(XVFB) $(SCAD) -D 'part="assembly"' -o render/bottom.png $(VIEW) --camera=-2,0,6,180,0,0,150 shard.scad
	$(XVFB) $(SCAD) -D 'part="terminated"' -o render/terminated.png $(VIEW) --camera=-6,0,6,55,0,150,150 shard.scad
	$(XVFB) $(SCAD) -D 'part="exploded"' -o render/exploded.png $(VIEW) --camera=-2,0,6,70,0,30,165 shard.scad

clean:
	rm -rf stl render

.PHONY: all stl render clean
