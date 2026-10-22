/* Portal Mill Console: the Debug page's HAL tab, a halshow and halmeter in the browser.
   Left: every pin, signal and parameter (with its links), filtered and
   grouped by component; right: a watch list whose values come 10 times a
   second, bits as lights and numbers with a trend. On a machine the data is
   millweb's (hal_list, hal_watch, hal_set); in the simulation it is the
   simulated pins. Classic script after app.js. */
"use strict";

const hal = { data: null, kind: "pin", q: "", watch: [], sel: null, hist: new Map(), last: {}, open: new Set(), writable: false };
try { hal.watch = JSON.parse(localStorage.getItem("pmc.halwatch") || "[]").filter((w) => w && w.name && w.kind).slice(0, 64); } catch (e) { /* storage blocked */ }
const HAL_ROWS = 400;  // rows drawn at once: filter for the rest

// the source: millweb's, or the simulation's few pins
const halSim = {
  list: async () => ({
    pins: pins.map(([n, f]) => { const v = f(); return [n, /^(TRUE|FALSE)$/.test(v) ? "bit" : "float", "OUT", n.split(".")[0], v === "TRUE" ? true : v === "FALSE" ? false : +v, ""]; }),
    sigs: [], params: [],
  }),
  watch: async () => null,
  set: async () => ({ ok: false, err: "the simulation's pins are read only" }),
  values: async (names) => Object.fromEntries(names.map((n) => { const p = pins.find((x) => x[0] === n); if (!p) return [n, null]; const v = p[1](); return [n, v === "TRUE" ? true : v === "FALSE" ? false : +v]; })),
};
const halSrc = () => (LIVE ? LIVE.hal : halSim);

async function halLoad() {
  $("hal-count").textContent = "reading …";
  try { hal.data = await halSrc().list(); } catch (e) { hal.data = null; $("hal-count").textContent = String(e.message || e); return; }
  hal.writable = !!(LIVE && LIVE.halWritable && LIVE.halWritable());
  $("hal-count").textContent = `${hal.data.pins.length} pins · ${hal.data.sigs.length} signals · ${hal.data.params.length} parameters`;
  renderHalList(); halWatchSend(); renderHalWatch();
}

// ------------------------------------------------------------ the browser
const halFmt = (v) => v === null || v === undefined ? "—" : typeof v === "boolean" ? (v ? "TRUE" : "FALSE") : typeof v === "number" ? (Number.isInteger(v) ? String(v) : (+v.toFixed(6)).toString()) : String(v);
function halRows() {
  if (!hal.data) return [];
  const q = hal.q.toLowerCase();
  const src = hal.kind === "pin" ? hal.data.pins.map(([n, t, d, o, v, s]) => ({ kind: "pin", name: n, type: t, dir: d, owner: o, value: v, link: s }))
    : hal.kind === "sig" ? hal.data.sigs.map(([n, t, v, drv, rd]) => ({ kind: "sig", name: n, type: t, dir: "", owner: "", value: v, link: drv, readers: rd }))
    : hal.data.params.map(([n, t, d, o, v]) => ({ kind: "param", name: n, type: t, dir: d, owner: o, value: v, link: "" }));
  return q ? src.filter((r) => r.name.toLowerCase().includes(q) || (r.link || "").toLowerCase().includes(q) || (r.owner || "").toLowerCase().includes(q)) : src;
}
function renderHalList() {
  const rows = halRows(), groups = new Map();
  for (const r of rows) { const g = hal.kind === "sig" ? (r.name.split(/[-_.]/)[0] || r.name) : r.name.split(".")[0]; if (!groups.has(g)) groups.set(g, []); groups.get(g).push(r); }
  let drawn = 0, html = "";
  const many = rows.length > 60 && !hal.q;  // big lists open folded, a filter opens them
  for (const [g, list] of groups) {
    const open = !many || hal.open.has(g);
    html += `<details class="hal-g" data-g="${esc(g)}"${open ? " open" : ""}><summary><b>${esc(g)}</b> <span class="muted">${list.length}</span></summary>`;
    if (open) for (const r of list) {
      if (drawn++ >= HAL_ROWS) break;
      const watched = hal.watch.some((w) => w.name === r.name && w.kind === r.kind);
      html += `<div class="hal-row${hal.sel && hal.sel.name === r.name && hal.sel.kind === r.kind ? " sel" : ""}" data-name="${esc(r.name)}" data-kind="${r.kind}">
        <span class="n">${esc(r.name)}</span><span class="t">${r.type}${r.dir ? " " + r.dir : ""}</span><span class="v">${esc(halFmt(r.value))}</span>
        <span class="l">${r.link ? (hal.kind === "pin" ? (r.dir === "OUT" ? "⇒ " : "⇐ ") : "⇐ ") + esc(r.link) : ""}</span><span class="w">${watched ? "●" : "+"}</span></div>`;
    }
    html += "</details>";
    if (drawn >= HAL_ROWS) { html += `<p class="note">${rows.length - HAL_ROWS} more: filter to see them.</p>`; break; }
  }
  $("hal-list").innerHTML = html || `<p class="note">${hal.data ? "Nothing matches." : "Not read yet."}</p>`;
}
function halFind(kind, name) {
  if (!hal.data) return null;
  if (kind === "pin") { const p = hal.data.pins.find((x) => x[0] === name); return p && { kind, name, type: p[1], dir: p[2], owner: p[3], value: p[4], link: p[5] }; }
  if (kind === "sig") { const g = hal.data.sigs.find((x) => x[0] === name); return g && { kind, name, type: g[1], value: g[2], link: g[3], readers: g[4] }; }
  const q = hal.data.params.find((x) => x[0] === name); return q && { kind, name, type: q[1], dir: q[2], owner: q[3], value: q[4] };
}
// what a pin or signal is joined to: click a name to go to it
function renderHalDetail() {
  const r = hal.sel && halFind(hal.sel.kind, hal.sel.name), el = $("hal-detail");
  if (!r) { el.innerHTML = `<p class="note">Click a pin, signal or parameter: it joins the watch list, and its links show here.</p>`; return; }
  const go = (kind, n) => `<button class="lnk" data-go-kind="${kind}" data-go="${esc(n)}">${esc(n)}</button>`;
  let links = "";
  if (r.kind === "pin") {
    const g = r.link && halFind("sig", r.link);
    links = r.link ? `<dt>Signal</dt><dd>${go("sig", r.link)}</dd>` + (g ? `<dt>Its driver</dt><dd>${g.link ? (g.link === r.name ? "this pin" : go("pin", g.link)) : '<span class="muted">nothing</span>'}</dd><dt>Its readers</dt><dd>${(g.readers || []).map((p) => (p === r.name ? "this pin" : go("pin", p))).join(" ") || '<span class="muted">nothing</span>'}</dd>` : "")
      : `<dt>Signal</dt><dd class="muted">none (unconnected)</dd>`;
  }
  if (r.kind === "sig") links = `<dt>Driven by</dt><dd>${r.link ? go("pin", r.link) : '<span class="muted">nothing</span>'}</dd><dt>Read by</dt><dd>${(r.readers || []).map((p) => go("pin", p)).join(" ") || '<span class="muted">nothing</span>'}</dd>`;
  el.innerHTML = `<h3>${esc(r.name)} <span class="pill info">${r.kind === "sig" ? "SIGNAL" : r.kind === "param" ? "PARAMETER" : "PIN"}</span></h3>
    <dl class="kv"><dt>Type</dt><dd>${r.type}${r.dir ? " · " + r.dir : ""}</dd>${r.owner ? `<dt>Component</dt><dd>${esc(r.owner)}</dd>` : ""}${links}</dl>`;
}

// ------------------------------------------------------------ the watch list
function halWatchSend() { try { localStorage.setItem("pmc.halwatch", JSON.stringify(hal.watch)); } catch (e) { /* storage blocked */ } halSrc().watch(hal.watch.map((w) => w.name)); }
function halAdd(kind, name) {
  if (!hal.watch.some((w) => w.name === name && w.kind === kind)) { if (hal.watch.length >= 64) { msg("The watch list holds 64.", "warn"); return; } hal.watch.push({ kind, name }); halWatchSend(); }
  hal.sel = { kind, name }; renderHalDetail(); renderHalList(); renderHalWatch();
}
// can this one be set? an input pin on no signal, a writable parameter, a signal nothing drives
function halSettable(w) {
  if (!hal.writable) return false;
  const r = halFind(w.kind, w.name); if (!r) return false;
  return (r.kind === "pin" && (r.dir === "IN" || r.dir === "IO") && !r.link) || (r.kind === "param" && r.dir === "RW") || (r.kind === "sig" && !r.link);
}
function renderHalWatch() {
  $("hal-watch").innerHTML = hal.watch.length ? `<thead><tr><th>Name</th><th class="n">Value</th><th>Trend</th><th></th></tr></thead><tbody>` + hal.watch.map((w, i) => {
    const r = halFind(w.kind, w.name), bit = r && r.type === "bit";
    return `<tr data-i="${i}"><td class="num" title="${esc(w.kind)}${r ? " · " + r.type + (r.dir ? " " + r.dir : "") : ""}">${esc(w.name)}</td>
      <td class="n">${bit ? `<span class="led" id="hw-led-${i}"></span>` : ""}<span id="hw-v-${i}">—</span></td>
      <td>${bit ? "" : `<canvas class="spark" id="hw-s-${i}" width="140" height="26"></canvas>`}</td>
      <td class="acts">${r && r.type !== "port" ? `<button class="btn sm" data-scope="${i}" title="Add to the scope">∿</button>` : ""}${halSettable(w) ? `<button class="btn sm" data-set="${i}">Set</button>` : ""}<button class="btn sm" data-rm="${i}" title="Stop watching">×</button></td></tr>`;
  }).join("") + "</tbody>" : `<tbody><tr><td class="muted">Nothing watched yet.</td></tr></tbody>`;
  halShow(hal.last);
}
// values in: the table's numbers and lights, the trends (the last 200)
function halShow(vals) {
  hal.last = Object.assign(hal.last, vals || {});
  hal.watch.forEach((w, i) => {
    const v = hal.last[w.name], el = $("hw-v-" + i); if (!el) return;
    el.textContent = halFmt(v);
    const led = $("hw-led-" + i); if (led) led.classList.toggle("on", v === true);
    if (typeof v === "number") { const h = hal.hist.get(w.name) || []; if (vals && w.name in vals) { h.push(v); if (h.length > 200) h.shift(); } hal.hist.set(w.name, h); sparkline($("hw-s-" + i), h); }
  });
}
function sparkline(cv, h) {
  if (!cv || h.length < 2) return;
  const g = cv.getContext("2d"), W = cv.width, H = cv.height;
  let lo = Math.min(...h.slice(-200)), hi = Math.max(...h.slice(-200)); if (hi - lo < 1e-12) { lo -= 1; hi += 1; }
  g.clearRect(0, 0, W, H); g.strokeStyle = C.accent; g.lineWidth = 1.2; g.beginPath();
  h.forEach((v, k) => { const x = k / (h.length - 1) * (W - 2) + 1, y = H - 2 - (v - lo) / (hi - lo) * (H - 4); k ? g.lineTo(x, y) : g.moveTo(x, y); });
  g.stroke();
}

// ------------------------------------------------------------ controls
$("hal-q").addEventListener("input", (e) => { hal.q = e.target.value.trim(); renderHalList(); });
$("hal-kind").addEventListener("click", (e) => { const b = e.target.closest("button[data-k]"); if (!b) return; hal.kind = b.dataset.k; $$("#hal-kind button").forEach((x) => x.setAttribute("aria-pressed", String(x === b))); renderHalList(); });
$("hal-refresh").addEventListener("click", halLoad);
$("hal-list").addEventListener("click", (e) => { const r = e.target.closest(".hal-row"); if (r) halAdd(r.dataset.kind, r.dataset.name); });
$("hal-list").addEventListener("toggle", (e) => { const d = e.target.closest("details.hal-g"); if (!d) return; if (d.open) { if (!hal.open.has(d.dataset.g)) { hal.open.add(d.dataset.g); renderHalList(); } } else hal.open.delete(d.dataset.g); }, true);
$("hal-detail").addEventListener("click", (e) => { const b = e.target.closest("[data-go]"); if (b) halAdd(b.dataset.goKind, b.dataset.go); });
$("hal-watch").addEventListener("click", (e) => {
  const rm = e.target.closest("[data-rm]"), set = e.target.closest("[data-set]");
  if (rm) { hal.watch.splice(+rm.dataset.rm, 1); halWatchSend(); renderHalWatch(); renderHalList(); return; }
  if (set) {
    const w = hal.watch[+set.dataset.set], r = halFind(w.kind, w.name);
    ask(`Set ${w.name}`, `${r.type}${r.type === "bit" ? ": 1 or 0" : ""}. This changes the running machine's HAL (halcmd ${w.kind === "sig" ? "sets" : "setp"}).`, halFmt(hal.last[w.name]) === "TRUE" ? "1" : halFmt(hal.last[w.name]) === "FALSE" ? "0" : halFmt(hal.last[w.name]), async (v) => {
      const res = await halSrc().set(w.name, v.trim(), w.kind);
      if (!res.ok) msg(`${w.name}: ${res.err}`, "warn"); else msg(`${w.name} = ${v.trim()}`, "ok");
    }, "text");
  }
});
$("hal-clear").addEventListener("click", () => { hal.watch = []; halWatchSend(); renderHalWatch(); renderHalList(); });
// the simulation: its pins' values at 10 Hz while the page is open
setInterval(async () => { if (!LIVE && st.page === "debug" && hal.watch.length) halShow(await halSim.values(hal.watch.map((w) => w.name))); }, 100);
// the list's rows on screen: their values once a second (the list itself is read once)
let halRowsBusy = false;
setInterval(async () => {
  if (st.page !== "debug" || dbgTab !== "hal" || halRowsBusy || !hal.data) return;
  const rows = [...document.querySelectorAll("#hal-list .hal-row")]; if (!rows.length) return;
  halRowsBusy = true;
  try {
    const v = await halSrc().values(rows.map((r) => r.dataset.name));
    for (const r of rows) if (r.dataset.name in v) { const c = r.querySelector(".v"); if (c) c.textContent = halFmt(v[r.dataset.name]); }
  } finally { halRowsBusy = false; }
}, 1000);
