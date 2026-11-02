"""A CAM setup's scene dump (a zip: scene.json, the stock's and the placed
parts' STL files) into the console's 3D setup.

scene.json (version 1):
    {"version": 1, "units": "mm" | "inch", "work_zero": "top_center" ...,
     "stock": {"size", "min", "max", "material", "model": "stock.stl"},
     "parts": [{"name", "design", "body", "up", "copies": [{"copy", "model", "rotation_deg", "min", "max"}]}],
     "placed_model": "placed.stl", "unplaced": [...]}
Every model is already placed in the work's coordinates (the scene's work
zero at the origin), so each goes into the setup at position 0 in the work
frame: the stock as the workpiece, each part copy as a finished part (seen
through, the material removal compares the cut stock with them).

read_scene(data) checks the zip (names, sizes, a zip bomb, each model a real
STL/OBJ, every model scene.json names present) and returns
{"scene", "files": {name: bytes}, "warnings"}; scene_objects(scene, folder)
gives the setup's objects for the files stored under folder/.
"""
import io
import json
import math
import re
import zipfile

MAX_ZIP = 6 * 1024 * 1024        # the upload (base64 in one websocket message of 8 MB)
MAX_TOTAL = 64 * 1024 * 1024     # all of it unpacked
MAX_MODEL = 5 * 1024 * 1024      # one model, as the console's own uploads
MAX_FILES = 200
MAX_COPIES = 60                  # part copies (the setup holds 64 objects)
NAME = re.compile(r"^[\w\- ][\w.\- ]*$")
MODEL = re.compile(r"\.(stl|obj)$", re.I)


def model_check(name, data):
    """None when data looks like the STL or OBJ its name says, else why not (as machine.model_check)."""
    if name.lower().endswith(".stl"):
        if len(data) >= 84 and len(data) == 84 + 50 * int.from_bytes(data[80:84], "little"):
            return None
        head = data[:2048].decode("latin-1").lower()
        if head.lstrip().startswith("solid") and "facet" in data[:1 << 20].decode("latin-1").lower():
            return None
        return "not an STL file (neither binary nor ASCII)"
    text = data[:1 << 20].decode("latin-1")
    if re.search(r"(?m)^v\s+[-+\d.eE]+\s+[-+\d.eE]+\s+[-+\d.eE]+", text) and re.search(r"(?m)^f\s", data.decode("latin-1")):
        return None
    return "not an OBJ file (no vertices or faces)"


def _safe_name(n):
    """a member's path inside the zip, checked: parts/x.stl, no '..', nothing absolute or hidden"""
    n = n.replace("\\", "/")
    parts = [p for p in n.split("/") if p]
    if n.startswith("/") or not parts or len(parts) > 4 or any(not NAME.match(p) or len(p) > 120 for p in parts):
        raise ValueError("the zip holds an unusable name: %r" % n[:80])
    return "/".join(parts)


def _vec(v, what):
    if not isinstance(v, (list, tuple)) or len(v) != 3:
        raise ValueError("scene.json: %s is three numbers" % what)
    try:
        out = [float(x) for x in v]
    except (TypeError, ValueError):
        raise ValueError("scene.json: %s is three numbers" % what)
    if not all(math.isfinite(x) and abs(x) < 1e5 for x in out):
        raise ValueError("scene.json: %s out of range" % what)
    return out


def read_scene(data):
    """The zip's scene: {"scene" (checked scene.json), "files" {member: bytes}, "warnings" [text]}; ValueError else."""
    if len(data) > MAX_ZIP:
        raise ValueError("the scene is %.1f MB; up to %d MB" % (len(data) / 1e6, MAX_ZIP >> 20))
    try:
        z = zipfile.ZipFile(io.BytesIO(data))
    except zipfile.BadZipFile:
        raise ValueError("not a zip file")
    infos = [i for i in z.infolist() if not i.is_dir()]
    if len(infos) > MAX_FILES:
        raise ValueError("the zip holds %d files; up to %d" % (len(infos), MAX_FILES))
    if sum(i.file_size for i in infos) > MAX_TOTAL:
        raise ValueError("the zip unpacks to more than %d MB" % (MAX_TOTAL >> 20))
    # a single top folder (zipped "cnc-scene/...") is dropped
    names = [i.filename.replace("\\", "/") for i in infos]
    tops = {n.split("/")[0] for n in names}
    strip = (tops.pop() + "/") if len(tops) == 1 and all("/" in n for n in names) else ""
    files, warnings = {}, []
    for i, n in zip(infos, names):
        rel = _safe_name(n[len(strip):])
        if rel != "scene.json" and not MODEL.search(rel):
            warnings.append("left out %s (not scene.json or a 3D model)" % rel)
            continue
        if i.file_size > (1 << 20 if rel == "scene.json" else MAX_MODEL):
            raise ValueError("%s is too large (%.1f MB)" % (rel, i.file_size / 1e6))
        with z.open(i) as f:
            b = f.read(MAX_MODEL + 1)  # never more than the header says it holds, whatever it claims
        if len(b) != i.file_size:
            raise ValueError("%s: its size doesn't match the zip's directory" % rel)
        files[rel] = b
    if "scene.json" not in files:
        raise ValueError("no scene.json in the zip")
    try:
        s = json.loads(files.pop("scene.json").decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as e:
        raise ValueError("scene.json: not JSON (%s)" % e)
    scene = check_scene(s, files, warnings)
    used = {scene["stock"]["model"]} | {c["model"] for p in scene["parts"] for c in p["copies"]} | ({scene["placed_model"]} if scene["placed_model"] else set())
    for n in sorted(set(files) - used):
        warnings.append("%s is in the zip but not in scene.json: left out" % n)
        del files[n]
    for n, b in files.items():
        err = model_check(n, b)
        if err:
            raise ValueError("%s: %s" % (n, err))
    return {"scene": scene, "files": files, "warnings": warnings}


def check_scene(s, files, warnings):
    """scene.json checked and normalised (raises ValueError with the reason)."""
    if not isinstance(s, dict):
        raise ValueError("scene.json: not an object")
    if s.get("version", 1) != 1:
        warnings.append("scene.json version %r: read as version 1" % s.get("version"))
    units = str(s.get("units", "mm")).lower()
    if units not in ("mm", "inch", "in"):
        raise ValueError("scene.json: units %r (mm or inch)" % units)
    st = s.get("stock")
    if not isinstance(st, dict):
        raise ValueError("scene.json: no stock")

    def model(name, what):
        n = _safe_name(str(name or ""))
        if not MODEL.search(n):
            raise ValueError("scene.json: %s's model %s is not an STL or OBJ" % (what, n))
        if n not in files:
            raise ValueError("scene.json: %s's model %s is not in the zip" % (what, n))
        return n

    mn, mx = _vec(st.get("min"), "stock.min"), _vec(st.get("max"), "stock.max")
    if any(b <= a for a, b in zip(mn, mx)):
        raise ValueError("scene.json: stock.min is not below stock.max")
    stock = {"min": mn, "max": mx, "size": [b - a for a, b in zip(mn, mx)],
             "material": str(st.get("material", ""))[:60], "model": model(st.get("model"), "the stock")}
    parts, ncopies = [], 0
    for k, p in enumerate(s.get("parts") or []):
        if not isinstance(p, dict):
            raise ValueError("scene.json: parts[%d] is not an object" % k)
        name = str(p.get("name") or "part %d" % (k + 1))[:60]
        copies = []
        for c in p.get("copies") or []:
            if not isinstance(c, dict):
                raise ValueError("scene.json: %s: a copy is not an object" % name)
            cm, cx = _vec(c.get("min"), name + " min"), _vec(c.get("max"), name + " max")
            copies.append({"copy": int(c.get("copy", len(copies) + 1)), "model": model(c.get("model"), name),
                           "rotation_deg": float(c.get("rotation_deg", 0) or 0), "min": cm, "max": cx})
            if any(a < b0 - 0.01 or b > b1 + 0.01 for a, b, b0, b1 in zip(cm, cx, mn, mx)):
                warnings.append("%s #%d reaches outside the stock" % (name, copies[-1]["copy"]))
        ncopies += len(copies)
        parts.append({"name": name, "up": str(p.get("up", "+z"))[:4], "body": str(p.get("body", ""))[:40], "copies": copies})
    if ncopies > MAX_COPIES:
        raise ValueError("scene.json: %d part copies; the 3D setup holds up to %d" % (ncopies, MAX_COPIES))
    flat = [(p["name"], c) for p in parts for c in p["copies"]]
    for i, (na, a) in enumerate(flat):  # boxes overlapping in XY: parts placed into each other
        for nb, b in flat[i + 1:]:
            if all(a["min"][k] < b["max"][k] - 0.01 and b["min"][k] < a["max"][k] - 0.01 for k in (0, 1)):
                warnings.append("%s #%d and %s #%d overlap (their boxes)" % (na, a["copy"], nb, b["copy"]))
    unplaced = s.get("unplaced") or []
    if unplaced:
        warnings.append("%d part(s) not placed by the CAM setup: %s" % (len(unplaced), ", ".join(str(u.get("name", u) if isinstance(u, dict) else u)[:40] for u in unplaced[:8])))
    placed = s.get("placed_model")
    return {"units": "inch" if units in ("inch", "in") else "mm", "work_zero": str(s.get("work_zero", ""))[:40],
            "stock": stock, "parts": parts, "placed_model": model(placed, "placed_model") if placed else None,
            "unplaced": len(unplaced)}


def scene_objects(scene, folder):
    """The 3D setup's objects for a scene stored under folder/ (program-folder paths)."""
    scale = 25.4 if scene["units"] == "inch" else 1.0
    st = scene["stock"]
    dims = " x ".join("%g" % round(v * scale, 3) for v in st["size"])
    zero = (", zero %s" % scene["work_zero"].replace("_", " ")) if scene["work_zero"] else ""
    objs = [{"id": "scene-stock", "name": ("Stock %s %s" % (dims, st["material"])).strip()[:66] + zero, "role": "workpiece",
             "src": "model:%s/%s" % (folder, st["model"]), "frame": "work", "pos": [0.0, 0.0, 0.0],
             "rot": [0.0, 0.0, 0.0], "scale": scale, "look": "solid"}]
    for i, p in enumerate(scene["parts"]):
        for c in p["copies"]:
            objs.append({"id": "scene-p%d-%d" % (i + 1, c["copy"]), "name": "%s #%d" % (p["name"], c["copy"]), "role": "part",
                         "src": "model:%s/%s" % (folder, c["model"]), "frame": "work", "pos": [0.0, 0.0, 0.0],
                         "rot": [0.0, 0.0, 0.0], "scale": scale, "look": "glass"})
    return objs


def summary(scene):
    """One line for messages: the stock, its material, the parts."""
    st = scene["stock"]
    n = sum(len(p["copies"]) for p in scene["parts"])
    return "stock %s mm%s, %d part%s (%d cop%s)" % (
        " x ".join("%g" % v for v in st["size"]), (" " + st["material"]) if st["material"] else "", len(scene["parts"]),
        "" if len(scene["parts"]) == 1 else "s", n, "y" if n == 1 else "ies")
