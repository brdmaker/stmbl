/* Portal Mill Console: what stands on the table in the 3D view, the setup.
   Objects are fixtures or workpieces: the built-in vise and stock, or 3D
   models (STL, OBJ) from the program folder. Click one in the view to select
   it; its panel moves, turns, scales and shows it. On a machine the setup is
   millweb's (every browser sees the same); in the simulation it is local.
   Classic script after app.js: shares its top-level names (st, R, off ...). */
"use strict";

const SETUP_DEFAULT = [
  { id: "vise", name: "Vise", role: "fixture", src: "builtin:vise", frame: "work", pos: [0, 0, -STOCK.h], rot: [0, 0, 0], scale: 1, look: "solid" },
  { id: "stock", name: `Stock ${STOCK.w} x ${STOCK.l} x ${STOCK.h}`, role: "workpiece", src: "builtin:stock", frame: "work", pos: [0, 0, 0], rot: [0, 0, 0], scale: 1, look: "solid" },
];
const setup = { objects: SETUP_DEFAULT.map((o) => JSON.parse(JSON.stringify(o))), sel: null, nodes: new Map(), geoms: new Map() };
const ROLE_COLOR = { fixture: 0x56677b, workpiece: 0xd2d9e0 };

// ------------------------------------------------------------ 3D model files
// STL (binary or ASCII) and OBJ to a BufferGeometry, in the file's units (mm)
function parseModel(name, buf) {
  const pos = /\.obj$/i.test(name) ? objTriangles(new TextDecoder().decode(buf)) : stlTriangles(buf);
  if (!pos.length) throw new Error("no triangles in it");
  if (pos.length / 9 > 2e6) throw new Error("more than 2 million triangles");
  const g = new THREE.BufferGeometry();
  g.setAttribute("position", new THREE.BufferAttribute(pos, 3));
  g.computeVertexNormals(); g.computeBoundingBox();
  return g;
}
function stlTriangles(buf) {
  const dv = new DataView(buf);
  if (buf.byteLength >= 84) {
    const n = dv.getUint32(80, true);
    if (84 + 50 * n === buf.byteLength) {  // binary: normal, three vertices, attribute
      const out = new Float32Array(n * 9);
      for (let i = 0; i < n; i++) for (let k = 0; k < 9; k++) out[i * 9 + k] = dv.getFloat32(84 + i * 50 + 12 + k * 4, true);
      return out;
    }
  }
  const out = [], re = /vertex\s+([-+\d.eE]+)\s+([-+\d.eE]+)\s+([-+\d.eE]+)/g, text = new TextDecoder().decode(buf);
  for (let m; (m = re.exec(text));) out.push(+m[1], +m[2], +m[3]);
  return new Float32Array(out.slice(0, out.length - (out.length % 9)));
}
function objTriangles(text) {
  const v = [], out = [];
  for (const line of text.split("\n")) {
    const w = line.trim().split(/\s+/);
    if (w[0] === "v") v.push([+w[1], +w[2], +w[3]]);
    else if (w[0] === "f") {
      const ix = w.slice(1).map((t) => { const i = parseInt(t, 10); return i < 0 ? v.length + i : i - 1; });
      for (let k = 1; k + 1 < ix.length; k++) for (const i of [ix[0], ix[k], ix[k + 1]]) { const p = v[i] || [0, 0, 0]; out.push(p[0], p[1], p[2]); }
    }
  }
  return new Float32Array(out);
}
const modelInfo = (g) => { const b = g.boundingBox, s = new THREE.Vector3(); b.getSize(s); return { tris: g.attributes.position.count / 3, size: [s.x, s.y, s.z] }; };

// ------------------------------------------------------------ building the objects
function builtinGroup(kind, material, edges) {
  const g = new THREE.Group();
  const box = (w, d, h, x, y, z) => {
    const geo = new THREE.BoxGeometry(w, d, h), m = new THREE.Mesh(geo, material);
    m.position.set(x, y, z); m.add(new THREE.LineSegments(new THREE.EdgesGeometry(geo), edges)); g.add(m);
  };
  if (kind === "stock") box(STOCK.w, STOCK.l, STOCK.h, STOCK.w / 2, STOCK.l / 2, -STOCK.h / 2);  // its origin: the top front-left corner
  else {  // the vise: its origin on the base plate, at the clamped stock's front-left corner
    box(STOCK.w + 70, STOCK.l + 40, 14, STOCK.w / 2, STOCK.l / 2, -7);
    box(STOCK.w + 70, 22, 34, STOCK.w / 2, -12, 3); box(STOCK.w + 70, 22, 34, STOCK.w / 2, STOCK.l + 12, 3);
  }
  return g;
}
// the scene's objects from setup.objects (models: once their geometry is here)
function buildObjects() {
  if (!R) return;
  for (const [, n] of setup.nodes) { R.objGroup.remove(n.node); n.material.dispose(); }
  setup.nodes.clear();
  const edges = new THREE.LineBasicMaterial({ color: 0x4cc9ff, transparent: true, opacity: .16 });
  for (const o of setup.objects) {
    const material = new THREE.MeshStandardMaterial({ color: ROLE_COLOR[o.role], metalness: .55, roughness: .4, side: THREE.DoubleSide });
    let node;
    if (o.src.startsWith("builtin:")) node = builtinGroup(o.src.slice(8), material, edges);
    else {
      const geo = setup.geoms.get(o.src.slice(6));
      if (!geo) { requestModel(o.src.slice(6)); continue; }
      node = new THREE.Mesh(geo, material);
      if (geo.attributes.position.count < 180000) node.add(new THREE.LineSegments(new THREE.EdgesGeometry(geo, 35), edges));
    }
    node.userData.objId = o.id;
    R.objGroup.add(node);
    setup.nodes.set(o.id, { node, material });
  }
  placeObjects();
  // a model just added: placed once its size is known, then saved
  for (const ob of setup.objects.filter((o) => o.fresh && setup.nodes.has(o.id))) {
    delete ob.fresh;
    if (ob.role === "workpiece") placeAtZero(ob);
    else {  // a fixture: centred under the work zero, on the table
      const b = objExtent(ob), w0 = [off().x + st.g92.x, off().y + st.g92.y];
      if (b) ob.pos = [w0[0] - (b.min.x + b.max.x) / 2, w0[1] - (b.min.y + b.max.y) / 2, 0];
      placeOnTable(ob);
    }
    placeObjects(); saveSetup();
  }
  applyLooks();
}
const pendingModels = new Set();
function requestModel(path) {  // live: the machine's file
  if (pendingModels.has(path) || !LIVE) return;
  pendingModels.add(path);
  LIVE.modelGet(path).then((buf) => {
    pendingModels.delete(path);
    if (!buf) return;
    try { setup.geoms.set(path, parseModel(path, buf)); } catch (e) { msg(`${path}: ${e.message}`, "warn"); return; }
    buildObjects();
  });
}
// positions: an object in the work frame moves with G5x, G92 and the rotation
function placeObjects() {
  if (!R) return;
  const o = off(), g = st.g92, rz = st.rot * Math.PI / 180;
  R.triad.position.set(o.x + g.x, o.y + g.y, o.z + g.z + ZTOP); R.triad.rotation.z = rz;
  for (const ob of setup.objects) {
    const n = setup.nodes.get(ob.id); if (!n) continue;
    const [x, y, z] = ob.pos, d = Math.PI / 180;
    if (ob.frame === "work") {
      n.node.position.set(o.x + g.x + x * Math.cos(rz) - y * Math.sin(rz), o.y + g.y + x * Math.sin(rz) + y * Math.cos(rz), o.z + g.z + z + ZTOP);
      n.node.rotation.set(ob.rot[0] * d, ob.rot[1] * d, ob.rot[2] * d + rz, "ZYX");
    } else {
      n.node.position.set(x, y, z + ZTOP);
      n.node.rotation.set(ob.rot[0] * d, ob.rot[1] * d, ob.rot[2] * d, "ZYX");
    }
    n.node.scale.setScalar(ob.scale);
  }
  if (R.selBox) R.selBox.update();
}
// solid, see-through, hidden: the role's switch first (fixtures, workpiece), then the object's own
function applyLooks() {
  for (const ob of setup.objects) {
    const n = setup.nodes.get(ob.id); if (!n) continue;
    const role = ob.role === "fixture" ? st.show.fix : st.show.work;
    const look = role === false ? "hidden" : role === "glass" ? "glass" : ob.look;
    n.node.visible = look !== "hidden";
    const m = n.material, glass = look === "glass";
    m.transparent = glass; m.opacity = glass ? 0.28 : 1; m.depthWrite = !glass;
    m.color.setHex(ROLE_COLOR[ob.role]);
    m.emissive.setHex(ob.id === setup.sel ? 0x1a5a7a : 0x000000); m.emissiveIntensity = ob.id === setup.sel ? 0.9 : 0;
    m.needsUpdate = true;
  }
}

// ------------------------------------------------------------ selecting, the panel
const objOf = (id) => setup.objects.find((o) => o.id === id);
function selectObject(id) {
  setup.sel = id && objOf(id) ? id : null;
  if (R) {
    if (R.selBox) { R.scene.remove(R.selBox); R.selBox = null; }
    const n = setup.sel && setup.nodes.get(setup.sel);
    if (n) { R.selBox = new THREE.BoxHelper(n.node, 0x4cc9ff); R.scene.add(R.selBox); }
  }
  applyLooks(); renderObjPanel();
}
function renderObjPanel() {
  const ob = setup.sel && objOf(setup.sel), p = $("objpanel");
  p.hidden = !ob; if (!ob) return;
  $("ob-name").textContent = ob.name || ob.src;
  $("ob-src").textContent = ob.src.startsWith("model:") ? ob.src.slice(6) : "built-in";
  $("ob-role").value = ob.role; $("ob-frame").value = ob.frame;
  ["x", "y", "z"].forEach((a, i) => { $("ob-" + a).value = +ob.pos[i].toFixed(3); $("ob-r" + a).value = +ob.rot[i].toFixed(2); });
  $("ob-scale").value = ob.scale;
  $$("#ob-look button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.look === ob.look)));
}
// the object's extent in its frame, from its origin (after rotation and scale)
function objExtent(ob) {
  const n = setup.nodes.get(ob.id); if (!n) return null;
  const saved = n.node.position.clone(); n.node.position.set(0, 0, 0);
  if (ob.frame === "work") n.node.rotation.z -= st.rot * Math.PI / 180;
  n.node.updateMatrixWorld(true);
  const b = new THREE.Box3().setFromObject(n.node);
  n.node.position.copy(saved); placeObjects();
  return b;
}
function changed(ob) { placeObjects(); applyLooks(); renderObjPanel(); saveSetup(); }
// where a new or moved object goes: a workpiece's top at work Z0 and its front-left at X0 Y0; a fixture under it, on the table
function placeAtZero(ob) {
  const b = objExtent(ob); if (!b) return;
  const w0 = [off().x + st.g92.x, off().y + st.g92.y, off().z + st.g92.z];
  const p = [-b.min.x, -b.min.y, -b.max.z];
  ob.pos = ob.frame === "work" ? p : p.map((v, i) => v + w0[i]);
}
function placeOnTable(ob) {
  const b = objExtent(ob); if (!b) return;
  const table = -ZTOP;  // machine Z of the table top (the tool-tip frame)
  ob.pos[2] = (ob.frame === "work" ? table - (off().z + st.g92.z) : table) - b.min.z;
}
let saveTimer = null;
function saveSetup() {  // the machine's setup after edits settle; the simulation's stays in the page
  if (!LIVE) return;
  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => LIVE.setupSave(setup.objects), 300);
}
function setupLoad(objects) {  // from the machine
  setup.objects = JSON.parse(JSON.stringify(objects));
  if (setup.sel && !objOf(setup.sel)) setup.sel = null;
  buildObjects(); selectObject(setup.sel);
}
// a model from the File page into the setup
function setupAddModel(path, role) {
  const id = "m" + Date.now().toString(36);
  const ob = { id, name: path.slice(path.lastIndexOf("/") + 1), role, src: "model:" + path, frame: role === "workpiece" ? "work" : "machine", pos: [0, 0, 0], rot: [0, 0, 0], scale: 1, look: "solid", fresh: true };
  setup.objects.push(ob);
  buildObjects();
  return id;
}

// the panel's controls
$("ob-close").addEventListener("click", () => selectObject(null));
$("objpanel").addEventListener("change", (e) => {
  const ob = objOf(setup.sel); if (!ob) return;
  const id = e.target.id, v = +e.target.value;
  if (id === "ob-role") ob.role = e.target.value;
  else if (id === "ob-frame") {  // stays where it is: the position re-expressed in the other frame
    const w0 = [off().x + st.g92.x, off().y + st.g92.y, off().z + st.g92.z], rz = st.rot * Math.PI / 180, [x, y, z] = ob.pos;
    if (e.target.value === "machine" && ob.frame === "work") { ob.pos = [w0[0] + x * Math.cos(rz) - y * Math.sin(rz), w0[1] + x * Math.sin(rz) + y * Math.cos(rz), w0[2] + z]; ob.rot[2] += st.rot; }
    else if (e.target.value === "work" && ob.frame === "machine") { const dx = x - w0[0], dy = y - w0[1]; ob.pos = [dx * Math.cos(-rz) - dy * Math.sin(-rz), dx * Math.sin(-rz) + dy * Math.cos(-rz), z - w0[2]]; ob.rot[2] -= st.rot; }
    ob.frame = e.target.value;
  } else if (/^ob-[xyz]$/.test(id) && isFinite(v)) ob.pos["xyz".indexOf(id[3])] = v;
  else if (/^ob-r[xyz]$/.test(id) && isFinite(v)) ob.rot["xyz".indexOf(id[4])] = v;
  else if (id === "ob-scale" && v > 0) ob.scale = v;
  changed(ob);
});
$("ob-look").addEventListener("click", (e) => { const b = e.target.closest("button[data-look]"), ob = objOf(setup.sel); if (b && ob) { ob.look = b.dataset.look; changed(ob); } });
$("ob-zero").addEventListener("click", () => { const ob = objOf(setup.sel); if (ob) { placeAtZero(ob); changed(ob); } });
$("ob-table").addEventListener("click", () => { const ob = objOf(setup.sel); if (ob) { placeOnTable(ob); changed(ob); } });
$("ob-turn").addEventListener("click", () => { const ob = objOf(setup.sel); if (ob) { ob.rot[2] = (ob.rot[2] + 90) % 360; changed(ob); } });
$("ob-flip").addEventListener("click", () => { const ob = objOf(setup.sel); if (ob) { ob.rot[0] = (ob.rot[0] + 90) % 360; changed(ob); } });
$("ob-del").addEventListener("click", () => {
  const ob = objOf(setup.sel); if (!ob) return;
  ask(`Remove ${ob.name} from the setup?`, ob.src.startsWith("model:") ? "Its model file stays in the program folder." : "Built in: it can't be added back from the File page (only by resetting the setup).", "", () => {
    setup.objects = setup.objects.filter((o) => o !== ob); setup.sel = null; buildObjects(); selectObject(null); saveSetup();
  }, "confirm");
});

// clicking in the view selects what is under the pointer (a drag still orbits)
(function pickInView() {
  const cv = $("scene"); let down = null;
  cv.addEventListener("pointerdown", (e) => { down = { x: e.clientX, y: e.clientY, t: performance.now() }; });
  cv.addEventListener("pointerup", (e) => {
    if (!R || !down || Math.hypot(e.clientX - down.x, e.clientY - down.y) > 5 || performance.now() - down.t > 500) { down = null; return; }
    down = null;
    const r = cv.getBoundingClientRect(), ray = new THREE.Raycaster();
    ray.setFromCamera(new THREE.Vector2((e.clientX - r.left) / r.width * 2 - 1, -(e.clientY - r.top) / r.height * 2 + 1), R.cam);
    const hit = ray.intersectObjects(R.objGroup.children, true).find((h) => h.object.isMesh && visibleUp(h.object));
    let n = hit && hit.object; while (n && n.userData.objId === undefined) n = n.parent;
    selectObject(n ? n.userData.objId : null);
  });
  const visibleUp = (o) => { for (; o; o = o.parent) if (!o.visible) return false; return true; };
})();
