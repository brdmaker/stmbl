# brdstepper KiCad project, generated from gen/parts.py (KiCad 7, its Python
# pcbnew module, rsvg-convert and ImageMagick for the renders).
#
#   make            schematic, check, board, DRC, BOM, PDF, renders
#   make sch        the schematic and its netlist check
#   make pcb        the board (placement redone: hand edits to it are lost)


all: sch pcb drc bom doc

sch:
	python3 gen/gen.py
	python3 gen/check.py

pcb: sch
	python3 gen/pcb.py

drc:
	python3 gen/drc.py

bom:
	python3 gen/bom.py

doc:
	mkdir -p doc
	kicad-cli sch export pdf -o doc/brdstepper-schematic.pdf brdstepper.kicad_sch
	gen/render.sh doc/brdstepper-placement.png

.PHONY: all sch pcb drc bom doc
