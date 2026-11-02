# stman schematic: stmbl v4.0's LV sheets (hw/kicad/v4.0) converted to KiCad 7 with their drawing kept,
# edited for stman, plus the new sheets (docs/stman.md). Needs KiCad 7 (kicad-cli).
#
#   make            build, check, PDF
#   make sch        build the sheets (gen/build.py)
#   make check      the netlist against stmbl's and stman's design (gen/check.py)
#   make doc        doc/stman-schematic.pdf

all: sch check doc

sch:
	python3 gen/build.py

check:
	python3 gen/check.py

doc:
	mkdir -p doc
	kicad-cli sch export pdf stman.kicad_sch -o doc/stman-schematic.pdf

.PHONY: all sch check doc
