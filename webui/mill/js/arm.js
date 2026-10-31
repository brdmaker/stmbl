/* Portal Mill Console: an arm. The server's machine type "arm" (LinuxCNC with genserkins, armkins, ...: joints that
   aren't axes) turns the console into an arm's, as linuxcnc/arm6hmi's QtVCP screen:
   - the 3D arm from its model files (the server's arm_model, armmodel.js: linuxcnc/vis3d's .vis, the arm's .arm,
     armguard's cell, the tool table), posed from LinuxCNC's joints; the tools on the flange or in their docks; the
     cell's obstacles coloured by the guard (green clear, amber near, red stopped);
   - the position as X Y Z A B C; the joints (within their limits, following error, homed); the jog pad by joint
     (joint mode) or by axis (world mode); the guard, the gripper, the power quota, Record point (teach);
   - the Tools page's tool changer: each tool in its dock or on the flange, Mount (T<n> M6), Put back (T0 M6).
   The simulation: the ARC Mate with its tool changer (arm_demo.js), its joints from the program's tool point by IK.
   Classic script after armmodel.js and machmodel.js; app.js calls in where R.type is "arm". */
"use strict";

let ARM = null;  // {v (the model), q [deg], abc, mounted, frame ("joint" | "world"), held, ...} while the console is an arm's
let armSaved = null;  // the mill's own state while an arm's replaces it (the simulation)
const ARM_HOME = [0, -90, 0, 0, -90, 180];  // the ARC Mate's home (genserkins' theta): flange up, X 799 Z 1271
const ARM_GRIP = ["OPEN", "CLOSING", "GRIPPED", "WRONG PART", "EMPTY", "OPENING", "LOST"];  // armgrip.0.state
const ARM_CHG = ["", "lock sensors timeout", "wrong tool", "tool lost", "unlock away from the dock", "moving on another tool's TCP"];
const ARM_GUARD = ["", "collision", "protective stop"];

// ------------------------------------------------------------ entering and leaving
function armEnter() {
  const was = !!ARM;
  ARM = { v: null, q: ARM_HOME.slice(), abc: [0, 0, 0], mounted: 0, frame: "joint", held: null, lastTip: null, reach: false, teach: 0, grip: { state: 0, gap: 75 }, built: false };
  document.body.classList.add("arm");
  if (!was && !LIVE) {  // the simulation: an arm's tools, offsets, travel and program
    armSaved = { tools: st.tools, offsets: st.offsets.map((x) => ({ ...x })), travel: JSON.parse(JSON.stringify(TRAVEL)), library: library.slice(), prog, tool: st.tool, pos: { ...st.pos }, g92: { ...st.g92 } };
    st.tools = [{ t: 1, p: 1, d: 0, l: 0, wear: 0, life: 100, c: "torch" }, { t: 2, p: 2, d: 0, l: 0, wear: 0, life: 100, c: "gripper" }];
    st.offsets.forEach((x) => { x.x = 0; x.y = 0; x.z = 0; });
    st.g92 = { x: 0, y: 0, z: 0 }; st.tool = 0; st.toolSel = 1;
    Object.assign(TRAVEL, { x: [-2000, 2000], y: [-2000, 2000], z: [-200, 2200] });
    stopAll(); st.motion = null; st.run = "idle";
    library.length = 0; library.push(plan(armDemo()));
    prog = library[0];
  }
  armUi(true);
  if (!was && typeof renderProgram === "function") { renderProgram(); fillToolSelects(); rebuildPath(); }
  armLoad();
}
function armExit() {
  if (!ARM) return;
  ARM = null;
  document.body.classList.remove("arm");
  if (armSaved) {
    st.tools = armSaved.tools; armSaved.offsets.forEach((x, i) => Object.assign(st.offsets[i], x));
    Object.assign(TRAVEL, armSaved.travel); library.length = 0; library.push(...armSaved.library);
    prog = armSaved.prog; st.tool = armSaved.tool; st.pos = armSaved.pos; st.g92 = armSaved.g92; armSaved = null;
    renderProgram(); fillToolSelects();
  }
  armUi(false);
  rebuildPath();
}
// the model: the machine's (arm_model), the simulation's (arm_demo.js)
async function armLoad() {
  let b = null;
  if (LIVE) {
    const r = await LIVE.armModel();
    if (!r || !r.ok) { msg(`The arm's 3D model: ${(r && r.err) || "not loaded"}`, "warn"); return; }
    b = r;
  } else b = ARM_DEMO;
  if (!ARM) return;
  ARM.v = ARMMODEL.load(b);
  ARM.files = b.files;
  if (ARM.v.errors.length) msg(`The arm's model: ${ARM.v.errors.slice(0, 3).join("; ")}`, "warn");
  if (!LIVE) armSimPose();
  armBuildScene();
  armRenderPanel();
  if (st.page === "tool") renderTools();
}

// the console's arm bits: the DRO's A B C, the joints panel, the jog pad, the pages an arm has no use for
function armUi(on) {
  const brand = document.querySelector(".brand span");
  if (brand) { if (!brand.dataset.mill2) brand.dataset.mill2 = brand.textContent; brand.textContent = on ? "arm · 6 joints · stmbl · LinuxCNC 2.9" : brand.dataset.mill2; }
  for (const p of ["probe", "dyn"]) { const b = document.querySelector(`#nav button[data-p="${p}"]`); if (b) b.hidden = on; }
  // the DRO: A B C under X Y Z
  $$("#dro .axis.abc").forEach((e) => e.remove());
  if (on) $("dro").insertAdjacentHTML("beforeend", ["a", "b", "c"].map((a) => `
    <div class="axis abc"><span class="n">${a.toUpperCase()}</span><span class="h" id="h-${a}"></span>
      <span class="vals"><span class="v" id="d-${a}">0.000</span><span class="sub" id="ds-${a}">°</span></span><span class="acts"></span><span class="fe"><b></b></span></div>`).join(""));
  let panel = $("arm-panel");
  if (on && !panel) {
    panel = document.createElement("div"); panel.className = "panel"; panel.id = "arm-panel";
    panel.innerHTML = `<h2>Joints <small id="arm-jmode">—</small></h2><div class="armj" id="arm-joints"></div>
      <dl class="kv armkv"><dt>Guard</dt><dd><span class="pill off" id="arm-guard">—</span> <span id="arm-clear">—</span></dd>
        <dt>Gripper</dt><dd id="arm-grip">—</dd><dt>Power</dt><dd id="arm-quota">—</dd><dt>Teach</dt><dd id="arm-teach">0 points</dd></dl>
      <div class="row"><button class="btn sm" id="arm-gopen">Open</button><button class="btn sm" id="arm-gclose">Close</button><span class="spacer"></span><button class="btn sm accent" id="arm-rec" title="The pose as a G1 line at the end of the teach file">Record point</button></div>`;
    const first = document.querySelector("aside.ctl .panel");
    first.after(panel);
  }
  if (panel) panel.hidden = !on;
  // the jog pad: joints or axes
  let pad = $("armpad");
  if (on && !pad) {
    pad = document.createElement("div"); pad.id = "armpad"; pad.className = "armpad";
    $("jogpad").after(pad);
  }
  if (pad) pad.hidden = !on;
  if (on) armRenderPad();
}
function armRenderPad() {
  const pad = $("armpad"); if (!pad || !ARM) return;
  const n = 6, joint = ARM.frame === "joint";
  pad.innerHTML = `<div class="seg" id="armpad-frame" role="group" aria-label="Jog frame"><button data-f="joint" aria-pressed="${joint}">JOINT</button><button data-f="world" aria-pressed="${!joint}">WORLD</button></div>` +
    [...Array(n)].map((_, i) => { const ax = joint ? "j" + i : "xyzabc"[i], lab = joint ? "J" + (i + 1) : ax.toUpperCase();
      return `<div class="armrow"><b>${lab}</b><button data-ax="${ax}" data-s="-1" aria-label="${lab} minus">${lab}−</button><button data-ax="${ax}" data-s="1" aria-label="${lab} plus">${lab}+</button></div>`; }).join("");
}

// ------------------------------------------------------------ the scene
// the built-in groups update3d's callers expect; the arm itself is built once its model is here
function armBuild(machine) {
  const g = () => new THREE.Group(), bridge = g(), carriage = g(), ram = g(), headG = g(), toolG = g(), coolant = g();
  machine.add(bridge); bridge.add(carriage); carriage.add(ram); ram.add(headG); headG.add(toolG); ram.add(coolant);
  // the floor: a disc, the reach's ring
  const floor = new THREE.Mesh(new THREE.CircleGeometry(2200, 64), new THREE.MeshStandardMaterial({ color: 0x1b2128, roughness: 0.9 }));
  floor.position.z = ZTOP - 1; machine.add(floor);
  const root = new THREE.Group(); root.position.z = ZTOP; root.scale.setScalar(1000); machine.add(root);
  R.armRoot = root;
  if (ARM) { ARM.built = false; if (ARM.v) armBuildScene(); }  // its model came first (the simulation's)
  return { bridge, carriage, ram, headG, toolG, coolant };
}
function armMat(name) {
  const m = (ARM.v.mats[name]) || { color: [0.7, 0.72, 0.75], metal: 0.2, rough: 0.5, emissive: 0, alpha: 1, accent: false };
  const accent = new THREE.Color(0x4cc9ff), col = m.accent ? accent : new THREE.Color(...m.color);
  return new THREE.MeshStandardMaterial({ color: col, metalness: m.metal, roughness: m.rough, emissive: m.accent ? accent : col, emissiveIntensity: m.accent ? 0.9 : m.emissive,
    transparent: m.alpha < 1, opacity: m.alpha, depthWrite: m.alpha >= 1, side: THREE.DoubleSide });
}
function armGeom(p) {
  if (p.shape === "box") return new THREE.BoxGeometry(...p.size);
  const lathe = (pts) => { const g = new THREE.LatheGeometry(pts.map(([x, y]) => new THREE.Vector2(x, y)), 32); g.rotateX(Math.PI / 2); return g; };
  if (p.shape === "tube") { const h = p.h / 2; return lathe([[p.ri, -h], [p.r, -h], [p.r, h], [p.ri, h], [p.ri, -h]]); }
  if (p.shape === "capsule") {  // a profile: the bottom half-sphere, the side, the top half-sphere
    const pts = [], h = p.len / 2;
    for (let k = 0; k <= 8; k++) { const t = -Math.PI / 2 + k * Math.PI / 16; pts.push([p.r * Math.cos(t), -h + p.r * Math.sin(t)]); }
    for (let k = 0; k <= 8; k++) { const t = k * Math.PI / 16; pts.push([p.r * Math.cos(t), h + p.r * Math.sin(t)]); }
    pts[0][0] = 0; pts[pts.length - 1][0] = 0;
    return lathe(pts);
  }
  if (p.shape === "mesh") {
    const f = ARM.files[p.file];
    if (!f || !f.b64) return null;
    const bin = atob(f.b64), buf = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) buf[i] = bin.charCodeAt(i);
    try { return parseModel(p.file, buf.buffer); } catch (e) { msg(`${p.file}: ${e.message}`, "warn"); return null; }
  }
  return null;
}
function armMesh(p, matCache) {
  const g = armGeom(p); if (!g) return null;
  const mat = matCache[p.mat] || (matCache[p.mat] = armMat(p.mat));
  const m = new THREE.Mesh(g, mat); m.matrixAutoUpdate = false; m.matrix.fromArray(p.M);
  return m;
}
function armBuildScene() {
  if (!R || R.type !== "arm" || !ARM || !ARM.v || !R.armRoot) return;
  const root = R.armRoot, v = ARM.v, mats = {};
  root.clear();
  ARM.links = [];
  for (let j = 0; j <= v.n; j++) { const L = new THREE.Group(); L.matrixAutoUpdate = false; root.add(L); ARM.links.push(L); }
  for (const p of v.parts) { const m = armMesh(p, mats); if (m) (ARM.links[Math.min(p.link, v.n)] || root).add(m); }
  // the tools: each a group in the flange's frame, on the flange or at its dock
  ARM.toolG = {};
  for (const t of Object.values(v.tools)) {
    const g = new THREE.Group(); g.matrixAutoUpdate = false;
    for (const p of (t.parts.length ? t.parts : t.bodies)) { const m = armMesh(p, mats); if (m) g.add(m); }
    ARM.toolG[t.id] = g;
    if (t.dock) {  // its stand under the dock: a fork plate, a post
      const at = ARMMODEL.point(t.dock, [0, 0, 0.09]), top = at[2] - 0.05, sm = mats.stand || (mats.stand = armMat("stand"));
      const plate = new THREE.Mesh(new THREE.BoxGeometry(0.11, 0.11, 0.012), sm); plate.position.set(at[0], at[1], top - 0.006); root.add(plate);
      if (top > 0.02) { const post = new THREE.Mesh(new THREE.CylinderGeometry(0.025, 0.025, top - 0.012, 16), sm); post.rotation.x = Math.PI / 2; post.position.set(at[0], at[1], (top - 0.012) / 2); root.add(post); }
    }
  }
  // the cell's obstacles: see-through, their edges; coloured by the guard
  ARM.obsMat = new THREE.MeshStandardMaterial({ color: 0x5fe0a0, transparent: true, opacity: 0.22, depthWrite: false, side: THREE.DoubleSide });
  ARM.obsEdge = new THREE.LineBasicMaterial({ color: 0x5fe0a0, transparent: true, opacity: 0.7 });
  for (const o of v.obs) {
    const g = armGeom(o); if (!g) continue;
    const m = new THREE.Mesh(g, ARM.obsMat); m.matrixAutoUpdate = false; m.matrix.fromArray(o.M);
    m.add(new THREE.LineSegments(new THREE.EdgesGeometry(g, 30), ARM.obsEdge));
    root.add(m);
  }
  // the tool point
  ARM.tcpMark = new THREE.Mesh(new THREE.SphereGeometry(0.008, 12, 8), new THREE.MeshBasicMaterial({ color: 0xffb547, depthTest: false }));
  ARM.tcpMark.renderOrder = 11;
  ARM.mountedShown = -1;
  ARM.built = true;
}
// the mounted tool's tool point [m, flange frame] (the model's own without one)
function armTcp() {
  const t = ARM && ARM.v && ARM.v.tools[armMounted()];
  return t && t.tcp ? t.tcp : ARM && ARM.v ? ARM.v.tcp : [0, 0, 0];
}
const armMounted = () => (LIVE ? (st.arm && st.arm.tool ? Math.round(st.arm.tool.mounted) : st.tool) : ARM.mounted);
const armQ = () => (LIVE && st.armJ ? st.armJ : ARM.q);

// every frame: the links from the joints, the tools, the guard's colours, the trail, the camera
function armUpdate3d(dt, tip, moving, cutting) {
  R.machine.visible = st.show.machine;
  R.pathLine.visible = st.show.path; R.trail.visible = st.show.path; R.limits.visible = st.show.limits; R.coolant.visible = false;
  if (R.pathMirror) R.pathMirror.visible = false;
  if (!ARM || !ARM.built) { view3d(dt, [0, 0, ZTOP + 1000], [0, 0, ZTOP + 800]); return; }
  if (!LIVE) armSimStep(dt, tip);
  const v = ARM.v, F = ARMMODEL.frames(v, armQ());
  ARM.links.forEach((L, j) => { L.matrix.fromArray(F[j]); L.matrixWorldNeedsUpdate = true; });
  // the tools: the mounted one on the flange, the others in their docks
  const mt = armMounted();
  if (mt !== ARM.mountedShown) {
    for (const [id, g] of Object.entries(ARM.toolG)) {
      const t = v.tools[id], on = +id === mt;
      if (g.parent) g.parent.remove(g);
      if (on) { g.matrix.identity(); ARM.links[v.n].add(g); }
      else if (t.dock) { g.matrix.fromArray(t.dock); R.armRoot.add(g); }
    }
    ARM.links[v.n].add(ARM.tcpMark);
    ARM.mountedShown = mt;
  }
  ARM.tcpMark.position.set(...armTcp());
  // the guard: stopped red, near amber (under twice its limit), else green
  const gd = st.arm && st.arm.guard, prevent = v.prevent || 10;
  const col = gd && gd.state ? 0xff4d5e : gd && gd.clearance != null && gd.clearance < 2 * prevent ? 0xffb547 : 0x5fe0a0;
  ARM.obsMat.color.setHex(col); ARM.obsEdge.color.setHex(col);
  // the tool point in the scene (mm), the trail
  const P = ARMMODEL.point(ARMMODEL.mul(F[v.n], ARMMODEL.T(...armTcp())), [0, 0, 0]), sp = [P[0] * 1000, P[1] * 1000, P[2] * 1000 + ZTOP];
  if (moving && cutting) {
    const p = R.trailPos;
    if (R.trailLen < R.trailN) { p.set(sp, R.trailLen * 3); R.trailLen++; } else { p.copyWithin(0, 3); p.set(sp, (R.trailN - 1) * 3); }
    R.trailGeo.attributes.position.needsUpdate = true; R.trailGeo.setDrawRange(0, R.trailLen);
  }
  view3d(dt, sp, [0, 0, ZTOP + 800]);
}
function armCam(kind) {
  const z = ZTOP, f = ARM && ARM.built ? (() => { const P = ARMMODEL.pose(ARM.v, armQ(), armTcp()).xyz; return [P[0], P[1], P[2] + z]; })() : [800, 0, z + 1200];
  return {
    iso: [[2600, -2600, z + 2200], [300, 0, z + 700]], top: [[300, -1, z + 4200], [300, 0, z]], front: [[300, -3800, z + 900], [300, 0, z + 800]],
    side: [[3800, 0, z + 900], [0, 0, z + 800]], part: [[f[0] + 700, f[1] - 900, f[2] + 600], f], follow: [[f[0] + 700, f[1] - 900, f[2] + 600], f],
  }[kind];
}

// ------------------------------------------------------------ the simulation: joints by IK from the program's tool point
function armSimPose() {  // the tool point (DRO, st.pos) from the joints
  const p = ARMMODEL.pose(ARM.v, ARM.q, armTcp());
  st.pos = { x: p.xyz[0], y: p.xyz[1], z: p.xyz[2] }; ARM.abc = p.abc; ARM.lastTip = { ...st.pos };
}
function armSimStep(dt, tip) {
  const v = ARM.v;
  if (ARM.held) {  // a held jog key
    const h = ARM.held, rate = st.jogRate / 60 * dt * h.s;
    if (h.ax[0] === "j") armSimJoint(+h.ax.slice(1), rate);
    else armSimWorld(h.ax, rate);
    return;
  }
  const t = st.pos, L = ARM.lastTip;
  if (L && Math.abs(t.x - L.x) < 1e-6 && Math.abs(t.y - L.y) < 1e-6 && Math.abs(t.z - L.z) < 1e-6) return;
  const s = ARMMODEL.ik(v, { xyz: [t.x, t.y, t.z], abc: ARM.abc }, ARM.q, armTcp(), 60);
  if (s.ok && armInLimits(s.q)) { ARM.q = s.q; ARM.reach = false; }
  else if (!ARM.reach) { ARM.reach = true; msg(`X${t.x.toFixed(1)} Y${t.y.toFixed(1)} Z${t.z.toFixed(1)}: out of the arm's reach or its joints' limits`, "warn"); }
  ARM.lastTip = { ...t };
}
// the joints' limits as the joint values are: LinuxCNC's [JOINT_n] (live); the model's (arm_sim's: offsets from
// theta0) moved by theta0 when the joint values are the DH theta
function armLimit(j) {
  const lim = LIVE && LIVE.info().arm && LIVE.info().arm.jlim;
  if (lim && lim[j]) return [lim[j].min, lim[j].max];
  const l = ARM && ARM.v && ARM.v.lim[j], t0 = ARM && ARM.v && ARM.v.q_theta && ARM.v.dh[j] ? ARM.v.dh[j].theta0 * 180 / Math.PI : 0;
  return l ? [l[0] + t0, l[1] + t0] : [-180, 180];
}
function armInLimits(q) { return q.every((x, j) => { const l = armLimit(j); return x >= l[0] - 1e-6 && x <= l[1] + 1e-6; }); }
function armSimJoint(j, d) {
  const q = ARM.q.slice(); q[j] += d;
  if (!armInLimits(q)) { msg(`J${j + 1}: at its limit`, "warn"); ARM.held = null; return; }
  ARM.q = q; armSimPose();
}
function armSimWorld(ax, d) {
  if (/[abc]/.test(ax)) {
    const abc = ARM.abc.slice(); abc["abc".indexOf(ax)] += d;
    const s = ARMMODEL.ik(ARM.v, { xyz: [st.pos.x, st.pos.y, st.pos.z], abc }, ARM.q, armTcp(), 25);
    if (s.ok && armInLimits(s.q)) { ARM.q = s.q; ARM.abc = abc; armSimPose(); } else { msg(`${ax.toUpperCase()}: out of reach`, "warn"); ARM.held = null; }
    return;
  }
  st.pos = { ...st.pos, [ax]: st.pos[ax] + d };
}

// ------------------------------------------------------------ the jog pad, the panel's buttons
function armJog(ax, s) {
  if (LIVE) return LIVE.jogAxis(ax, s);
  if (!ready("jog")) return;
  if (busy()) { msg("Jogging is off while the program runs.", "warn"); return; }
  if (st.jogInc === 0) { ARM.held = { ax, s }; return; }
  if (ax[0] === "j") armSimJoint(+ax.slice(1), s * st.jogInc); else armSimWorld(ax, s * st.jogInc);
}
document.addEventListener("pointerdown", (e) => {
  if (!ARM) return;
  const b = e.target.closest && e.target.closest("#armpad button");
  if (!b) return;
  if (b.dataset.f) { ARM.frame = b.dataset.f; armRenderPad(); return; }
  armJog(b.dataset.ax, +b.dataset.s);
});
addEventListener("pointerup", () => { if (ARM) ARM.held = null; });
document.addEventListener("click", (e) => {
  if (!ARM || !e.target.closest) return;
  const id = e.target.id, info = LIVE ? LIVE.info().arm || {} : {}, grip = info.grip || { open: "M65 P0", close: "M64 P0" };
  if (id === "arm-rec") {
    if (LIVE) { LIVE.teach(st.jogRate).then((r) => { if (r && r.ok) { ARM.teach = r.points; armRenderPanel(); } }); return; }
    const p = ARMMODEL.pose(ARM.v, ARM.q, armTcp()), line = "G1 " + [...p.xyz, ...p.abc].map((x, i) => "XYZABC"[i] + x.toFixed(3)).join(" ") + " F" + Math.round(st.jogRate);
    ARM.teach++; armRenderPanel(); msg(`Recorded ${line} (${ARM.teach} points in teach.ngc)`, "ok");
  } else if (id === "arm-gopen" || id === "arm-gclose") {
    const close = id === "arm-gclose";
    if (LIVE) { LIVE.mdi(close ? grip.close : grip.open); return; }
    ARM.grip = { state: close ? 4 : 0, gap: close ? 0 : 75 }; armRenderPanel(); msg(`Gripper ${close ? "closed: EMPTY (nothing between its jaws)" : "open"}`, "info");
  } else if (e.target.dataset && e.target.dataset.armhome !== undefined) {
    const j = +e.target.dataset.armhome;
    if (LIVE) LIVE.home("J" + j); else msg(`J${j + 1} homed (absolute encoder)`, "ok");
  } else if (e.target.dataset && e.target.dataset.mount !== undefined) armMount(+e.target.dataset.mount);
});
function armMount(t) {
  if (LIVE) { const ready = (LIVE.info().arm || {}).tool_ready; if (ready && t) LIVE.mdi(ready); LIVE.mdi(`T${t} M6`); return; }
  ARM.mounted = t; st.tool = t; buildTool(); armSimPose(); renderTools();
  msg(t ? `T${t} M6: ${ARM.v.tools[t].name} on the flange` : "T0 M6: the tool back in its dock", "ok");
}

// ------------------------------------------------------------ the panels
function armDro(show, mach) {
  const abc = LIVE ? (st.armAx ? st.armAx.slice(3, 6) : [0, 0, 0]) : ARM ? ARM.abc : [0, 0, 0];
  ["a", "b", "c"].forEach((a, i) => { const d = $("d-" + a), x = +abc[i] || 0; if (d) d.textContent = (Math.abs(x) < 5e-4 ? 0 : x).toFixed(3) + "°"; });
  armRenderPanel();
}
let armPanelAt = 0;
function armRenderPanel() {
  if (!ARM || !$("arm-joints")) return;
  const now = performance.now(); if (now - armPanelAt < 150) return; armPanelAt = now;
  const q = armQ(), info = LIVE ? LIVE.info().arm || {} : {}, jh = LIVE ? st.armJH || [] : q.map(() => true), fe = LIVE ? st.armJfe || [] : q.map(() => 0);
  $("arm-joints").innerHTML = q.map((x, j) => {
    const l = armLimit(j), u = Math.max(0, Math.min(1, (x - l[0]) / (l[1] - l[0] || 1)));
    const near = u < 0.05 || u > 0.95, f = Math.abs(fe[j] || 0);
    return `<div class="armjrow"><b>J${j + 1}</b><span class="h${jh[j] ? " on" : ""}" title="${jh[j] ? "homed" : "not homed"}"></span>
      <span class="bar${near ? " warn" : ""}"><i style="left:${(u * 100).toFixed(1)}%"></i></span><span class="num">${x.toFixed(2)}°</span><span class="num muted">${(f * 1000).toFixed(0)} m°</span>
      <button class="btn sm" data-armhome="${j}" title="Home J${j + 1}">REF</button></div>`;
  }).join("");
  const jm = LIVE ? st.armJmode : ARM.frame;
  $("arm-jmode").textContent = jm === "world" ? "world mode" : "joint mode";
  const gd = st.arm && st.arm.guard, prevent = ARM.v ? ARM.v.prevent || 10 : 10;
  const gp = $("arm-guard");
  if (gd) {
    const stop = !!gd.state, near = !stop && gd.clearance < 2 * prevent;
    gp.textContent = stop ? `STOP · ${ARM_GUARD[gd.reason] || "stop"}` : near ? "NEAR" : "ARMED"; gp.className = "pill " + (stop ? "bad" : near ? "warn" : "ok");
    $("arm-clear").textContent = `${gd.clearance != null ? gd.clearance.toFixed(0) : "—"} mm clear (limit ${prevent})`;
  } else { gp.textContent = LIVE ? "no guard" : "simulated"; gp.className = "pill off"; $("arm-clear").textContent = ""; }
  const gr = LIVE ? st.arm && st.arm.grip : ARM.grip;
  $("arm-grip").textContent = gr ? `${ARM_GRIP[Math.round(gr.state)] || gr.state} · ${(+(gr.gap_fb ?? gr.gap) || 0).toFixed(1)} mm${gr.payload_mass ? ` · ${gr.payload_mass.toFixed(2)} kg` : ""}` : "none";
  $("arm-gopen").disabled = $("arm-gclose").disabled = !gr;
  const qa = st.arm && st.arm.quota;
  $("arm-quota").textContent = qa ? `${Math.round(qa.total_power)} W · feed ${Math.round(100 * qa.feed_scale)} %${qa.limiting ? " · LIMITING" : ""}` : "—";
  $("arm-teach").textContent = `${ARM.teach} point${ARM.teach === 1 ? "" : "s"} · ${info.teach || "teach.ngc"}`;
}
// the Tools page: the tool changer's tools, in their docks or on the flange
function armRenderTools() {
  let el = $("tl-arm");
  if (!el) { el = document.createElement("div"); el.id = "tl-arm"; el.className = "card"; $("tl-note").after(el); }
  el.hidden = !ARM || !ARM.v || !Object.keys(ARM.v.tools).length;
  if (el.hidden) return;
  const mt = armMounted(), ch = st.arm && st.arm.changer, tl = st.arm && st.arm.tool, fault = tl && tl.fault;
  const busyNow = LIVE ? st.run === "run" || st.lmode === "mdi" && st.run !== "idle" : busy();
  el.innerHTML = `<h3>Tool changer <span class="pill ${fault ? "bad" : "ok"}">${fault ? "FAULT" : "READY"}</span></h3>
    <table class="t"><tbody>${Object.values(ARM.v.tools).map((t) => `<tr data-armtool="${t.id}"><td class="num">T${t.id}</td><td>${esc(t.name)}</td>
      <td><span class="pill ${t.id === mt ? "info" : "off"}">${t.id === mt ? "ON FLANGE" : "IN DOCK"}</span></td>
      <td class="n">${t.tcp ? `TCP ${t.tcp.map((x) => (x * 1000).toFixed(0)).join(" ")} mm` : ""}</td>
      <td><button class="btn sm" data-mount="${t.id}"${t.id === mt || busyNow ? " disabled" : ""}>${t.id === mt ? "Mounted" : "Mount"}</button></td></tr>`).join("")}</tbody></table>
    <div class="row"><button class="btn sm" data-mount="0"${!mt || busyNow ? " disabled" : ""}>Put the tool back</button></div>
    <dl class="kv"><dt>Lock</dt><dd>${tl ? (tl.valve ? "lock" : "release") : "—"}${ch ? ` · ${ch.locked ? "locked" : ch.unlocked ? "unlocked" : "between"}${ch.present ? ", tool present" : ", no tool"}` : ""}</dd>
      <dt>Kinematics</dt><dd>${st.arm && st.arm.kins_tool != null ? (st.arm.kins_tool ? `T${Math.round(st.arm.kins_tool)} TCP` : "the flange") : LIVE ? "—" : mt ? `T${mt} TCP` : "the flange"}</dd>
      ${fault ? `<dt>Fault</dt><dd>${ARM_CHG[tl.reason] || tl.reason}</dd>` : ""}</dl>`;
}

// ------------------------------------------------------------ the simulation's program: pick and place over the docks' side
function armDemo() {
  const p = Prog("ARM_PICK_PLACE.ngc"); p.curTool = 0;
  const mv = (x, y, z, f) => { const ln = p.emit((f ? "G1" : "G0") + ` X${x.toFixed(3)} Y${y.toFixed(3)} Z${z.toFixed(3)} A0 B0 C0` + (f ? ` F${f}` : "")); const a = p.pos; p.segs.push({ k: "mv", x0: a.x, y0: a.y, z0: a.z, x, y, z, f: f || 0, rapid: !f, line: ln, tool: p.curTool || 0 }); p.pos = { x, y, z }; };
  p.emit("%"); p.emit("(ARM · PICK AND PLACE · A SQUARE OF FOUR PLACES · SIMULATION)"); p.emit("G21 G90 G17");
  p.pos = { x: 799.3, y: 0.2, z: 1270.9 };
  for (const [x, y] of [[800, -300], [800, 300], [600, 300], [600, -300]]) {
    mv(x, y, 1100, 0); mv(x, y, 900, 3000); p.m("M64 P0", "io"); mv(x, y, 1100, 3000); p.m("M65 P0", "io");
  }
  mv(799.3, 0.2, 1270.9, 0);
  p.emit("M2"); p.emit("%");
  return p;
}
