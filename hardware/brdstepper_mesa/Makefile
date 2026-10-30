# brdstepper KiCad project, generated from gen/parts.py and gen/board.py by
# hardware/kigen (KiCad 7, its Python pcbnew module, rsvg-convert and
# ImageMagick for the renders).
#
#   make            schematic, check, board, DRC, BOM, PDF, renders
#   make sch        the schematic and its netlist check
#   make pcb        the board (placement redone: hand edits to it are lost)

KIGEN = python3 ../kigen/kigen.py

all: sch pcb drc bom doc

sch:
	$(KIGEN) sch .
	$(KIGEN) check .

pcb: sch
	$(KIGEN) pcb .

drc:
	$(KIGEN) drc .

bom:
	$(KIGEN) bom .

doc:
	$(KIGEN) doc .

.PHONY: all sch pcb drc bom doc
