"""Write brdstepper-bom.csv from parts.py: one line per value and footprint,
with the references, the variant and the notes (✱: check its datasheet).

  python3 bom.py [OUTDIR]
"""
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import parts  # noqa: E402

OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.dirname(HERE)
VARIANT_A = {"U8", "J4", "R20", "C38"}                      # smart-serial
VARIANT_B = {"U9", "J5", "J6", "JP1", "R21", "C39", "C40"}  # CAN
NOT_FITTED = {"H1", "H2", "H3", "H4", "TP1", "TP2", "TP3", "JP1"}


def refkey(r):
    i = len(r.rstrip("0123456789"))
    return (r[:i], int(r[i:] or 0))


def main():
    rows = {}
    for plist in parts.SHEETS.values():
        for ref, lib_id, value, fp, nets, note in plist:
            if ref in NOT_FITTED:
                continue
            var = "A" if ref in VARIANT_A else "B" if ref in VARIANT_B else ""
            key = (value, fp, var)
            row = rows.setdefault(key, {"refs": [], "notes": []})
            row["refs"].append(ref)
            if note and note not in row["notes"]:
                row["notes"].append(note)
    path = os.path.join(OUT, "brdstepper-bom.csv")
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["References", "Qty", "Value", "Footprint", "Variant", "Notes"])
        for (value, fp, var), row in sorted(rows.items(), key=lambda kv: refkey(sorted(kv[1]["refs"], key=refkey)[0])):
            refs = sorted(row["refs"], key=refkey)
            w.writerow([" ".join(refs), len(refs), value, fp.split(":")[1], var, "; ".join(row["notes"])])
    print("wrote %s (%d lines)" % (path, len(rows)))


if __name__ == "__main__":
    main()
