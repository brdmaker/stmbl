/* Portal Mill Console: the Measure page. The vibration sensors
   (docs/vibsensor.md) through vibmon: a card per sensor (velocity against
   its warn / alarm levels, acceleration, kurtosis, temperature, spindle
   orders, tooth passing, chatter, bearing envelope, a trend), a spectrum
   with the spindle's orders marked, the alarms (crash, lost, level), naming
   the sensors by tapping them, and a console to each.
   On a machine the data is millweb's ([MILLWEB] VIBMON: "vib", "vibev",
   "vibspectrum", "vibconsole" messages, vib_* ops); in the simulation four
   simulated sensors. Classic script after drives.js. */
"use strict";

const ms = { up: false, status: null, trend: {}, spec: {}, sel: null, range: 5000, auto: false, tapped: null, con: null };
const MS_LEVEL = [["ok", "OK"], ["warn", "WARN"], ["bad", "ALARM"]];
const MS_POS = [["", "—"], ["base", "base"], ["x", "X axis"], ["y", "Y axis"], ["z", "Z axis"], ["spindle", "spindle"]];
const msf = (v, d = 2) => (v === null || v === undefined || !isFinite(v) ? "—" : (+v).toFixed(d));
const msMax = (a) => (a ? Math.max(...a) : null);
const msSrc = () => (LIVE ? LIVE.vib : vibSim);

// ------------------------------------------------------------ from the server (or the simulation)
function vibShow(up, status) {
  ms.up = !!up;
  ms.status = status || null;
  const t = performance.now() / 1000;
  for (const s of (status && status.sensors) || []) {
    const f = s.features;
    if (!f) continue;
    const tr = ms.trend[s.node] || (ms.trend[s.node] = []);
    if (!tr.length || tr[tr.length - 1][2] !== f.seq) tr.push([t, msMax(f.vel), f.seq]);
    while (tr.length && tr[0][0] < t - 120) tr.shift();  // two minutes
  }
  if (ms.sel === null && status && status.sensors.length) ms.sel = status.sensors[0].node;
  if (st.page === "meas") renderMeasure();
  measBadge();
}
function vibEvent(e) {
  if (e.type === "tap") { ms.tapped = e.uid; if (st.page === "meas") renderMsSensors(); }
}
function vibSpectrumIn(node, spectrum) {
  if (spectrum) ms.spec[node] = spectrum;
  if (st.page === "meas" && node === ms.sel) drawSpectrum();
}
function vibConsoleIn(node, text) {
  if (!text) return;
  const o = $("ms-con-out");
  o.textContent = (o.textContent + text).slice(-8000);
  o.scrollTop = o.scrollHeight;
}
function measBadge() {
  const a = ms.status && ms.status.alarms, b = document.querySelector('#nav button[data-p="meas"]');
  if (b) b.classList.toggle("alarm", !!(a && (a.crash || a.level >= 2)));
}

// ------------------------------------------------------------ the page
function measEnter() { renderMeasure(); if (ms.sel !== null) msSrc().spectrum(ms.sel); }
function sensorOf(node) { return ((ms.status && ms.status.sensors) || []).find((s) => s.node === node); }
function sensorLabel(s) { return s.name || s.position || "node " + s.node; }

function renderMeasure() {
  const s = ms.status, a = s && s.alarms;
  $("ms-link").className = "pill " + (ms.up ? "ok" : LIVE && LIVE.hasVib() ? "warn" : "off");
  $("ms-link").textContent = ms.up ? (LIVE ? "VIBMON" : "SIMULATED") : LIVE && LIVE.hasVib() ? "NO VIBMON" : "NOT SET UP";
  $("ms-alarms").innerHTML = a ? [
    a.crash ? `<span class="pill bad">CRASH</span>` : "",
    a.lost ? `<span class="pill warn">SENSOR LOST</span>` : "",
    a.level ? `<span class="pill ${MS_LEVEL[a.level][0]}">VIBRATION ${MS_LEVEL[a.level][1]}</span>` : "",
    a.temp_level ? `<span class="pill ${MS_LEVEL[a.temp_level][0]}">TEMPERATURE ${MS_LEVEL[a.temp_level][1]}</span>` : "",
    !a.crash && !a.lost && !a.level && !a.temp_level ? `<span class="pill ok">ALL OK</span>` : "",
  ].join("") : "";
  const m = s && s.machine;
  $("ms-machine").textContent = m ? `${Math.round(m.rpm)} rpm · ${m.teeth} teeth · feed ${Math.round(m.feed)}` : "";
  $("ms-ident").setAttribute("aria-pressed", String(!!(s && s.identifying)));
  renderMsCards();
  renderMsSensors();
  renderMsSelects();
  drawSpectrum();
}

function renderMsCards() {
  const el = $("ms-cards"), list = (ms.status && ms.status.sensors) || [];
  if (!list.length) {
    el.innerHTML = `<div class="panel"><h2>Vibration sensors</h2><p class="note">${LIVE && !LIVE.hasVib()
      ? "None here. On the machine: run <code>tools/vibmon/vibmon.py --can can1 --sock /run/vibmon.sock --hal</code> and set <code>[MILLWEB] VIBMON = /run/vibmon.sock</code> (docs/vibsensor.md)."
      : "No sensors found on the measurement bus yet."}</p></div>`;
    return;
  }
  el.innerHTML = list.map((s) => {
    const f = s.features || {}, u = s.summary || {}, vel = msMax(f.vel), lv = f.level || 0;
    const state = s.lost ? ["warn", "LOST"] : s.crash ? ["bad", "CRASH"] : MS_LEVEL[lv];
    return `<div class="panel ms-card${s.node === ms.sel ? " sel" : ""}" data-node="${s.node}">
      <div class="h"><b>${esc(sensorLabel(s))}</b><span class="muted">node ${s.node}</span><span class="pill ${state[0]}" data-k="state">${state[1]}</span></div>
      <div class="ms-big ${MS_LEVEL[lv][0]}" data-k="vel">${msf(vel, 2)}<small> mm/s</small></div>
      <canvas class="ms-spark" data-spark="${s.node}"></canvas>
      <dl class="kv">
        <dt>RMS x / y / z</dt><dd>${(f.rms || []).map((v) => msf(v, 3)).join(" / ") || "—"} g</dd>
        <dt>Peak</dt><dd>${msf(msMax(f.peak), 2)} g</dd>
        <dt>Kurtosis</dt><dd>${msf(msMax(f.kurt), 1)}</dd>
        <dt>Temperature</dt><dd>${msf(f.temp, 1)} °C</dd>
        <dt>1× · 2× · 3×</dt><dd>${(u.ord || []).map((v) => msf(v, 3)).join(" · ") || "—"} g</dd>
        <dt>Tooth passing</dt><dd>${msf(u.tpf, 3)} g</dd>
        <dt>Chatter</dt><dd>${u.chat_a ? `${msf(u.chat_f, 0)} Hz · ${msf(u.chat_a, 3)} g` : "—"}</dd>
        <dt>Bearing band</dt><dd>${msf(f.env, 3)} g${u.env_f ? ` · ${msf(u.env_f, 1)} Hz` : ""}</dd>
      </dl>
      <div class="ft"><button class="btn sm" data-spec="${s.node}">Spectrum</button><button class="btn sm" data-con="${s.node}">Console</button>${s.crash ? `<button class="btn sm" data-reset="${s.node}">Reset crash</button>` : ""}</div>
    </div>`;
  }).join("");
  for (const cv of el.querySelectorAll("canvas[data-spark]")) drawSpark(cv, ms.trend[+cv.dataset.spark] || []);
}

function drawSpark(cv, tr) {
  const w = (cv.width = cv.clientWidth || 200), h = (cv.height = 34), g = cv.getContext("2d");
  g.clearRect(0, 0, w, h);
  if (tr.length < 2) return;
  const t1 = tr[tr.length - 1][0], t0 = Math.max(t1 - 120, tr[0][0]), span = Math.max(t1 - t0, 1), mx = Math.max(1, ...tr.map((p) => p[1]));
  g.strokeStyle = getComputedStyle(document.body).getPropertyValue("--accent") || "#4cc9ff";
  g.lineWidth = 1.5;
  g.beginPath();
  tr.forEach((p, i) => { const x = ((p[0] - t0) / span) * w, y = h - 2 - (p[1] / mx) * (h - 4); i ? g.lineTo(x, y) : g.moveTo(x, y); });
  g.stroke();
}

function renderMsSelects() {
  const list = (ms.status && ms.status.sensors) || [];
  const opts = list.map((s) => `<option value="${s.node}"${s.node === ms.sel ? " selected" : ""}>${esc(sensorLabel(s))}</option>`).join("");
  for (const id of ["ms-spec-node", "ms-con-node"]) if (document.activeElement !== $(id) && $(id).innerHTML !== opts) $(id).innerHTML = opts;
}

function renderMsSensors() {
  const s = ms.status, list = (s && s.sensors) || [];
  if (document.querySelector("#ms-sensors :focus")) return;  // not while a name is typed
  const missing = list.filter((x) => x.lost);
  $("ms-sensors").innerHTML = `<tr><th>Sensor</th><th>Node</th><th>Name</th><th>Position</th><th></th></tr>` + list.map((x) => `
    <tr class="${x.uid === ms.tapped ? "tapped" : ""}" data-uid="${esc(x.uid)}">
      <td class="mono" title="${esc(x.uid)}">…${esc(x.uid.slice(-8))}${x.uid === ms.tapped ? ' <span class="pill info">TAPPED</span>' : ""}</td>
      <td class="num">${x.node}</td>
      <td><input data-k="name" value="${esc(x.name || "")}" maxlength="16" aria-label="Name"></td>
      <td><select data-k="position" aria-label="Position">${MS_POS.map(([v, l]) => `<option value="${v}"${(x.position || "") === v ? " selected" : ""}>${l}</option>`).join("")}</select></td>
      <td><button class="btn sm" data-assign>Save</button>${!x.lost && missing.length ? `<select data-replace aria-label="Replaces"><option value="">replaces…</option>${missing.map((o) => `<option value="${esc(o.uid)}">${esc(sensorLabel(o))}</option>`).join("")}</select>` : ""}</td>
    </tr>`).join("");
  $("ms-ident-note").textContent = s && s.identifying
    ? (ms.tapped ? "Tapped: the highlighted sensor. Give it a name and a position, Save, then tap the next." : "Identify is on: knock on a sensor (a firm tap with a screwdriver's handle).")
    : "Which sensor is which: Identify, then tap one on the machine; its row lights up.";
}

// ------------------------------------------------------------ the spectrum
function drawSpectrum() {
  const cv = $("ms-spec-cv"), sp = ms.spec[ms.sel], s = sensorOf(ms.sel);
  const w = (cv.width = cv.clientWidth || 600), h = (cv.height = cv.clientHeight || 220), g = cv.getContext("2d");
  const css = getComputedStyle(document.body), ink = css.getPropertyValue("--ink-dim") || "#999", acc = css.getPropertyValue("--accent") || "#4cc9ff";
  g.clearRect(0, 0, w, h);
  if (!sp) { $("ms-spec-info").textContent = s ? "— (Get)" : "—"; return; }
  const fmax = ms.range || sp.df * (sp.rms.length - 1), kmax = Math.min(sp.rms.length - 1, Math.floor(fmax / sp.df));
  let amax = 1e-6;
  for (let k = 1; k <= kmax; k++) amax = Math.max(amax, sp.rms[k]);
  const X = (f) => 40 + (f / fmax) * (w - 50), Y = (a) => h - 20 - (a / amax) * (h - 30);
  g.strokeStyle = ink; g.fillStyle = ink; g.font = "10px monospace"; g.lineWidth = 1;
  g.beginPath(); g.moveTo(40, 8); g.lineTo(40, h - 20); g.lineTo(w - 10, h - 20); g.stroke();
  for (let i = 0; i <= 5; i++) { const f = (fmax * i) / 5; g.fillText(f >= 1000 ? (f / 1000).toFixed(1) + "k" : f.toFixed(0), X(f) - (i === 5 ? 26 : 10), h - 6); }
  g.fillText(amax.toFixed(3) + " g", 2, 14);
  // markers: the spindle's orders, tooth passing, chatter
  const u = (s && s.summary) || {}, f1 = (u.rpm || 0) / 60, teeth = (ms.status && ms.status.machine.teeth) || 0;
  const marks = [];
  if (f1 > 0) { [1, 2, 3].forEach((k) => marks.push([k * f1, k + "×", "#8fa"])); if (teeth) marks.push([teeth * f1, "TP", "#fd6"]); }
  if (u.chat_a) marks.push([u.chat_f, "chatter", "#f66"]);
  for (const [f, l, c] of marks) if (f < fmax) { g.strokeStyle = c; g.fillStyle = c; g.setLineDash([3, 3]); g.beginPath(); g.moveTo(X(f), 10); g.lineTo(X(f), h - 20); g.stroke(); g.setLineDash([]); g.fillText(l, X(f) + 2, 20); }
  g.strokeStyle = acc; g.lineWidth = 1.2; g.beginPath();
  for (let k = 1; k <= kmax; k++) { const x = X(k * sp.df), y = Y(sp.rms[k]); k > 1 ? g.lineTo(x, y) : g.moveTo(x, y); }
  g.stroke();
  let pk = 1;
  for (let k = 1; k <= kmax; k++) if (sp.rms[k] > sp.rms[pk]) pk = k;
  $("ms-spec-info").textContent = `${esc(sensorLabel(s || { node: ms.sel }))} · ${sp.rms.length} bins of ${sp.df.toFixed(1)} Hz · peak ${(pk * sp.df).toFixed(0)} Hz ${sp.rms[pk].toFixed(3)} g`;
  cv.dataset.peak = (pk * sp.df).toFixed(1);
  ms.view = { X, fmax, sp };
}
$("ms-spec-cv").addEventListener("mousemove", (e) => {
  const v = ms.view, cv = $("ms-spec-cv");
  if (!v) return;
  const r = cv.getBoundingClientRect(), f = ((e.clientX - r.left - 40) / (r.width - 50)) * v.fmax, k = Math.round(f / v.sp.df);
  if (k >= 1 && k < v.sp.rms.length) $("ms-spec-read").textContent = `${(k * v.sp.df).toFixed(1)} Hz · ${v.sp.rms[k].toFixed(4)} g`;
});

// ------------------------------------------------------------ actions
async function msDo(p, what) {
  const r = await p;
  if (r && r.ok === false) msg(`${what}: ${r.err}`, "warn");
  return r;
}
$("ms-cards").addEventListener("click", (e) => {
  const b = e.target.closest("button"), card = e.target.closest(".ms-card");
  if (b && b.dataset.spec) { ms.sel = +b.dataset.spec; msDo(msSrc().spectrum(ms.sel), "Spectrum"); renderMeasure(); return; }
  if (b && b.dataset.con) { ms.sel = +b.dataset.con; renderMsSelects(); $("ms-con-node").value = b.dataset.con; $("ms-con-in").focus(); return; }
  if (b && b.dataset.reset) { msDo(msSrc().reset(+b.dataset.reset), "Reset"); return; }
  if (card) { ms.sel = +card.dataset.node; renderMeasure(); }
});
$("ms-reset").addEventListener("click", () => msDo(msSrc().reset(null), "Reset"));
$("ms-ident").addEventListener("click", () => { ms.tapped = null; msDo(msSrc().identify(!(ms.status && ms.status.identifying)), "Identify"); });
$("ms-rescan").addEventListener("click", () => msDo(msSrc().rescan(), "Rescan"));
$("ms-spec-get").addEventListener("click", () => ms.sel !== null && msDo(msSrc().spectrum(ms.sel), "Spectrum"));
$("ms-spec-node").addEventListener("change", (e) => { ms.sel = +e.target.value; renderMeasure(); msSrc().spectrum(ms.sel); });
$("ms-spec-auto").addEventListener("change", (e) => { ms.auto = e.target.checked; });
$("ms-spec-range").addEventListener("click", (e) => {
  const b = e.target.closest("button[data-r]");
  if (!b) return;
  ms.range = +b.dataset.r;
  $$("#ms-spec-range button").forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
  drawSpectrum();
});
$("ms-sensors").addEventListener("click", (e) => {
  const tr = e.target.closest("tr[data-uid]");
  if (!tr || !e.target.closest("button[data-assign]")) return;
  const name = tr.querySelector('[data-k="name"]').value.trim(), position = tr.querySelector('[data-k="position"]').value;
  msDo(msSrc().assign(tr.dataset.uid, name, position), "Save");
  document.activeElement.blur();
});
$("ms-sensors").addEventListener("change", (e) => {
  const sel = e.target.closest("select[data-replace]"), tr = e.target.closest("tr[data-uid]");
  if (!sel || !sel.value) return;
  if (confirm("The new sensor takes the old one's node, name and position; the old one leaves the list.")) msDo(msSrc().replace(sel.value, tr.dataset.uid), "Replace");
  sel.value = "";
});
$("ms-con-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const node = +$("ms-con-node").value, line = $("ms-con-in").value.trim();
  if (!line) return;
  vibConsoleIn(node, `> ${line}\n`);
  $("ms-con-in").value = "";
  const r = await msDo(msSrc().console(node, line), "Console");
  if (r && r.ok !== false) setTimeout(() => msSrc().consoleRead(node), 400);
});
setInterval(() => { if (st.page === "meas" && ms.auto && ms.sel !== null) msSrc().spectrum(ms.sel); }, 1000);

// ------------------------------------------------------------ the simulation: four sensors on a running spindle
const vibSim = (() => {
  const S = [
    { uid: "5ea5000000000000000000b1", node: 1, name: "base", position: "base", v: 0.6 },
    { uid: "5ea5000000000000000000c2", node: 2, name: "X axis", position: "x", v: 1.4 },
    { uid: "5ea5000000000000000000d3", node: 3, name: "Y axis", position: "y", v: 1.1 },
    { uid: "5ea5000000000000000000e4", node: 4, name: "spindle", position: "spindle", v: 3.2 },
  ];
  let ident = false, seq = 0;
  const crash = {};
  const ok = () => Promise.resolve({ ok: true });
  function status() {
    seq++;
    const rpm = 9000, f1 = rpm / 60, n = (a) => a * (1 + 0.08 * Math.sin(seq / 7 + a));
    const sensors = S.map((s) => {
      const vel = n(s.v), level = vel >= 11.2 ? 2 : vel >= 4.5 ? 1 : 0;
      return {
        uid: s.uid, node: s.node, name: s.name, position: s.position, key: s.position, lost: false, crash: !!crash[s.node],
        features: { seq, level, crash: crash[s.node] ? 1 : 0, temp_level: 0, rms: [vel / 15, vel / 18, vel / 40], peak: [vel / 5, vel / 6, vel / 12],
          vel: [vel, vel * 0.8, vel * 0.3], kurt: [3.1, 3.0, 2.9], env: 0.02, temp: 28 + s.node },
        summary: { fft_n: 2048, fft_f: f1, fft_a: vel / 16, ord: [vel / 16, vel / 60, vel / 120], tpf: s.node === 4 ? 0.12 : 0.02,
          chat_f: s.node === 4 ? 1730 : 0, chat_a: s.node === 4 ? 0.05 : 0, env_f: 0, rpm },
      };
    });
    return { machine: { rpm, feed: 1200, teeth: 3, tool: 1, spindle: true, running: true }, identifying: ident,
      alarms: { crash: Object.values(crash).some(Boolean), lost: false, level: Math.max(...sensors.map((x) => x.features.level)), temp_level: 0 }, sensors };
  }
  function spectrum(node) {
    const s = S.find((x) => x.node === node), df = 26667 / 2048, rpm = 9000, f1 = rpm / 60, rms = [];
    for (let k = 0; k < 1025; k++) {
      const f = k * df, tone = (fc, a) => a * Math.exp(-0.5 * ((f - fc) / df) ** 2);
      rms.push(0.002 + 0.001 * Math.random() + tone(f1, s.v / 16) + tone(2 * f1, s.v / 60) + tone(3 * f1, 0.03) + (node === 4 ? tone(1730, 0.05) + tone(450, 0.12) : 0));
    }
    setTimeout(() => vibSpectrumIn(node, { seq: seq, axis: 0, df, rms }), 50);
    return ok();
  }
  setInterval(() => { if (!LIVE) vibShow(true, status()); }, 200);
  return {
    spectrum,
    reset: (node) => { for (const s of S) if (node === null || node === s.node) crash[s.node] = false; return ok(); },
    identify: (on) => { ident = on; if (on) setTimeout(() => { if (ident) vibEvent({ type: "tap", uid: S[3].uid, node: 4 }); }, 1500); return ok(); },
    assign: (uid, name, position) => { const s = S.find((x) => x.uid === uid); if (s) { s.name = name; s.position = position || null; } return ok(); },
    replace: () => Promise.resolve({ ok: false, err: "the simulation has no spare sensor" }),
    rescan: ok,
    console: (node, line) => { setTimeout(() => vibConsoleIn(node, /=/.test(line) ? "OK\n" : `${line}: (simulated sensor)\n`), 100); return ok(); },
    consoleRead: () => ok(),
    crash: (node) => { crash[node] = true; },  // for the tests
  };
})();
