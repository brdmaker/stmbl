/* Portal Mill Console: a CAM setup's scene dump (a zip: scene.json, the stock's and the placed parts'
   STL files) read in the browser, for the import dialog and the simulation. No DOM, no THREE: node's
   tests require it (test/scene_test.js). millweb checks the same again (server/scene.py) when it
   stores the scene on the machine.

   Every model in the scene is already placed in the work's coordinates (the scene's work zero at the
   origin), so each goes into the 3D setup at position 0 in the work frame: the stock as the workpiece,
   each part copy as a finished part (see-through; the material removal compares the cut stock with
   them). */
"use strict";

const SCENE = (() => {
  const MAX_ZIP = 6 * 1024 * 1024, MAX_TOTAL = 64 * 1024 * 1024, MAX_MODEL = 5 * 1024 * 1024, MAX_FILES = 200, MAX_COPIES = 60;
  const NAME = /^[\w\- ][\w.\- ]*$/, MODEL = /\.(stl|obj)$/i;

  // ------------------------------------------------------------ zip
  // {name: Uint8Array}; stored and deflated members (the browser's and node's DecompressionStream)
  async function unzip(buf) {
    const u = buf instanceof Uint8Array ? buf : new Uint8Array(buf), dv = new DataView(u.buffer, u.byteOffset, u.byteLength);
    let e = -1;
    for (let i = u.length - 22; i >= Math.max(0, u.length - 65557); i--) if (dv.getUint32(i, true) === 0x06054b50) { e = i; break; }
    if (e < 0) throw new Error("not a zip file");
    const n = dv.getUint16(e + 10, true), cdOff = dv.getUint32(e + 16, true);
    if (n > MAX_FILES) throw new Error(`the zip holds ${n} files; up to ${MAX_FILES}`);
    const out = {}, dec = new TextDecoder();
    let p = cdOff, total = 0;
    for (let k = 0; k < n; k++) {
      if (p + 46 > u.length || dv.getUint32(p, true) !== 0x02014b50) throw new Error("a broken zip (its directory)");
      const method = dv.getUint16(p + 10, true), csize = dv.getUint32(p + 20, true), size = dv.getUint32(p + 24, true);
      const nl = dv.getUint16(p + 28, true), xl = dv.getUint16(p + 30, true), cl = dv.getUint16(p + 32, true), loc = dv.getUint32(p + 42, true);
      const name = dec.decode(u.subarray(p + 46, p + 46 + nl));
      p += 46 + nl + xl + cl;
      if (name.endsWith("/")) continue;
      total += size;
      if (total > MAX_TOTAL) throw new Error(`the zip unpacks to more than ${MAX_TOTAL >> 20} MB`);
      if (size > MAX_MODEL) throw new Error(`${name} is too large (${(size / 1e6).toFixed(1)} MB)`);
      const ds = loc + 30 + dv.getUint16(loc + 26, true) + dv.getUint16(loc + 28, true), raw = u.subarray(ds, ds + csize);
      let data;
      if (method === 0) data = raw.slice();
      else if (method === 8) data = new Uint8Array(await new Response(new Blob([raw]).stream().pipeThrough(new DecompressionStream("deflate-raw"))).arrayBuffer());
      else throw new Error(`${name}: compression method ${method} isn't supported (zip it with deflate)`);
      if (data.length !== size) throw new Error(`${name}: its size doesn't match the zip's directory`);
      out[name.replace(/\\/g, "/")] = data;
    }
    return out;
  }

  // ------------------------------------------------------------ checks
  function safeName(n) {
    n = String(n).replace(/\\/g, "/");
    const parts = n.split("/").filter(Boolean);
    if (n.startsWith("/") || !parts.length || parts.length > 4 || parts.some((p) => !NAME.test(p) || p.length > 120)) throw new Error(`the zip holds an unusable name: ${JSON.stringify(n.slice(0, 80))}`);
    return parts.join("/");
  }
  function modelCheck(name, b) {
    if (/\.stl$/i.test(name)) {
      if (b.length >= 84 && b.length === 84 + 50 * new DataView(b.buffer, b.byteOffset, b.byteLength).getUint32(80, true)) return null;
      const head = new TextDecoder("latin1").decode(b.subarray(0, 2048)).toLowerCase();
      if (head.trimStart().startsWith("solid") && new TextDecoder("latin1").decode(b.subarray(0, 1 << 20)).toLowerCase().includes("facet")) return null;
      return "not an STL file (neither binary nor ASCII)";
    }
    const text = new TextDecoder("latin1").decode(b);
    return /^v\s+[-+\d.eE]+\s+[-+\d.eE]+\s+[-+\d.eE]+/m.test(text) && /^f\s/m.test(text) ? null : "not an OBJ file (no vertices or faces)";
  }
  const vec = (v, what) => {
    if (!Array.isArray(v) || v.length !== 3 || v.some((x) => !Number.isFinite(+x) || Math.abs(+x) >= 1e5)) throw new Error(`scene.json: ${what} is three numbers`);
    return v.map(Number);
  };

  // the zip's scene: {scene, files: {name: Uint8Array}, warnings}; throws with the reason
  async function read(buf) {
    if (buf.byteLength > MAX_ZIP) throw new Error(`the scene is ${(buf.byteLength / 1e6).toFixed(1)} MB; up to ${MAX_ZIP >> 20} MB`);
    const raw = await unzip(buf), names = Object.keys(raw), tops = new Set(names.map((n) => n.split("/")[0]));
    const strip = tops.size === 1 && names.every((n) => n.includes("/")) ? [...tops][0] + "/" : "";
    const files = {}, warnings = [];
    for (const n of names) {
      const rel = safeName(n.slice(strip.length));
      if (rel !== "scene.json" && !MODEL.test(rel)) { warnings.push(`left out ${rel} (not scene.json or a 3D model)`); continue; }
      files[rel] = raw[n];
    }
    if (!files["scene.json"]) throw new Error("no scene.json in the zip");
    let s;
    try { s = JSON.parse(new TextDecoder().decode(files["scene.json"])); } catch (e) { throw new Error(`scene.json: not JSON (${e.message})`); }
    delete files["scene.json"];
    const scene = check(s, files, warnings);
    const used = new Set([scene.stock.model, ...scene.parts.flatMap((p) => p.copies.map((c) => c.model)), ...(scene.placed_model ? [scene.placed_model] : [])]);
    for (const n of Object.keys(files).sort()) if (!used.has(n)) { warnings.push(`${n} is in the zip but not in scene.json: left out`); delete files[n]; }
    for (const [n, b] of Object.entries(files)) { const err = modelCheck(n, b); if (err) throw new Error(`${n}: ${err}`); }
    return { scene, files, warnings };
  }

  function check(s, files, warnings) {
    if (!s || typeof s !== "object" || Array.isArray(s)) throw new Error("scene.json: not an object");
    if ((s.version ?? 1) !== 1) warnings.push(`scene.json version ${JSON.stringify(s.version)}: read as version 1`);
    const units = String(s.units ?? "mm").toLowerCase();
    if (!["mm", "inch", "in"].includes(units)) throw new Error(`scene.json: units ${JSON.stringify(units)} (mm or inch)`);
    const st = s.stock;
    if (!st || typeof st !== "object") throw new Error("scene.json: no stock");
    const model = (name, what) => {
      const n = safeName(name || "");
      if (!MODEL.test(n)) throw new Error(`scene.json: ${what}'s model ${n} is not an STL or OBJ`);
      if (!files[n]) throw new Error(`scene.json: ${what}'s model ${n} is not in the zip`);
      return n;
    };
    const mn = vec(st.min, "stock.min"), mx = vec(st.max, "stock.max");
    if (mx.some((b, k) => b <= mn[k])) throw new Error("scene.json: stock.min is not below stock.max");
    const stock = { min: mn, max: mx, size: mx.map((b, k) => b - mn[k]), material: String(st.material || "").slice(0, 60), model: model(st.model, "the stock") };
    const parts = [];
    let ncopies = 0;
    (s.parts || []).forEach((p, k) => {
      if (!p || typeof p !== "object") throw new Error(`scene.json: parts[${k}] is not an object`);
      const name = String(p.name || `part ${k + 1}`).slice(0, 60), copies = [];
      for (const c of p.copies || []) {
        const cm = vec(c.min, name + " min"), cx = vec(c.max, name + " max");
        copies.push({ copy: Math.trunc(+c.copy || copies.length + 1), model: model(c.model, name), rotation_deg: +c.rotation_deg || 0, min: cm, max: cx });
        if (cm.some((a, i) => a < mn[i] - 0.01) || cx.some((b, i) => b > mx[i] + 0.01)) warnings.push(`${name} #${copies[copies.length - 1].copy} reaches outside the stock`);
      }
      ncopies += copies.length;
      parts.push({ name, up: String(p.up || "+z").slice(0, 4), body: String(p.body || "").slice(0, 40), copies });
    });
    if (ncopies > MAX_COPIES) throw new Error(`scene.json: ${ncopies} part copies; the 3D setup holds up to ${MAX_COPIES}`);
    const flat = parts.flatMap((p) => p.copies.map((c) => [p.name, c]));
    flat.forEach(([na, a], i) => flat.slice(i + 1).forEach(([nb, b]) => {
      if ([0, 1].every((k) => a.min[k] < b.max[k] - 0.01 && b.min[k] < a.max[k] - 0.01)) warnings.push(`${na} #${a.copy} and ${nb} #${b.copy} overlap (their boxes)`);
    }));
    const unplaced = Array.isArray(s.unplaced) ? s.unplaced : [];
    if (unplaced.length) warnings.push(`${unplaced.length} part(s) not placed by the CAM setup: ${unplaced.slice(0, 8).map((u) => String(u && u.name || u).slice(0, 40)).join(", ")}`);
    return { units: units === "mm" ? "mm" : "inch", work_zero: String(s.work_zero || "").slice(0, 40), stock, parts,
      placed_model: s.placed_model ? model(s.placed_model, "placed_model") : null, unplaced: unplaced.length };
  }

  // the 3D setup's objects for a scene stored under folder/ (as server/scene.py's scene_objects)
  function objects(scene, folder) {
    const scale = scene.units === "inch" ? 25.4 : 1, st = scene.stock;
    const dims = st.size.map((v) => +(v * scale).toFixed(3)).join(" x "), zero = scene.work_zero ? `, zero ${scene.work_zero.replace(/_/g, " ")}` : "";
    const objs = [{ id: "scene-stock", name: `Stock ${dims} ${st.material}`.trim().slice(0, 66) + zero, role: "workpiece", src: `model:${folder}/${st.model}`,
      frame: "work", pos: [0, 0, 0], rot: [0, 0, 0], scale, look: "solid" }];
    scene.parts.forEach((p, i) => p.copies.forEach((c) => objs.push({ id: `scene-p${i + 1}-${c.copy}`, name: `${p.name} #${c.copy}`, role: "part",
      src: `model:${folder}/${c.model}`, frame: "work", pos: [0, 0, 0], rot: [0, 0, 0], scale, look: "glass" })));
    return objs;
  }
  const folderName = (n) => "scenes/" + (String(n || "").replace(/\.zip$/i, "").replace(/[^\w.\- ]/g, "_").replace(/^[ .]+|[ .]+$/g, "").slice(0, 60) || "scene");
  const copies = (scene) => scene.parts.reduce((a, p) => a + p.copies.length, 0);

  return { unzip, read, objects, folderName, copies, MAX_ZIP };
})();
if (typeof module !== "undefined") module.exports = SCENE;
