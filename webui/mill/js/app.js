/* Portal Mill Console: the console (UI, 3D, pages) and its built-in
   simulated machine. live.js connects the same console to millweb; without
   it (opened from disk, or ?sim) the simulation runs. Classic script: its
   top-level names are shared with live.js. */
"use strict";
const $ = (id) => document.getElementById(id);
const $$ = (sel) => [...document.querySelectorAll(sel)];
const tok = (n) => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const C = { accent: tok("--accent"), ok: tok("--ok"), warn: tok("--warn"), stop: tok("--stop"), cut: tok("--cut"), line: tok("--line"), faint: tok("--ink-faint"), ink: tok("--ink"), mono: tok("--f-mono") };
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const pad2 = (n) => String(n).padStart(2, "0");
const mmss = (s) => { s = Math.max(0, Math.round(s)); return s >= 3600 ? Math.floor(s / 3600) + ":" + pad2(Math.floor(s / 60) % 60) + ":" + pad2(s % 60) : pad2(Math.floor(s / 60)) + ":" + pad2(s % 60); };
const clock = () => { const d = new Date(); return pad2(d.getHours()) + ":" + pad2(d.getMinutes()) + ":" + pad2(d.getSeconds()); };
const esc = (s) => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/"/g, "&quot;");
const nline = (n) => "N" + String(n).padStart(4, "0");

// The live machine (live.js sets it when millweb serves the page); null: the simulation.
let LIVE = null;

// ============================================================ the machine
const TRAVEL = { x: [0, 1000], y: [0, 1000], z: [-215, 0] };  // machine coordinates (Z: the tool tip, 0 = top)
const ZTOP = 234;      // height of the tool tip above the table at machine Z 0 (G54 Z −195: the stock top 39 mm up, on its fixture plate)
const STOCK = { w: 120, l: 80, h: 25 };
const RAPID = 30000, ACC = 4900;

// ============================================================ state
const st = {
  estop: false, on: true, homed: { x: true, y: true, z: true }, page: "main",
  run: "idle", t: 0, seg: 0, motion: null,
  pos: { x: 0, y: 0, z: 30 },
  wcs: 0, g92: { x: 0, y: 0, z: 0 }, rot: 0,
  offsets: [
    { n: "G54", x: 440, y: 460, z: -195 }, { n: "G55", x: 640, y: 460, z: -195 },
    { n: "G56", x: 0, y: 0, z: 0 }, { n: "G57", x: 0, y: 0, z: 0 }, { n: "G58", x: 0, y: 0, z: 0 }, { n: "G59", x: 0, y: 0, z: 0 },
    { n: "G59.1", x: 0, y: 0, z: 0 }, { n: "G59.2", x: 0, y: 0, z: 0 }, { n: "G59.3", x: 0, y: 0, z: 0 },
  ],
  tools: [
    { t: 1, p: 1, d: 12.0, l: 62.415, wear: 0.004, life: 72, c: "12 mm 3-flute roughing endmill" },
    { t: 3, p: 3, d: 6.0, l: 48.130, wear: 0.002, life: 82, c: "6 mm 3-flute carbide endmill" },
    { t: 5, p: 5, d: 5.0, l: 71.902, wear: 0.000, life: 95, c: "5 mm carbide drill" },
    { t: 7, p: 7, d: 10.0, l: 55.660, wear: 0.000, life: 100, c: "10 mm 90° chamfer mill" },
    { t: 12, p: 12, d: 50.0, l: 40.220, wear: 0.010, life: 64, c: "50 mm face mill, 4 inserts" },
    { t: 99, p: 99, d: 2.0, l: 110.000, wear: 0, life: 100, c: "3D touch probe" },
  ],
  tool: 3, toolSel: 3,
  spindle: { dir: 0, s: 0, rpm: 0, load: 0 },
  cool: { flood: false, mist: false, air: false },
  ovr: { feed: 1, rapid: .5, spin: 1, maxv: 1 },
  dro: "work", units: "mm", dec: 3, exag: 200, kbd: true, m1: true, bd: false, lift: true, confirm: false,
  show: { machine: true, path: true, limits: false, defl: false }, color: "err",
  jogInc: 1, jogRate: 2000,
  err: 0, errHist: [], yawHist: [], errs: [], p99: 0, yaw: 0, fe: [0, 0, 0, 0], bus: 325, quota: 1,
  pc: { shaper: true, defl: true, shrink: true, jerk: true },
  feedNow: 0, velNow: 0, selLine: 0, cutTime: 0, spindleHrs: 1834.2,
};

// ============================================================ programs
// Each generator returns { name, lines[], segs[] } in work coordinates.
function Prog(name) {
  const p = { name, lines: [], segs: [], tools: new Set(), pos: { x: 0, y: 0, z: 30 }, curTool: 3 };
  p.emit = (t) => { p.lines.push(t); return p.lines.length; };
  const here = () => ({ x0: p.pos.x, y0: p.pos.y, z0: p.pos.z, x: p.pos.x, y: p.pos.y, z: p.pos.z });
  p.mv = (x, y, z, f) => {
    const r = f === 0, a = p.pos;
    const w = (k, v) => (v !== a[k] ? " " + k.toUpperCase() + (+v).toFixed(3) : "");
    const ln = p.emit((r ? "G0" : "G1") + w("x", x) + w("y", y) + w("z", z) + (r ? "" : " F" + f));
    p.segs.push({ k: "mv", x0: a.x, y0: a.y, z0: a.z, x, y, z, f, rapid: r, line: ln, tool: p.curTool });
    p.pos = { x, y, z };
  };
  p.tc = (t) => { if (p.pos.z < 30) p.mv(p.pos.x, p.pos.y, 30, 0); const ln = p.emit(`T${t} M6`); p.segs.push({ k: "tc", tool: t, line: ln, ...here() }); p.curTool = t; p.tools.add(t); p.emit("G43 H" + t); };
  p.m = (code, kind) => { const ln = p.emit(code); p.segs.push({ k: kind, line: ln, ...here() }); };
  return p;
}
function head(p, title) { p.emit("%"); p.emit("(" + title + ")"); p.emit("G21 G90 G54 G17 G94 G40 G49"); p.emit("G64 P0.02"); }
function tail(p) { p.mv(p.pos.x, p.pos.y, 30, 0); p.m("M5", "m5"); p.m("M9", "m9"); p.emit("M30"); p.emit("%"); }
function drill(p, x, y, depth, peck, f) {
  p.mv(x, y, 3, 0);
  for (let z = -peck; ; z -= peck) { const zz = Math.max(z, depth); p.mv(x, y, zz, f); p.mv(x, y, 1, 0); if (zz <= depth) break; p.mv(x, y, zz + 0.5, 0); }
  p.mv(x, y, 3, 0);
}
function genBracket() {
  const p = Prog("BRACKET-AL6082_OP1.ngc"); head(p, "BRACKET AL6082 OP1 · FACE, POCKET, CONTOUR, DRILL");
  p.tc(12); p.m("S9000 M3", "m3"); p.m("M8", "m8");
  p.emit("(FACE · T12 D50)");
  p.mv(-30, 10, 5, 0); p.mv(-30, 10, 0, 1500);
  for (let i = 0, y = 10; y <= 72; y += 31, i++) { p.mv(i % 2 ? -30 : 150, y, 0, 3600); if (y + 31 <= 72) p.mv(i % 2 ? -30 : 150, y + 31, 0, 3600); }
  p.mv(p.pos.x, p.pos.y, 5, 0);
  p.tc(3); p.m("S18000 M3", "m3");
  p.emit("(POCKET · T3 D6 · TWO DEPTHS)");
  const pocket = (dz) => {
    p.mv(60, 40, 3, 0); p.mv(60, 40, dz, 600);
    for (let k = 1; k <= 6; k++) {
      const w = 6 + k * 4.4, h = 3 + k * 2.7, r = Math.min(4, h), pts = [];
      const add = (cx, cy, a0) => { for (let s = 0; s <= 6; s++) { const a = a0 + s * Math.PI / 12; pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]); } };
      add(60 + w - r, 40 - h + r, -Math.PI / 2); add(60 + w - r, 40 + h - r, 0); add(60 - w + r, 40 + h - r, Math.PI / 2); add(60 - w + r, 40 - h + r, Math.PI);
      pts.push(pts[0]);
      for (const q of pts) p.mv(+q[0].toFixed(3), +q[1].toFixed(3), dz, 3200);
    }
    p.mv(p.pos.x, p.pos.y, 3, 0);
  };
  pocket(-4); pocket(-8);
  p.m("M1", "m1");
  p.emit("(CONTOUR · T3)");
  const ctr = [[2, 2], [118, 2], [118, 62], [104, 78], [16, 78]];
  for (let s = 0; s <= 12; s++) { const a = Math.PI / 2 + s * Math.PI / 24; ctr.push([16 + 14 * Math.cos(a), 64 + 14 * Math.sin(a)]); }
  ctr.push([2, 2]);
  p.mv(-4, 2, 5, 0); p.mv(-4, 2, -10, 900);
  for (const q of ctr) p.mv(+q[0].toFixed(3), +q[1].toFixed(3), -10, 2400);
  p.mv(p.pos.x, p.pos.y, 5, 0);
  p.tc(5); p.m("S6000 M3", "m3");
  p.emit("(DRILL 4X · T5 D5 · PECK 3)");
  for (const [hx, hy] of [[14, 14], [106, 14], [106, 66], [30, 66]]) drill(p, hx, hy, -18, 3, 450);
  tail(p); return p;
}
function genFacing(o) {
  const p = Prog(`FACING_${o.w}x${o.l}.ngc`); head(p, `FACING ${o.w} x ${o.l} DEPTH ${o.d} · WIZARD`);
  const tl = toolOf(o.t) || { d: 12 }, step = tl.d * o.s / 100;
  p.tc(o.t); p.m("S12000 M3", "m3"); p.m("M8", "m8");
  p.mv(-tl.d, 0, 5, 0); p.mv(-tl.d, 0, -o.d, 800);
  for (let i = 0, y = 0; y <= o.l + 1e-6; y += step, i++) { p.mv(i % 2 ? -tl.d : o.w + tl.d, +y.toFixed(3), -o.d, o.f); if (y + step <= o.l + 1e-6) p.mv(i % 2 ? -tl.d : o.w + tl.d, +(y + step).toFixed(3), -o.d, o.f); }
  tail(p); return p;
}
function genBolt(o) {
  const p = Prog(`BOLTCIRCLE_${o.n}x_R${o.r}.ngc`); head(p, `BOLT CIRCLE ${o.n} HOLES R${o.r} · WIZARD`);
  p.tc(o.t); p.m("S6000 M3", "m3"); p.m("M8", "m8");
  for (let i = 0; i < o.n; i++) { const a = (o.a + i * 360 / o.n) * Math.PI / 180; drill(p, +(o.x + o.r * Math.cos(a)).toFixed(3), +(o.y + o.r * Math.sin(a)).toFixed(3), o.d, o.p, 400); }
  tail(p); return p;
}
function genCircPocket(o) {
  const p = Prog(`CPOCKET_D${o.dia}.ngc`); head(p, `CIRCULAR POCKET D${o.dia} DEPTH ${o.d} · WIZARD`);
  const tl = toolOf(o.t) || { d: 6 };
  p.tc(o.t); p.m("S16000 M3", "m3");
  const rmax = o.dia / 2 - tl.d / 2;
  for (let z = -o.sd; ; z -= o.sd) {
    const zz = Math.max(z, o.d);
    p.mv(o.x, o.y, 2, 0); p.mv(o.x, o.y, zz, 400);
    for (let r = Math.min(tl.d * 0.6, rmax); r > 0; r = Math.min(rmax, r + tl.d * 0.6)) {
      for (let s = 0; s <= 36; s++) { const a = s * Math.PI / 18; p.mv(+(o.x + r * Math.cos(a)).toFixed(3), +(o.y + r * Math.sin(a)).toFixed(3), zz, 2800); }
      if (r >= rmax) break;
    }
    p.mv(p.pos.x, p.pos.y, 2, 0);
    if (zz <= o.d) break;
  }
  tail(p); return p;
}
function genAdaptive() {
  const p = Prog("HOUSING-7075_OP2.ngc"); head(p, "HOUSING 7075 OP2 · ADAPTIVE CLEARING, FINISH");
  p.tc(1); p.m("S14000 M3", "m3"); p.m("M8", "m8");
  for (const dz of [-3, -6, -9]) {
    p.mv(20, 20, 3, 0); p.mv(20, 20, dz, 500);
    for (let k = 0; k < 9; k++) {
      const cx = 20 + k * 10;
      for (let s = 0; s <= 18; s++) { const a = s * Math.PI / 9; p.mv(+(cx + 9 * Math.cos(a) + s * 0.55).toFixed(3), +(40 + 22 * Math.sin(a)).toFixed(3), dz, 6000); }
    }
    p.mv(p.pos.x, p.pos.y, 3, 0);
  }
  p.tc(3); p.m("S18000 M3", "m3");
  p.mv(10, 10, 3, 0); p.mv(10, 10, -9, 600);
  for (const q of [[110, 10], [110, 70], [10, 70], [10, 10]]) p.mv(q[0], q[1], -9, 2000);
  tail(p); return p;
}
// timing, and each segment's predicted contour error: from the corner it
// makes with the next (what pcomp's model says the head adds after its own
// leads); along the segment it shows over its last ~8 mm
function plan(p) {
  let t = 0, cut = 0;
  const S = p.segs;
  for (let i = 0; i < S.length; i++) {
    const s = S[i];
    s.len = Math.hypot(s.x - s.x0, s.y - s.y0, s.z - s.z0);
    if (s.k === "tc") s.t = 5.5;
    else if (s.k !== "mv") s.t = s.k === "m3" ? 1.2 : 0.05;
    else { const f = (s.rapid ? RAPID : s.f) / 60; s.t = s.len / f + (s.len > 0 ? Math.min(f / ACC, Math.sqrt(s.len / ACC)) : 0); }
    s.t0 = t; t += s.t;
    if (s.k === "mv" && !s.rapid) cut += s.len;
    let ang = 0;
    const n = S[i + 1];
    if (s.k === "mv" && !s.rapid && n && n.k === "mv" && !n.rapid) {
      const a = [s.x - s.x0, s.y - s.y0], b = [n.x - n.x0, n.y - n.y0], la = Math.hypot(...a), lb = Math.hypot(...b);
      if (la > 1e-6 && lb > 1e-6) ang = Math.acos(clamp((a[0] * b[0] + a[1] * b[1]) / (la * lb), -1, 1));
    }
    const v = s.k === "mv" && !s.rapid ? s.f / 60 : 0;
    s.err = Math.min(60, 2 + (v * v * ang / 0.8) / Math.pow(2 * Math.PI * 28, 2) * 1000 * 0.35);
    s.errMean = s.err * Math.min(1, 8 / Math.max(s.len || 0, 1e-6));
    s.cut = s.k === "mv" && !s.rapid && Math.min(s.z, s.z0) < 0.5;
  }
  p.total = t; p.cutLen = cut;
  const xs = S.flatMap((s) => [s.x0, s.x]), ys = S.flatMap((s) => [s.y0, s.y]), zs = S.flatMap((s) => [s.z0, s.z]);
  p.ext = { x: [Math.min(...xs), Math.max(...xs)], y: [Math.min(...ys), Math.max(...ys)], z: [Math.min(...zs), Math.max(...zs)] };
  return p;
}
const toolOf = (t) => st.tools.find((x) => x.t === t);
const library = [plan(genBracket()), plan(genAdaptive()), plan(genFacing({ w: 120, l: 80, d: 0.5, s: 70, f: 4800, t: 12 })), plan(genBolt({ x: 60, y: 40, r: 28, n: 8, a: 0, d: -12, p: 3, t: 5 }))];
let prog = library[0];

// ============================================================ helpers
const off = () => st.offsets[st.wcs];
const toMach = (p) => ({ x: p.x + off().x + st.g92.x, y: p.y + off().y + st.g92.y, z: p.z + off().z + st.g92.z });
const toWork = (m) => ({ x: m.x - off().x - st.g92.x, y: m.y - off().y - st.g92.y, z: m.z - off().z - st.g92.z });
const inside = (m) => m.x >= TRAVEL.x[0] - 1e-6 && m.x <= TRAVEL.x[1] + 1e-6 && m.y >= TRAVEL.y[0] - 1e-6 && m.y <= TRAVEL.y[1] + 1e-6 && m.z >= TRAVEL.z[0] - 1e-6 && m.z <= TRAVEL.z[1] + 1e-6;
const busy = () => st.run === "run" || st.run === "hold" || st.run === "step";
function ready(why) {
  if (st.estop) { msg("E-STOP is active: release it first (F1).", "warn"); return false; }
  if (!st.on) { msg("The machine is off: switch it on (F2).", "warn"); return false; }
  if (why !== "home" && !(st.homed.x && st.homed.y && st.homed.z)) { msg("Home all axes first.", "warn"); return false; }
  return true;
}
const fmtLen = (v, sign = true) => { const u = st.units === "in" ? v / 25.4 : v, d = st.units === "in" ? st.dec + 1 : st.dec; return (sign ? (u < 0 ? "−" : " ") : "") + Math.abs(u).toFixed(d); };
const pct = (a, q) => { if (!a.length) return 0; const s = [...a].sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.floor(s.length * q))]; };

// ============================================================ events
const events = [];
const evLevels = { ok: "OK", info: "INFO", warn: "WARN", bad: "FAULT" };
let evFilter = "all";
function msg(text, lvl = "info") {
  events.unshift({ t: clock(), lvl, text });
  if (events.length > 200) events.pop();
  $("msg").textContent = text;
  $("msg").style.color = lvl === "warn" ? C.warn : lvl === "bad" ? C.stop : C.ink;
  renderEvents();
}
function renderEvents() {
  const list = events.filter((e) => evFilter === "all" || e.lvl === evFilter || (evFilter === "warn" && e.lvl === "bad"));
  $("events").innerHTML = list.map((e) => `<li class="${e.lvl}"><time>${e.t}</time><span class="k">${evLevels[e.lvl]}</span><span>${esc(e.text)}</span></li>`).join("") || `<li class="info"><time>—</time><span class="k"></span><span>No events.</span></li>`;
  const warns = events.filter((e) => e.lvl === "warn" || e.lvl === "bad").length;
  const b = $("nav-stat-badge"); if (b) b.textContent = warns ? String(warns) : "";
}

// ============================================================ navigation
const pages = [
  ["main", "MAIN", '<path d="M3 21V9l9-6 9 6v12"/><path d="M9 21v-7h6v7"/>'],
  ["file", "FILE", '<path d="M6 3h8l4 4v14H6z"/><path d="M14 3v4h4"/><path d="M9 13h6M9 17h6"/>'],
  ["wiz", "WIZARDS", '<circle cx="12" cy="12" r="7"/><circle cx="12" cy="5" r="1.4"/><circle cx="19" cy="12" r="1.4"/><circle cx="12" cy="19" r="1.4"/><circle cx="5" cy="12" r="1.4"/>'],
  ["off", "OFFSETS", '<path d="M4 20V4M4 20h16"/><path d="M9 15l7-7"/><circle cx="16" cy="8" r="2"/>'],
  ["tool", "TOOLS", '<path d="M10 3h4v6l-1 2v10h-2V11l-1-2z"/>'],
  ["probe", "PROBE", '<circle cx="12" cy="17" r="3"/><path d="M12 14V3M8 6h8"/>'],
  ["dyn", "DYNAMICS", '<path d="M2 12h3l2-6 3 12 3-9 2 5 2-2h5"/>'],
  ["stat", "STATUS", '<path d="M12 3l9 16H3z"/><path d="M12 10v4M12 17v.5"/>'],
  ["set", "SETTINGS", '<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M5 19l2-2M17 7l2-2"/>'],
];
$("nav").innerHTML = pages.map(([id, l, icon]) => `<button data-p="${id}" aria-current="${id === "main" ? "page" : "false"}"><svg viewBox="0 0 24 24" aria-hidden="true">${icon}</svg>${l}${id === "stat" ? '<span class="badge" id="nav-stat-badge"></span>' : ""}</button>`).join("");
const paneIds = { main: "p-main", file: "p-file", wiz: "p-wiz", off: "p-off", tool: "p-tool", probe: "p-probe", dyn: "p-dyn", stat: "p-stat", set: "p-set" };
function showPage(id) {
  st.page = id;
  $$("#nav button").forEach((b) => b.setAttribute("aria-current", b.dataset.p === id ? "page" : "false"));
  for (const [k, v] of Object.entries(paneIds)) $(v).hidden = k !== id;
  if (id === "main") requestAnimationFrame(resize3d);
  if (id === "file") renderFiles();
  if (id === "off") renderOffsets();
  if (id === "tool") renderTools();
  if (id === "wiz") fillToolSelects();
  try { localStorage.setItem("pmc.page", id); } catch (e) { /* storage blocked */ }
}
$("nav").addEventListener("click", (e) => { const b = e.target.closest("button"); if (b) showPage(b.dataset.p); });

// ============================================================ DRO
$("dro").innerHTML = ["x", "y", "z"].map((a) => `
  <div class="axis"><span class="n">${a.toUpperCase()}</span><span class="h" id="h-${a}" title="Homed"></span>
    <span class="vals"><span class="v" id="d-${a}">0.000</span><span class="sub" id="ds-${a}">—</span></span>
    <span class="acts"><button data-zero="${a}" title="G10 L20: this axis to 0 in the active offset">ZERO</button><button data-touch="${a}" title="Touch off: type this axis' value">SET</button><button data-home="${a}" title="Home this axis">REF</button></span>
    <span class="fe"><b id="fe-${a}"></b></span></div>`).join("");
$("dro").addEventListener("click", (e) => {
  const b = e.target.closest("button"); if (!b) return;
  if (b.dataset.zero) zeroAxis(b.dataset.zero, 0);
  else if (b.dataset.touch) ask(`Touch off ${b.dataset.touch.toUpperCase()}`, `This position's ${off().n} value, in ${st.units}.`, "0", (v) => zeroAxis(b.dataset.touch, st.units === "in" ? v * 25.4 : v));
  else if (b.dataset.home) homeAxis(b.dataset.home);
});
function zeroAxis(a, val) {
  if (LIVE) return LIVE.zero(a, val);
  if (busy()) { msg("Not while the program runs.", "warn"); return; }
  const m = toMach(st.pos)[a];
  off()[a] = +(m - st.g92[a] - val).toFixed(4);
  st.pos[a] = val;
  msg(`G10 L20 P${st.wcs + 1} ${a.toUpperCase()}${val.toFixed(3)}: ${off().n} ${a.toUpperCase()} = ${off()[a].toFixed(3)}`, "ok");
  rebuildPath(); if (st.page === "off") renderOffsets();
}
function homeAxis(a) {
  if (LIVE) return LIVE.home(a);
  if (!ready("home")) return;
  st.homed[a] = false; updateChips();
  setTimeout(() => { st.homed[a] = true; updateChips(); msg(`${a.toUpperCase()} homed: absolute scale, no motion`, "ok"); }, 450);
}
$("b-homeall").addEventListener("click", () => LIVE ? LIVE.home("all") : ["z", "x", "y"].forEach((a, i) => setTimeout(() => homeAxis(a), i * 300)));
$("b-zeroall").addEventListener("click", () => { if (LIVE) return LIVE.zeroAll(); zeroAxis("x", 0); zeroAxis("y", 0); zeroAxis("z", 0); });
$("b-gotozero").addEventListener("click", () => { const sz = Math.max(st.pos.z, 10); moveTo([{ x: st.pos.x, y: st.pos.y, z: sz }, { x: 0, y: 0, z: sz }, { x: 0, y: 0, z: 5 }], `Go to ${off().n} X0 Y0 Z5`); });
$("b-park").addEventListener("click", () => { const w = toWork({ x: 500, y: 980, z: 0 }); moveTo([{ x: st.pos.x, y: st.pos.y, z: w.z }, { x: w.x, y: w.y, z: w.z }], "Park: Z up, the bridge to the back"); });
$$("#dro-mode button").forEach((b) => b.addEventListener("click", () => { st.dro = b.dataset.c; $$("#dro-mode button").forEach((x) => x.setAttribute("aria-pressed", String(x === b))); }));

// ============================================================ motion
function moveTo(points, label, feed = 0, quiet) {
  if (LIVE) return LIVE.moveTo(points, label, feed);
  if (!ready()) return false;
  if (busy()) { msg("Stop the program first.", "warn"); return false; }
  const segs = []; let a = { ...st.pos };
  for (const q of points) {
    const m = toMach(q); if (!inside(m)) { msg(`${label}: beyond the travel (machine ${m.x.toFixed(1)}, ${m.y.toFixed(1)}, ${m.z.toFixed(1)})`, "warn"); return false; }
    segs.push({ k: "mv", x0: a.x, y0: a.y, z0: a.z, x: q.x, y: q.y, z: q.z, f: feed, rapid: feed === 0, line: 0, tool: st.tool }); a = { ...q };
  }
  st.motion = plan({ segs, name: label, kind: "mdi" }); st.t = 0; st.seg = 0; setRun("run"); if (!quiet) msg(label, "info");
  return true;
}
function setRun(r) { st.run = r; updateChips(); }
let confirmPending = false, stepTarget = -1, holdLift = 0;
function startCycle(fromLine) {
  if (LIVE) return LIVE.cycleStart(fromLine);
  if (!ready()) return;
  if (st.motion && st.motion !== prog && busy()) { msg("A move is still running.", "warn"); return; }
  if ((st.run === "hold" || st.run === "step") && st.motion === prog) { setRun("run"); msg("Cycle resumed", "ok"); return; }
  if (st.confirm && !confirmPending) { confirmPending = true; msg("Press Cycle start again to confirm.", "warn"); setTimeout(() => (confirmPending = false), 4000); return; }
  confirmPending = false;
  st.motion = prog; st.t = 0; st.seg = 0;
  prog.segs[0].x0 = st.pos.x; prog.segs[0].y0 = st.pos.y; prog.segs[0].z0 = st.pos.z;
  if (fromLine) {
    const s = prog.segs.find((x) => x.line >= fromLine && x.k === "mv");
    if (s) { st.t = s.t0; s.x0 = st.pos.x; s.y0 = st.pos.y; s.z0 = Math.max(st.pos.z, 5); msg(`Run from ${nline(fromLine)}: the tool, spindle and offsets are set from the lines before it`, "info"); }
  }
  setRun("run"); msg(`Cycle start · ${prog.name}`, "ok");
}
$("b-start").addEventListener("click", () => startCycle(0));
$("b-hold").addEventListener("click", () => {
  if (LIVE) return LIVE.cycle(st.run === "hold" ? "resume" : "pause");
  if (st.run === "run" || st.run === "step") { setRun("hold"); msg("Feed hold" + (st.lift && st.spindle.dir ? ": Z lifted 5 mm, spindle running" : ""), "warn"); }
  else if (st.run === "hold") { setRun("run"); msg("Resumed", "ok"); }
});
$("b-step").addEventListener("click", () => {
  if (LIVE) return LIVE.cycle("step");
  if (st.motion !== prog || !busy()) startCycle(0);
  if (st.motion === prog) { stepTarget = st.seg + 1; setRun("step"); msg("Single block: one line, then hold", "info"); }
});
$("b-stop").addEventListener("click", stopAll);
function stopAll() {
  if (LIVE) return LIVE.cycle("stop");
  const was = st.run;
  setRun("idle"); st.motion = null; st.t = 0; st.seg = 0;
  if (was !== "idle" && was !== "done") msg("Stopped. The program is back at its first line.", "info");
}
$("b-runfrom").addEventListener("click", () => startCycle(st.selLine));

// ============================================================ machine power / estop
function updateChips() {
  const set = (id, cls, text) => { const e = $(id); e.className = "chip-st " + cls; e.lastChild.textContent = text; };
  set("ch-estop", st.estop ? "bad" : "ok", st.estop ? "E-STOP" : "E-STOP OK");
  set("ch-on", st.on ? "ok" : "", st.on ? "MACHINE ON" : "MACHINE OFF");
  const h = st.homed.x && st.homed.y && st.homed.z;
  set("ch-home", h ? "ok" : "warn", h ? "HOMED" : "NOT HOMED");
  set("ch-mode", "ok", LIVE ? ({ auto: "AUTO", mdi: "MDI", manual: "MANUAL" }[st.lmode] || "MANUAL") : st.page === "probe" ? "PROBE" : st.motion === prog && st.run !== "idle" ? "AUTO" : st.motion ? "MDI" : "MANUAL");
  set("ch-run", st.run === "run" ? "run" : st.run === "hold" || st.run === "step" ? "warn" : st.run === "done" ? "ok" : "", { idle: "IDLE", run: "RUNNING", hold: "FEED HOLD", step: "SINGLE BLOCK", done: "DONE" }[st.run]);
  $("b-power").setAttribute("aria-pressed", String(st.on));
  $("b-estop").setAttribute("aria-pressed", String(st.estop));
  ["x", "y", "z"].forEach((a) => $("h-" + a).classList.toggle("on", st.homed[a]));
}
function toggleEstop() {
  if (LIVE) return LIVE.estop();
  st.estop = !st.estop;
  if (st.estop) { st.on = false; stopAll(); st.spindle.dir = 0; msg("E-STOP: drives quick-stopped, spindle braked, holding brakes on", "bad"); }
  else msg("E-STOP released. Switch the machine on (F2).", "ok");
  updateChips();
}
function togglePower() {
  if (LIVE) return LIVE.power();
  if (st.estop) { msg("Release E-STOP first.", "warn"); return; }
  st.on = !st.on; if (!st.on) { stopAll(); st.spindle.dir = 0; }
  msg(st.on ? "Machine on: drives enabled, gantry link up (0.6 ms)" : "Machine off: drives disabled", st.on ? "ok" : "info"); updateChips();
}
$("b-estop").addEventListener("click", toggleEstop);
$("b-power").addEventListener("click", togglePower);

// ============================================================ spindle, coolant, overrides
function spindle(dir, s) {
  if (LIVE) return LIVE.spindle(dir, s);
  if (dir && !ready()) return;
  st.spindle.dir = dir; if (s !== undefined) st.spindle.s = s;
  if (dir && !st.spindle.s) st.spindle.s = 12000;
}
function setCool(k, v) { st.cool[k] = v; $("cl-" + k).setAttribute("aria-pressed", String(v)); }
$("sp-cw").addEventListener("click", () => { spindle(1); msg(`M3 S${st.spindle.s}`, "info"); });
$("sp-ccw").addEventListener("click", () => { spindle(-1); msg(`M4 S${st.spindle.s}`, "info"); });
$("sp-stop").addEventListener("click", () => { spindle(0); msg("M5", "info"); });
for (const k of ["flood", "mist", "air"]) $("cl-" + k).addEventListener("click", () => {
  if (LIVE) return LIVE.coolant(k, !st.cool[k]); setCool(k, !st.cool[k]); msg(`${k[0].toUpperCase() + k.slice(1)} ${st.cool[k] ? "on" : "off"}`, "info"); });
for (const [id, key] of [["o-feed", "feed"], ["o-rapid", "rapid"], ["o-spin", "spin"], ["o-maxv", "maxv"]]) {
  $(id).addEventListener("input", (e) => { st.ovr[key] = e.target.value / 100; $(id + "-v").textContent = e.target.value + " %"; if (LIVE) LIVE.override(key, st.ovr[key]); });
}
$$("[data-reset]").forEach((b) => b.addEventListener("click", () => { const i = $(b.dataset.reset); i.value = 100; i.dispatchEvent(new Event("input")); }));

// ============================================================ program view
let lastLine = 0;
function renderProgram() {
  $("h-prog").textContent = prog.name;
  $("pg-sub").textContent = `${prog.lines.length} lines · ${[...prog.tools].map((t) => "T" + t).join(" ")}`;
  $("gcode").innerHTML = prog.lines.map((l, i) => `<div id="gl-${i + 1}" data-l="${i + 1}"><span>${nline(i + 1)}</span>${esc(l)}</div>`).join("");
  lastLine = 0; st.selLine = 0; $("b-runfrom").disabled = true; $("b-runfrom").textContent = "Run from line";
}
$("gcode").addEventListener("click", (e) => {
  const d = e.target.closest("div[data-l]"); if (!d) return;
  $$("#gcode .sel").forEach((x) => x.classList.remove("sel"));
  d.classList.add("sel"); st.selLine = +d.dataset.l; $("b-runfrom").disabled = false;
  $("b-runfrom").textContent = `Run from ${nline(st.selLine)}`;
});
$("b-edit").addEventListener("click", () => { fileSel = library.indexOf(prog); showPage("file"); });
function loadProgram(p) {
  if (LIVE) return LIVE.loadProgram(p);
  if (busy()) { msg("Stop the program before loading another.", "warn"); return false; }
  if (!library.includes(p)) library.push(p);
  prog = p; stopAll(); renderProgram(); rebuildPath();
  msg(`Loaded ${p.name}: ${mmss(p.total)} estimated, ${Math.round(p.cutLen)} mm of cutting`, "ok");
  return true;
}

// ============================================================ FILE page
let fileSel = 0;
function renderFiles() {
  if (LIVE) return LIVE.renderFiles();
  $("files").innerHTML = library.map((p, i) => `<button class="file" data-i="${i}" aria-pressed="${i === fileSel}"><span><b>${esc(p.name)}</b><br><span class="muted">${p.lines.length} lines · ${[...p.tools].map((t) => "T" + t).join(" ")}</span></span><span class="num muted">${mmss(p.total)}</span><span class="btn sm ${p === prog ? "accent" : ""}">${p === prog ? "Loaded" : "Load"}</span></button>`).join("");
  selectFile(fileSel);
}
function selectFile(i) {
  if (LIVE) return LIVE.selectFile(i);
  fileSel = clamp(i, 0, library.length - 1);
  $$("#files .file").forEach((b) => b.setAttribute("aria-pressed", String(+b.dataset.i === fileSel)));
  renderPreflight(library[fileSel]);
}
// a program's preflight: travel, tools, time and predicted error, its code
function renderPreflight(p) {
  $("fl-name").textContent = p.name;
  const e = p.ext, mn = toMach({ x: e.x[0], y: e.y[0], z: e.z[0] }), mx = toMach({ x: e.x[1], y: e.y[1], z: e.z[1] });
  const fits = inside(mn) && inside(mx), missing = [...p.tools].filter((t) => !toolOf(t)), deep = e.z[0] < -STOCK.h;
  $("fl-cards").innerHTML = `
    <div class="card"><h3>Travel <span class="pill ${fits ? "ok" : "bad"}">${fits ? "FITS" : "OUTSIDE"}</span></h3><dl class="kv"><dt>X</dt><dd>${e.x[0].toFixed(1)} … ${e.x[1].toFixed(1)}</dd><dt>Y</dt><dd>${e.y[0].toFixed(1)} … ${e.y[1].toFixed(1)}</dd><dt>Z</dt><dd>${e.z[0].toFixed(1)} … ${e.z[1].toFixed(1)}</dd></dl><p class="note">In ${off().n}, checked against the soft limits.</p></div>
    <div class="card"><h3>Tools <span class="pill ${missing.length ? "bad" : "ok"}">${missing.length ? "MISSING" : "ALL IN THE TABLE"}</span></h3><dl class="kv">${[...p.tools].map((t) => { const tl = toolOf(t); return `<dt>T${t}</dt><dd>${tl ? "Ø" + tl.d.toFixed(1) + " · L " + tl.l.toFixed(2) + " · life " + tl.life + " %" : "not in the table"}</dd>`; }).join("")}</dl></div>
    <div class="card"><h3>Time and dynamics <span class="pill ${deep ? "warn" : "info"}">${deep ? "THROUGH THE STOCK" : "ESTIMATE"}</span></h3><dl class="kv"><dt>Cycle (planner, 0.5 g)</dt><dd>${mmss(p.total)}</dd><dt>Cutting length</dt><dd>${Math.round(p.cutLen)} mm</dd><dt>Predicted path error p99</dt><dd>${Math.round(pct(p.segs.filter((s) => s.cut).map((s) => s.errMean), 0.99))} µm</dd><dt>Tool changes</dt><dd>${p.segs.filter((s) => s.k === "tc").length}</dd></dl></div>`;
  $("fl-code").textContent = p.lines.slice(0, 400).join("\n") + (p.lines.length > 400 ? `\n… ${p.lines.length - 400} more lines` : "");
}
$("files").addEventListener("click", (e) => {
  const b = e.target.closest(".file"); if (!b) return;
  const i = +b.dataset.i;
  if (e.target.closest(".btn") && library[i] !== prog) { if (loadProgram(library[i])) { fileSel = i; renderFiles(); return; } }
  selectFile(i);
});

// ============================================================ WIZARDS
function fillToolSelects() {
  const opts = st.tools.filter((t) => t.t !== 99).map((t) => `<option value="${t.t}">T${t.t} · Ø${t.d}</option>`).join("");
  for (const [id, def] of [["fz-t", 12], ["bc-t", 5], ["cp-t", 3]]) { const s = $(id), v = s.value || def; s.innerHTML = opts; s.value = toolOf(+v) ? v : String(st.tools[0].t); }
}
const num = (id) => +$(id).value;
$("fz-go").addEventListener("click", () => { if (loadProgram(plan(genFacing({ w: num("fz-w"), l: num("fz-l"), d: Math.abs(num("fz-d")), s: clamp(num("fz-s"), 10, 95), f: num("fz-f"), t: num("fz-t") })))) showPage("main"); });
$("bc-go").addEventListener("click", () => { if (loadProgram(plan(genBolt({ x: num("bc-x"), y: num("bc-y"), r: num("bc-r"), n: clamp(Math.round(num("bc-n")), 2, 36), a: num("bc-a"), d: -Math.abs(num("bc-d")), p: Math.max(0.5, num("bc-p")), t: num("bc-t") })))) showPage("main"); });
$("cp-go").addEventListener("click", () => { if (loadProgram(plan(genCircPocket({ x: num("cp-x"), y: num("cp-y"), dia: Math.max(num("cp-dia"), 8), d: -Math.abs(num("cp-d")), sd: Math.max(0.2, num("cp-sd")), t: num("cp-t") })))) showPage("main"); });

// ============================================================ OFFSETS page
function renderOffsets() {
  $("off-t").innerHTML = `<thead><tr><th>System</th><th class="n">X</th><th class="n">Y</th><th class="n">Z</th><th></th></tr></thead><tbody>` +
    st.offsets.map((o, i) => `<tr class="${i === st.wcs ? "active" : ""}"><td>${o.n}${i === st.wcs ? " · active" : ""}</td>${["x", "y", "z"].map((a) => `<td class="n"><input id="of-${i}-${a}" type="number" step="0.001" value="${o[a].toFixed(3)}" aria-label="${o.n} ${a.toUpperCase()}"></td>`).join("")}<td><button class="btn sm" data-act="${i}">Use</button></td></tr>`).join("") +
    `<tr><td>G92</td>${["x", "y", "z"].map((a) => `<td class="n">${st.g92[a].toFixed(3)}</td>`).join("")}<td></td></tr></tbody>`;
  $("rot-v").textContent = st.rot.toFixed(3) + "°";
}
$("off-t").addEventListener("change", (e) => { const m = e.target.id.match(/^of-(\d)-([xyz])$/); if (!m) return; if (LIVE) return LIVE.offset(+m[1], m[2], +e.target.value); const mach = toMach(st.pos); st.offsets[+m[1]][m[2]] = +e.target.value; if (+m[1] === st.wcs) { st.pos = toWork(mach); rebuildPath(); } msg(`G10 L2 P${+m[1] + 1} ${m[2].toUpperCase()}${(+e.target.value).toFixed(3)}`, "ok"); });
$("off-t").addEventListener("click", (e) => { const b = e.target.closest("button[data-act]"); if (b) setWcs(+b.dataset.act); });
function setWcs(i) {
  if (LIVE) return LIVE.wcs(i);
  if (busy()) { msg("Not while the program runs.", "warn"); return; }
  const m = toMach(st.pos); st.wcs = i; st.pos = toWork(m); rebuildPath(); renderOffsets(); $("dro-wcs").textContent = off().n; msg(`${off().n} active`, "info");
}
$("off-set").addEventListener("click", () => { if (LIVE) return LIVE.zeroAll(); if (busy()) return; const m = toMach(st.pos); off().x = +(m.x - st.g92.x).toFixed(3); off().y = +(m.y - st.g92.y).toFixed(3); off().z = +(m.z - st.g92.z).toFixed(3); st.pos = { x: 0, y: 0, z: 0 }; rebuildPath(); renderOffsets(); msg(`${off().n} set to the current position`, "ok"); });
$("off-g92").addEventListener("click", () => { if (LIVE) return LIVE.g92clear(); const m = toMach(st.pos); st.g92 = { x: 0, y: 0, z: 0 }; st.pos = toWork(m); rebuildPath(); renderOffsets(); msg("G92.1: G92 cleared", "info"); });
$("tp-go").addEventListener("click", () => probeCycle("plate"));

// ============================================================ TOOLS page
function renderTools() {
  $("tool-t").innerHTML = `<thead><tr><th>T</th><th class="n">Pocket</th><th class="n">Ø mm</th><th class="n">Length</th><th class="n">Wear</th><th class="n">Life</th><th>Comment</th></tr></thead><tbody>` +
    st.tools.map((t, i) => `<tr class="${t.t === st.tool ? "active" : t.t === st.toolSel ? "sel" : ""}" data-t="${t.t}"><td class="num">T${t.t}${t.t === st.tool ? " ● in spindle" : ""}</td>
      <td class="n">${t.p}</td><td class="n"><input data-k="d" data-i="${i}" type="number" step="0.001" value="${t.d.toFixed(3)}" aria-label="T${t.t} diameter"></td>
      <td class="n"><input data-k="l" data-i="${i}" type="number" step="0.001" value="${t.l.toFixed(3)}" aria-label="T${t.t} length"></td>
      <td class="n"><input data-k="wear" data-i="${i}" type="number" step="0.001" value="${t.wear.toFixed(3)}" aria-label="T${t.t} wear"></td>
      <td class="n"><span class="pill ${t.life < 70 ? "warn" : "ok"}">${t.life} %</span></td>
      <td><input class="txt" data-k="c" data-i="${i}" value="${esc(t.c)}" aria-label="T${t.t} comment"></td></tr>`).join("") + "</tbody>";
}
$("tool-t").addEventListener("click", (e) => { const r = e.target.closest("tr[data-t]"); if (!r || e.target.tagName === "INPUT") return; st.toolSel = +r.dataset.t; renderTools(); });
$("tool-t").addEventListener("change", (e) => { const i = e.target.dataset.i; if (i === undefined) return; const t = st.tools[+i], k = e.target.dataset.k; t[k] = k === "c" ? e.target.value : +e.target.value; if (LIVE) return LIVE.toolsSave(); msg(`Tool table: T${t.t} ${{ d: "diameter", l: "length", wear: "wear", c: "comment" }[k]} updated`, "ok"); if (t.t === st.tool) buildTool(); });
$("tl-load").addEventListener("click", () => toolChange(st.toolSel));
$("tl-add").addEventListener("click", () => { if (LIVE) return LIVE.toolAdd(); const n = Math.max(...st.tools.filter((t) => t.t < 99).map((t) => t.t)) + 1; st.tools.splice(st.tools.length - 1, 0, { t: n, p: n, d: 8, l: 50, wear: 0, life: 100, c: "new tool" }); st.toolSel = n; renderTools(); fillToolSelects(); msg(`T${n} added`, "info"); });
$("tl-del").addEventListener("click", () => { if (LIVE) return LIVE.toolDelete(st.toolSel); if (st.toolSel === st.tool) { msg("Unload the tool before deleting it.", "warn"); return; } st.tools = st.tools.filter((t) => t.t !== st.toolSel); renderTools(); fillToolSelects(); msg(`T${st.toolSel} deleted`, "info"); });
$("tl-meas").addEventListener("click", () => { if (LIVE) return LIVE.toolMeasure();
  const above = toWork({ x: 980, y: 20, z: -40 }), touch = toWork({ x: 980, y: 20, z: -110 }), safe = toWork({ x: 0, y: 0, z: 0 }).z;
  if (moveTo([{ ...st.pos, z: safe }, { x: above.x, y: above.y, z: safe }, above, { ...above, z: touch.z }, { ...above, z: safe }], `Measuring T${st.tool} on the tool setter`, 0)) {
    st.motion.segs[3].rapid = false; st.motion.segs[3].f = 300; plan(st.motion);
    st.onDone = () => { const t = toolOf(st.tool); if (!t) return; t.l = +(t.l + (Math.random() - .5) * 0.02).toFixed(3); if (st.page === "tool") renderTools(); msg(`T${st.tool} length ${t.l.toFixed(3)} mm (setter, latch 25 mm/min)`, "ok"); };
  }
});
function toolChange(t) {
  if (LIVE) return LIVE.toolLoad(t);
  if (!ready()) return; if (busy()) { msg("Not while the program runs.", "warn"); return; }
  if (!toolOf(t)) { msg(`T${t} is not in the tool table.`, "warn"); return; }
  st.tool = t; buildTool(); if (st.page === "tool") renderTools(); msg(`T${t} M6: ${toolOf(t).c}`, "ok");
}

// ============================================================ PROBE page
const probeOps = [
  ["z", "Z surface", '<rect x="6" y="28" width="52" height="12"/><path d="M32 6v16"/><circle cx="32" cy="24" r="2.4"/>'],
  ["corner", "Outside corner", '<path d="M14 36V14h36"/><path d="M8 8l4 4M56 20l-4-4M20 42l-4-4"/>'],
  ["edgex", "Edge X", '<path d="M30 6v34"/><path d="M14 22h10"/><circle cx="12" cy="22" r="2.4"/>'],
  ["edgey", "Edge Y", '<path d="M8 28h48"/><path d="M32 10v10"/><circle cx="32" cy="8" r="2.4"/>'],
  ["bore", "Bore centre", '<circle cx="32" cy="23" r="15"/><path d="M22 23h20M32 23v8"/>'],
  ["boss", "Boss centre", '<circle cx="32" cy="23" r="10"/><path d="M8 23h8M48 23h8"/>'],
  ["angle", "Edge angle", '<path d="M8 34l48-10"/><circle cx="20" cy="20" r="2.4"/><circle cx="44" cy="15" r="2.4"/>'],
  ["tool", "Tool setter", '<rect x="24" y="30" width="16" height="10"/><path d="M32 6v18"/>'],
];
let probeOp = "z", probeRes = null;
$("probe-ops").innerHTML = probeOps.map(([k, l, s]) => `<button class="op" data-op="${k}" aria-pressed="${k === probeOp}"><svg viewBox="0 0 64 46" fill="none" stroke="currentColor" stroke-width="1.6" aria-hidden="true">${s}</svg>${l}</button>`).join("");
$("probe-ops").addEventListener("click", (e) => { const b = e.target.closest(".op"); if (!b) return; probeOp = b.dataset.op; $$("#probe-ops .op").forEach((x) => x.setAttribute("aria-pressed", String(x === b))); });
$("pr-go").addEventListener("click", () => {
  if (!ready()) return;
  if (busy()) { msg("Stop the program first.", "warn"); return; }
  if (probeOp === "tool") { $("tl-meas").click(); return; }
  if (st.tool !== 99) { msg("Load the probe first: T99 M6 (Tools page or MDI).", "warn"); return; }
  probeCycle(probeOp);
});
function probeCycle(op) {
  if (LIVE) return LIVE.probe(op);
  const name = op === "plate" ? "Touch plate Z" : probeOps.find((o) => o[0] === op)[1];
  const top = op === "plate" ? +$("tp-th").value : 0, startZ = Math.max(st.pos.z, top + 5);
  if (!moveTo([{ x: st.pos.x, y: st.pos.y, z: startZ }, { x: st.pos.x, y: st.pos.y, z: top + 0.3 }, { x: st.pos.x, y: st.pos.y, z: startZ }], `${name}: probing`, 300)) return;
  $("pr-op").textContent = name;
  st.onDone = () => {
    const n = () => (Math.random() - .5) * 0.004, m = toMach(st.pos);
    const r = { z: [["Surface Z (machine)", (-195 + n()).toFixed(4)], ["Repeat, 2 touches", "0.0012"]],
      corner: [["Corner X (machine)", (440 + n()).toFixed(4)], ["Corner Y (machine)", (460 + n()).toFixed(4)], ["Angle", "0.012°"]],
      edgex: [["Edge X (machine)", (440 + n()).toFixed(4)]], edgey: [["Edge Y (machine)", (460 + n()).toFixed(4)]],
      bore: [["Centre X (machine)", (m.x + n()).toFixed(4)], ["Centre Y (machine)", (m.y + n()).toFixed(4)], ["Diameter", (32.005 + n()).toFixed(4)]],
      boss: [["Centre X (machine)", (m.x + n()).toFixed(4)], ["Centre Y (machine)", (m.y + n()).toFixed(4)], ["Diameter", (40.002 + n()).toFixed(4)]],
      angle: [["Edge angle", "0.018°"], ["Over", "100.000 mm"]], plate: [["Surface Z (machine)", (-195 + n()).toFixed(4)], ["Plate", $("tp-th").value + " mm"]] }[op];
    probeRes = { op, r };
    $("pr-res").innerHTML = r.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join("");
    $("pr-apply").disabled = op === "plate";
    if (op === "plate") { const mach = toMach(st.pos); off().z = -195; st.pos = toWork(mach); rebuildPath(); $("tp-res").textContent = `${off().n} Z = −195.000 (the plate's ${$("tp-th").value} mm taken off)`; if (st.page === "off") renderOffsets(); }
    msg(`${name}: ${r.map((x) => x[0] + " " + x[1]).join(" · ")}`, "ok");
  };
}
$("pr-apply").addEventListener("click", () => {
  if (!probeRes) return;
  if (LIVE) return LIVE.probeApply(probeRes);
  const r = probeRes.r, o = off(), mach = toMach(st.pos);
  if (probeRes.op === "z") o.z = +(+r[0][1]).toFixed(3);
  if (probeRes.op === "corner") { o.x = +(+r[0][1]).toFixed(3); o.y = +(+r[1][1]).toFixed(3); }
  if (probeRes.op === "edgex") o.x = +(+r[0][1]).toFixed(3);
  if (probeRes.op === "edgey") o.y = +(+r[0][1]).toFixed(3);
  if (probeRes.op === "bore" || probeRes.op === "boss") { o.x = +(+r[0][1]).toFixed(3); o.y = +(+r[1][1]).toFixed(3); }
  if (probeRes.op === "angle") st.rot = 0.018;
  st.pos = toWork(mach); rebuildPath(); msg(`${o.n} set from the probe result`, "ok");
});

// ============================================================ MDI
const mdiHist = ["G0 Z30", "G10 L20 P1 X0 Y0", "T3 M6", "M3 S18000", "G0 X60 Y40"];
function renderMdi() { $("mdi-hist").innerHTML = mdiHist.map((c) => `<li>${esc(c)}</li>`).join(""); }
$("mdi-hist").addEventListener("click", (e) => { const li = e.target.closest("li"); if (li) { $("mdi").value = li.textContent; $("mdi").focus(); } });
$("mdi-form").addEventListener("submit", (e) => { e.preventDefault(); const c = $("mdi").value.trim(); if (c) runMdi(c); });
function runMdi(c) {
  if (LIVE) { mdiHist.unshift(c); if (mdiHist.length > 30) mdiHist.pop(); renderMdi(); $("mdi").value = ""; return LIVE.mdi(c); }
  if (!ready()) return;
  if (busy()) { msg("MDI waits until the program stops.", "warn"); return; }
  const u = c.toUpperCase();
  const w = {}; for (const m of u.matchAll(/([A-Z])\s*(-?\d*\.?\d+)/g)) (w[m[1]] = w[m[1]] || []).push(+m[2]);
  const has = (l, v) => (w[l] || []).includes(v);
  mdiHist.unshift(c); if (mdiHist.length > 30) mdiHist.pop(); renderMdi(); $("mdi").value = "";
  if (has("G", 10) && has("L", 20)) { const pidx = clamp(Math.round((w.P || [st.wcs + 1])[0]) - 1, 0, 8); if (pidx !== st.wcs) setWcs(pidx); for (const a of ["X", "Y", "Z"]) if (w[a]) zeroAxis(a.toLowerCase(), w[a][0]); return; }
  const g5 = [54, 55, 56, 57, 58, 59].find((g) => has("G", g)); if (g5) { setWcs(g5 - 54); return; }
  if (w.M && w.M.includes(6) && w.T) { toolChange(w.T[0]); return; }
  if (w.M && (w.M.includes(3) || w.M.includes(4))) { spindle(w.M.includes(3) ? 1 : -1, w.S ? w.S[0] : undefined); msg(c, "ok"); return; }
  if (w.M && w.M.includes(5)) { spindle(0); msg(c, "ok"); return; }
  if (w.M && (w.M.includes(7) || w.M.includes(8) || w.M.includes(9))) { setCool("flood", w.M.includes(8)); setCool("mist", w.M.includes(7)); msg(c, "ok"); return; }
  if (w.G && (w.G.includes(0) || w.G.includes(1)) && (w.X || w.Y || w.Z)) {
    const t = { x: w.X ? w.X[0] : st.pos.x, y: w.Y ? w.Y[0] : st.pos.y, z: w.Z ? w.Z[0] : st.pos.z };
    moveTo([t], c, w.G.includes(1) ? (w.F ? w.F[0] : 1000) : 0); return;
  }
  msg(`${c}: accepted (simulated)`, "info");
}

// ============================================================ jog
const incs = [0.001, 0.01, 0.1, 1, 10, 0];
$("jog-inc").innerHTML = incs.map((v) => `<button data-v="${v}" aria-pressed="${v === st.jogInc}">${v ? v : "CONT"}</button>`).join("");
$("jog-inc").addEventListener("click", (e) => { const b = e.target.closest("button"); if (!b) return; st.jogInc = +b.dataset.v; $$("#jog-inc button").forEach((x) => x.setAttribute("aria-pressed", String(x === b))); });
$("jog-rate").addEventListener("input", (e) => { st.jogRate = +e.target.value; $("jog-rate-v").textContent = st.jogRate + " mm/min"; });
let jogHeld = null;
function jogStep(dir) {
  if (LIVE) return LIVE.jog(dir);
  if (!ready()) return;
  if (busy()) { msg("Jogging is off while the program runs.", "warn"); return; }
  if (dir === "home") { $("b-gotozero").click(); return; }
  const a = dir[0], s = dir[1] === "+" ? 1 : -1;
  if (st.jogInc === 0) { jogHeld = { a, s }; return; }
  const t = { ...st.pos }; t[a] += s * st.jogInc;
  if (!inside(toMach(t))) { msg(`Soft limit: ${a.toUpperCase()} can't go further`, "warn"); return; }
  st.pos = t;
}
$("jogpad").addEventListener("pointerdown", (e) => { const b = e.target.closest("button"); if (b) jogStep(b.dataset.j); });
addEventListener("pointerup", () => { if (LIVE && jogHeld) LIVE.jogEnd(); jogHeld = null; });
function subtab(k) { $("st-mdi").setAttribute("aria-selected", String(k === "mdi")); $("st-jog").setAttribute("aria-selected", String(k === "jog")); $("sp-mdi").hidden = k !== "mdi"; $("sp-jog").hidden = k !== "jog"; }
$("st-mdi").addEventListener("click", () => subtab("mdi"));
$("st-jog").addEventListener("click", () => subtab("jog"));
const keyJog = { ArrowLeft: "x-", ArrowRight: "x+", ArrowUp: "y+", ArrowDown: "y-", PageUp: "z+", PageDown: "z-" };
addEventListener("keydown", (e) => {
  const inField = /INPUT|TEXTAREA|SELECT/.test(document.activeElement && document.activeElement.tagName);
  if (e.key === "F1") { e.preventDefault(); toggleEstop(); return; }
  if (e.key === "F2") { e.preventDefault(); togglePower(); return; }
  if (e.ctrlKey && e.key.toLowerCase() === "r") { e.preventDefault(); startCycle(0); return; }
  if (e.ctrlKey && e.key.toLowerCase() === "m") { e.preventDefault(); showPage("main"); subtab("mdi"); $("mdi").focus(); return; }
  if (e.key === "Escape") { if (!$("dlg").hidden) { $("dlg").hidden = true; return; } if (st.run === "run") $("b-hold").click(); else stopAll(); return; }
  if (inField || !st.kbd || !keyJog[e.key]) return;
  e.preventDefault();
  if (st.jogInc === 0) { if (!e.repeat) jogStep(keyJog[e.key]); } else jogStep(keyJog[e.key]);
});
addEventListener("keyup", (e) => { if (keyJog[e.key]) { if (LIVE && jogHeld) LIVE.jogEnd(); jogHeld = null; } });

// ============================================================ settings
const segSet = (id, attr, fn) => $$(`#${id} button`).forEach((b) => b.addEventListener("click", () => { $$(`#${id} button`).forEach((x) => x.setAttribute("aria-pressed", String(x === b))); fn(b.dataset[attr]); }));
segSet("set-units", "u", (v) => { st.units = v; $("f-units").textContent = v; });
segSet("set-dec", "d", (v) => (st.dec = +v));
segSet("set-exag", "x", (v) => { st.exag = +v; $("defl-btn").textContent = "deflection ×" + v; });
for (const [id, key] of [["set-kbd", "kbd"], ["set-m1", "m1"], ["set-bd", "bd"], ["set-lift", "lift"], ["set-confirm", "confirm"]]) {
  $(id).addEventListener("click", () => { st[key] = !st[key]; $(id).setAttribute("aria-pressed", String(st[key])); $(id).textContent = st[key] ? "On" : "Off"; if (LIVE && (key === "m1" || key === "bd")) LIVE.opt(key, st[key]); });
}
$("ev-clear").addEventListener("click", () => { events.length = 0; renderEvents(); });
segSet("ev-filter", "f", (v) => { evFilter = v; renderEvents(); });
for (const k of ["shaper", "defl", "shrink", "jerk"]) {
  $("pc-" + k).addEventListener("click", () => {
    st.pc[k] = !st.pc[k]; $("pc-" + k).setAttribute("aria-pressed", String(st.pc[k]));
    if (LIVE) LIVE.pcomp(k, st.pc[k]);
    const on = Object.values(st.pc).some(Boolean); $("pc-pill").textContent = on ? "ON" : "OFF"; $("pc-pill").className = "pill " + (on ? "ok" : "off");
    $("pc-delay").textContent = (st.pc.shaper ? 14 : 0) + (st.pc.jerk ? 10 : 0) + " ms";
    msg(`pcomp ${k} ${st.pc[k] ? "on" : "off"} (from the next standstill)`, "info");
  });
}
$("b-recal").addEventListener("click", () => { if (LIVE) { $("recal-note").textContent = "On the machine: run linuxcnc/dynwiz from the controller's console; its result shows here when the backend reads dynwiz.json (not yet)."; return; } if (busy()) { $("recal-note").textContent = "Stop the program first: the measurement moves the machine."; return; } $("recal-note").textContent = "Queued: mass, modes, circle, verify. Clear the table, then confirm at the machine."; msg("Re-measure requested (dynwiz run)", "info"); });

// ============================================================ dialog
let dlgCb = null;
function ask(title, note, val, cb) { $("dlg-t").textContent = title; $("dlg-n").textContent = note; $("dlg-in").value = val; dlgCb = cb; $("dlg").hidden = false; $("dlg-in").focus(); $("dlg-in").select(); }
$("dlg-form").addEventListener("submit", (e) => { e.preventDefault(); $("dlg").hidden = true; if (dlgCb) dlgCb(+$("dlg-in").value); });
$("dlg-cancel").addEventListener("click", () => ($("dlg").hidden = true));

// ============================================================ 3D: the machine
let R = null;
function init3d() {
  if (!window.THREE) { $("vfallback").hidden = false; return; }
  const cv = $("scene");
  let renderer;
  try { renderer = new THREE.WebGLRenderer({ canvas: cv, antialias: true }); } catch (e) { $("vfallback").hidden = false; return; }
  renderer.setPixelRatio(Math.min(2, devicePixelRatio || 1));
  renderer.setClearColor(0x080c11, 1);
  const scene = new THREE.Scene();
  scene.fog = new THREE.Fog(0x080c11, 5000, 11000);
  const cam = new THREE.PerspectiveCamera(36, 1, 5, 30000);
  cam.up.set(0, 0, 1);
  const controls = THREE.OrbitControls ? new THREE.OrbitControls(cam, cv) : null;
  if (controls) { controls.enableDamping = true; controls.dampingFactor = 0.12; controls.screenSpacePanning = true; controls.addEventListener("start", () => { if (R) { R.follow = false; R.camAnim = null; $$("[data-cam]").forEach((b) => b.setAttribute("aria-pressed", "false")); } }); }
  scene.add(new THREE.HemisphereLight(0xd6e6ff, 0x1a2230, 1.25));
  const key = new THREE.DirectionalLight(0xffffff, 1.35); key.position.set(-1500, -2200, 3000); scene.add(key);
  const rim = new THREE.DirectionalLight(0x7fd8ff, 0.7); rim.position.set(2500, 2500, 800); scene.add(rim);

  const M = (color, metal = .5, rough = .55, extra = {}) => new THREE.MeshStandardMaterial({ color, metalness: metal, roughness: rough, ...extra });
  const mat = {
    frame: M(0x313d4b, .45, .55), table: M(0x4a5868, .55, .38), slot: M(0x0b0f14, .2, .9), rail: M(0x9aa6b4, .9, .25),
    magnet: M(0x161c24, .3, .7), bridge: M(0x3c5168, .45, .42), accent: M(0x4cc9ff, .2, .4, { emissive: 0x0b3346, emissiveIntensity: .9 }),
    carriage: M(0x4a6078, .45, .4), spindle: M(0xdfe7ef, .6, .25), dark: M(0x1a212b, .3, .6), stock: M(0xd2d9e0, .6, .3),
    vise: M(0x56677b, .55, .4), tool: M(0xeef3f8, .7, .2),
    coil: M(0x1f6f8f, .4, .4, { emissive: 0x0a2c3a, emissiveIntensity: .8 }), feet: M(0x0d1117, .2, .9),
  };
  const edgeMat = new THREE.LineBasicMaterial({ color: 0x4cc9ff, transparent: true, opacity: .16 });
  const box = (w, d, h, m, x, y, z, edges = true) => {
    const g = new THREE.BoxGeometry(w, d, h), mesh = new THREE.Mesh(g, m); mesh.position.set(x, y, z);
    if (edges) mesh.add(new THREE.LineSegments(new THREE.EdgesGeometry(g), edgeMat));
    return mesh;
  };
  const cyl = (r, h, m, x, y, z, seg = 32) => { const g = new THREE.CylinderGeometry(r, r, h, seg); g.rotateX(Math.PI / 2); const mesh = new THREE.Mesh(g, m); mesh.position.set(x, y, z); return mesh; };

  // the frame on its feet, the table with T-slots, the side beams with rails and magnet tracks
  const machine = new THREE.Group(); scene.add(machine);
  for (const [fx, fy] of [[-180, -160], [1180, -160], [-180, 1160], [1180, 1160], [500, -160], [500, 1160]]) {
    machine.add(box(120, 120, 560, mat.frame, fx, fy, -400)); machine.add(cyl(70, 30, mat.feet, fx, fy, -695));
  }
  machine.add(box(1520, 1480, 100, mat.frame, 500, 500, -130));
  machine.add(box(1180, 1140, 60, mat.table, 500, 500, -30));
  for (let x = -50; x <= 1050; x += 100) machine.add(box(14, 1140, 2, mat.slot, x, 500, 1, false));
  for (const sx of [-140, 1140]) {
    machine.add(box(110, 1400, 180, mat.frame, sx, 500, 30));
    machine.add(box(24, 1360, 22, mat.rail, sx, 500, 131));
    machine.add(box(70, 1360, 70, mat.magnet, sx + (sx < 500 ? -82 : 82), 500, 95));
    machine.add(box(4, 1300, 14, mat.accent, sx + (sx < 500 ? 58 : -58), 500, 100, false)); // the scale
  }
  // tool setter
  machine.add(cyl(18, 40, mat.dark, 980, 20, 20)); machine.add(cyl(12, 6, mat.accent, 980, 20, 43));
  // the stock in its fixture (placed from the work offset)
  const fixture = new THREE.Group(); scene.add(fixture);
  fixture.add(box(STOCK.w + 70, STOCK.l + 40, 14, mat.vise, STOCK.w / 2, STOCK.l / 2, 7));
  fixture.add(box(STOCK.w + 70, 22, 34, mat.vise, STOCK.w / 2, -12, 17)); fixture.add(box(STOCK.w + 70, 22, 34, mat.vise, STOCK.w / 2, STOCK.l + 12, 17));
  const stockMesh = box(STOCK.w, STOCK.l, STOCK.h, mat.stock, STOCK.w / 2, STOCK.l / 2, 14 + STOCK.h / 2); fixture.add(stockMesh);
  // the work origin: on the stock's top front-left corner
  const triad = new THREE.Group(); fixture.add(triad);
  for (const [d, c] of [[[1, 0, 0], 0xff4d5e], [[0, 1, 0], 0x5fe0a0], [[0, 0, 1], 0x4cc9ff]]) triad.add(new THREE.ArrowHelper(new THREE.Vector3(...d), new THREE.Vector3(0, 0, 0), 40, c, 9, 5));

  // the bridge: Y bearing carriages, linear motor coils, uprights, the beam with the X axis
  const bridge = new THREE.Group(); machine.add(bridge);
  for (const sx of [-140, 1140]) {
    bridge.add(box(150, 190, 40, mat.carriage, sx, 0, 162));
    bridge.add(box(60, 170, 54, mat.coil, sx + (sx < 500 ? -82 : 82), 0, 102));
    bridge.add(box(120, 150, 300, mat.bridge, sx, 0, 330));
  }
  bridge.add(box(1420, 150, 170, mat.bridge, 500, 0, 560));
  bridge.add(box(1300, 10, 8, mat.accent, 500, -78, 630, false));
  bridge.add(box(1240, 30, 30, mat.rail, 500, -88, 600)); bridge.add(box(1240, 30, 30, mat.rail, 500, -88, 520));
  bridge.add(box(1240, 24, 50, mat.magnet, 500, -80, 470));
  for (let i = 0; i < 24; i++) bridge.add(box(40, 60, 26, mat.dark, -80 + i * 48, 10, 660, false)); // cable chain
  // the X carriage, the Z ram, the spindle and the tool: the tool tip sits at the carriage's x, 220 mm in front of the beam
  const carriage = new THREE.Group(); bridge.add(carriage);
  carriage.add(box(240, 50, 280, mat.carriage, 0, -130, 540));
  carriage.add(box(200, 20, 60, mat.coil, 0, -100, 470));
  carriage.add(box(120, 8, 12, mat.accent, 0, -158, 650, false));
  const ram = new THREE.Group(); carriage.add(ram);
  ram.add(box(130, 110, 460, mat.carriage, 0, -210, 470));
  ram.add(box(8, 6, 300, mat.accent, 0, -268, 520, false));
  const headG = new THREE.Group(); ram.add(headG);
  headG.add(cyl(62, 210, mat.spindle, 0, -220, 175, 40));
  headG.add(cyl(66, 14, mat.accent, 0, -220, 275, 40));
  headG.add(cyl(36, 26, mat.dark, 0, -220, 58, 32));
  const toolG = new THREE.Group(); toolG.position.set(0, -220, 0); headG.add(toolG);
  const coolant = new THREE.Group(); headG.add(coolant);
  for (const a of [0.6, 2.4, 4.2]) { const n = cyl(4, 60, mat.accent, 48 * Math.cos(a), -220 + 48 * Math.sin(a), 52, 8); n.rotation.y = .45 * Math.cos(a); n.rotation.x = -.45 * Math.sin(a); coolant.add(n); }

  // the travel limits, the toolpath, the trail of the tool, chips
  const limits = new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(1000, 1000, 215)), new THREE.LineBasicMaterial({ color: 0xffb547, transparent: true, opacity: .6 }));
  limits.position.set(500, 500, ZTOP - 215 / 2); scene.add(limits);
  const pathGeo = new THREE.BufferGeometry();
  const pathLine = new THREE.LineSegments(pathGeo, new THREE.LineBasicMaterial({ vertexColors: true })); scene.add(pathLine);
  const trailN = 2000, trailPos = new Float32Array(trailN * 3);
  const trailGeo = new THREE.BufferGeometry(); trailGeo.setAttribute("position", new THREE.BufferAttribute(trailPos, 3)); trailGeo.setDrawRange(0, 0);
  const trail = new THREE.Line(trailGeo, new THREE.LineBasicMaterial({ color: 0xff8a3d })); scene.add(trail);
  const sparkN = 80, sparkPos = new Float32Array(sparkN * 3).fill(-5000), sparkVel = new Float32Array(sparkN * 3), sparkLife = new Float32Array(sparkN);
  const sparkGeo = new THREE.BufferGeometry(); sparkGeo.setAttribute("position", new THREE.BufferAttribute(sparkPos, 3));
  scene.add(new THREE.Points(sparkGeo, new THREE.PointsMaterial({ color: 0xffa25c, size: 3 })));

  R = { renderer, cam, controls, scene, machine, bridge, carriage, ram, headG, toolG, coolant, fixture, limits, pathGeo, pathLine, trail, trailGeo, trailPos, trailN, trailLen: 0, sparkGeo, sparkPos, sparkVel, sparkLife, sparkN, mat, cyl, camAnim: null, follow: false };
  buildTool(); rebuildPath(); setCam("iso", true);
  new ResizeObserver(resize3d).observe($("viewer"));
  resize3d();
}
function resize3d() {
  if (!R) return;
  const r = $("viewer").getBoundingClientRect(); if (r.width < 10 || r.height < 10) return;
  R.renderer.setSize(r.width, r.height, false); R.cam.aspect = r.width / r.height; R.cam.updateProjectionMatrix();
}
function buildTool() {
  const t = toolOf(st.tool) || { d: 6, l: 50, c: "" };
  $("v-tool").textContent = "T" + st.tool; $("f-tool").textContent = "T" + st.tool;
  $("sp-tool").textContent = `T${st.tool} Ø${t.d} · ${t.c.replace(/^[\d.]+ mm /, "")}`;
  if (!R) return;
  while (R.toolG.children.length) R.toolG.remove(R.toolG.children[0]);
  // between the tip (local z 0) and the spindle nose (z 45): the holder, then the cutter
  const holder = new THREE.Mesh(new THREE.CylinderGeometry(20, 27, 20, 32), R.mat.dark); holder.rotation.x = Math.PI / 2; holder.position.set(0, 0, 35); R.toolG.add(holder);
  if (st.tool === 99) {
    R.toolG.add(R.cyl(2.2, 24, R.mat.tool, 0, 0, 13.5, 12));
    const ball = new THREE.Mesh(new THREE.SphereGeometry(t.d / 2 + 1, 16, 12), R.mat.accent); ball.position.set(0, 0, 1.5); R.toolG.add(ball);
  } else {
    R.toolG.add(R.cyl(Math.max(1, t.d / 2), 26, R.mat.tool, 0, 0, 13, t.d > 20 ? 8 : 24));
  }
}
const ramp = (u) => { const S = [[47, 124, 255], [76, 201, 255], [95, 224, 160], [255, 181, 71], [255, 77, 94]]; u = clamp(u, 0, 1) * (S.length - 1); const i = Math.min(S.length - 2, Math.floor(u)), f = u - i; return S[i].map((v, j) => (v + (S[i + 1][j] - v) * f) / 255); };
const toolColors = { 1: [1, .54, .24], 3: [.3, .79, 1], 5: [.37, .88, .63], 7: [1, .71, .28], 12: [.75, .55, 1] };
let pathDone = -2;
function rebuildPath() {
  if (!R) return;
  const S = prog.segs.filter((s) => s.k === "mv"), n = S.length;
  const pos = new Float32Array(n * 6), col = new Float32Array(n * 6);
  const o = off(), g = st.g92;
  S.forEach((s, i) => pos.set([s.x0 + o.x + g.x, s.y0 + o.y + g.y, s.z0 + o.z + g.z + ZTOP, s.x + o.x + g.x, s.y + o.y + g.y, s.z + o.z + g.z + ZTOP], i * 6));
  R.pathGeo.setAttribute("position", new THREE.BufferAttribute(pos, 3));
  R.pathGeo.setAttribute("color", new THREE.BufferAttribute(col, 3));
  R.pathGeo.computeBoundingSphere();
  R.pathSegs = S; pathDone = -2; colorPath(true);
  // the stock's top is the work Z0 of the G54 setup; the fixture sits under it
  const top = o.z + g.z + ZTOP;
  R.fixture.position.set(o.x + g.x, o.y + g.y, top - 14 - STOCK.h);
  R.fixture.rotation.z = st.rot * Math.PI / 180;
  R.trailLen = 0; R.trailGeo.setDrawRange(0, 0);
}
function colorPath(all) {
  if (!R || !R.pathSegs) return;
  const S = R.pathSegs, col = R.pathGeo.getAttribute("color");
  let doneTo = -1;
  if (st.motion === prog) { const cur = prog.segs[st.seg]; doneTo = S.findIndex((s) => s.t0 >= (cur ? cur.t0 : st.t)); if (doneTo < 0) doneTo = S.length; }
  else if (st.run === "done") doneTo = S.length;
  if (!all && doneTo === pathDone) return;
  pathDone = doneTo;
  S.forEach((s, i) => {
    let c;
    if (s.rapid) c = [.35, .42, .5];
    else if (st.color === "err") c = ramp(s.errMean / 60);
    else if (st.color === "feed") c = ramp(s.f / 6000);
    else c = toolColors[s.tool] || [.8, .8, .8];
    const k = doneTo >= 0 && i < doneTo ? 1 : s.rapid ? .45 : .5;
    col.setXYZ(i * 2, c[0] * k, c[1] * k, c[2] * k); col.setXYZ(i * 2 + 1, c[0] * k, c[1] * k, c[2] * k);
  });
  col.needsUpdate = true;
}
function setCam(kind, instant) {
  if (!R) return;
  $$("[data-cam]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.cam === kind)));
  R.follow = kind === "follow";
  const o = off(), sx = o.x + STOCK.w / 2, sy = o.y + STOCK.l / 2, sz = o.z + ZTOP;
  const views = {
    iso: [[-1100, -1700, 1700], [500, 560, 120]], top: [[500, 499, 3300], [500, 500, 0]], front: [[500, -2600, 520], [500, 500, 260]],
    side: [[-2600, 500, 520], [500, 500, 260]], part: [[sx - 240, sy - 380, sz + 330], [sx, sy, sz - 10]],
    follow: [[sx - 260, sy - 420, sz + 300], [sx, sy, sz]],
  };
  const v = views[kind];
  R.camAnim = { from: [R.cam.position.toArray(), R.controls ? R.controls.target.toArray() : [500, 500, 0]], to: v, t: instant ? 1 : 0 };
}
$$("[data-cam]").forEach((b) => b.addEventListener("click", () => setCam(b.dataset.cam)));
$$("[data-col]").forEach((b) => b.addEventListener("click", () => {
  st.color = b.dataset.col; $$("[data-col]").forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
  $("legend").hidden = st.color === "op"; $("leg-b").textContent = st.color === "err" ? "60 µm" : "6000 mm/min"; colorPath(true);
}));
$$("[data-show]").forEach((b) => b.addEventListener("click", () => { const k = b.dataset.show; st.show[k] = !st.show[k]; b.setAttribute("aria-pressed", String(st.show[k])); }));

function update3d(dt, tip, moving, cutting) {
  if (!R) return;
  const m = toMach(tip), k = st.show.defl ? st.exag : 0;
  // the head lags under acceleration and the bridge yaws: drawn k times larger
  const yaw = st.yaw * 1e-6 * k * (st.fe[1] > st.fe[2] ? 1 : -1), hx = st.err * 1e-3 * k * .6, hy = st.err * 1e-3 * k * .8;
  R.bridge.position.set(0, m.y + 220, 0);
  R.bridge.rotation.z = yaw;
  R.carriage.position.set(m.x, 0, 0);
  R.ram.position.set(0, 0, m.z + ZTOP);
  R.headG.position.set(-hx, -hy, 0);
  R.toolG.rotation.z = (R.toolG.rotation.z + st.spindle.rpm / 60 * 2 * Math.PI * dt * 0.02) % (2 * Math.PI);
  R.machine.visible = st.show.machine;
  R.pathLine.visible = st.show.path; R.trail.visible = st.show.path;
  R.limits.visible = st.show.limits;
  R.coolant.visible = st.cool.flood || st.cool.mist;
  const wx = m.x - hx, wy = m.y - hy, wz = m.z + ZTOP;
  if (moving && cutting) {
    const p = R.trailPos;
    if (R.trailLen < R.trailN) { p.set([wx, wy, wz], R.trailLen * 3); R.trailLen++; } else { p.copyWithin(0, 3); p.set([wx, wy, wz], (R.trailN - 1) * 3); }
    R.trailGeo.attributes.position.needsUpdate = true; R.trailGeo.setDrawRange(0, R.trailLen);
  }
  for (let i = 0; i < R.sparkN; i++) {
    if (R.sparkLife[i] <= 0 && moving && cutting && st.spindle.rpm > 1000 && Math.random() < .3) {
      R.sparkLife[i] = .35 + Math.random() * .4; const a = Math.random() * 6.28;
      R.sparkPos.set([wx, wy, wz + 2], i * 3); R.sparkVel.set([Math.cos(a) * 280, Math.sin(a) * 280, 120 + Math.random() * 180], i * 3);
    }
    if (R.sparkLife[i] > 0) {
      R.sparkLife[i] -= dt; for (let j = 0; j < 3; j++) R.sparkPos[i * 3 + j] += R.sparkVel[i * 3 + j] * dt;
      R.sparkVel[i * 3 + 2] -= 900 * dt;
      if (R.sparkLife[i] <= 0) R.sparkPos.set([0, 0, -5000], i * 3);
    }
  }
  R.sparkGeo.attributes.position.needsUpdate = true;
  if (R.camAnim) {
    const a = R.camAnim; a.t = Math.min(1, a.t + dt * 1.8); const e = 1 - Math.pow(1 - a.t, 3);
    const lerp = (p, q) => p.map((v, i) => v + (q[i] - v) * e);
    R.cam.position.set(...lerp(a.from[0], a.to[0])); if (R.controls) R.controls.target.set(...lerp(a.from[1], a.to[1]));
    if (a.t >= 1) R.camAnim = null;
  } else if (R.follow && R.controls) {
    const tgt = new THREE.Vector3(wx, wy, wz), d = tgt.clone().sub(R.controls.target).multiplyScalar(Math.min(1, dt * 4));
    R.controls.target.add(d); R.cam.position.add(d);
  }
  if (R.controls) R.controls.update(); else R.cam.lookAt(500, 500, 100);
  colorPath(false);
  R.renderer.render(R.scene, R.cam);
}

// ============================================================ plots
function plotCanvas(id) {
  const cv = $(id); if (!cv || cv.offsetParent === null) return null;
  const r = cv.getBoundingClientRect(), d = Math.min(2, devicePixelRatio || 1);
  if (cv.width !== Math.round(r.width * d) || cv.height !== Math.round(r.height * d)) { cv.width = Math.round(r.width * d); cv.height = Math.round(r.height * d); }
  const g = cv.getContext("2d"); g.setTransform(d, 0, 0, d, 0, 0); g.clearRect(0, 0, r.width, r.height); g.font = "10px " + C.mono;
  return [g, r.width, r.height];
}
function plotLine(id, data, max, color, unit) {
  const P = plotCanvas(id); if (!P) return; const [g, w, h] = P;
  g.strokeStyle = C.line; g.fillStyle = C.faint; g.lineWidth = 1;
  for (const f of [0.5, 1]) { const y = h - 8 - f * (h - 16); g.beginPath(); g.moveTo(30, y); g.lineTo(w, y); g.stroke(); g.fillText(String(Math.round(max * f)), 2, y + 3); }
  g.fillText(unit, 2, h - 4);
  if (data.length < 2) return;
  const n = 200, x = (i) => 30 + (i + n - data.length) / (n - 1) * (w - 34), y = (v) => h - 8 - Math.min(v, max) / max * (h - 16);
  g.beginPath(); data.forEach((v, i) => (i ? g.lineTo(x(i), y(v)) : g.moveTo(x(i), y(v))));
  g.strokeStyle = color; g.lineWidth = 1.5; g.stroke();
  g.lineTo(x(data.length - 1), h - 8); g.lineTo(x(0), h - 8); g.closePath(); g.globalAlpha = .15; g.fillStyle = color; g.fill(); g.globalAlpha = 1;
  g.fillStyle = color; g.beginPath(); g.arc(x(data.length - 1), y(data[data.length - 1]), 2.6, 0, 6.29); g.fill();
}
const fyAt = (X) => { const u = 2 * (X - 500) / 1200; return 1 / Math.sqrt(1 / 53.5 ** 2 + (1 / 27.8 ** 2 - 1 / 53.5 ** 2) * (1 - u * u)); };
function plotBeam(machX) {
  const P = plotCanvas("pl-beam"); if (!P) return; const [g, w, h] = P;
  const x = (X) => 30 + X / 1000 * (w - 36), y = (f) => h - 16 - (f - 20) / 40 * (h - 24);
  g.fillStyle = C.faint; g.strokeStyle = C.line;
  for (const f of [30, 40, 50]) { g.beginPath(); g.moveTo(30, y(f)); g.lineTo(w, y(f)); g.stroke(); g.fillText(f + "", 4, y(f) + 3); }
  for (const X of [0, 500, 1000]) g.fillText("X" + X, clamp(x(X) - 10, 30, w - 40), h - 3);
  g.beginPath(); for (let X = 0; X <= 1000; X += 10) (X ? g.lineTo(x(X), y(fyAt(X))) : g.moveTo(x(X), y(fyAt(X)))); g.strokeStyle = C.accent; g.lineWidth = 1.6; g.stroke();
  g.fillStyle = C.warn; g.beginPath(); g.arc(x(machX), y(fyAt(machX)), 4, 0, 6.29); g.fill();
  g.fillText(fyAt(machX).toFixed(1) + " Hz", clamp(x(machX) + 6, 30, w - 60), y(fyAt(machX)) - 6);
}
function plotSpectrum(rpm, load, moving) {
  const P = plotCanvas("pl-spec"); if (!P) return; const [g, w, H] = P;
  const fmax = 1200, h = H - 16, flutes = st.tool === 12 ? 4 : st.tool === 5 ? 2 : 3, tooth = rpm / 60 * flutes, bins = 120;
  for (let f = 0; f <= fmax; f += 300) { const xx = 4 + f / fmax * (w - 8); g.fillStyle = C.line; g.fillRect(xx, 0, 1, h); g.fillStyle = C.faint; g.fillText(String(f), Math.min(xx + 2, w - 28), H - 3); }
  for (let b = 0; b < bins; b++) {
    const f = (b + .5) / bins * fmax, pk = (fc, a, bw) => a * Math.exp(-(((f - fc) / bw) ** 2));
    let a = .04 + Math.random() * .03;
    if (rpm > 100) a += pk(tooth, .55 * load + .1, 12) + pk(2 * tooth, .2 * load, 12) + pk(rpm / 60, .12, 8);
    a += pk(28, moving ? .35 : .04, 6) + pk(45, moving ? .2 : .03, 6);
    const bh = Math.min(1, a) * (h - 6);
    g.fillStyle = Math.abs(f - tooth) < 20 && rpm > 100 ? C.cut : f < 60 ? C.warn : C.accent;
    g.globalAlpha = .85; g.fillRect(4 + b / bins * (w - 8), h - bh, (w - 8) / bins - 1, bh);
  }
  g.globalAlpha = 1;
  $("spec-note").textContent = `Tooth pass ${Math.round(tooth)} Hz (${flutes} flutes at ${Math.round(rpm)} rpm); head modes 28 and 45 Hz. A line growing between them is chatter starting.`;
}

// ============================================================ drives, pins
const drives = ["X", "Y1", "Y2", "Z", "S"];
$("drv-t").innerHTML = `<thead><tr><th>Drive</th><th class="n">A</th><th class="n">Ferr µm</th><th class="n">HV °C</th><th class="n">W</th></tr></thead><tbody>` + drives.map((d) => `<tr><td><span class="led on" id="dl-${d}"></span>${d}</td><td class="n" id="dc-${d}">0</td><td class="n" id="df-${d}">0</td><td class="n" id="dt-${d}">38</td><td class="n" id="dp-${d}">0</td></tr>`).join("") + "</tbody>";
const pins = [["joint.0.f-error", () => (st.fe[0] / 1000).toFixed(5)], ["joint.1.f-error", () => (st.fe[1] / 1000).toFixed(5)], ["joint.2.f-error", () => (st.fe[2] / 1000).toFixed(5)],
  ["pcomp.0.delay", () => ((st.pc.shaper ? 14 : 0) + (st.pc.jerk ? 10 : 0)).toFixed(1)], ["y1: gantry0.yaw", () => (st.yaw * 1e-6 * 1.2 * 2 * Math.PI / 0.06096).toExponential(2)],
  ["y1: gantry0.active", () => (st.on ? "TRUE" : "FALSE")], ["spindle.0.speed-in", () => (st.spindle.rpm / 60).toFixed(1)], ["spindle.0.at-speed", () => (st.spindle.dir && Math.abs(st.spindle.rpm - st.spindle.s * st.ovr.spin) < 300 ? "TRUE" : "FALSE")],
  ["powerquota.0.feed-scale", () => st.quota.toFixed(3)], ["motion.current-vel", () => (st.velNow / 60).toFixed(2)], ["halui.machine.is-on", () => (st.on ? "TRUE" : "FALSE")]];
$("pins").innerHTML = `<thead><tr><th>Pin</th><th class="n">Value</th></tr></thead><tbody>` + pins.map((p, i) => `<tr><td class="num" style="font-size:11.5px">${p[0]}</td><td class="n" id="pin-${i}">—</td></tr>`).join("") + "</tbody>";

// ============================================================ main loop
let last = performance.now(), slow = 0;
function frame(now) {
  const dt = Math.min(0.1, (now - last) / 1000); last = now;
  if (LIVE) {  // the machine is real: live.js keeps st up to date
    const r = LIVE.step(dt);
    present(dt, { ...st.pos }, r.seg, r.moving, r.cutting, r.pTot);
    requestAnimationFrame(frame);
    return;
  }
  const mo = st.motion, running = st.run === "run" || st.run === "step";
  let seg = null, u = 1, moving = false;
  if (mo && busy()) {
    if (running && st.on && !st.estop) {
      const s0 = mo.segs[st.seg] || mo.segs[0];
      const ovr = s0.k === "mv" ? (s0.rapid ? st.ovr.rapid * 2 : st.ovr.feed * st.quota) * st.ovr.maxv : 1;
      const atSpeed = !s0.cut || (st.spindle.dir && Math.abs(st.spindle.rpm - st.spindle.s * st.ovr.spin) < 600);
      if (atSpeed) st.t += dt * ovr;
    }
    let i = st.seg; while (i < mo.segs.length - 1 && mo.segs[i].t0 + mo.segs[i].t <= st.t) i++;
    if (i !== st.seg && mo === prog) {
      for (let j = st.seg + 1; j <= i; j++) {
        const s = mo.segs[j - 1];
        if (s.k === "tc") { st.tool = s.tool; buildTool(); msg(`T${s.tool} M6: ${(toolOf(s.tool) || { c: "" }).c}`, "info"); }
      }
      const s = mo.segs[i];
      if (s.k === "m3") { const m = prog.lines[s.line - 1].match(/S(\d+)/); spindle(1, m ? +m[1] : 12000); }
      if (s.k === "m5") spindle(0);
      if (s.k === "m8") setCool("flood", true);
      if (s.k === "m9") setCool("flood", false);
      if (s.k === "m1" && st.m1 && st.run === "run") { setRun("hold"); msg("M1 optional stop: check the part, then Cycle start", "warn"); }
      if (st.run === "step" && i >= stepTarget) setRun("hold");
    }
    st.seg = i; seg = mo.segs[i];
    u = seg.t > 0 ? clamp((st.t - seg.t0) / seg.t, 0, 1) : 1;
    st.pos = { x: seg.x0 + (seg.x - seg.x0) * u, y: seg.y0 + (seg.y - seg.y0) * u, z: seg.z0 + (seg.z - seg.z0) * u };
    moving = running && st.on && seg.k === "mv" && u < 1;
    if (st.t >= mo.total - 1e-9 && running) {
      if (mo === prog) { setRun("done"); st.motion = null; st.cutTime += prog.total; msg(`${prog.name} finished · ${mmss(prog.total)} · path p99 ${Math.round(st.p99)} µm`, "ok"); }
      else { setRun("idle"); st.motion = null; if (st.onDone) { const f = st.onDone; st.onDone = null; f(); } }
    }
  }
  if (jogHeld && st.on && !st.estop && !busy()) {
    const t = { ...st.pos }; t[jogHeld.a] += jogHeld.s * st.jogRate / 60 * dt * st.ovr.maxv;
    if (inside(toMach(t))) { st.pos = t; moving = true; } else { jogHeld = null; msg("Soft limit reached", "warn"); }
  }
  const liftTarget = st.run === "hold" && st.lift && st.spindle.dir ? 5 : 0;
  holdLift += (liftTarget - holdLift) * Math.min(1, dt * 4);
  const tip = { x: st.pos.x, y: st.pos.y, z: st.pos.z + holdLift };

  const target = st.on && !st.estop && st.spindle.dir ? st.spindle.s * st.ovr.spin : 0;
  st.spindle.rpm += (target - st.spindle.rpm) * Math.min(1, dt * 2.2);
  const cutting = !!(seg && seg.cut && moving);
  const engage = cutting ? (seg.tool === 12 ? 0.55 : seg.tool === 5 ? 0.5 : seg.z <= -7 ? 0.82 : seg.z <= -3 ? 0.66 : 0.4) : 0;
  st.spindle.load += ((cutting ? engage * st.ovr.feed * 100 : st.spindle.rpm > 100 ? 4 : 0) + (cutting ? Math.random() * 6 : 0) - st.spindle.load) * Math.min(1, dt * 4);

  const corner = seg && u > 0.85 ? (u - 0.85) / 0.15 : 0;
  const pcGain = (st.pc.shaper ? 0.45 : 1) * (st.pc.defl ? 0.55 : 1) * (st.pc.jerk ? 0.85 : 1);
  const errNow = moving && seg && !seg.rapid ? (3 + seg.err * corner * st.ovr.feed ** 2) * pcGain * 2.2 + Math.random() * 1.5 : 0.5 + Math.random() * 0.4;
  st.err += (errNow - st.err) * Math.min(1, dt * 12);
  const fe = (k) => (moving ? (seg && seg.rapid ? 16 : 5) * (Math.random() - .5) + corner * 7 * k : (Math.random() - .5) * .6);
  st.fe = [fe(1), fe(1.1), fe(.95), fe(.6)];
  st.yaw += ((moving ? Math.abs(st.fe[1] - st.fe[2]) / 1.2 + Math.random() * 1.2 : Math.random() * .3) - st.yaw) * Math.min(1, dt * 8);
  st.feedNow = moving && seg ? (seg.rapid ? RAPID * st.ovr.rapid : seg.f * st.ovr.feed * st.quota) * st.ovr.maxv : jogHeld ? st.jogRate : 0;
  st.velNow += (st.feedNow - st.velNow) * Math.min(1, dt * 6);
  const pTot = (moving ? (seg && seg.rapid ? .9 : .35) + Math.random() * .2 : .05) + st.spindle.rpm / 18000 * (.4 + st.spindle.load / 100 * 1.8);
  st.quota = pTot > 3.0 ? clamp(3.0 / pTot, .6, 1) : 1;
  st.bus += ((moving && seg && seg.rapid && u > .9 ? 352 : 323 + Math.random() * 3) - st.bus) * Math.min(1, dt * 6);

  present(dt, tip, seg, moving, cutting, pTot);
  requestAnimationFrame(frame);
}

// the 3D view every frame, the panels at 20 Hz
function present(dt, tip, seg, moving, cutting, pTot) {
  update3d(dt, tip, moving, cutting);
  slow += dt;
  if (slow > 0.05) {
    slow = 0;
    st.errHist.push(st.err); if (st.errHist.length > 200) st.errHist.shift();
    st.yawHist.push(st.yaw); if (st.yawHist.length > 200) st.yawHist.shift();
    if (moving && seg && !seg.rapid) { st.errs.push(st.err); if (st.errs.length > 3000) st.errs.shift(); }
    if (st.errs.length && Math.random() < .2) st.p99 = pct(st.errs, .99);
    renderUi(tip, seg, moving, pTot);
  }
}

function renderUi(tip, seg, moving, pTot) {
  const mach = toMach(tip);
  const dst = seg && st.motion ? { x: seg.x - tip.x, y: seg.y - tip.y, z: seg.z - tip.z } : { x: 0, y: 0, z: 0 };
  const show = st.dro === "mach" ? mach : st.dro === "dtg" ? dst : tip;
  ["x", "y", "z"].forEach((a, k) => {
    $("d-" + a).textContent = fmtLen(show[a]);
    $("ds-" + a).textContent = st.dro === "mach" ? `${off().n} ${fmtLen(tip[a], false)}` : `machine ${fmtLen(mach[a], false)}`;
    const v = st.fe[k === 2 ? 3 : k], wpc = Math.min(50, Math.abs(v) / 20 * 50), b = $("fe-" + a);
    b.style.width = wpc + "%"; b.style.left = (v < 0 ? 50 - wpc : 50) + "%"; b.style.background = Math.abs(v) > 12 ? C.warn : C.accent;
  });
  $("ds-y").textContent += ` · Y1−Y2 ${(st.fe[1] - st.fe[2]).toFixed(1)} µm`;
  const fcmd = seg && seg.k === "mv" && !seg.rapid ? seg.f : 0;
  $("modal").textContent = LIVE && LIVE.modal ? LIVE.modal : [off().n, "G17", st.units === "in" ? "G20" : "G21", "G90", "G94", "G40", "G43 H" + st.tool, "G64 P0.02", st.spindle.dir > 0 ? "M3" : st.spindle.dir < 0 ? "M4" : "M5", st.cool.flood ? "M8" : st.cool.mist ? "M7" : "M9", "T" + st.tool, "F" + fcmd, "S" + st.spindle.s].join(" ");
  $("sp-rpm").textContent = Math.round(st.spindle.rpm).toLocaleString("en-US");
  const atSpeed = st.spindle.dir && Math.abs(st.spindle.rpm - st.spindle.s * st.ovr.spin) < 300;
  $("sp-led").classList.toggle("on", !!atSpeed);
  $("sp-cmd").textContent = `S${st.spindle.s} · ${st.spindle.dir > 0 ? "CW" : st.spindle.dir < 0 ? "CCW" : "stopped"}${atSpeed ? " · at speed" : ""}`;
  $("sp-load").textContent = Math.round(st.spindle.load) + " %";
  $("sp-bar").firstChild.style.width = Math.min(100, st.spindle.load) + "%"; $("sp-bar").className = "bar" + (st.spindle.load > 85 ? " warn" : "");
  $("fd-v").textContent = Math.round(st.feedNow).toLocaleString("en-US");
  $("fd-cmd").textContent = seg && seg.k === "mv" && seg.rapid && moving ? "rapid" : `F${fcmd} · mm/min`;
  $("vel-v").textContent = Math.round(st.velNow).toLocaleString("en-US");
  $("f-feed").textContent = "F" + Math.round(st.feedNow); $("f-wcs").textContent = off().n; $("dro-wcs").textContent = off().n;
  const tt = st.motion === prog ? st.t : st.run === "done" ? prog.total : 0;
  $("t-el").textContent = mmss(tt); $("t-rem").textContent = mmss(prog.total - tt); $("t-clock").textContent = clock().slice(0, 5);
  const total = prog.total || 1;
  $("pg-bar").style.width = (tt / total * 100) + "%";
  const curLine = st.motion === prog && seg ? seg.line : 0;
  $("pg-stat").textContent = `${Math.round(tt / total * 100)} % · ${curLine ? nline(curLine) : "—"} · ${mmss(prog.total)} total`;
  if (curLine !== lastLine) {
    const p = $("gl-" + lastLine); if (p) p.classList.remove("cur");
    const c = $("gl-" + curLine); if (c) { c.classList.add("cur"); const gc = $("gcode"); if (gc.offsetParent) gc.scrollTop = c.offsetTop - gc.offsetTop - gc.clientHeight / 2; }
    lastLine = curLine;
  }
  updateChips();
  $("v-err").textContent = Math.round(st.err) + " µm"; $("v-yaw").textContent = st.yaw.toFixed(1) + " µrad";
  if (st.page === "dyn") {
    $("pc-modes").textContent = `44.6 / ${fyAt(clamp(mach.x, 0, 1000)).toFixed(1)} Hz`;
    $("pc-err").textContent = `${Math.round(st.err)} / ${Math.round(st.p99)} µm`;
    $("gl-fe").textContent = `${st.fe[1].toFixed(1)} / ${st.fe[2].toFixed(1)} µm`;
    $("gl-yaw").textContent = st.yaw.toFixed(1) + " µrad";
    $("gl-pill").textContent = st.on ? "LINKED" : "DOWN"; $("gl-pill").className = "pill " + (st.on ? "ok" : "off");
    $("pw-bus").textContent = Math.round(st.bus) + " V"; $("pw-p").textContent = pTot.toFixed(2) + " kW";
    $("pw-ch").textContent = st.bus > 345 ? Math.round((st.bus - 340) * 40) + " W" : "0 W";
    $("pw-fs").textContent = Math.round(st.quota * 100) + " %";
    $("pw-bar").firstChild.style.width = Math.min(100, pTot / 3.2 * 100) + "%";
    $("pw-pill").textContent = st.quota < 1 ? "LIMITING FEED" : "IN QUOTA"; $("pw-pill").className = "pill " + (st.quota < 1 ? "warn" : "ok");
    plotLine("pl-err", st.errHist, 60, C.accent, "µm"); plotLine("pl-yaw", st.yawHist, 30, C.ok, "µrad"); plotBeam(clamp(mach.x, 0, 1000));
    plotSpectrum(st.spindle.rpm, st.spindle.load / 100, moving);
    if (LIVE) LIVE.renderDyn(); else {
    const cur = { X: Math.abs(st.fe[0]) * .12 + (moving ? .6 : .05), Y1: Math.abs(st.fe[1]) * .1 + (moving ? .9 : .08), Y2: Math.abs(st.fe[2]) * .1 + (moving ? .9 : .08), Z: .6 + Math.abs(st.fe[3]) * .05, S: st.spindle.rpm / 18000 * (1.2 + st.spindle.load / 100 * 6) };
    const fev = { X: st.fe[0], Y1: st.fe[1], Y2: st.fe[2], Z: st.fe[3], S: 0 };
    drives.forEach((d) => {
      $("dc-" + d).textContent = cur[d].toFixed(1); $("df-" + d).textContent = d === "S" ? "—" : fev[d].toFixed(1);
      $("dp-" + d).textContent = Math.round(cur[d] * (d === "S" ? 300 : 90)); $("dt-" + d).textContent = Math.round(38 + cur[d] * 3);
      $("dl-" + d).classList.toggle("on", st.on && !st.estop);
    });
    }
  }
  if (st.page === "stat") {
    if (LIVE) LIVE.renderPins(); else pins.forEach((p, i) => ($("pin-" + i).textContent = p[1]()));
    $("mstats").innerHTML = [["Spindle hours", (st.spindleHrs + st.cutTime / 3600).toFixed(1)], ["Cut time this shift", mmss(st.cutTime + (st.motion === prog ? st.t : 0))], ["Y travel since service", "1 284 m"], ["Lube cycles", "4 102"], ["Drive faults, last 30 days", "0"], ["DC bus now", Math.round(st.bus) + " V"]].map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join("");
  }
}

// ============================================================ start
// the console, whichever machine it shows
function bootCommon() {
  renderProgram(); renderMdi(); fillToolSelects(); renderEvents();
  init3d();
  let startPage = "main"; try { startPage = localStorage.getItem("pmc.page") || "main"; } catch (e) { /* storage blocked */ }
  if (location.hash && paneIds[location.hash.slice(1)]) startPage = location.hash.slice(1);
  showPage(paneIds[startPage] ? startPage : "main");
  setCam("iso", true);
  requestAnimationFrame(frame);
}
// the simulated machine: opens mid-cut (the pocket's second depth), held for
// inspection, so every panel shows a working machine
function bootSim(note) {
  for (const [t, l, x] of [["07:10:03", "warn", "Spindle load 92 % in a pocket corner (BRACKET OP1, N0118)"], ["06:44:50", "ok", "HOUSING-7075_OP2 finished · 3:51 · path p99 24 µm"], ["06:02:14", "info", "Machine model from dynwiz: 44.6 / 53.5 / 27.8 Hz"], ["06:02:12", "info", "Gantry link up: Y1 ↔ Y2, 0.6 ms"], ["06:02:11", "ok", "Machine on, homed: absolute scales on all axes"]]) events.push({ t, lvl: l, text: x });
  bootCommon();
  st.tool = 3; buildTool(); spindle(1, 18000); st.spindle.rpm = 18000; setCool("flood", true);
  st.motion = prog;
  const s0 = prog.segs.find((s) => s.k === "mv" && s.cut && s.tool === 3 && s.z < -6);
  st.t = s0.t0 + 9; st.seg = 0; while (st.seg < prog.segs.length - 1 && prog.segs[st.seg].t0 + prog.segs[st.seg].t <= st.t) st.seg++;
  setRun("hold");
  for (let k = 0; k < 120; k++) { st.errHist.push(4 + 10 * Math.abs(Math.sin(k / 7)) + Math.random() * 3); st.yawHist.push(2 + 4 * Math.abs(Math.sin(k / 9)) + Math.random()); }
  msg(note || "Simulation: OP1 held in the pocket for inspection. Cycle start resumes.", "info");
}
