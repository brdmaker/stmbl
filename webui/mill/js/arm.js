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
  ARM = { v: null, q: ARM_HOME.slice(), abc: [0, 0, 0], mounted: 0, frame: "joint", held: null, lastTip: null, reach: false, teach: 0, points: [], goal: null, grip: { state: 0, gap: 75 }, built: false };
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
  armPoints();
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
        <dt>Gripper</dt><dd id="arm-grip">—</dd><dt>Power</dt><dd id="arm-quota">—</dd><dt>Teach</dt><dd id="arm-teach">0 points</dd>
        <dt id="arm-enable-k">Enabling</dt><dd><span class="pill off" id="arm-enable">—</span></dd></dl>
      <div class="row"><button class="btn sm" id="arm-gopen">Open</button><button class="btn sm" id="arm-gclose">Close</button><span class="spacer"></span><button class="btn sm" id="arm-home" title="Every joint to the home pose at once (joint mode)">Go home</button><button class="btn sm accent" id="arm-rec" title="The pose as a G1 line after the teach file's last point">Record point</button></div>
      <details class="armpts" id="arm-pts"><summary>Teach points <small id="arm-ptn">0</small></summary><ol id="arm-ptlist"></ol>
        <div class="row"><button class="btn sm" id="arm-ptload" title="The teach file as the program (Cycle start runs it)">Load as program</button><span class="spacer"></span><button class="btn sm" id="arm-ptclear">Clear</button></div></details>`;
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
// the frames: JOINT (each joint), WORLD (X Y Z, A B C about the base's axes), TOOL (along and about the tool's own
// axes: "tx".."tc"; live, one MDI move the server works out)
const ARM_FRAMES = [["joint", "JOINT"], ["world", "WORLD"], ["tool", "TOOL"]];
function armRenderPad() {
  const pad = $("armpad"); if (!pad || !ARM) return;
  const n = 6, f = ARM.frame, joint = f === "joint";
  pad.innerHTML = `<div class="seg" id="armpad-frame" role="group" aria-label="Jog frame">${ARM_FRAMES.map(([k, l]) => `<button data-f="${k}" aria-pressed="${f === k}">${l}</button>`).join("")}</div>` +
    [...Array(n)].map((_, i) => { const ax = joint ? "j" + i : (f === "tool" ? "t" : "") + "xyzabc"[i], lab = joint ? "J" + (i + 1) : "XYZABC"[i];
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
  st.pos = { x: p.xyz[0], y: p.xyz[1], z: p.xyz[2] }; ARM.abc = p.abc; ARM.lastTip = { ...st.pos }; ARM.lastAbc = p.abc.slice();
}
function armSimStep(dt, tip) {
  const v = ARM.v;
  if (ARM.held) {  // a held jog key
    const h = ARM.held, rate = st.jogRate / 60 * dt * h.s;
    armSimJog(h.ax, rate);
    return;
  }
  if (ARM.goal) {  // Go home: every joint at once, arriving together (the jog rate as °/min)
    const d = ARM.goal.map((g, j) => g - ARM.q[j]), far = Math.max(...d.map(Math.abs)), step = st.jogRate / 60 * dt;
    if (far <= step) { ARM.q = ARM.goal.slice(); ARM.goal = null; msg("Home", "ok"); } else ARM.q = ARM.q.map((x, j) => x + d[j] * step / far);
    armSimPose();
    return;
  }
  const t = st.pos, L = ARM.lastTip, A = ARM.lastAbc, abc = ARM.abc;
  if (L && A && Math.abs(t.x - L.x) < 1e-6 && Math.abs(t.y - L.y) < 1e-6 && Math.abs(t.z - L.z) < 1e-6 && abc.every((x, k) => Math.abs(x - A[k]) < 1e-6)) return;
  const s = ARMMODEL.ik(v, { xyz: [t.x, t.y, t.z], abc }, ARM.q, armTcp(), 60);
  if (s.ok && armInLimits(s.q)) { ARM.q = s.q; ARM.reach = false; }
  else if (!ARM.reach) { ARM.reach = true; msg(`X${t.x.toFixed(1)} Y${t.y.toFixed(1)} Z${t.z.toFixed(1)} A${abc[0].toFixed(1)} B${abc[1].toFixed(1)} C${abc[2].toFixed(1)}: out of the arm's reach or its joints' limits`, "warn"); }
  ARM.lastTip = { ...t }; ARM.lastAbc = abc.slice();
}
// a program's or MDI's segment with A B C (the simulation): from abc0 to abc, linear as LinuxCNC's rotary axes
function armSegAbc(seg, u) {
  if (!seg.abc0) seg.abc0 = ARM.abc.slice();
  ARM.abc = seg.abc0.map((a, k) => a + (seg.abc[k] - a) * u);
}
function armSimJog(ax, d) {
  if (ax[0] === "j") armSimJoint(+ax.slice(1), d);
  else if (ax[0] === "t") armSimTool("xyzabc".indexOf(ax[1]), d);
  else armSimWorld(ax, d);
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
// the tool frame: k 0..2 along the tool's X Y Z [mm], 3..5 about them [deg]; the tool point stays for a rotation
function armSimTool(k, d) {
  const P = ARMMODEL.pose(ARM.v, ARM.q, armTcp()), M = P.M;
  let xyz = P.xyz, abc = ARM.abc;
  if (k < 3) xyz = xyz.map((x, r) => x + M[k * 4 + r] * d);
  else {
    const r = d * Math.PI / 180, rot = ARMMODEL.rpy(k === 3 ? r : 0, k === 4 ? r : 0, k === 5 ? r : 0);
    abc = ARMMODEL.rpyOf(ARMMODEL.mul(M, rot)).map((x, i) => x + 360 * Math.round((ARM.abc[i] - x) / 360));
  }
  const s = ARMMODEL.ik(ARM.v, { xyz, abc }, ARM.q, armTcp(), 25);
  if (s.ok && armInLimits(s.q)) { ARM.q = s.q; armSimPose(); ARM.abc = abc; ARM.lastAbc = abc.slice(); }
  else { msg(`Tool ${"XYZABC"[k]}: out of reach`, "warn"); ARM.held = null; }
}
// MDI in the simulation: G0/G1 with X Y Z A B C (absolute), T<n> M6, the gripper's M64/M65 P0. false: not an arm's
function armMdi(c) {
  const u = c.toUpperCase().replace(/\(.*?\)/g, ""), w = {};
  for (const m of u.matchAll(/([A-Z])\s*(-?\d*\.?\d+)/g)) (w[m[1]] = w[m[1]] || []).push(+m[2]);
  const has = (l, v) => (w[l] || []).includes(v), one = (l, d) => (w[l] ? w[l][0] : d);
  if (w.T && has("M", 6)) { armMount(one("T", 0)); return true; }
  if (has("M", 64) || has("M", 65)) { ARM.grip = has("M", 64) ? { state: 4, gap: 0 } : { state: 0, gap: 75 }; armRenderPanel(); msg(`${c}: gripper ${has("M", 64) ? "closed (EMPTY)" : "open"}`, "ok"); return true; }
  if (!(has("G", 0) || has("G", 1)) || !["X", "Y", "Z", "A", "B", "C"].some((l) => w[l])) return false;
  const t = { x: one("X", st.pos.x), y: one("Y", st.pos.y), z: one("Z", st.pos.z) }, abc = ["A", "B", "C"].map((l, k) => one(l, ARM.abc[k]));
  const s = ARMMODEL.ik(ARM.v, { xyz: [t.x, t.y, t.z], abc }, ARM.q, armTcp(), 80);
  if (!s.ok || !armInLimits(s.q)) { msg(`${c}: out of the arm's reach or its joints' limits`, "warn"); return true; }
  const rapid = has("G", 0) && !has("G", 1), seg = { k: "mv", x0: st.pos.x, y0: st.pos.y, z0: st.pos.z, ...t, abc0: ARM.abc.slice(), abc, f: rapid ? 0 : one("F", 1000), rapid, line: 0, tool: st.tool };
  st.motion = plan({ segs: [seg], name: c, kind: "mdi" }); st.t = 0; st.seg = 0; setRun("run"); msg(c, "info");
  return true;
}

// ------------------------------------------------------------ the jog pad, the panel's buttons
function armJog(ax, s) {
  if (LIVE) return LIVE.jogAxis(ax, s);
  if (!ready("jog")) return;
  if (busy()) { msg("Jogging is off while the program runs.", "warn"); return; }
  if (st.jogInc === 0) { ARM.held = { ax, s }; return; }
  armSimJog(ax, s * st.jogInc);
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
    if (LIVE) { LIVE.teach(st.jogRate).then((r) => { if (r && r.ok) { ARM.teach = r.points; armRenderPanel(); armPoints(); } }); return; }
    const p = ARMMODEL.pose(ARM.v, ARM.q, armTcp()), line = "G1 " + [...p.xyz, ...p.abc].map((x, i) => "XYZABC"[i] + x.toFixed(3)).join(" ") + " F" + Math.round(st.jogRate);
    ARM.points.push(line); ARM.teach = ARM.points.length; armRenderPanel(); armRenderPoints(); msg(`Recorded ${line} (${ARM.teach} points in teach.ngc)`, "ok");
  } else if (id === "arm-home") {
    if (LIVE) { LIVE.armHome(st.jogRate); return; }
    if (!ready("jog")) return;
    if (busy()) { msg("Not while the program runs.", "warn"); return; }
    ARM.goal = ARM_HOME.slice();
  } else if (e.target.dataset && e.target.dataset.pt) {
    armPoint(e.target.dataset.pt, +e.target.dataset.i);
  } else if (id === "arm-ptload") {
    armPointsLoad();
  } else if (id === "arm-ptclear") {
    if (!ARM.points.length) return;
    ask("Clear the teach points?", `All ${ARM.points.length} of them, from the teach file.`, "", () => armPoint("clear", 0), "confirm");
  } else if (id === "arm-gopen" || id === "arm-gclose") {
    const close = id === "arm-gclose";
    if (LIVE) { LIVE.mdi(close ? grip.close : grip.open); return; }
    ARM.grip = { state: close ? 4 : 0, gap: close ? 0 : 75 }; armRenderPanel(); msg(`Gripper ${close ? "closed: EMPTY (nothing between its jaws)" : "open"}`, "info");
  } else if (e.target.dataset && e.target.dataset.armhome !== undefined) {
    const j = +e.target.dataset.armhome;
    if (LIVE) LIVE.home("J" + j); else msg(`J${j + 1} homed (absolute encoder)`, "ok");
  } else if (e.target.dataset && e.target.dataset.mount !== undefined) armMount(+e.target.dataset.mount);
});
// ------------------------------------------------------------ the teach points
// the list: live the server's teach file (op teach), in the simulation this page's
function armPoints() {
  if (!ARM) return;
  if (!LIVE) { armRenderPoints(); return; }
  LIVE.teachOp("list").then((r) => { if (r && r.ok && ARM) { ARM.points = r.points; ARM.teach = r.points.length; armRenderPoints(); armRenderPanel(); } });
}
function armRenderPoints() {
  const ol = $("arm-ptlist"); if (!ol || !ARM) return;
  const n = ARM.points.length;
  $("arm-ptn").textContent = String(n);
  ol.innerHTML = ARM.points.map((l, i) => `<li><span class="num">${esc(l.replace(/^G1 /, ""))}</span><span class="b">
    <button class="btn sm" data-pt="go" data-i="${i}" title="Move there (MDI)">Go</button><button class="btn sm" data-pt="up" data-i="${i}"${i ? "" : " disabled"} title="One place earlier">↑</button><button class="btn sm" data-pt="down" data-i="${i}"${i < n - 1 ? "" : " disabled"} title="One place later">↓</button><button class="btn sm" data-pt="delete" data-i="${i}" title="Delete it">×</button></span></li>`).join("") || `<li class="muted">None yet: jog there, Record point.</li>`;
  $("arm-ptload").disabled = $("arm-ptclear").disabled = !n;
}
function armPoint(action, i) {
  if (action === "go") { const l = ARM.points[i]; if (!l) return; if (LIVE) LIVE.mdi(l); else runMdi(l); return; }
  if (LIVE) { LIVE.teachOp(action, i).then((r) => { if (r && r.ok && ARM) { ARM.points = r.points; ARM.teach = r.points.length; armRenderPoints(); armRenderPanel(); } }); return; }
  const P = ARM.points;
  if (action === "clear") P.length = 0;
  else if (action === "delete") P.splice(i, 1);
  else { const k = i + (action === "up" ? -1 : 1); if (k >= 0 && k < P.length) [P[i], P[k]] = [P[k], P[i]]; }
  ARM.teach = P.length; armRenderPoints(); armRenderPanel();
}
// the teach file as the program: live LinuxCNC opens it; the simulation makes its moves
function armPointsLoad() {
  if (LIVE) { const f = (LIVE.info().arm || {}).teach || "teach.ngc"; LIVE.loadProgram({ server: true, name: f }); return; }
  if (busy()) { msg("Stop the program first.", "warn"); return; }
  const p = Prog("teach.ngc"); p.curTool = ARM.mounted;
  p.emit("(the teach points)"); p.emit("G21 G90");
  let at = { ...st.pos }, abc0 = ARM.abc.slice();
  for (const l of ARM.points) {
    const w = {}; for (const m of l.toUpperCase().matchAll(/([A-Z])\s*(-?\d*\.?\d+)/g)) w[m[1]] = +m[2];
    const to = { x: w.X ?? at.x, y: w.Y ?? at.y, z: w.Z ?? at.z }, abc = ["A", "B", "C"].map((k, i) => w[k] ?? abc0[i]), rapid = /^G0\b/.test(l);
    const ln = p.emit(l);
    p.segs.push({ k: "mv", x0: at.x, y0: at.y, z0: at.z, ...to, abc0, abc, f: rapid ? 0 : w.F || 1000, rapid, line: ln, tool: ARM.mounted });
    at = to; abc0 = abc;
  }
  p.emit("M2");
  const pl = plan(p);
  library.unshift(pl); prog = pl;
  renderProgram(); rebuildPath();
  msg(`teach.ngc loaded: ${ARM.points.length} points (Cycle start runs them)`, "ok");
}

function armMount(t) {
  if (LIVE) {  // the ready pose first (done before M6: LinuxCNC takes one MDI line at a time)
    const ready = (LIVE.info().arm || {}).tool_ready;
    (ready && t ? LIVE.mdi(ready, true) : Promise.resolve({ ok: true })).then((r) => { if (r && r.ok) LIVE.mdi(`T${t} M6`); });
    return;
  }
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
  $("arm-jmode").textContent = { world: "world mode", tool: "tool frame", coord: "coordinated" }[jm] || "joint mode";
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
  $("arm-home").disabled = LIVE ? !info.home : false;
  // the enabling switch (live, when the machine has one): released, LinuxCNC inhibits every jog
  const en = st.arm && st.arm.enable, sw = !!info.enable_switch, held = !sw || (en && en.held);
  $("arm-enable-k").hidden = $("arm-enable").parentNode.hidden = !sw;
  if (sw) { $("arm-enable").textContent = held ? "HELD" : "RELEASED · hold it to jog"; $("arm-enable").className = "pill " + (held ? "ok" : "warn"); }
  const pad = $("armpad"); if (pad) pad.classList.toggle("inhibit", !held);
  const tl = st.arm && st.arm.tool;
  if (st.page === "tool" && ARM.v && ARM.toolsDrawn !== `${armBusy()}/${armMounted()}/${!!(tl && tl.fault)}`) armRenderTools();
}
// the Tools page: the tool changer's tools, in their docks or on the flange; drawn again when the machine
// goes busy or idle, or another tool is on the flange (its buttons' enabled state)
const armBusy = () => (LIVE ? st.run === "run" || st.lmode === "mdi" && st.run !== "idle" : busy());
function armRenderTools() {
  let el = $("tl-arm");
  if (!el) { el = document.createElement("div"); el.id = "tl-arm"; el.className = "card"; $("tl-note").after(el); }
  el.hidden = !ARM || !ARM.v || !Object.keys(ARM.v.tools).length;
  if (el.hidden) return;
  const mt = armMounted(), ch = st.arm && st.arm.changer, tl = st.arm && st.arm.tool, fault = tl && tl.fault;
  const busyNow = armBusy();
  ARM.toolsDrawn = `${busyNow}/${mt}/${!!fault}`;
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
  let abc0 = [0, 0, 0];
  const mv = (x, y, z, f, abc = [0, 0, 0]) => {
    const ln = p.emit((f ? "G1" : "G0") + ` X${x.toFixed(3)} Y${y.toFixed(3)} Z${z.toFixed(3)} ` + abc.map((v, k) => "ABC"[k] + v.toFixed(3)).join(" ") + (f ? ` F${f}` : ""));
    const a = p.pos; p.segs.push({ k: "mv", x0: a.x, y0: a.y, z0: a.z, x, y, z, abc0, abc, f: f || 0, rapid: !f, line: ln, tool: p.curTool || 0 }); p.pos = { x, y, z }; abc0 = abc;
  };
  p.emit("%"); p.emit("(ARM · PICK AND PLACE · A SQUARE OF FOUR PLACES · SIMULATION)"); p.emit("G21 G90 G17");
  p.pos = { x: 799.3, y: 0.2, z: 1270.9 };
  for (const [x, y] of [[800, -300], [800, 300], [600, 300], [600, -300]]) {
    mv(x, y, 1100, 0); mv(x, y, 900, 3000); p.m("M64 P0", "io"); mv(x, y, 1100, 3000); p.m("M65 P0", "io");
  }
  p.emit("(TILT THE TOOL: B 30, THEN C 45 ABOUT THE BASE, BACK UPRIGHT)");
  mv(700, 0, 1100, 0); mv(700, 0, 1100, 3000, [0, 30, 0]); mv(700, 0, 1100, 3000, [0, 30, 45]); mv(700, 0, 1100, 3000);
  mv(799.3, 0.2, 1270.9, 0);
  p.emit("M2"); p.emit("%");
  return p;
}
