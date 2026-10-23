#!/bin/sh
# Render the board's top and bottom (as seen from below) side by side: render.sh OUT.png
set -e
D=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
kicad-cli pcb export svg --layers F.Cu,F.CrtYd,F.SilkS,Edge.Cuts,F.Fab --page-size-mode 2 --exclude-drawing-sheet -o "$T/top.svg" "$D/brdstepper.kicad_pcb" >/dev/null
kicad-cli pcb export svg --mirror --layers B.Cu,B.CrtYd,B.SilkS,Edge.Cuts,B.Fab --page-size-mode 2 --exclude-drawing-sheet -o "$T/bot.svg" "$D/brdstepper.kicad_pcb" >/dev/null
rsvg-convert -w 700 -b white "$T/top.svg" -o "$T/top.png"
rsvg-convert -w 700 -b white "$T/bot.svg" -o "$T/bot.png"
convert "$T/top.png" "$T/bot.png" +append "$1"
rm -r "$T"
