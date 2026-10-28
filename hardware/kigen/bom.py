"""Write <name>-bom.csv from parts.py: one line per value and footprint,
with the references, the variant and the notes (✱: check its datasheet).
parts.VARIANTS ({ref: variant}) and parts.NOT_FITTED (refs) are optional.

  kigen.py bom <board dir>
"""
import csv
import os



def refkey(r):
    i = len(r.rstrip("0123456789"))
    return (r[:i], int(r[i:] or 0))


def run(out, parts):
    variants, not_fitted = getattr(parts, "VARIANTS", {}), getattr(parts, "NOT_FITTED", set())
    rows = {}
    for plist in parts.SHEETS.values():
        for ref, lib_id, value, fp, nets, note in plist:
            if ref in not_fitted:
                continue
            var = variants.get(ref, "")
            key = (value, fp, var)
            row = rows.setdefault(key, {"refs": [], "notes": []})
            row["refs"].append(ref)
            if note and note not in row["notes"]:
                row["notes"].append(note)
    path = os.path.join(out, parts.PROJECT + "-bom.csv")
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["References", "Qty", "Value", "Footprint", "Variant", "Notes"])
        for (value, fp, var), row in sorted(rows.items(), key=lambda kv: refkey(sorted(kv[1]["refs"], key=refkey)[0])):
            refs = sorted(row["refs"], key=refkey)
            w.writerow([" ".join(refs), len(refs), value, fp.split(":")[1], var, "; ".join(row["notes"])])
    print("wrote %s (%d lines)" % (path, len(rows)))

