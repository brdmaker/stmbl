# stman schematic: stmbl v4.0's LV sheets (hw/kicad/v4.0) converted to KiCad 7 with their drawing kept,
# edited for stman, plus the new sheets (docs/stman.md). Needs KiCad 7 (kicad-cli).
#
#   make            build, check, PDF
#   make sch        build the sheets (gen/build.py)
#   make check      the netlist against stmbl's and stman's design (gen/check.py)
#   make doc        doc/stman-schematic.pdf

all: sch check pcb doc

sch:
	python3 gen/build.py

check:
	python3 gen/check.py

pcb:
	python3 gen/pcb.py

doc:
	mkdir -p doc
	kicad-cli sch export pdf stman.kicad_sch -o doc/stman-schematic.pdf
	kicad-cli pcb export svg stman.kicad_pcb -l F.Fab,F.SilkS,Edge.Cuts,F.CrtYd --page-size-mode 2 --exclude-drawing-sheet -o doc/top.svg
	kicad-cli pcb export svg stman.kicad_pcb -l B.Fab,B.SilkS,Edge.Cuts,B.CrtYd --page-size-mode 2 --exclude-drawing-sheet --mirror -o doc/bottom.svg
	rsvg-convert -w 1100 -b white doc/top.svg -o doc/stman-placement-top.png
	rsvg-convert -w 1100 -b white doc/bottom.svg -o doc/stman-placement-bottom.png
	rm -f doc/top.svg doc/bottom.svg

.PHONY: all sch check pcb doc
