/* Portal Mill Console: a lathe. machine.json's type "lathe" (LinuxCNC's [DISPLAY] LATHE) turns the console
   into a lathe's: X (the cross slide, shown as a diameter in G7) and Z (along the spindle), the tools on a
   post in front of the spindle's axis (+X toward the operator) or behind it, a turret's tools by their
   orientation, the stock turning in the chuck, lathe programs and wizards (face, turn, thread, groove).

   The view: the spindle's axis along scene x through y 0, z 0, the chuck's face at x 0; the work's X0 is the
   axis, its Z0 the stock's face (LATHE.face along x); scene y = -X for a front post, +X for a back one, so
   the top view is the operator's (AXIS's). Positions are radii, as LinuxCNC keeps them: only the DRO and the
   G-code's X words are diameters. Classic script after machmodel.js; app.js calls in where R.type is "lathe". */
"use strict";

let LATHE = null;  // { post, dia, face, stockD, stockL, s (scene y per +X) } while the console is a lathe's
let latheSaved = null;  // the mill's own state while a lathe's replaces it (the simulation)

// ------------------------------------------------------------ entering and leaving
function latheEnter(o = {}) {
  const was = !!LATHE;
  LATHE = { post: o.post === "back" ? "back" : "front", dia: o.dia !== false, face: 60, stockD: 32, stockL: 60, spin: 0 };
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
  // see-through, and not in the depth buffer: the toolpath inside it shows
  const stock = cylX(16, 60, mat.stock, 30); stock.material = mat.stock.clone(); stock.material.transparent = true; stock.material.opacity = 0.4; stock.material.depthWrite = false;
  const stockG = new THREE.Group(); stockG.add(stock); machine.add(stockG);  // the stock: shown with a model's parts too
  const carriage = new THREE.Group(), ram = new THREE.Group(), headG = new THREE.Group(), toolG = new THREE.Group(), coolant = new THREE.Group(), bridge = new THREE.Group();
  machine.add(bridge); bridge.add(carriage); carriage.add(ram); ram.add(headG); headG.add(toolG);
  // the saddle on the ways, the cross slide on it (toward the operator for a front post), the tool post
  carriage.add(box(110, 200, 30, mat.frame, 30, 0, -100));
  ram.add(box(90, 120, 22, mat.dark, 30, s * 70, -74));
  ram.add(box(46, 46, 60, mat.dark, 30, s * 40, -32));
  const noz = R.cyl(2, 24, mat.accent, 0, s * 14, 14, 8); noz.rotation.x = s * 0.7; coolant.add(noz);  // aimed at the tip
  ram.add(coolant);
  R.spinG = spinG; R.stock = stock; R.stockG = stockG;
  return { bridge, carriage, ram, headG, toolG, coolant };
}
function latheStock() {
  if (!R || !R.stock || !LATHE) return;
  R.stock.scale.set(LATHE.stockL / 60, LATHE.stockD / 32, LATHE.stockD / 32);
  R.stock.position.x = LATHE.stockL / 2;
  LATHE.face = LATHE.stockL;
}

// every frame: the slides at the tool, the stock turning, the trail and chips, the camera
function latheUpdate3d(dt, tip, moving, cutting) {
  const [tx, ty] = latheVp(tip.x, 0, tip.z);
  R.carriage.position.set(tx, 0, 0); R.ram.position.set(0, ty, 0); R.headG.position.set(0, 0, 0);
  LATHE.spin = (LATHE.spin + st.spindle.rpm / 60 * 2 * Math.PI * dt * (st.spindle.dir || 1) * 0.15) % (2 * Math.PI);
  if (R.spinG) R.spinG.rotation.x = LATHE.spin;
  if (R.stockG) R.stockG.rotation.x = LATHE.spin;
  R.machine.visible = st.show.machine; R.workG.position.set(0, 0, 0); R.objGroup.visible = false;
  R.triad.position.set(LATHE.face, 0, 0); R.triad.rotation.z = 0;
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
// LinuxCNC's orientation (Q, 1-9): where the tool's controlled point is on its nose, seen with +Z to the right
// and +X up (a back post's view; a front post mirrors it). The body lies the other way.
const Q_DIR = { 1: [1, 1], 2: [1, -1], 3: [-1, -1], 4: [-1, 1], 5: [0, 1], 6: [1, 0], 7: [0, -1], 8: [-1, 0], 9: [0, 0] };
const LATHE_TOOLS = [
  { t: 1, p: 1, x: 0, l: 0, d: 0.8, q: 3, wear: 0, life: 90, c: "CCMT 09 R0.4 · OD turning, 80°" },
  { t: 2, p: 2, x: 1.2, l: -2.5, d: 0.4, q: 3, wear: 0, life: 100, c: "DCMT 07 R0.2 · OD finishing, 55°" },
  { t: 3, p: 3, x: 0.6, l: 1.5, d: 0.2, q: 8, wear: 0, life: 100, c: "16ER 60° · OD threading" },
  { t: 4, p: 4, x: 0.4, l: 3.0, d: 0.2, q: 3, wear: 0, life: 85, c: "2 mm · grooving and parting" },
  { t: 5, p: 5, x: -0.8, l: 0.5, d: 0.4, q: 1, wear: 0, life: 100, c: "boring bar Ø10 · ID turning" },
  { t: 6, p: 6, x: 0, l: 20, d: 6, q: 7, wear: 0, life: 100, c: "6 mm drill · on the centre" },
];
function latheBuildTool() {
  const t = toolOf(st.tool) || { d: 0.8, q: 3, c: "" };
  while (R.toolG.children.length) R.toolG.remove(R.toolG.children[0]);
  const s = LATHE.s, q = Q_DIR[t.q || 3] || [-1, -1], m = R.mat;
  if (t.q === 7 || t.q === 5) {  // a drill or a centre tool on the axis, along Z
    const g = new THREE.CylinderGeometry(t.d / 2, t.d / 2, 60, 16); g.rotateZ(Math.PI / 2);
    const dr = new THREE.Mesh(g, m.tool); dr.position.set(30 * (t.q === 7 ? 1 : -1), 0, 0); R.toolG.add(dr);
    return;
  }
  // the insert: a flat rhombus (80°, 55° for a finishing tool, 60° for a thread) in the XZ plane, its tip on the
  // controlled point, its bisector toward the body (away from the nose's point)
  const bz = -q[1], bxr = -q[0], bl = Math.hypot(bz, bxr) || 1, b = [bz / bl, s * bxr / bl];  // in the scene's x, y
  const ang = (/55°/.test(t.c) ? 55 : /60°|thread/i.test(t.c) ? 60 : 80) * Math.PI / 180 / 2, L = 9;
  const rot = (v, a) => [v[0] * Math.cos(a) - v[1] * Math.sin(a), v[0] * Math.sin(a) + v[1] * Math.cos(a)];
  const p1 = rot(b, ang).map((v) => v * L), p3 = rot(b, -ang).map((v) => v * L), p2 = b.map((v) => v * 2 * L * Math.cos(ang));
  const sh = new THREE.Shape(); sh.moveTo(0, 0); sh.lineTo(p1[0], p1[1]); sh.lineTo(p2[0], p2[1]); sh.lineTo(p3[0], p3[1]); sh.closePath();
  const g = new THREE.ExtrudeGeometry(sh, { depth: 4, bevelEnabled: false }); g.translate(0, 0, -2);
  R.toolG.add(new THREE.Mesh(g, m.accent));
  // the holder: a 12 mm shank from behind the insert to the post: across (along X) for an OD tool, along Z
  // for a boring bar (an inside tool's body can't reach across the axis)
  const inside = bxr < 0, c = p2.map((v) => v * 0.6), len = 60;
  if (inside) R.toolG.add(R.box(len, 10, 10, m.dark, c[0] + len / 2 * Math.sign(bz || 1), c[1], 0));
  else R.toolG.add(R.box(12, len, 12, m.dark, c[0], c[1] + s * len / 2, 0));
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
  $("tool-t").innerHTML = `<thead><tr><th>T</th><th class="n">Pocket</th><th class="n">X off</th><th class="n">Z off</th><th class="n">Nose R</th><th class="n">Q</th><th class="n">Wear</th><th>Comment</th></tr></thead><tbody>` +
    st.tools.map((t, i) => `<tr class="${t.t === st.tool ? "active" : t.t === st.toolSel ? "sel" : ""}" data-t="${t.t}"><td class="num">T${t.t}${t.t === st.tool ? " ● in the turret" : ""}</td>
      <td class="n">${t.p}</td>
      <td class="n"><input data-k="x" data-i="${i}" type="number" step="0.001" value="${(+(t.x || 0)).toFixed(3)}" aria-label="T${t.t} X offset"></td>
      <td class="n"><input data-k="l" data-i="${i}" type="number" step="0.001" value="${(+(t.l || 0)).toFixed(3)}" aria-label="T${t.t} Z offset"></td>
      <td class="n"><input data-k="d" data-scale="2" data-i="${i}" type="number" step="0.1" value="${(t.d / 2).toFixed(2)}" aria-label="T${t.t} nose radius"></td>
      <td class="n"><select data-k="q" data-i="${i}" aria-label="T${t.t} orientation">${[0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map((n) => `<option value="${n}"${(t.q || 0) === n ? " selected" : ""}>${n}</option>`).join("")}</select></td>
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
