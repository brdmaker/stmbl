/* Portal Mill Console: the machine in the 3D view from its own 3D model, and the built-in machine of
   machine.json's type (app.js buildBuiltin: a gantry, or a fixed bridge over a table moving in Y).
   machine.json's "model" (the Configure page's 3D model tab): parts, STL or
   OBJ files in the machine's models/ folder, each moving with the axes it
   rides on (fixed, with Y, with X and Y, with X, Y and Z, against X for a
   moving table …), placed as they are with the machine at X0 Y0 Z0 and the
   tool tip at the origin; and the built-in gantry, kept or hidden. Classic
   script after setup3d.js (parseModel). */
"use strict";

const mm3d = { builtin: true, parts: [], geoms: new Map(), pending: new Map(), group: null, nodes: [] };
const MOVES_T = [["", "fixed: the frame, the table"], ["Y", "with Y: a gantry's bridge"], ["XY", "with X and Y: a gantry's X carriage"], ["XYZ", "with X, Y, Z: the Z ram, the spindle"],
  ["X", "with X"], ["Z", "with Z"], ["XZ", "with X and Z"], ["YZ", "with Y and Z"], ["-X", "against X: a table under a fixed spindle"], ["-Y", "against Y: a table"], ["-X-Y", "against X and Y: a cross table"]];
const movesOf = (s) => { const k = { X: 0, Y: 0, Z: 0 }; String(s || "").replace(/(-?)([XYZ])/g, (_, n, a) => { k[a] = n ? -1 : 1; return ""; }); return k; };

// the built-in gantry's parts on or off (the tool and the coolant stay: they are at the tip whatever the model)
function mm3dBuiltin(on) {
  if (!R) return;
  const keep = new Set();
  R.toolG.traverse((o) => keep.add(o)); R.coolant.traverse((o) => keep.add(o));
  R.machine.traverse((o) => { if ((o.isMesh || o.isLine) && !keep.has(o)) o.visible = on; });
}
// a model ({builtin, parts}); getFile(name): its bytes (ArrayBuffer), or null; type: the built-in
// machine, "gantry" or "bridge" (machine.json's type; the travel box from TRAVEL)
function mm3dApply(model, getFile, type) {
  if (!R) return;
  model = model || {};
  if (type && type !== R.type) buildBuiltin(type); else sizeLimits();
  mm3d.builtin = model.builtin !== false;
  mm3d.parts = Array.isArray(model.parts) ? model.parts.map((p) => ({ ...p })) : [];
  mm3dBuiltin(mm3d.builtin);
  if (!mm3d.group) { mm3d.group = new THREE.Group(); R.scene.add(mm3d.group); }
  for (const p of mm3d.parts) {
    if (!p.file || mm3d.geoms.has(p.file) || mm3d.pending.has(p.file)) continue;
    mm3d.pending.set(p.file, getFile(p.file).then((buf) => {
      mm3d.pending.delete(p.file);
      if (!buf) return;
      try { mm3d.geoms.set(p.file, parseModel(p.file, buf)); } catch (e) { msg(`3D model ${p.file}: ${e.message}`, "warn"); return; }
      mm3dBuild();
    }));
  }
  mm3dBuild();
}
function mm3dBuild() {
  for (const n of mm3d.nodes) if (n) n.mat.dispose();
  mm3d.group.clear(); mm3d.nodes = [];
  for (const p of mm3d.parts) {
    const g = mm3d.geoms.get(p.file);
    if (!g) { mm3d.nodes.push(null); continue; }
    const op = p.opacity ?? 1;
    const mat = new THREE.MeshStandardMaterial({ color: new THREE.Color(p.color || "#8a99ab"), metalness: 0.35, roughness: 0.55, transparent: op < 1, opacity: op, depthWrite: op >= 1, side: THREE.DoubleSide });
    const mesh = new THREE.Mesh(g, mat);
    const r = (p.rot || [0, 0, 0]).map((d) => (+d || 0) * Math.PI / 180);
    mesh.rotation.set(r[0], r[1], r[2]);
    mesh.scale.setScalar(+p.scale || 1);
    const holder = new THREE.Group(); holder.add(mesh); mm3d.group.add(holder);
    mm3d.nodes.push({ holder, mat, p, k: movesOf(p.moves) });
  }
  if (typeof applyLooks === "function") applyLooks();
}
// every frame: the tip in machine coordinates (m); a part at its place plus the axes it rides on
function mm3dPlace(m) {
  if (!mm3d.group) return;
  mm3d.group.visible = st.show.machine;
  for (const n of mm3d.nodes) {
    if (!n) continue;
    const [px, py, pz] = (n.p.pos || [0, 0, 0]).map((v) => +v || 0);
    n.holder.position.set(px + n.k.X * m.x, py + n.k.Y * m.y, pz + ZTOP + n.k.Z * m.z);
  }
}
// the saved model: the machine's (millweb-supervisor), or the simulation's
async function mm3dLoad() {
  if (!R) return;
  if (LIVE && !LIVE.supervised()) return mm3dApply({ builtin: true, parts: [] }, async () => null, LIVE.info().type);
  const src = LIVE ? LIVE.conf : cfgSim, r = await src.get();
  if (!r.ok) return;
  let m = {};
  try { m = JSON.parse(r.text); } catch (e) { return; }
  if (!LIVE) mm3dTravel(m.axes);  // the simulation: the travel the example machine has (live: LinuxCNC's limits)
  mm3dApply(m.model, src.modelGet, LIVE ? LIVE.info().type : m.type || "gantry");
}
// machine.json's axes as the travel (the simulation only: live, LinuxCNC's INI says)
function mm3dTravel(axes) {
  for (const a of ["x", "y", "z"]) {
    const ax = (axes || {})[a.toUpperCase()];
    if (ax && Number.isFinite(+ax.min) && Number.isFinite(+ax.max) && +ax.max > +ax.min) TRAVEL[a] = [+ax.min, +ax.max];
  }
}
