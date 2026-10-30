/* Portal Mill Console: a lathe. machine.json's type "lathe" (LinuxCNC's [DISPLAY] LATHE) turns the console
   into a lathe's: X (the cross slide, shown as a diameter in G7) and Z (along the spindle), the tools on a
   post in front of the spindle's axis (+X toward the operator) or behind it, a turret's tools by their
   orientation, the stock turning in the chuck, lathe programs and wizards (face, turn, thread, groove).

   The view: the spindle's axis along scene x through y 0, z 0, the chuck's face at x 0; the work's X0 is the
   axis, its Z0 at LATHE.face along x (the stock's face at its set-up's Z); scene y = -X for a front post, +X for a back one, so
   the top view is the operator's (AXIS's). Positions are radii, as LinuxCNC keeps them: only the DRO and the
   G-code's X words are diameters. Classic script after stock.js and machmodel.js; app.js calls in where R.type is
   "lathe".

   The stock (Ø, bore, length out of the chuck, its face's Z; the setup's, saved with it) is cut as the program
   runs: stock.js sweeps each move's insert through it, the view draws what is left coloured by the tool that
   cut it. A program's whole run is checked when it is shown on the File page (Preflight: Material removal). */
"use strict";

let LATHE = null;  // { post, dia, face, stockD, stockL, s (scene y per +X) } while the console is a lathe's
let latheSaved = null;  // the mill's own state while a lathe's replaces it (the simulation)

// ------------------------------------------------------------ entering and leaving
function latheEnter(o = {}) {
  const was = !!LATHE;
  LATHE = { post: o.post === "back" ? "back" : "front", dia: o.dia !== false, face: 59, stockD: 32, stockL: 60, spin: 0, final: false };
  LATHE.s = LATHE.post === "front" ? -1 : 1;
  document.body.classList.add("lathe");
  if (!was && !LIVE) {  // the simulation: a lathe's tools, offsets, travel and programs
    latheSaved = { tools: st.tools, offsets: st.offsets.map((x) => ({ ...x })), travel: JSON.parse(JSON.stringify(TRAVEL)), library: library.slice(), prog, tool: st.tool, pos: { ...st.pos } };
    st.tools = LATHE_TOOLS.map((t) => ({ ...t }));
    st.offsets.forEach((x, i) => { x.x = 0; x.y = 0; x.z = i === 0 ? -150 : i === 1 ? -120 : 0; });
    st.g92 = { x: 0, y: 0, z: 0 }; st.tool = 1; st.toolSel = 1;
    library.length = 0; library.push(plan(latheDemo()), plan(latheFace({ d0: 34, d1: 0, z: 1, step: 0.5, f: 0.12, v: 150, t: 1 })));
    stopAll(); st.motion = null; st.run = "idle";  // the mill's held program goes with it
    prog = library[0]; st.pos = { x: 25, y: 0, z: 10 }; st.spindle.dir = 0;
  }
  if (!LIVE) { TRAVEL.y = [0, 0]; if (o.travel) Object.assign(TRAVEL, o.travel); }
  latheUi(true);
  if (typeof renderProgram === "function" && !was) { renderProgram(); fillToolSelects(); }
  buildTool();
  rebuildPath();
}
function latheExit() {
  if (!LATHE) return;
  LATHE = null;
  document.body.classList.remove("lathe");
  if (latheSaved) {
    st.tools = latheSaved.tools; latheSaved.offsets.forEach((x, i) => Object.assign(st.offsets[i], x));
    Object.assign(TRAVEL, latheSaved.travel); library.length = 0; library.push(...latheSaved.library);
    prog = latheSaved.prog; st.tool = latheSaved.tool; st.pos = latheSaved.pos; latheSaved = null;
    renderProgram(); fillToolSelects();
  }
  latheUi(false);
  rebuildPath();
}
// the console's lathe bits: the DRO's Y, the jog pad's labels, pages a lathe has no use for
function latheUi(on) {
  const brand = document.querySelector(".brand span");
  if (brand) { if (!brand.dataset.mill) brand.dataset.mill = brand.textContent; brand.textContent = on ? `lathe · ${LATHE && LATHE.post === "back" ? "back" : "front"} tool post · stmbl · LinuxCNC 2.9` : brand.dataset.mill; }
  const tn = $("tl-note"); if (tn) { if (!tn.dataset.mill) tn.dataset.mill = tn.textContent; tn.textContent = on ? "X and Z offsets from the turret's reference tool; Nose R and Q (the orientation, 1–9) for nose radius compensation (G41/G42) and the 3D view. Touch off: turn a diameter, measure it, SET it on X; face, SET Z0." : tn.dataset.mill; }
  $("tl-meas").hidden = on;
  const row = $("d-y") && $("d-y").closest(".axis"); if (row) row.hidden = on;
  $("b-zeroall").textContent = on ? "Zero XZ" : "Zero XYZ";
  for (const p of ["probe", "dyn"]) { const b = document.querySelector(`#nav button[data-p="${p}"]`); if (b) b.hidden = on; }
  const lbl = { "x-": "Z−", "x+": "Z+", "y+": LATHE && LATHE.s < 0 ? "X−" : "X+", "y-": LATHE && LATHE.s < 0 ? "X+" : "X−" };
  $$("#jogpad button").forEach((b) => {
    const d = b.dataset.j;
    if (!b.dataset.mill) b.dataset.mill = b.textContent;
    b.textContent = on && lbl[d] ? lbl[d] : b.dataset.mill;
    if (d === "z+" || d === "z-") b.hidden = on;
  });
  $$("#p-wiz > .cards").forEach((c) => { c.hidden = on ? !c.classList.contains("lathe-cards") : c.classList.contains("lathe-cards"); });
  const cams = { iso: "ISO", top: on ? "XZ" : "TOP", front: "FRONT", side: on ? "END" : "SIDE" };
  $$("[data-cam]").forEach((b) => { if (cams[b.dataset.cam]) b.textContent = cams[b.dataset.cam]; });
}

// ------------------------------------------------------------ the view's frame
// a point in work coordinates (X a radius) -> the scene
const latheVp = (x, y, z) => [LATHE.face + z, LATHE.s * x, 0];

// the built-in lathe: bed, headstock and chuck (they turn), tailstock; the saddle (Z) and the cross slide
// (X) with the tool post. The same groups as a mill's: carriage (Z), ram (X), headG, toolG (the tool's tip)
function latheBuild(machine) {
  const { mat, box } = R, s = LATHE ? LATHE.s : -1;
  const cylX = (r, l, m, x, seg = 40) => { const g = new THREE.CylinderGeometry(r, r, l, seg); g.rotateZ(Math.PI / 2); const mesh = new THREE.Mesh(g, m); mesh.position.set(x, 0, 0); return mesh; };
  machine.add(box(760, 170, 40, mat.frame, 130, 0, -150));                                   // the bed
  for (const y of [-55, 55]) machine.add(box(700, 22, 14, mat.rail, 150, y, -123));          // its ways
  machine.add(box(220, 180, 220, mat.bridge, -170, 0, -20));                                 // the headstock
  machine.add(box(90, 90, 120, mat.carriage, 470, 0, -70));                                  // the tailstock
  machine.add(cylX(14, 60, mat.spindle, 410));                                               // its quill
  const spinG = new THREE.Group(); machine.add(spinG);                                      // turns with the spindle
  spinG.add(cylX(55, 40, mat.dark, -20));
  for (let k = 0; k < 3; k++) {
    const jaw = box(16, 14, 26, mat.rail, -6, 0, 0, false), a = k * 2 * Math.PI / 3;
    jaw.position.set(-6, Math.cos(a) * 34, Math.sin(a) * 34); jaw.rotation.x = a; spinG.add(jaw);
  }
  const stockG = new THREE.Group(); machine.add(stockG);  // the stock (latheRemMesh): shown with a model's parts too
  const carriage = new THREE.Group(), ram = new THREE.Group(), headG = new THREE.Group(), toolG = new THREE.Group(), coolant = new THREE.Group(), bridge = new THREE.Group();
  machine.add(bridge); bridge.add(carriage); carriage.add(ram); ram.add(headG); headG.add(toolG);
  // the saddle on the ways, the cross slide on it (toward the operator for a front post), the tool post
  carriage.add(box(110, 200, 30, mat.frame, 30, 0, -100));
  ram.add(box(90, 120, 22, mat.dark, 30, s * 70, -74));
  ram.add(box(46, 46, 60, mat.dark, 30, s * 40, -32));
  const noz = R.cyl(2, 24, mat.accent, 0, s * 14, 14, 8); noz.rotation.x = s * 0.7; coolant.add(noz);  // aimed at the tip
  ram.add(coolant);
  R.spinG = spinG; R.stock = null; R.stockG = stockG; LREM.R = null; LREM.meshes = [];
  return { bridge, carriage, ram, headG, toolG, coolant };
}
// the stock's set-up: Ø, bore (a tube's), out of the chuck, its face at work Z, the chuck's Ø
const LSTOCK_DEF = { d: 32, bore: 0, stick: 60, face: 1, chuckD: 100 };
let latheVer = 0;  // the tools changed: the checks again
function latheStockOpts() {
  let o = typeof setup !== "undefined" && setup.lathe;
  if (!o && !LIVE) try { o = JSON.parse(localStorage.getItem("pmc.lathestock") || "null"); } catch (e) { /* storage blocked */ }
  return { ...LSTOCK_DEF, ...(o || {}) };
}
function latheStockSave(o) {
  setup.lathe = { ...latheStockOpts(), ...o };
  if (LIVE) saveSetup(); else try { localStorage.setItem("pmc.lathestock", JSON.stringify(setup.lathe)); } catch (e) { /* storage blocked */ }
  latheStock();
}
// the scene from the set-up: the chuck's face at scene x 0, the stock out of it, the work's Z0 at LATHE.face
function latheStock() {
  if (!LATHE) return;
  const o = latheStockOpts();
  LATHE.stockD = o.d; LATHE.stockL = o.stick; LATHE.face = o.stick - o.face;
  if (R && R.type === "lathe") rebuildPath();
}

// ------------------------------------------------------------ the stock as the program cuts it (stock.js)
const LREM = { R: null, prog: null, key: "", ver: 0, built: -1, at: 0, meshes: [], checked: null };
const latheRemKey = () => JSON.stringify(latheStockOpts()) + "|" + latheVer;
function latheToolCol(t) {
  if (toolColors[t]) return toolColors[t];
  const h = (t * 0.137) % 1, c = new THREE.Color().setHSL(h, 0.75, 0.6);
  return [c.r, c.g, c.b];
}
// every frame: the stock as far as the program is (the move on, as far as it is); the whole program when it is
// done or the finished part is asked for; the raw stock otherwise
function latheRemFollow(now) {
  if (!R || !R.stockG) return;
  let to = 0, u = 0;
  if (st.motion === prog && busy()) {
    to = st.seg;
    const s = prog.segs[to];
    if (!LIVE && s && s.t > 0) u = clamp((st.t - s.t0) / s.t, 0, 1);
  } else if (st.run === "done" || LATHE.final) to = prog.segs.length;
  const key = latheRemKey();
  if (!LREM.R || LREM.prog !== prog || LREM.key !== key || to < LREM.R.i) {
    if (LREM.prog !== prog || LREM.key !== key) latheCheckLoaded();
    LREM.R = LSTOCK.begin({ ...latheStockOpts(), dz: 0.05 }); LREM.prog = prog; LREM.key = key; LREM.ver++;
  }
  if (to > LREM.R.i || u > 0) {
    const lim = Math.min(to, LREM.R.i + 4000);  // a long way to go: some each frame
    LSTOCK.run(LREM.R, prog.segs, st.tools, lim, lim === to ? u : 0, false);
    LREM.ver++;
  }
  if (LREM.ver !== LREM.built && (now - LREM.at > 120 || !busy())) { latheRemMesh(); LREM.built = LREM.ver; LREM.at = now; }
}
// what is left, revolved: one mesh a solid stretch, coloured by the cutting tool; a part cut off where it was,
// see-through
function latheRemMesh() {
  for (const m of LREM.meshes) { R.stockG.remove(m); m.geometry.dispose(); }
  LREM.meshes = [];
  const S = LREM.R.S;
  latheProfile(S.rin, S.rout, S.tin, S.tout, S.thr, 0, S, false);
  for (const p of S.parts) latheProfile(p.rin, p.rout, p.tin, p.tout, p.thr, p.i0, S, true);
}
// a thread's slices: their major Ø, a saw of its depth (0.5 mm a tooth: the look, not its pitch)
function latheProfile(rin, rout, tin, tout, thr, i0, S, off) {
  const n = rout.length, step = Math.max(1, Math.round(Math.max(S.stick / 600, S.dz) / S.dz)), z = (i) => S.z0 + (i0 + i + 0.5) * S.dz;
  const solid = (i) => rout[i] > rin[i] + 1e-6;
  const runs = []; let cur = null;
  for (let i = 0; i < n; i += step) {
    const j = Math.min(n - 1, i + (step >> 1));
    if (solid(j)) { if (!cur) runs.push(cur = []); cur.push(j); } else cur = null;
  }
  const base = [0.62, 0.66, 0.71], h = step * S.dz / 2;
  const mix = (t) => { if (!t) return base; const c = latheToolCol(t); return [c[0] * 0.7 + base[0] * 0.3, c[1] * 0.7 + base[1] * 0.3, c[2] * 0.7 + base[2] * 0.3]; };
  for (const run of runs) {
    const a = run[0], b = run[run.length - 1], pts = [], col = [];
    const add = (r, zz, c) => { pts.push(new THREE.Vector2(Math.max(r, 0), zz)); col.push(c); };
    add(rin[a], z(a) - h, base); add(rout[a], z(a) - h, base);
    for (const i of run) {
      if (thr[i] > 0) { const zz = z(i), u = ((zz % 0.5) + 0.5) % 0.5 / 0.5; add(rout[i] - thr[i] * (1 - Math.abs(2 * u - 1)), zz, mix(tout[i])); }
      else add(rout[i], z(i), mix(tout[i]));
    }
    add(rout[b], z(b) + h, mix(tout[b])); add(rin[b], z(b) + h, base);
    for (let k = run.length - 1; k >= 0; k--) add(rin[run[k]], z(run[k]), mix(tin[run[k]]));
    add(rin[a], z(a) - h, base);
    const g = new THREE.LatheGeometry(pts, 48);
    const c = new Float32Array(g.attributes.position.count * 3);
    for (let i = 0; i <= 48; i++) for (let j = 0; j < pts.length; j++) c.set(col[j], (i * pts.length + j) * 3);
    g.setAttribute("color", new THREE.BufferAttribute(c, 3));
    g.rotateZ(-Math.PI / 2);  // its axis (the profile's height, the work's Z) along the scene's x
    if (!LREM.mat) {
      LREM.mat = new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0.45, roughness: 0.45, side: THREE.DoubleSide });
      LREM.matOff = LREM.mat.clone(); LREM.matOff.transparent = true; LREM.matOff.opacity = 0.85; LREM.matOff.depthWrite = false;
    }
    const m = new THREE.Mesh(g, off ? LREM.matOff : LREM.mat);
    m.position.x = LATHE.face;
    R.stockG.add(m); LREM.meshes.push(m);
  }
}
// a program's whole run through the stock: its findings and numbers (kept on it until the stock or tools change)
async function latheCheck(p) {
  const key = latheRemKey();
  if (p.removal && p.removal.key === key) return p.removal;
  const R2 = LSTOCK.begin({ ...latheStockOpts(), dz: 0.05 });
  for (let i = 0; i < p.segs.length;) {
    i = Math.min(p.segs.length, i + 20000);
    LSTOCK.run(R2, p.segs, st.tools, i);
    if (i < p.segs.length) await nextFrame();
  }
  const order = { bad: 0, warn: 1, info: 2 };
  R2.find.sort((a, b) => order[a.level] - order[b.level] || a.line - b.line);
  R2.key = key; p.removal = R2;
  return R2;
}
// the loaded program checked: its findings in the view, a word when something would go wrong
function latheCheckLoaded() {
  const p = prog;
  if (!p || !p.segs.length) { latheMarks([]); return; }
  latheCheck(p).then((r) => {
    if (p !== prog) return;
    latheMarks(r.find);
    const bad = r.find.filter((f) => f.level === "bad");
    if (bad.length && LREM.checked !== p) msg(`${p.name}: ${bad.length} crash risk${bad.length > 1 ? "s" : ""} in the simulation. ${nline(bad[0].line)}: ${bad[0].text}. File page, Preflight.`, "warn");
    LREM.checked = p;
    if (st.page === "file" && $("fl-rem")) renderPreflight(p);
  });
}
// the findings in the view: a mark at each
function latheMarks(find) {
  if (!R) return;
  if (!R.findG) { R.findG = new THREE.Group(); R.findG.renderOrder = 12; R.workG.add(R.findG); }
  R.findG.clear();
  const col = { bad: 0xff4d5e, warn: 0xffb547, info: 0x4cc9ff };
  for (const f of find.slice(0, 200)) {
    const m = new THREE.Mesh(R.markGeo || (R.markGeo = new THREE.SphereGeometry(1.4, 12, 8)), new THREE.MeshBasicMaterial({ color: col[f.level], depthTest: false }));
    m.renderOrder = 12; m.position.set(...latheVp(f.x, 0, f.z)); R.findG.add(m);
  }
}
// the Preflight card: the stock's set-up, what the run takes off, what it would do wrong
function latheRemCard(p) {
  const o = latheStockOpts(), el = document.createElement("div");
  el.className = "card"; el.id = "fl-rem";
  const num = (k, l, step) => `<label>${l}<input id="ls-${k}" type="number" step="${step}" value="${o[k]}"></label>`;
  const head = (pill, cls) => `<h3>Material removal <span class="pill ${cls}">${pill}</span></h3>`;
  const form = `<div class="form">${num("d", "Stock Ø", 0.5)}${num("bore", "Bore Ø (tube)", 0.5)}${num("stick", "Out of the chuck", 1)}${num("face", "Its face at Z", 0.1)}${num("chuckD", "Chuck Ø", 5)}</div>`;
  const r = p.removal && p.removal.key === latheRemKey() ? p.removal : null;
  if (!r) { el.innerHTML = head("CHECKING…", "info") + form; latheCheck(p).then(() => { if ($("fl-rem") === el && st.page === "file") el.replaceWith(latheRemCard(p)); }); return el; }
  const nb = remCount(r.find, "bad"), nw = remCount(r.find, "warn"), S = r.S;
  let dmax = 0, last = -1; for (let i = 0; i < S.n; i++) if (LSTOCK.solid(S, i)) { dmax = Math.max(dmax, S.rout[i] * 2); last = i; }
  const cm3 = (v) => (v / 1000).toFixed(1) + " cm³";
  const tools = Object.entries(r.stat.byTool).filter(([, b]) => b.vol > 0.5).map(([t, b]) => `<dt>T${t}</dt><dd>${cm3(b.vol)} · ${b.mrr.toFixed(1)}/min · ap ${b.depth.toFixed(2)}</dd>`).join("");
  const part = S.parts[S.parts.length - 1];
  // the threads: their stretches, depth and major Ø
  const threads = []; for (const P of [S, ...S.parts]) { const o = P === S ? 0 : P.i0; let a = -1; for (let i = 0; i <= P.thr.length; i++) { if (i < P.thr.length && P.thr[i] > 0) { if (a < 0) a = i; } else if (a >= 0) { const sl = Array.from(P.thr.slice(a, i)); threads.push(`<dt>Thread</dt><dd>Ø${(P.rout[a] * 2).toFixed(2)} · ${Math.max(...sl).toFixed(2)} deep · Z${(S.z0 + (o + i) * S.dz).toFixed(1)}…${(S.z0 + (o + a) * S.dz).toFixed(1)}</dd>`); a = -1; } } }
  el.innerHTML = head(nb ? `${nb} CRASH RISK${nb > 1 ? "S" : ""}` : nw ? `${nw} WARNING${nw > 1 ? "S" : ""}` : "CLEAN", nb ? "bad" : nw ? "warn" : "ok") + form +
    `<dl class="kv"><dt>Removed</dt><dd>${cm3(r.stat.vol)} of ${cm3(S.vol0)}</dd><dt>Cut · air</dt><dd>${mmss(r.stat.cutT)} · ${mmss(r.stat.air)}</dd>${tools}
      ${threads.join("")}${part ? `<dt>Part off</dt><dd>Ø${(Math.max(...part.rout) * 2).toFixed(2)} × ${(part.rout.length * S.dz).toFixed(1)}</dd>` : ""}
      <dt>In the chuck</dt><dd>${last >= 0 ? `Ø${dmax.toFixed(2)} × ${((last + 1) * S.dz).toFixed(1)}` : "nothing"}</dd></dl>` +
    (r.find.length ? remFindList(r.find) : `<p class="note">No rapid into the stock, the tools and holders clear of the part and the chuck, every cut within its insert.</p>`) +
    `<div class="row"><button class="btn sm" id="ls-final" aria-pressed="${LATHE.final}">${LATHE.final ? "Showing the finished part" : "Show the finished part"}</button></div>`;
  return el;
}
document.addEventListener("change", (e) => {
  const m = e.target.id && e.target.id.match(/^ls-(d|bore|stick|face|chuckD)$/); if (!m) return;
  const v = +e.target.value; if (!Number.isFinite(v)) return;
  latheStockSave({ [m[1]]: v });
  const p = LIVE ? null : library[fileSel];
  if (st.page === "file") { if (LIVE) LIVE.selectFile(fileSel); else renderPreflight(p); }
});
document.addEventListener("click", (e) => {
  if (e.target.id === "ls-final") { LATHE.final = !LATHE.final; e.target.setAttribute("aria-pressed", String(LATHE.final)); e.target.textContent = LATHE.final ? "Showing the finished part" : "Show the finished part"; return; }
  const li = e.target.closest && e.target.closest(".rem-find li[data-line]"); if (!li) return;
  // the line in the loaded program's listing
  const n = +li.dataset.line;
  if ($("fl-name").textContent !== prog.name) { msg(`Load the program to see ${nline(n)} in it.`, "info"); return; }
  showPage("main"); st.selLine = n; renderListing();
  const d = $("gl-" + n); if (d) { d.scrollIntoView({ block: "center" }); $("b-runfrom").disabled = false; $("b-runfrom").textContent = `Run from ${nline(n)}`; }
});

// every frame: the slides at the tool, the stock turning, the trail and chips, the camera
function latheUpdate3d(dt, tip, moving, cutting) {
  const [tx, ty] = latheVp(tip.x, 0, tip.z);
  R.carriage.position.set(tx, 0, 0); R.ram.position.set(0, ty, 0); R.headG.position.set(0, 0, 0);
  LATHE.spin = (LATHE.spin + st.spindle.rpm / 60 * 2 * Math.PI * dt * (st.spindle.dir || 1) * 0.15) % (2 * Math.PI);
  if (R.spinG) R.spinG.rotation.x = LATHE.spin;
  if (R.stockG) R.stockG.rotation.x = LATHE.spin;
  R.machine.visible = st.show.machine; R.workG.position.set(0, 0, 0); R.objGroup.visible = false;
  R.triad.position.set(LATHE.face, 0, 0); R.triad.rotation.z = 0;
  latheRemFollow(performance.now());
  mm3dPlace({ x: tx, y: ty, z: 0, lathe: true });
  R.pathLine.visible = st.show.path; R.trail.visible = st.show.path; if (R.pathMirror) R.pathMirror.visible = st.show.path;
  R.limits.visible = st.show.limits; R.coolant.visible = st.cool.flood || st.cool.mist;
  if (moving && cutting) {
    const p = R.trailPos;
    if (R.trailLen < R.trailN) { p.set([tx, ty, 0], R.trailLen * 3); R.trailLen++; } else { p.copyWithin(0, 3); p.set([tx, ty, 0], (R.trailN - 1) * 3); }
    R.trailGeo.attributes.position.needsUpdate = true; R.trailGeo.setDrawRange(0, R.trailLen);
  }
  for (let i = 0; i < R.sparkN; i++) {  // chips: thrown off the tool, falling into the tray
    if (R.sparkLife[i] <= 0 && moving && cutting && st.spindle.rpm > 100 && Math.random() < .3) {
      R.sparkLife[i] = .4 + Math.random() * .4;
      R.sparkPos.set([tx, ty, 2], i * 3); R.sparkVel.set([(Math.random() - .5) * 120, LATHE.s * (80 + Math.random() * 120), 60 + Math.random() * 120], i * 3);
    }
    if (R.sparkLife[i] > 0) {
      R.sparkLife[i] -= dt; for (let j = 0; j < 3; j++) R.sparkPos[i * 3 + j] += R.sparkVel[i * 3 + j] * dt;
      R.sparkVel[i * 3 + 2] -= 900 * dt;
      if (R.sparkLife[i] <= 0) R.sparkPos.set([0, 0, -5000], i * 3);
    }
  }
  R.sparkGeo.attributes.position.needsUpdate = true;
  view3d(dt, [tx, ty, 0], [LATHE.face / 2 + 60, 0, -20]);
}
// the travel box: the soft limits in the work's frame
function latheLimits() {
  const o = off(), g = st.g92;
  const z0 = LATHE.face + TRAVEL.z[0] - o.z - g.z, z1 = LATHE.face + TRAVEL.z[1] - o.z - g.z;
  const x0 = LATHE.s * (TRAVEL.x[0] - o.x - g.x), x1 = LATHE.s * (TRAVEL.x[1] - o.x - g.x);
  R.limits.scale.set(Math.max(1, z1 - z0), Math.max(1, Math.abs(x1 - x0)), 2);
  R.limits.position.set((z0 + z1) / 2, (x0 + x1) / 2, 0);
}
function latheCam(kind) {
  const f = LATHE.face, [tx, ty] = latheVp(st.pos.x, 0, st.pos.z);
  return {
    // the work area framed: the stock, the tool's reach around it (the whole machine: zoom out)
    iso: [[-120, LATHE.s * 380 - 40, 260], [f / 2 + 40, LATHE.s * 30, -10]], top: [[f / 2 + 30, LATHE.s * 25 - 1, 380], [f / 2 + 30, LATHE.s * 25, 0]],
    front: [[f / 2 + 40, -420, 40], [f / 2 + 40, 0, 0]], side: [[620, 0, 90], [0, 0, 0]],
    part: [[f - 40, LATHE.s * 220 - 60, 160], [f / 2, 0, 0]], follow: [[tx - 80, ty + LATHE.s * 220, 160], [tx, ty, 0]],
  }[kind];
}

// ------------------------------------------------------------ the tools: a holder and its insert, by orientation
// LinuxCNC's orientation (Q, 1-9): where the nose's centre is from the controlled point (glcanon.py's lathe_shapes):
// Q2 an outside turning tool (its point toward -X -Z), Q3 a boring bar, Q6 a thread or a parting tool across,
// Q7 a drill on the axis. The outline and holder are stock.js's, the ones the checks cut with.
// ins: the insert's shape (80°, 55°, 35° rhombus, 60° thread, groove, round, drill); w: a grooving insert's width;
// cl: its cutting edge length (the tool database's tag in tool.tbl's comment)
const INSERTS = [["", "—"], ["80", "80° rhombus"], ["55", "55° rhombus"], ["35", "35° rhombus"], ["60", "60° thread"], ["groove", "groove / part"], ["round", "round"], ["drill", "drill"]];
const LATHE_TOOLS = [
  { t: 1, p: 1, x: 0, l: 0, d: 0.8, q: 2, ins: "80", cl: 9, wear: 0, life: 90, c: "CCMT 09 R0.4 · OD turning, 80°" },
  { t: 2, p: 2, x: 1.2, l: -2.5, d: 0.4, q: 2, ins: "55", cl: 7, wear: 0, life: 100, c: "DCMT 07 R0.2 · OD finishing, 55°" },
  { t: 3, p: 3, x: 0.6, l: 1.5, d: 0.2, q: 6, ins: "60", cl: 8, wear: 0, life: 100, c: "16ER 60° · OD threading" },
  { t: 4, p: 4, x: 0.4, l: 3.0, d: 0.2, q: 2, ins: "groove", w: 2, cl: 20, wear: 0, life: 85, c: "2 mm · grooving and parting" },
  { t: 5, p: 5, x: -0.8, l: 0.5, d: 0.4, q: 3, ins: "55", cl: 7, wear: 0, life: 100, c: "boring bar Ø10 · ID turning" },
  { t: 6, p: 6, x: 0, l: 20, d: 6, q: 7, ins: "drill", cl: 40, fl: 2, wear: 0, life: 100, c: "6 mm drill · on the centre" },
];
function latheBuildTool() {
  const t = toolOf(st.tool) || { d: 0.8, q: 2 };
  while (R.toolG.children.length) R.toolG.remove(R.toolG.children[0]);
  const sh = LSTOCK.toolShape(t), s = LATHE.s, m = R.mat;
  // a hull (x a radius, z) as a flat shape in the scene's x (Z) and y (s·X)
  const flat = (h, depth, mat) => {
    const pts = [...h.lo, ...h.hi.slice(1, -1).reverse()], sh2 = new THREE.Shape();
    pts.forEach(([x, z], k) => (k ? sh2.lineTo(z, s * x) : sh2.moveTo(z, s * x)));
    sh2.closePath();
    const g = new THREE.ExtrudeGeometry(sh2, { depth, bevelEnabled: false }); g.translate(0, 0, -depth / 2);
    return new THREE.Mesh(g, mat);
  };
  R.toolG.add(flat(sh.poly, sh.ins === "drill" ? Math.max(1, t.d) : 4, sh.ins === "drill" ? m.tool : m.accent));
  if (sh.holder) R.toolG.add(flat(sh.holder, sh.side > 0 ? 12 : 10, m.dark));
}

// ------------------------------------------------------------ the DRO and the jog pad
function latheDro(show, mach, tip) {
  const d = LATHE.dia && st.dro !== "dtg" ? 2 : 1;
  $("d-x").textContent = (d === 2 ? "Ø" : "") + fmtLen(show.x * d).trim();
  $("ds-x").textContent = d === 2 ? `r ${fmtLen(show.x, false)} · machine ${fmtLen(mach.x * 2, false)}` : `machine ${fmtLen(mach.x, false)}`;
}
// the pad's arrows as the top view shows them: left/right Z, up/down X (up: away from the operator, front post)
function latheJogDir(dir) {
  if (dir === "home") return dir;
  const m = { "x-": "z-", "x+": "z+", "y+": LATHE.s < 0 ? "x-" : "x+", "y-": LATHE.s < 0 ? "x+" : "x-" };
  return m[dir] || null;
}

// ------------------------------------------------------------ the tools page
const TURRET_E = ["", "no index within 1.5 turns", "the index where it shouldn't be: slipped or lost steps", "the change took too long", "no such station"];
// the turret's state (live: lathe.turret's pins through slow; the simulation: the tool's station of 6)
function latheTurret() {
  const tu = st.turret || (LIVE ? null : { station: st.tool > 0 ? (st.tool - 1) % 6 + 1 : 0, stations: 6, homed: st.tool > 0, busy: false, fault: false, error: 0 });
  let el = $("tl-turret");
  if (!el) { el = document.createElement("p"); el.id = "tl-turret"; el.className = "note"; const n = $("tl-note"); if (n) n.after(el); else return; }
  el.hidden = !tu;
  if (!tu) return;
  el.classList.toggle("bad", !!tu.fault);
  el.textContent = tu.fault ? `Turret fault: ${TURRET_E[tu.error] || "error " + tu.error}. Reset the E-STOP: the turret homes again on the next change.`
    : `Turret: ${tu.busy ? "turning…" : tu.homed ? `station ${tu.station} of ${tu.stations}` : "not homed (the first M6 finds its index)"}. Tool n is on station (n − 1) mod ${tu.stations} + 1.`;
}
function latheRenderTools() {
  latheTurret();
  $("tool-t").innerHTML = `<thead><tr><th>T</th><th class="n">Pocket</th><th class="n">X off</th><th class="n">Z off</th><th class="n">Nose R</th><th class="n">Q</th><th class="n">Insert</th><th class="n">Width</th><th class="n">Edge</th><th class="n">Wear</th><th>Comment</th></tr></thead><tbody>` +
    st.tools.map((t, i) => `<tr class="${t.t === st.tool ? "active" : t.t === st.toolSel ? "sel" : ""}" data-t="${t.t}"><td class="num">T${t.t}${t.t === st.tool ? " ● in the turret" : ""}</td>
      <td class="n">${t.p}</td>
      <td class="n"><input data-k="x" data-i="${i}" type="number" step="0.001" value="${(+(t.x || 0)).toFixed(3)}" aria-label="T${t.t} X offset"></td>
      <td class="n"><input data-k="l" data-i="${i}" type="number" step="0.001" value="${(+(t.l || 0)).toFixed(3)}" aria-label="T${t.t} Z offset"></td>
      <td class="n"><input data-k="d" data-scale="2" data-i="${i}" type="number" step="0.1" value="${(t.d / 2).toFixed(2)}" aria-label="T${t.t} nose radius"></td>
      <td class="n"><select data-k="q" data-i="${i}" aria-label="T${t.t} orientation">${[0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map((n) => `<option value="${n}"${(t.q || 0) === n ? " selected" : ""}>${n}</option>`).join("")}</select></td>
      <td class="n"><select data-k="ins" data-i="${i}" aria-label="T${t.t} insert">${INSERTS.map(([v, l]) => `<option value="${v}"${(t.ins || "") === v ? " selected" : ""}>${l}</option>`).join("")}</select></td>
      <td class="n"><input data-k="w" data-i="${i}" type="number" step="0.1" min="0" value="${t.w || ""}" placeholder="—" aria-label="T${t.t} insert width"></td>
      <td class="n"><input data-k="cl" data-i="${i}" type="number" step="0.5" min="0" value="${t.cl || ""}" placeholder="—" aria-label="T${t.t} cutting edge length"></td>
      <td class="n"><input data-k="wear" data-i="${i}" type="number" step="0.001" value="${(+(t.wear || 0)).toFixed(3)}" aria-label="T${t.t} wear"></td>
      <td><input class="txt" data-k="c" data-i="${i}" value="${esc(t.c)}" aria-label="T${t.t} comment"></td></tr>`).join("") + "</tbody>";
}

// ------------------------------------------------------------ programs: X words as diameters, positions as radii
// mv(d, z, f): to diameter d at z, f mm/rev (0: rapid); the segments carry mm/min at the spindle's rpm
function LProg(name, rpm = 1200) {
  const p = { name, lines: [], segs: [], tools: new Set(), pos: { x: 20, y: 0, z: 10 }, curTool: 1, rpm };
  p.emit = (t) => { p.lines.push(t); return p.lines.length; };
  const here = () => ({ x0: p.pos.x, y0: 0, z0: p.pos.z, x: p.pos.x, y: 0, z: p.pos.z });
  const fmt = (v) => (+v).toFixed(3);
  p.seg = (x, z, f, ln) => { const a = p.pos; p.segs.push({ k: "mv", x0: a.x, y0: 0, z0: a.z, x, y: 0, z, f: f ? f * p.rpm : 0, rapid: !f, line: ln, tool: p.curTool }); p.pos = { x, y: 0, z }; };
  p.mv = (d, z, f) => {
    const a = p.pos, x = d / 2, w = (k, v, s) => (Math.abs(v - a[k]) > 1e-9 ? ` ${s}${fmt(k === "x" ? v * 2 : v)}` : "");
    const ln = p.emit((f ? "G1" : "G0") + w("x", x, "X") + w("z", z, "Z") + (f ? ` F${f}` : ""));
    p.seg(x, z, f, ln);
  };
  // an arc to diameter d at z (G2 / G3 with its radius), in short segments
  p.arc = (d, z, r, cw, f) => {
    const a = p.pos, x1 = d / 2, ln = p.emit(`${cw ? "G2" : "G3"} X${fmt(d)} Z${fmt(z)} R${fmt(r)} F${f}`);
    const mx = (a.x + x1) / 2, mz = (a.z + z) / 2, dx = x1 - a.x, dz = z - a.z, c = Math.hypot(dx, dz), h = Math.sqrt(Math.max(0, r * r - c * c / 4));
    const sgn = cw ? -1 : 1, cx = mx - sgn * h * dz / c, cz = mz + sgn * h * dx / c;  // in (x, z): G2 clockwise seen from +Y (G18)
    const a0 = Math.atan2(a.x - cx, a.z - cz); let a1 = Math.atan2(x1 - cx, z - cz), da = a1 - a0;
    if (cw && da > 0) da -= 2 * Math.PI; if (!cw && da < 0) da += 2 * Math.PI;
    const n = Math.max(4, Math.ceil(Math.abs(da) / 0.15));
    for (let k = 1; k <= n; k++) { const t = a0 + da * k / n; p.seg(cx + r * Math.sin(t), cz + r * Math.cos(t), f, ln); }
  };
  p.tc = (t) => { if (p.pos.x < 25) p.mv(50, p.pos.z, 0); if (p.pos.z < 20) p.mv(50, 20, 0); const ln = p.emit(`T${t} M6 G43`); p.segs.push({ k: "tc", tool: t, line: ln, ...here() }); p.curTool = t; p.tools.add(t); };
  p.m = (code, kind, extra = {}) => { const ln = p.emit(code); p.segs.push({ k: kind, line: ln, ...here(), ...extra }); };
  p.spin = (rpm, css) => { p.rpm = rpm; p.m(css ? `G96 D3000 S${css} M3` : `G97 S${rpm} M3`, "m3", { rpm }); };
  return p;
}
function lHead(p, title) { p.emit("%"); p.emit(`(${title})`); p.emit("G18 G7 G21 G90 G40 G54 G95"); }
function lTail(p) { p.mv(50, 20, 0); p.m("M5", "m5"); p.m("M9", "m9"); p.emit("M30"); p.emit("%"); }
// G76 in the simulation: its passes as moves (the infeed's area per pass constant, as LinuxCNC's R1)
function lThread(p, o) {
  const depth = o.depth || 0.613 * o.pitch, n = Math.max(3, Math.ceil(depth / 0.12)), k = 0.85 * depth / Math.sqrt(n);
  const ln = p.emit(`G76 P${o.pitch} Z${(+o.z).toFixed(3)} I${(-0.5).toFixed(3)} J${(0.1).toFixed(3)} R1.5 K${depth.toFixed(3)} Q29.5 H2 E${o.pitch} L2`);
  const x0 = o.d / 2 + 1, zs = p.pos.z;
  for (let i = 1; i <= n + 2; i++) {
    const dd = Math.min(depth, k * Math.sqrt(i));
    p.seg(o.d / 2 - dd, zs, 0, ln); p.seg(o.d / 2 - dd, o.z, o.pitch, ln); p.seg(x0, o.z, 0, ln); p.seg(x0, zs, 0, ln);
  }
}
function latheDemo() {
  const p = LProg("SHAFT-M20_OP1.ngc"); lHead(p, "SHAFT M20 · FACE, ROUGH, FINISH, GROOVE, THREAD, PART OFF");
  p.tc(1); p.spin(1400, 160); p.m("M8", "m8");
  p.emit("(FACE · T1)");
  p.mv(34, 0.5, 0); p.mv(-0.8, 0.5, 0.12); p.mv(34, 2, 0); p.mv(34, 0, 0); p.mv(-0.8, 0, 0.1); p.mv(34, 2, 0);
  p.emit("(ROUGH · T1 · Ø32 → Ø20.5 over 40, Ø26.5 over 15)");
  for (const d of [29, 26.5]) { p.mv(d, 2, 0); p.mv(d, -55, 0.22); p.mv(d + 1, -55, 0.22); p.mv(d + 1, 2, 0); }
  for (const d of [24, 22, 20.5]) { p.mv(d, 2, 0); p.mv(d, -40, 0.22); p.mv(26.5, -40, 0.22); p.mv(27.5, 2, 0); }
  p.tc(2); p.spin(2000, 220);
  p.emit("(FINISH · T2 · chamfer, Ø19.8, R2, Ø26, chamfer)");
  p.mv(15.8, 2, 0); p.mv(15.8, 0, 0.08); p.mv(19.8, -2, 0.08); p.mv(19.8, -38, 0.08); p.arc(23.8, -40, 2, false, 0.08); p.mv(25, -40, 0.08); p.mv(26, -40.5, 0.08);
  p.mv(26, -55, 0.08); p.mv(30, -55, 0.08); p.mv(34, 2, 0);
  p.tc(4); p.spin(900, 90);
  p.emit("(GROOVE · T4 · 2 mm, Ø16.5 at Z−40, the thread's run-out)");
  p.mv(28, -40, 0); p.mv(22, -40, 0); p.mv(16.5, -40, 0.04); p.mv(22, -40, 0); p.mv(28, 2, 0);
  p.tc(3); p.spin(700);
  p.emit("(THREAD · T3 · M20x1.5 over 36)");
  p.mv(22, 5, 0); lThread(p, { d: 19.8, pitch: 1.5, z: -36 });
  p.tc(4); p.spin(800, 80);
  p.emit("(PART OFF · T4 at Z−57)");
  p.mv(36, -57, 0); p.mv(-0.5, -57, 0.04); p.mv(36, -57, 0);
  lTail(p); return p;
}
// the wizards: face, turn (rough to a diameter over a length, then a finishing pass), thread, groove / part off
function latheFace(o) {
  const p = LProg(`FACE_D${o.d0}.ngc`); lHead(p, `FACE Ø${o.d0} → Ø${o.d1} · ${o.z} mm IN ${o.step} STEPS · WIZARD`);
  p.tc(o.t); p.spin(Math.round(o.v * 1000 / (Math.PI * o.d0)), o.v); p.m("M8", "m8");
  for (let z = o.z - o.step; ; z -= o.step) { const zz = Math.max(z, 0); p.mv(o.d0 + 2, zz + 1, 0); p.mv(o.d0 + 2, zz, 0); p.mv(o.d1 - 0.8, zz, o.f); p.mv(o.d1, zz + 1, 0); if (zz <= 0) break; }
  lTail(p); return p;
}
function latheTurn(o) {
  const p = LProg(`TURN_D${o.d1}x${o.l}.ngc`); lHead(p, `TURN Ø${o.d0} → Ø${o.d1} OVER ${o.l} · ${o.ap} mm CUTS · WIZARD`);
  p.tc(o.t); p.spin(Math.round(o.v * 1000 / (Math.PI * o.d0)), o.v); p.m("M8", "m8");
  const fin = o.d1 + 2 * o.fin;
  for (let d = o.d0 - 2 * o.ap; d > fin - 1e-9 || d + 2 * o.ap > fin + 1e-6; d -= 2 * o.ap) {
    const dd = Math.max(d, fin); p.mv(dd, 2, 0); p.mv(dd, -o.l, o.f); p.mv(dd + 1, -o.l, o.f); p.mv(dd + 1, 2, 0);
    if (dd <= fin) break;
  }
  p.mv(o.d1, 2, 0); p.mv(o.d1, -o.l, o.f / 2); p.mv(o.d0 + 1, -o.l, o.f / 2); p.mv(o.d0 + 1, 2, 0);
  lTail(p); return p;
}
function latheThreadProg(o) {
  const p = LProg(`THREAD_M${o.d}x${o.pitch}.ngc`); lHead(p, `THREAD M${o.d}x${o.pitch} OVER ${o.l} · WIZARD`);
  p.tc(o.t); p.spin(o.rpm); p.mv(o.d + 2, 5, 0); lThread(p, { d: o.d, pitch: o.pitch, z: -o.l });
  lTail(p); return p;
}
function latheGroove(o) {
  const p = LProg(`GROOVE_D${o.d1}_Z${o.z}.ngc`); lHead(p, `${o.d1 <= 0 ? "PART OFF" : "GROOVE"} Ø${o.d0} → Ø${o.d1} AT Z${o.z} · WIZARD`);
  p.tc(o.t); p.spin(Math.round(o.v * 1000 / (Math.PI * o.d0)), o.v); p.m("M8", "m8");
  for (let k = 0; k * o.wt < Math.max(o.w, o.wt) - 1e-6; k++) { const z = o.z - Math.min(k * o.wt, o.w - o.wt); p.mv(o.d0 + 2, z, 0); p.mv(o.d1 > 0 ? o.d1 : -0.5, z, o.f); p.mv(o.d0 + 2, z, 0); }
  lTail(p); return p;
}

// ------------------------------------------------------------ the wizard cards
(function latheCards() {
  const pane = $("p-wiz"); if (!pane) return;
  const card = (id, title, fields, note) => `<div class="card"><h3>${title} <span class="pill info">lathe</span></h3><div class="form">${fields.map(([k, l, v, step]) => `<label>${l}<input id="${id}-${k}" type="number" value="${v}" step="${step || 0.1}"></label>`).join("")}
    <label>Tool<select id="${id}-t" class="lt-tool"></select></label></div><p class="note">${note}</p><div class="row"><button class="btn accent" id="${id}-go">Make and load</button></div></div>`;
  const d = document.createElement("div"); d.className = "cards lathe-cards"; d.hidden = true;
  d.innerHTML = card("lf", "Facing", [["d0", "From Ø", 34], ["d1", "To Ø", 0], ["z", "Stock on the face", 1, 0.1], ["step", "Per pass", 0.5, 0.05], ["f", "Feed mm/rev", 0.12, 0.01], ["v", "Cutting speed m/min", 150, 5]], "From the outside in at constant cutting speed (G96), the face at Z0.") +
    card("lt", "Turning", [["d0", "Stock Ø", 32], ["d1", "To Ø", 20], ["l", "Length", 40, 1], ["ap", "Cut depth (radius)", 1.0, 0.1], ["fin", "Finish allowance", 0.2, 0.05], ["f", "Feed mm/rev", 0.2, 0.01], ["v", "Cutting speed m/min", 160, 5]], "Roughing passes along Z to the allowance, then a finishing pass at half the feed.") +
    card("lth", "Threading", [["d", "Major Ø", 20], ["pitch", "Pitch", 1.5, 0.05], ["l", "Length", 30, 1], ["rpm", "Spindle rpm", 700, 10]], "G76, 29.5° compound infeed; the spindle's encoder keeps the passes in step.") +
    card("lg", "Grooving · parting", [["d0", "Stock Ø", 32], ["d1", "To Ø (0: part off)", 16], ["z", "At Z", -40, 0.5], ["w", "Groove width", 4, 0.1], ["wt", "Insert width", 2, 0.1], ["f", "Feed mm/rev", 0.04, 0.005], ["v", "Cutting speed m/min", 90, 5]], "Plunges across the width, each a full depth.");
  pane.appendChild(d);
  const n = (id) => +$(id).value, go = (p) => { if (loadProgram(plan(p))) showPage("main"); };
  $("lf-go").addEventListener("click", () => go(latheFace({ d0: n("lf-d0"), d1: n("lf-d1"), z: n("lf-z"), step: Math.max(0.05, n("lf-step")), f: n("lf-f"), v: n("lf-v"), t: n("lf-t") })));
  $("lt-go").addEventListener("click", () => go(latheTurn({ d0: n("lt-d0"), d1: n("lt-d1"), l: n("lt-l"), ap: Math.max(0.05, n("lt-ap")), fin: n("lt-fin"), f: n("lt-f"), v: n("lt-v"), t: n("lt-t") })));
  $("lth-go").addEventListener("click", () => go(latheThreadProg({ d: n("lth-d"), pitch: n("lth-pitch"), l: n("lth-l"), rpm: n("lth-rpm"), t: n("lth-t") })));
  $("lg-go").addEventListener("click", () => go(latheGroove({ d0: n("lg-d0"), d1: n("lg-d1"), z: n("lg-z"), w: n("lg-w"), wt: Math.max(0.1, n("lg-wt")), f: n("lg-f"), v: n("lg-v"), t: n("lg-t") })));
})();
function latheToolSelects() {
  const opts = st.tools.map((t) => `<option value="${t.t}">T${t.t} · ${esc(t.c)}</option>`).join("");
  for (const [id, def] of [["lf-t", 1], ["lt-t", 1], ["lth-t", 3], ["lg-t", 4]]) { const s = $(id); if (!s) continue; const v = s.value || def; s.innerHTML = opts; s.value = toolOf(+v) ? String(v) : String(st.tools[0] ? st.tools[0].t : 1); }
}
