// node test/scene_test.js: a CAM setup's scene dump read in the browser (js/scene.js)
"use strict";
const fs = require("fs");
const path = require("path");
const zlib = require("zlib");
const S = require("../js/scene.js");
let fails = 0, n = 0;
const ok = (c, what, got) => { n++; if (!c) { fails++; console.log("FAIL " + what + (got !== undefined ? ": " + JSON.stringify(got) : "")); } else console.log("ok   " + what); };
async function throws(p, why, what) { try { await p; ok(false, what, "no error"); } catch (e) { ok(String(e.message).includes(why), what, e.message); } }

// a small zip writer (stored or deflated members) for the cases below
function zip(files, deflate = true) {
  const crcT = []; for (let i = 0; i < 256; i++) { let c = i; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crcT[i] = c >>> 0; }
  const crc = (b) => { let c = ~0; for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (~c) >>> 0; };
  const loc = [], cen = []; let off = 0;
  for (const [name, d] of Object.entries(files)) {
    const data = Buffer.from(d), comp = deflate ? zlib.deflateRawSync(data) : data, nb = Buffer.from(name);
    const h = Buffer.alloc(30); h.writeUInt32LE(0x04034b50, 0); h.writeUInt16LE(20, 4); h.writeUInt16LE(deflate ? 8 : 0, 8);
    h.writeUInt32LE(crc(data), 14); h.writeUInt32LE(comp.length, 18); h.writeUInt32LE(data.length, 22); h.writeUInt16LE(nb.length, 26);
    const c = Buffer.alloc(46); c.writeUInt32LE(0x02014b50, 0); c.writeUInt16LE(20, 4); c.writeUInt16LE(20, 6); c.writeUInt16LE(deflate ? 8 : 0, 10);
    c.writeUInt32LE(crc(data), 16); c.writeUInt32LE(comp.length, 20); c.writeUInt32LE(data.length, 24); c.writeUInt16LE(nb.length, 28); c.writeUInt32LE(off, 42);
    loc.push(h, nb, comp); cen.push(c, nb); off += 30 + nb.length + comp.length;
  }
  const cd = Buffer.concat(cen), e = Buffer.alloc(22); e.writeUInt32LE(0x06054b50, 0); e.writeUInt16LE(cen.length / 2, 8); e.writeUInt16LE(cen.length / 2, 10);
  e.writeUInt32LE(cd.length, 12); e.writeUInt32LE(off, 16);
  return new Uint8Array(Buffer.concat([...loc, cd, e]));
}
function stl(a, b) {  // a box's 12 triangles, binary
  const v = [[a[0], a[1], a[2]], [b[0], a[1], a[2]], [b[0], b[1], a[2]], [a[0], b[1], a[2]], [a[0], a[1], b[2]], [b[0], a[1], b[2]], [b[0], b[1], b[2]], [a[0], b[1], b[2]]];
  const t = [[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]];
  const buf = Buffer.alloc(84 + 50 * t.length); buf.writeUInt32LE(t.length, 80);
  t.forEach((tri, i) => tri.forEach((k, j) => v[k].forEach((x, m) => buf.writeFloatLE(x, 84 + 50 * i + 12 + 12 * j + 4 * m))));
  return buf;
}
const small = () => ({
  sj: { version: 1, units: "mm", work_zero: "top_center", stock: { min: [-50, -25, -10], max: [50, 25, 0], material: "PE", model: "stock.stl" },
    parts: [{ name: "plate", copies: [{ copy: 1, model: "parts/a.stl", min: [-40, -20, -10], max: [-10, 20, 0] }, { copy: 2, model: "parts/b.stl", min: [10, -20, -10], max: [40, 20, 0] }] }] },
  files: { "stock.stl": stl([-50, -25, -10], [50, 25, 0]), "parts/a.stl": stl([-40, -20, -10], [-10, 20, 0]), "parts/b.stl": stl([10, -20, -10], [40, 20, 0]) },
});
const pack = (sj, files, o = {}) => zip({ [(o.prefix || "") + "scene.json"]: typeof sj === "string" ? sj : JSON.stringify(sj), ...Object.fromEntries(Object.entries(files).map(([k, v]) => [(o.prefix || "") + k, v])) }, o.deflate !== false);

(async () => {
  // the CAM setup's own dump
  const ex = await S.read(new Uint8Array(fs.readFileSync(path.join(__dirname, "scenes", "cnc-scene-example.zip"))));
  ok(ex.scene.stock.size.join(",") === "500,400,10" && ex.scene.stock.material === "aluminium 5083", "example: the stock 500 x 400 x 10 aluminium 5083", ex.scene.stock);
  ok(S.copies(ex.scene) === 5 && ex.scene.parts.length === 4 && ex.warnings.length === 0, "example: 4 parts, 5 copies, no warnings", ex.warnings);
  ok(Object.keys(ex.files).length === 7, "example: 7 models (stock, 5 copies, placed)", Object.keys(ex.files));
  const objs = S.objects(ex.scene, S.folderName("cnc-scene-example.zip"));
  ok(objs.length === 6 && objs[0].role === "workpiece" && objs.slice(1).every((o) => o.role === "part" && o.look === "glass"), "example: the stock a workpiece, the copies see-through parts");
  ok(objs[0].src === "model:scenes/cnc-scene-example/stock.stl" && objs[0].name === "Stock 500 x 400 x 10 aluminium 5083, zero top center", "example: in scenes/<zip name>/, named", objs[0]);
  ok(objs.every((o) => o.pos.every((v) => v === 0) && o.frame === "work" && o.scale === 1), "example: placed as they are, in the work frame");
  // stored or deflated, in a folder or not
  const { sj, files } = small();
  for (const o of [{}, { deflate: false }, { prefix: "my-scene/" }]) {
    const r = await S.read(pack(sj, files, o));
    ok(Object.keys(r.files).sort().join() === "parts/a.stl,parts/b.stl,stock.stl" && r.warnings.length === 0, `small scene ${JSON.stringify(o)}`, Object.keys(r.files));
  }
  ok(S.objects((await S.read(pack({ ...sj, units: "inch" }, files))).scene, "scenes/x").every((o) => o.scale === 25.4), "inches: scaled 25.4");
  ok(S.folderName("../../etc/x y?.zip") === "scenes/_.._etc_x y_", "a folder name from any file name: one safe part, no leading dot", S.folderName("../../etc/x y?.zip"));
  // warnings
  {
    const j = JSON.parse(JSON.stringify(sj)); j.parts[0].copies[1].min = [-20, -20, -10]; j.parts[0].copies[1].max = [60, 20, 0]; j.unplaced = [{ name: "bracket" }];
    const w = (await S.read(pack(j, { ...files, "notes.txt": "hi", "extra.stl": stl([0, 0, 0], [1, 1, 1]) }))).warnings;
    ok(w.some((x) => x.includes("outside the stock")) && w.some((x) => x.includes("overlap")) && w.some((x) => x.includes("bracket")), "warnings: outside, overlap, unplaced", w);
    ok(w.some((x) => x.includes("notes.txt")) && w.some((x) => x.includes("extra.stl") && x.includes("not in scene.json")), "warnings: files left out", w);
  }
  // refused
  await throws(S.read(new Uint8Array([1, 2, 3])), "not a zip", "refused: not a zip");
  await throws(S.read(pack("{", files)), "not JSON", "refused: scene.json not JSON");
  await throws(S.read(pack({ ...sj, units: "furlong" }, files)), "units", "refused: units");
  await throws(S.read(pack({ ...sj, stock: { ...sj.stock, model: "nope.stl" } }, files)), "not in the zip", "refused: a model missing");
  await throws(S.read(pack(sj, { ...files, "stock.stl": "rubbish" })), "not an STL", "refused: a broken STL");
  await throws(S.read(pack(sj, { ...files, "../evil.stl": stl([0, 0, 0], [1, 1, 1]) })), "unusable name", "refused: a name out of the folder");
  await throws(S.read(pack({ ...sj, stock: { ...sj.stock, max: [50, 25, -20] } }, files)), "not below", "refused: an empty stock");
  await throws(S.read(new Uint8Array(S.MAX_ZIP + 1)), "up to 6 MB", "refused: too large");
  console.log(`${n - fails}/${n} passed`);
  process.exit(fails ? 1 : 0);
})();
