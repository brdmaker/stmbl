/* Portal Mill Console: the Debug page's Drives tab. A card per stmbl drive
   (state, fault, bus voltage, temperatures, current, its smart-serial process
   data, how its console link is doing) and the drive's console, as over USB.
   On a machine the data is millweb's ([MILLWEB] DRIVE_LINK: "drv" and "dout"
   messages, drive_watch / drive_cmd / drive_detach); in the simulation four
   simulated drives with a small console of their own. Classic script after
   scope.js. */
"use strict";

const drv = { list: [], sel: null, watching: null, hist: [], hi: -1 };
try { drv.hist = JSON.parse(localStorage.getItem("pmc.dchist") || "[]").slice(-50); } catch (e) { /* storage blocked */ }

// ------------------------------------------------------------ the cards
const dfmt = (v, d = 1, u = "") => (v === null || v === undefined ? "—" : (+v).toFixed(d) + (u ? " " + u : ""));
const kb = (n) => (n > 9999 ? (n / 1024).toFixed(0) + " kB" : (n || 0) + " B");
const DRV_LINK = { ok: ["ok", "LINK OK"], silent: ["warn", "NO ANSWER"], missing: ["bad", "NO CONSOLE"], detached: ["warn", "DETACHED"], closed: ["off", "CLOSED"] };
function drvStateCls(d) {
  if (d.link !== "ok") return "off";
  return { ENABLED: "ok", PHASING: "info", DISABLED: "off", "SOFT FAULT": "bad", "HARD FAULT": "bad" }[d.state] || "off";
}
function drivesShow(list) {
  drv.list = Array.isArray(list) ? list : [];
  const first = !drv.sel && drv.list.length;
  if (first) drv.sel = drv.list[0].n;
  if (st.page === "debug" && dbgTab === "drives") { if (first) drvSelect(drv.sel); else renderDrives(); }
}
function renderDrives() {
  const el = $("drv-cards");
  if (!drv.list.length) {
    el.innerHTML = `<div class="panel"><h2>Drives</h2><p class="note">No drives here. On the machine: <code>[MILLWEB] DRIVE_LINK = X /tmp/drive-x hm2_7i96s.0.stbl.0.0</code>, a line per drive: its name, its console (sserialterm's pty or its USB port) and, if it has one, the HAL prefix of its smart-serial pins.</p></div>`;
    return;
  }
  el.innerHTML = drv.list.map((d) => {
    const pd = d.pd || null, link = DRV_LINK[d.link] || ["off", String(d.link).toUpperCase()];
    return `<div class="panel drv-card${d.n === drv.sel ? " sel" : ""}" data-n="${esc(d.n)}">
      <div class="h"><b>${esc(d.n)}</b><span class="pill ${drvStateCls(d)}" data-k="state">${esc(d.state || "—")}</span><span class="pill ${link[0]}" data-k="link">${link[1]}</span></div>
      <div class="fw" title="${esc(d.tty || "")}">${esc(d.fw || d.tty || "")}</div>
      ${d.fault ? `<div class="fault" data-k="fault">FAULT: ${esc(d.fault)}</div>` : d.last ? `<div class="lastf" data-k="last">Last fault: ${esc(d.last)}${d.last_at ? " · " + esc(d.last_at) : ""}</div>` : ""}
      ${d.link === "missing" && d.err ? `<p class="note">${esc(d.err)}</p>` : ""}
      ${d.link === "detached" ? `<p class="note">Left to other programs by ${esc(d.by || "?")}: <code>${esc(d.tty)}</code></p>` : ""}
      <dl>
        <dt>DC bus</dt><dd data-k="dc">${dfmt(d.dc, 0, "V")}</dd><dt>HV temp</dt><dd>${dfmt(d.hv_t, 0, "°C")}</dd><dt>Motor temp</dt><dd>${dfmt(d.mot_t, 0, "°C")}</dd><dt>AC current</dt><dd>${dfmt(d.ac, 2, "A")}</dd>
        <dt>Pos error</dt><dd>${dfmt(d.perr, 4)}</dd><dt>Saturation</dt><dd>${dfmt(d.sat, 2)}</dd>
        ${pd ? `<dt>Enable</dt><dd>${pd.enable === null || pd.enable === undefined ? "—" : pd.enable ? "on" : "off"}</dd><dt>Fault bit</dt><dd>${pd.fault ? "SET" : pd.fault === false ? "clear" : "—"}</dd>
        <dt>Velocity</dt><dd>${dfmt(pd.vel_fb, 1)}</dd><dt>Position</dt><dd>${dfmt(pd.pos_fb, 3)}</dd><dt>Current</dt><dd>${dfmt(pd.current, 2, "A")}</dd><dt>Power</dt><dd>${dfmt(pd.power, 0, "W")}</dd>` : ""}
      </dl>
      <div class="ft"><span>${d.age !== null && d.age !== undefined ? `values ${d.age} s old` : "no values yet"} · in ${kb(d.rx)} · out ${kb(d.tx)}</span>
        <button class="btn sm" data-con="${esc(d.n)}">Console</button>${LIVE ? `<button class="btn sm" data-det="${esc(d.n)}">${d.link === "detached" ? "Attach" : "Detach"}</button>` : ""}</div>
    </div>`;
  }).join("");
}
$("drv-cards").addEventListener("click", (e) => {
  const c = e.target.closest("[data-con]"), dt = e.target.closest("[data-det]");
  if (c) { drvSelect(c.dataset.con); $("dc-in").focus(); }
  if (dt) {
    const n = dt.dataset.det, d = drv.list.find((x) => x.n === n);
    if (d && d.link === "detached") drvSrc().detach(n, false).then((r) => { if (r && !r.ok) msg(r.err, "warn"); else if (drv.sel === n) drvSelect(n); });
    else ask(`Leave drive ${n}'s console to other programs?`, `millweb lets go of ${d ? d.tty : "it"} for 10 minutes (or until Attach), e.g. for servoterm-cli --dump. The card and this console stop meanwhile.`, "",
      () => drvSrc().detach(n, true).then((r) => { if (r && !r.ok) msg(r.err, "warn"); }), "confirm");
  }
});

// ------------------------------------------------------------ the console
function drvOut(text, cls) {
  const o = $("dc-out"), atEnd = o.scrollTop + o.clientHeight >= o.scrollHeight - 24;
  if (cls) { const s = document.createElement("span"); s.className = cls; s.textContent = text; o.appendChild(s); } else o.appendChild(document.createTextNode(text));
  while (o.childNodes.length > 4000) o.removeChild(o.firstChild);
  if (atEnd) o.scrollTop = o.scrollHeight;
}
function drvText(text) {
  for (const line of String(text).split(/(?<=\n)/)) if (line) drvOut(line, /^> /.test(line) ? "u" : /^(ERROR|! )/.test(line) ? "e" : /^WARNING/.test(line) ? "w" : null);
}
function drvMode() {
  const w = drvSrc().writable();
  $("dc-mode").textContent = w ? "READ + WRITE" : "READING"; $("dc-mode").className = "pill " + (w ? "warn" : "off");
}
async function drvSelect(n) {
  drv.sel = n; drv.watching = n;
  $("dc-out").textContent = ""; $("dc-name").textContent = n || "—";
  renderDrives(); drvMode();
  if (!n) return;
  const r = await drvSrc().watch(n);
  if (drv.sel !== n) return;
  if (r && r.ok) drvText(r.scroll || ""); else if (r) drvOut(`! ${r.err}\n`, "e");
}
function drvEnter() { if (!LIVE) drv.list = drvSim.list(); if (!drv.sel && drv.list.length) drv.sel = drv.list[0].n; renderDrives(); if (drv.sel && drv.watching !== drv.sel) drvSelect(drv.sel); else drvMode(); }
function drvLeave() { if (drv.watching) { drv.watching = null; drvSrc().watch(null); } }
async function drvSend(cmd, sure = false) {
  cmd = cmd.trim(); if (!cmd) return;
  if (!drv.sel) { msg("No drive to talk to.", "warn"); return; }
  if (!sure) {
    drv.hist = drv.hist.filter((x) => x !== cmd).concat([cmd]).slice(-50); drv.hi = -1;
    try { localStorage.setItem("pmc.dchist", JSON.stringify(drv.hist)); } catch (e) { /* storage blocked */ }
  }
  if (drv.watching !== drv.sel) await drvSelect(drv.sel);
  const r = await drvSrc().cmd(drv.sel, cmd, sure);
  if (r.ok) return;
  if (/^confirm:/.test(r.err)) ask(`Send "${cmd}" to drive ${drv.sel}?`, r.err.replace(/^confirm: /, "").replace(/^./, (c) => c.toUpperCase()) + ".", "", () => drvSend(cmd, true), "confirm");
  else drvOut(`! ${r.err}\n`, "e");
}
$("dc-form").addEventListener("submit", (e) => { e.preventDefault(); const v = $("dc-in").value; $("dc-in").value = ""; drvSend(v); });
$("dc-in").addEventListener("keydown", (e) => {
  if (e.key !== "ArrowUp" && e.key !== "ArrowDown") return;
  e.preventDefault(); if (!drv.hist.length) return;
  drv.hi = e.key === "ArrowUp" ? (drv.hi < 0 ? drv.hist.length - 1 : Math.max(0, drv.hi - 1)) : (drv.hi < 0 ? -1 : drv.hi + 1);
  if (drv.hi >= drv.hist.length) drv.hi = -1;
  $("dc-in").value = drv.hi < 0 ? "" : drv.hist[drv.hi];
});
$("dc-quick").addEventListener("click", (e) => { const b = e.target.closest("[data-c]"); if (b) drvSend(b.dataset.c); });
$("dc-clear").addEventListener("click", () => { $("dc-out").textContent = ""; });

// ------------------------------------------------------------ the simulation's drives
const drvSim = (() => {
  const names = ["X", "Y1", "Y2", "Z"], idx = { X: 0, Y1: 1, Y2: 2, Z: 3 }, t0 = Date.now();
  const set = {};  // pins set from the console
  let watching = null;
  const pinsOf = (n) => {
    const i = idx[n], fe = (st.fe[i] || 0) / 1000, on = st.on && !st.estop;
    return Object.assign({
      "fault0.state": on ? 1 : 0, "fault0.fault": 0, "fault0.last_fault": 0, "fault0.dc_volt": st.bus, "fault0.hv_temp": 38 + Math.abs(fe) * 300,
      "fault0.mot_temp": 34 + i, "fault0.ac_cur": on ? 0.4 + Math.abs(fe) * 80 : 0, "fault0.pos_error": on ? fe : 0, "fault0.sat": 0,
      "fault0.max_sat": 0.2, "fault0.max_pos_error": 5, "conf0.max_vel": 320, "conf0.max_ac_cur": 10, "conf0.polecount": 5, "linrev0.scale": 10, "pe0.p_in": on ? 15 + Math.abs(fe) * 900 : 15,
    }, set[n] || {});
  };
  const conf = "link pid\nlink pmsm\nlink enc_fb0\nlink sserial\nconf0.r = 2.15\nconf0.l = 0.023\nconf0.j = 0.000044\nconf0.polecount = 5\nconf0.max_vel = 320\nconf0.max_ac_cur = 10\nconf0.pos_bw = 42.8\nlinrev0.scale=10\n";
  const say = (n, text) => setTimeout(() => { if (watching === n && drv.sel === n) drvText(text); }, 60 + text.length * 2);  // ~ the link's speed, shortened
  const answer = (n, cmd) => {
    const p = pinsOf(n), w = cmd.split(/\s+/)[0];
    if (cmd.includes("=")) { const [k, v] = cmd.split("=").map((x) => x.trim()); if (!(k in p)) return `${k} not found\n`; (set[n] = set[n] || {})[k] = +v; return ""; }
    if (w === "about") return `######## software info ########\nSTMBL v0.9.4 (simulation)\nBranch master\nsize: 345128 crc:5be0c3a1\n`;
    if (w === "uptime") { const s = Math.round((Date.now() - t0) / 1000); return `up ${String(Math.floor(s / 3600)).padStart(2, "0")}:${String(Math.floor(s / 60) % 60).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}\n`; }
    if (w === "showconf") return conf;
    if (w === "list") return "conf0 fault0 hv0 iit0 linrev0 pe0 pid0 pmsm0 sserial0 term0\n";
    if (w === "help") return "about, showconf, list, uptime, a pin (fault0.dc_volt), a pin = value (the simulation keeps it)\n";
    const hits = Object.keys(p).filter((k) => k.startsWith(cmd));
    if (hits.length) return hits.map((k) => `${k} = ${(+p[k]).toFixed(6)}\n`).join("");
    return `${cmd}: not found\n`;
  };
  const DANGER = /^(flashsaveconf|flashloadconf|deleteconf|loadconf|bootloader|sserialboot|hardboot|reset|fw\w+|hv_update|jogl|jogr|start|stop|enable|disable|load|link|relink|fault)\b/;
  return {
    list: () => names.map((n) => {
      const p = pinsOf(n), on = st.on && !st.estop;
      return { n, tty: "(simulation)", link: "ok", fw: "STMBL v0.9.4 (simulation)", age: 0.4, rx: Math.round((Date.now() - t0) / 4), tx: Math.round((Date.now() - t0) / 12),
        state: on ? "ENABLED" : "DISABLED", fault: null, last: null, dc: p["fault0.dc_volt"], hv_t: p["fault0.hv_temp"], mot_t: p["fault0.mot_temp"], ac: p["fault0.ac_cur"],
        perr: p["fault0.pos_error"], sat: 0, pd: { enable: on, fault: false, pos_fb: n === "Z" ? st.pos.z : n === "X" ? st.pos.x : st.pos.y, vel_fb: 0, current: p["fault0.ac_cur"], power: p["pe0.p_in"] } };
    }),
    watch: async (n) => { watching = n; return { ok: true, scroll: n ? `(the simulation's drive ${n}: try about, showconf, fault0, conf0.max_vel = 300)\n` : "" }; },
    cmd: async (n, cmd, sure) => {
      if (cmd.length > 63) return { ok: false, err: "a line of 1 to 63 characters (the drive drops longer ones)" };
      if (DANGER.test(cmd) && !sure) return { ok: false, err: `confirm: '${cmd.split(" ")[0]}' can move the motor, restart the drive or rewrite its flash` };
      drvText(`> ${cmd}\n`);
      say(n, DANGER.test(cmd) ? "(the simulation doesn't run that)\n" : answer(n, cmd));
      return { ok: true };
    },
    detach: async () => ({ ok: false, err: "the simulation has no console to leave" }),
    writable: () => true,
  };
})();
const drvSrc = () => (LIVE ? LIVE.drives : drvSim);
setInterval(() => { if (!LIVE && st.page === "debug" && dbgTab === "drives") drivesShow(drvSim.list()); }, 500);
