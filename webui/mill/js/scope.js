/* Portal Mill Console: the Debug page's scope (a halscope in the browser),
   its tabs, and saved views. millweb's sampler streams the channels' values
   (scope_start, "scope" messages); here they are triggered and drawn: auto,
   normal, single and roll, a trigger on a channel's edge at a level and
   position, per-channel scale and offset, a cursor readout. Views keep the
   watch list and the scope's set-up under a name: on a machine for every
   browser, in the simulation in this browser. Classic script after hal.js. */
"use strict";

const SC_COLORS = ["#4cc9ff", "#5fe0a0", "#ffb84c", "#ff6b9a", "#b48cff", "#e8e86a", "#ff8a4c", "#9ad0ff"];
const scope = {
  ch: [], rate: 1000, tdiv: 0.1, mode: "auto", trig: { src: 0, edge: "rise", level: 0, pos: 0.2 },
  running: false, ring: [], size: 0, total: 0, armedAt: 0, searchFrom: 0, disp: null, info: "", state: "STOPPED",
};
let dbgTab = "hal";
let views = [], curView = null;
try { views = JSON.parse(localStorage.getItem("pmc.views") || "[]"); } catch (e) { /* storage blocked */ }

// ------------------------------------------------------------ tabs
function debugTab(t) {
  dbgTab = t;
  $$("#dbg-tab button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.t === t)));
  $("dbg-hal").hidden = t !== "hal"; $("dbg-scope").hidden = t !== "scope";
  if (t === "scope") { renderScopeSide(); requestAnimationFrame(drawScope); }
}
$("dbg-tab").addEventListener("click", (e) => { const b = e.target.closest("button[data-t]"); if (b) debugTab(b.dataset.t); });

// ------------------------------------------------------------ channels and settings
const scN = () => Math.max(10, Math.round(scope.tdiv * 10 * scope.rate));  // samples on screen: 10 divisions
function scopeReset() {
  scope.size = Math.max(4 * scN(), scope.rate * 2);
  scope.ring = scope.ch.map(() => new Float32Array(scope.size).fill(NaN));
  scope.total = 0; scope.armedAt = 0; scope.searchFrom = 0; scope.disp = null;
}
function renderScopeSide() {
  $("sc-ch").innerHTML = scope.ch.length ? scope.ch.map((c, i) => `<div class="sc-chan" data-i="${i}" style="--c:${c.color}">
      <div class="h"><b title="${esc(c.name)}">${esc(c.name)}</b><button class="btn sm" data-auto="${i}" title="Scale and offset from what is shown">auto</button><button class="btn sm" data-rm="${i}" title="Off the scope">×</button></div>
      <div class="r"><label title="units per division">/div <input type="number" step="any" data-k="scale" value="${c.scale}"></label><label title="the value at the middle line">mid <input type="number" step="any" data-k="offset" value="${c.offset}"></label></div></div>`).join("")
    : `<p class="note">No channels: add one below, or with ∿ in the HAL tab's watch list.</p>`;
  $("sc-src").innerHTML = scope.ch.map((c, i) => `<option value="${i}"${i === scope.trig.src ? " selected" : ""}>${esc(c.name)}</option>`).join("");
  $("sc-rate").value = String(scope.rate); $("sc-tdiv").value = String(scope.tdiv);
  $("sc-edge").value = scope.trig.edge; $("sc-level").value = scope.trig.level; $("sc-pos").value = Math.round(scope.trig.pos * 100);
  $$("#sc-mode button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.m === scope.mode)));
  const names = hal.data ? [...hal.data.pins.map((p) => p[0]), ...hal.data.sigs.map((g) => g[0]), ...hal.data.params.map((q) => q[0])] : hal.watch.map((w) => w.name);
  $("sc-names").innerHTML = [...new Set([...hal.watch.map((w) => w.name), ...names])].slice(0, 3000).map((n) => `<option value="${esc(n)}">`).join("");
  $("sc-run").textContent = scope.running ? "Stop" : "Run";
  $("dbg-tab").querySelector('[data-t="scope"]').textContent = scope.running ? "Scope ●" : "Scope";  // it runs on behind the HAL tab and other pages
  scopeStateUi();
}
function scopeStateUi() {
  const el = $("sc-state"); el.textContent = scope.state;
  el.className = "pill " + ({ RUNNING: "ok", ROLL: "ok", TRIGGERED: "ok", "WAITING FOR TRIGGER": "warn", STOPPED: "off" }[scope.state] || "info");
  $("sc-info").textContent = scope.info;
}
function scopeAdd(name, kind = "pin") {
  name = name.trim(); if (!name) return;
  if (scope.ch.some((c) => c.name === name)) return;
  if (scope.ch.length >= 8) { msg("The scope takes 8 channels.", "warn"); return; }
  scope.ch.push({ name, kind, color: SC_COLORS.find((c) => !scope.ch.some((x) => x.color === c)) || SC_COLORS[0], scale: 1, offset: 0, auto: true });
  scopeChanged();
}
function scopeChanged() {  // channels or rate changed: start again
  scopeReset(); renderScopeSide();
  if (scope.running) scopeRun();
  drawScope();
}

// ------------------------------------------------------------ running: the source streams, this triggers
async function scopeRun() {
  if (!scope.ch.length) { msg("Add a channel to the scope first.", "warn"); return; }
  scopeReset(); scope.running = true; scope.state = scope.mode === "roll" ? "ROLL" : "WAITING FOR TRIGGER"; scope.info = "";
  renderScopeSide();
  const r = await scopeSrc().start(scope.ch.map((c) => c.name), scope.rate);
  if (r && !r.ok) { scope.running = false; scope.state = "STOPPED"; scope.info = r.err; renderScopeSide(); }
}
function scopeStop() {
  if (!scope.running) return;
  scope.running = false; scope.state = "STOPPED"; scopeSrc().stop(); renderScopeSide();
}
// samples in: n rows of the channels' values
function scopeData(n, data, rate, missed) {
  if (!scope.running || !scope.ring.length) return;
  const C = scope.ch.length;
  for (let k = 0; k < n; k++) {
    const at = (scope.total + k) % scope.size;
    for (let c = 0; c < C; c++) scope.ring[c][at] = data[k * C + c];
  }
  scope.total += n;
  scope.info = `${rate.toFixed(0)} samples/s${missed ? ` · ${missed} late` : ""}`;
  scopeCapture();
}
const scAt = (c, i) => scope.ring[c][((i % scope.size) + scope.size) % scope.size];
function scopeTake(from) {  // the N samples from index `from` onwards, for the screen
  const N = scN();
  return { from, data: scope.ch.map((_, c) => { const a = new Float32Array(N); for (let k = 0; k < N; k++) a[k] = scAt(c, from + k); return a; }) };
}
function scopeCapture() {
  const N = scN(), pre = Math.round(scope.trig.pos * N), T = scope.total;
  if (scope.mode === "roll") { if (T >= 1) scope.disp = Object.assign(scopeTake(Math.max(0, T - N)), { trig: null }); return; }
  // look for the edge in the samples not looked at yet (with room for the pre-trigger part)
  const src = Math.min(scope.trig.src, scope.ch.length - 1), lv = scope.trig.level, rise = scope.trig.edge === "rise";
  let found = -1;
  for (let i = Math.max(scope.searchFrom, scope.armedAt + pre, 1); i < T; i++) {
    const a = scAt(src, i - 1), b = scAt(src, i);
    if (rise ? a < lv && b >= lv : a > lv && b <= lv) { found = i; break; }
  }
  if (found < 0) {
    scope.searchFrom = Math.max(scope.searchFrom, T);
    if (scope.mode === "auto" && T - scope.armedAt >= N) {  // no trigger in a screen's time: show what there is
      scope.disp = Object.assign(scopeTake(T - N), { trig: null }); scope.armedAt = T; scope.state = "RUNNING";
    } else if (scope.state !== "RUNNING" || scope.mode !== "auto") scope.state = "WAITING FOR TRIGGER";
    return;
  }
  if (T < found - pre + N) { scope.state = "TRIGGERED"; return; }  // the rest of the screen still to come
  scope.disp = Object.assign(scopeTake(found - pre), { trig: pre });
  scope.armedAt = found - pre + N; scope.searchFrom = scope.armedAt;
  scope.state = scope.mode === "single" ? "STOPPED" : "TRIGGERED";
  if (scope.mode === "single") { scope.running = false; scopeSrc().stop(); renderScopeSide(); }
  for (const c of scope.ch) if (c.auto) { autoScale(c, scope.ch.indexOf(c)); c.auto = false; renderScopeSide(); }
}

// ------------------------------------------------------------ drawing
function niceStep(x) { const p = Math.pow(10, Math.floor(Math.log10(x || 1))), m = x / p; return (m <= 1 ? 1 : m <= 2 ? 2 : m <= 5 ? 5 : 10) * p; }
function autoScale(c, i) {
  const d = scope.disp && scope.disp.data[i]; if (!d) return;
  let lo = Infinity, hi = -Infinity; for (const v of d) if (isFinite(v)) { lo = Math.min(lo, v); hi = Math.max(hi, v); }
  if (!isFinite(lo)) return;
  c.scale = niceStep(Math.max(hi - lo, Math.abs(hi) * 1e-3, 1e-6) / 6); c.offset = +((hi + lo) / 2).toPrecision(6);
}
let scMouse = null;
function drawScope() {
  const cv = $("sc-cv"); if (!cv || $("dbg-scope").hidden) return;
  const r = cv.getBoundingClientRect(), dpr = Math.min(2, devicePixelRatio || 1);
  if (cv.width !== Math.round(r.width * dpr) || cv.height !== Math.round(r.height * dpr)) { cv.width = Math.round(r.width * dpr); cv.height = Math.round(r.height * dpr); }
  const g = cv.getContext("2d"), W = cv.width, H = cv.height, dx = W / 10, dy = H / 8;
  g.clearRect(0, 0, W, H);
  g.strokeStyle = "rgba(120,150,180,.18)"; g.lineWidth = 1;
  for (let i = 1; i < 10; i++) { g.beginPath(); g.moveTo(i * dx, 0); g.lineTo(i * dx, H); g.stroke(); }
  for (let j = 1; j < 8; j++) { g.beginPath(); g.moveTo(0, j * dy); g.lineTo(W, j * dy); g.stroke(); }
  g.strokeStyle = "rgba(120,150,180,.4)"; g.beginPath(); g.moveTo(0, H / 2); g.lineTo(W, H / 2); g.stroke();
  const d = scope.disp, N = scN();
  g.font = `${11 * dpr}px ${C.mono || "monospace"}`;
  if (d) {
    scope.ch.forEach((c, i) => {
      const a = d.data[i]; if (!a) return;
      g.strokeStyle = c.color; g.lineWidth = 1.3 * dpr; g.beginPath();
      const y = (v) => H / 2 - (v - c.offset) / c.scale * dy;
      if (N <= W) { let pen = false; for (let k = 0; k < N; k++) { const v = a[k]; if (!isFinite(v)) { pen = false; continue; } const X = k / (N - 1) * W; pen ? g.lineTo(X, y(v)) : g.moveTo(X, y(v)); pen = true; } }
      else for (let px = 0; px < W; px++) {  // more samples than pixels: each column's min and max
        const k0 = Math.floor(px / W * N), k1 = Math.max(k0 + 1, Math.floor((px + 1) / W * N)); let lo = Infinity, hi = -Infinity;
        for (let k = k0; k < k1; k++) { const v = a[k]; if (isFinite(v)) { lo = Math.min(lo, v); hi = Math.max(hi, v); } }
        if (isFinite(lo)) { g.moveTo(px, y(lo)); g.lineTo(px, y(hi) - 0.5); }
      }
      g.stroke();
    });
    if (d.trig !== null && d.trig !== undefined) {  // the trigger: where and at what level
      const X = d.trig / (N - 1) * W, c = scope.ch[scope.trig.src];
      g.strokeStyle = "rgba(255,255,255,.35)"; g.setLineDash([4 * dpr, 4 * dpr]); g.beginPath(); g.moveTo(X, 0); g.lineTo(X, H); g.stroke(); g.setLineDash([]);
      if (c) { const Y = H / 2 - (scope.trig.level - c.offset) / c.scale * dy; g.fillStyle = c.color; g.beginPath(); g.moveTo(0, Y - 5 * dpr); g.lineTo(8 * dpr, Y); g.lineTo(0, Y + 5 * dpr); g.fill(); }
    }
  } else { g.fillStyle = "rgba(200,215,230,.5)"; g.fillText(scope.ch.length ? (scope.running ? "waiting for samples …" : "Run to start") : "add a channel", 12 * dpr, 20 * dpr); }
  // labels: each channel's scale, the time base
  scope.ch.forEach((c, i) => { g.fillStyle = c.color; g.fillText(`${c.name}  ${halFmt(c.scale)}/div`, 8 * dpr, (16 + i * 14) * dpr); });
  g.fillStyle = "rgba(200,215,230,.6)";
  const tl = scope.tdiv >= 1 ? scope.tdiv + " s" : scope.tdiv * 1000 + " ms";
  g.fillText(`${tl}/div · ${scope.rate} Hz`, W - 140 * dpr, H - 8 * dpr);
  // the cursor: time from the trigger, each channel's value
  if (scMouse && d) {
    const k = Math.max(0, Math.min(N - 1, Math.round(scMouse.x * dpr / W * (N - 1)))), X = k / (N - 1) * W;
    g.strokeStyle = "rgba(255,255,255,.5)"; g.beginPath(); g.moveTo(X, 0); g.lineTo(X, H); g.stroke();
    const t0 = d.trig !== null && d.trig !== undefined ? d.trig : 0, ms = (k - t0) / scope.rate * 1000;
    $("sc-read").textContent = `t ${ms >= 0 ? "+" : ""}${ms.toFixed(ms < 10 && ms > -10 ? 2 : 1)} ms\n` + scope.ch.map((c, i) => `${c.name}  ${halFmt(+d.data[i][k].toPrecision(7))}`).join("\n");
  } else $("sc-read").textContent = "";
}
setInterval(() => { if (st.page === "debug" && dbgTab === "scope") { drawScope(); scopeStateUi(); } }, 100);
$("sc-cv").addEventListener("mousemove", (e) => { const r = e.target.getBoundingClientRect(); scMouse = { x: e.clientX - r.left }; drawScope(); });
$("sc-cv").addEventListener("mouseleave", () => { scMouse = null; drawScope(); });

// ------------------------------------------------------------ the scope's controls
$("sc-run").addEventListener("click", () => (scope.running ? scopeStop() : scopeRun()));
$("sc-add-btn").addEventListener("click", () => { const v = $("sc-add").value; $("sc-add").value = ""; const w = hal.watch.find((x) => x.name === v.trim()); scopeAdd(v, w ? w.kind : "pin"); });
$("sc-add").addEventListener("keydown", (e) => { if (e.key === "Enter") $("sc-add-btn").click(); });
$("sc-ch").addEventListener("change", (e) => { const tr = e.target.closest("[data-i]"), k = e.target.dataset.k; if (!tr || !k) return; const v = +e.target.value; if (isFinite(v) && (k !== "scale" || v > 0)) { scope.ch[+tr.dataset.i][k] = v; drawScope(); } });
$("sc-ch").addEventListener("click", (e) => {
  const rm = e.target.closest("[data-rm]"), au = e.target.closest("[data-auto]");
  if (rm) { scope.ch.splice(+rm.dataset.rm, 1); scope.trig.src = Math.min(scope.trig.src, Math.max(0, scope.ch.length - 1)); scopeChanged(); }
  if (au) { autoScale(scope.ch[+au.dataset.auto], +au.dataset.auto); renderScopeSide(); drawScope(); }
});
$("sc-autoall").addEventListener("click", () => { scope.ch.forEach((c, i) => autoScale(c, i)); renderScopeSide(); drawScope(); });
$("sc-rate").addEventListener("change", (e) => { scope.rate = +e.target.value; scopeChanged(); });
$("sc-tdiv").addEventListener("change", (e) => { scope.tdiv = +e.target.value; scopeChanged(); });
$("sc-mode").addEventListener("click", (e) => { const b = e.target.closest("button[data-m]"); if (!b) return; scope.mode = b.dataset.m; scope.armedAt = scope.total; scope.searchFrom = scope.total; if (scope.running) scope.state = scope.mode === "roll" ? "ROLL" : "WAITING FOR TRIGGER"; renderScopeSide(); });
$("sc-src").addEventListener("change", (e) => { scope.trig.src = +e.target.value; });
$("sc-edge").addEventListener("change", (e) => { scope.trig.edge = e.target.value; });
$("sc-level").addEventListener("change", (e) => { if (isFinite(+e.target.value)) scope.trig.level = +e.target.value; drawScope(); });
$("sc-pos").addEventListener("input", (e) => { scope.trig.pos = +e.target.value / 100; });
// from the HAL tab's watch list: a "scope" button per row
$("hal-watch").addEventListener("click", (e) => { const b = e.target.closest("[data-scope]"); if (!b) return; const w = hal.watch[+b.dataset.scope]; scopeAdd(w.name, w.kind); msg(`${w.name} on the scope`, "info"); }, true);

// the source: millweb's sampler, or the simulation's pins sampled here
let simScopeTimer = null;
const scopeSim = {
  start: async (names, rate) => {
    clearInterval(simScopeTimer);
    let last = performance.now();
    simScopeTimer = setInterval(async () => {
      const now = performance.now(), n = Math.max(1, Math.round((now - last) / 1000 * rate)); last = now;
      const v = await halSim.values(names), row = names.map((nm) => (typeof v[nm] === "boolean" ? +v[nm] : v[nm] ?? NaN));
      const data = new Float32Array(n * names.length); for (let k = 0; k < n; k++) data.set(row, k * names.length);
      scopeData(n, data, rate, 0);
    }, 50);
    return { ok: true };
  },
  stop: () => clearInterval(simScopeTimer),
};
const scopeSrc = () => (LIVE ? LIVE.scope : scopeSim);

// ------------------------------------------------------------ views: the watch list and the scope, by name
function viewOf() {
  return { watch: hal.watch.map((w) => ({ kind: w.kind, name: w.name })),
    scope: { ch: scope.ch.map(({ name, kind, color, scale, offset }) => ({ name, kind, color, scale, offset })), rate: scope.rate, tdiv: scope.tdiv, mode: scope.mode, trig: { ...scope.trig } } };
}
function renderViews() {
  $("dbg-views").innerHTML = views.length ? views.map((v, i) => `<span class="view" aria-current="${v.name === curView}"><button data-view="${i}" title="${v.watch.length} watched · ${(v.scope.ch || []).length} on the scope${i < 9 ? ` · Alt+${i + 1}` : ""}">${esc(v.name)}</button><button class="x" data-vdel="${i}" title="Delete the view">×</button></span>`).join("")
    : `<p class="note">None yet: set up the watch list and the scope, then Save view.</p>`;
}
function viewsLoad(list) { views = Array.isArray(list) ? list : []; if (st.page === "debug") renderViews(); }
async function viewsStore(list) {
  if (LIVE) { const r = await LIVE.viewsSave(list); if (!r.ok) msg("Views: " + r.err, "warn"); return r.ok; }
  views = list; try { localStorage.setItem("pmc.views", JSON.stringify(list)); } catch (e) { /* storage blocked */ }
  renderViews(); return true;
}
function viewOpen(i) {
  const v = views[i]; if (!v) return;
  const s = v.scope || {};
  hal.watch = (v.watch || []).map((w) => ({ kind: w.kind, name: w.name })); halWatchSend();
  const was = scope.running; scopeStop();
  scope.ch = (s.ch || []).map((c) => ({ ...c, auto: false }));
  if (s.rate) scope.rate = s.rate; if (s.tdiv) scope.tdiv = s.tdiv; if (s.mode) scope.mode = s.mode; if (s.trig) scope.trig = { ...scope.trig, ...s.trig };
  curView = v.name; scopeReset(); renderViews(); renderHalWatch(); renderHalList(); renderScopeSide(); drawScope();
  if (was && dbgTab === "scope") scopeRun();
  msg(`View: ${v.name}`, "info");
}
$("dbg-views").addEventListener("click", (e) => {
  const o = e.target.closest("[data-view]"), d = e.target.closest("[data-vdel]");
  if (o) viewOpen(+o.dataset.view);
  if (d) { const v = views[+d.dataset.vdel]; ask(`Delete the view ${v.name}?`, "For every browser.", "", () => { if (curView === v.name) curView = null; viewsStore(views.filter((x) => x !== v)); }, "confirm"); }
});
$("dbg-save").addEventListener("click", () => ask("Save view", "The watch list and the scope's set-up under this name (an existing name is replaced).", curView || "", async (name) => {
  name = name.trim(); if (!name) return;
  const list = views.filter((v) => v.name !== name).concat([{ name, ...viewOf() }]);
  if (await viewsStore(list)) { curView = name; renderViews(); msg(`View ${name} saved`, "ok"); }
}, "text"));
addEventListener("keydown", (e) => { if (st.page === "debug" && e.altKey && /^[1-9]$/.test(e.key)) { e.preventDefault(); viewOpen(+e.key - 1); } });
