"""server/scene.py: a CAM setup's scene dump (zip) into the 3D setup."""
import io
import json
import os
import struct
import sys
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import scene  # noqa: E402

EXAMPLE = os.path.join(HERE, "..", "..", "test", "scenes", "cnc-scene-example.zip")


def stl(box):
    """a binary STL: the 12 triangles of a box ((x0,y0,z0),(x1,y1,z1))"""
    (a, b, c), (d, e, f) = box
    v = [(a, b, c), (d, b, c), (d, e, c), (a, e, c), (a, b, f), (d, b, f), (d, e, f), (a, e, f)]
    tris = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
    out = b"test box".ljust(80, b" ") + struct.pack("<I", len(tris))
    for t in tris:
        out += struct.pack("<3f", 0, 0, 0) + b"".join(struct.pack("<3f", *v[i]) for i in t) + b"\0\0"
    return out


def make_zip(scene_json, files, prefix=""):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(prefix + "scene.json", scene_json if isinstance(scene_json, str) else json.dumps(scene_json))
        for n, b in files.items():
            z.writestr(prefix + n, b)
    return buf.getvalue()


def small_scene():
    sj = {"version": 1, "units": "mm", "work_zero": "top_center",
          "stock": {"size": [100, 50, 10], "min": [-50, -25, -10], "max": [50, 25, 0], "material": "PE", "model": "stock.stl"},
          "parts": [{"name": "plate", "copies": [{"copy": 1, "model": "parts/1-plate-1.stl", "rotation_deg": 0, "min": [-40, -20, -10], "max": [-10, 20, 0]},
                                                 {"copy": 2, "model": "parts/1-plate-2.stl", "rotation_deg": 90, "min": [10, -20, -10], "max": [40, 20, 0]}]}],
          "placed_model": "placed.stl", "unplaced": []}
    files = {"stock.stl": stl(((-50, -25, -10), (50, 25, 0))), "parts/1-plate-1.stl": stl(((-40, -20, -10), (-10, 20, 0))),
             "parts/1-plate-2.stl": stl(((10, -20, -10), (40, 20, 0))), "placed.stl": stl(((-40, -20, -10), (40, 20, 0)))}
    return sj, files


class ReadScene(unittest.TestCase):
    def test_example(self):
        """the CAM setup's own dump: a 500 x 400 x 10 plate, 4 parts, 5 copies, placed in the work's coordinates"""
        r = scene.read_scene(open(EXAMPLE, "rb").read())
        s = r["scene"]
        self.assertEqual(s["stock"]["size"], [500.0, 400.0, 10.0])
        self.assertEqual(s["stock"]["material"], "aluminium 5083")
        self.assertEqual(s["work_zero"], "top_center")
        self.assertEqual([p["name"] for p in s["parts"]], ["nema23-flange", "slot-plate", "odd-slots", "slot-plate-upside-down"])
        self.assertEqual(sum(len(p["copies"]) for p in s["parts"]), 5)
        self.assertEqual(sorted(r["files"]), ["parts/1-nema23-flange-1.stl", "parts/1-nema23-flange-2.stl", "parts/2-slot-plate-1.stl",
                                              "parts/3-odd-slots-1.stl", "parts/4-slot-plate-upside-down-1.stl", "placed.stl", "stock.stl"])
        self.assertEqual(r["warnings"], [])
        objs = scene.scene_objects(s, "scenes/example")
        self.assertEqual([o["role"] for o in objs], ["workpiece"] + ["part"] * 5)
        self.assertEqual(objs[0]["src"], "model:scenes/example/stock.stl")
        self.assertEqual(objs[0]["name"], "Stock 500 x 400 x 10 aluminium 5083, zero top center")
        self.assertTrue(all(o["pos"] == [0.0, 0.0, 0.0] and o["frame"] == "work" and o["scale"] == 1.0 for o in objs))
        self.assertEqual(objs[1]["name"], "nema23-flange #1")
        self.assertEqual(len({o["id"] for o in objs}), 6)
        self.assertIn("500 x 400 x 10 mm aluminium 5083, 4 parts (5 copies)", scene.summary(s))

    def test_small_and_folder(self):
        sj, files = small_scene()
        for prefix in ("", "my-scene/"):  # zipped with its folder: the folder is dropped
            r = scene.read_scene(make_zip(sj, files, prefix))
            self.assertIn("parts/1-plate-2.stl", r["files"])
            self.assertEqual(r["warnings"], [])

    def test_inches(self):
        sj, files = small_scene()
        sj["units"] = "inch"
        objs = scene.scene_objects(scene.read_scene(make_zip(sj, files))["scene"], "scenes/x")
        self.assertTrue(all(o["scale"] == 25.4 for o in objs))
        self.assertTrue(objs[0]["name"].startswith("Stock 2540 x 1270 x 254"))

    def test_warnings(self):
        sj, files = small_scene()
        sj["parts"][0]["copies"][1]["min"] = [-20, -20, -10]  # into copy 1
        sj["parts"][0]["copies"][1]["max"] = [60, 20, 0]      # and past the stock
        sj["unplaced"] = [{"name": "bracket"}]
        files["notes.txt"] = b"hello"
        files["extra.stl"] = stl(((0, 0, 0), (1, 1, 1)))
        w = scene.read_scene(make_zip(sj, files))["warnings"]
        self.assertTrue(any("outside the stock" in x for x in w), w)
        self.assertTrue(any("overlap" in x for x in w), w)
        self.assertTrue(any("not placed" in x and "bracket" in x for x in w), w)
        self.assertTrue(any("notes.txt" in x for x in w), w)
        self.assertTrue(any("extra.stl" in x and "not in scene.json" in x for x in w), w)

    def test_refused(self):
        sj, files = small_scene()
        cases = [
            (b"not a zip", "not a zip"),
            (make_zip("{", files), "not JSON"),
            (make_zip({**sj, "units": "furlong"}, files), "units"),
            (make_zip({**sj, "stock": {**sj["stock"], "model": "nope.stl"}}, files), "not in the zip"),
            (make_zip({**sj, "stock": {**sj["stock"], "max": [50, 25, -20]}}, files), "not below"),
            (make_zip(sj, {**files, "stock.stl": b"rubbish"}), "not an STL"),
            (make_zip(sj, {**files, "../evil.stl": stl(((0, 0, 0), (1, 1, 1)))}), "unusable name"),
            (make_zip({**sj, "stock": {**sj["stock"], "model": "../stock.stl"}}, files), "unusable name"),
            (make_zip({k: v for k, v in sj.items() if k != "stock"}, files), "no stock"),
        ]
        for data, why in cases:
            with self.assertRaises(ValueError, msg=why) as e:
                scene.read_scene(data)
            self.assertIn(why, str(e.exception))
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:  # a zip bomb: tiny packed, huge unpacked
            for i in range(14):
                z.writestr("b%d.stl" % i, b"\0" * (5 * 1024 * 1024))
        with self.assertRaises(ValueError) as e:
            scene.read_scene(buf.getvalue())
        self.assertIn("unpacks to more than", str(e.exception))
        with self.assertRaises(ValueError) as e:
            scene.read_scene(b"\0" * (scene.MAX_ZIP + 1))
        self.assertIn("up to 6 MB", str(e.exception))


if __name__ == "__main__":
    unittest.main()
