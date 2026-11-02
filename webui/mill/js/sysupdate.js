/* Portal Mill Console: system updates of the controller (server/sysupdate.py). A "System" card on the
   Settings page (on the Configure page while LinuxCNC is down, the supervisor's only page): this system's
   version, the update server's releases with their notes, install any of them (newer or older) into the
   other slot, progress, then the controller restarts into it by itself; how the last update ended. While an
   install runs (until the restart) the releases are hidden and the buttons off. The header's UPDATE chip says a newer
   version is there. Only when the controller has an update helper (hello: sysupdate). */
"use strict";

const sys = { on: false, status: null, check: null, restarting: null, open: new Set(), said: null };

function sysCard() {
  let c = $("sys-card");
  if (c) return c;
  c = document.createElement("div");
  c.className = "card sys-card"; c.id = "sys-card";
  c.innerHTML = `<h3>System <span class="pill off" id="sys-pill">—</span></h3>
    <div class="sys-cur" id="sys-cur"></div>
    <div class="sys-last" id="sys-last" hidden></div>
    <div class="sys-prog" id="sys-prog" hidden><div class="bar"><b id="sys-bar"></b></div><span class="note" id="sys-prog-t"></span></div>
    <div class="row"><button class="btn sm" id="sys-check">Check for updates</button><button class="btn sm" id="sys-latest" hidden>Upgrade to latest</button><span class="spacer"></span><span class="note" id="sys-server"></span></div>
    <div class="sys-list" id="sys-list"></div>`;
  c.querySelector("#sys-check").onclick = async () => {
    $("sys-check").disabled = true;
    const r = await Live.L.sys.check();
    $("sys-check").disabled = false;
    if (!r.ok) msg(`Update check: ${r.err}`, "warn");
    else if (r.check && !r.check.ok) msg(r.check.error, "warn");
    else if (r.check) msg(r.check.update_available ? `New system version ${r.check.latest} available` : "The system is up to date", "ok");
  };
  c.querySelector("#sys-latest").onclick = () => { const k = sys.check; if (k && k.latest) sysInstall(k.releases[0]); };
  c.addEventListener("click", (e) => {
    const b = e.target.closest("[data-install]");
    if (b) { const r = (sys.check.releases || []).find((x) => x.version === b.dataset.install); if (r) sysInstall(r); }
    const n = e.target.closest("[data-notes]");
    if (n) { const v = n.dataset.notes; sys.open.has(v) ? sys.open.delete(v) : sys.open.add(v); sysRender(); }
  });
  return c;
}

// where the card lives: Settings, or (supervisor: LinuxCNC down) the Configure page
function sysPlace() {
  const c = sysCard();
  const home = document.body.classList.contains("sup-mode") ? $("p-config") : document.querySelector("#p-set .cards");
  if (c.parentNode !== home) home.prepend(c);
}

async function sysInstall(r) {
  const cur = sys.check && sys.check.current;
  const what = r.newer ? `Upgrade from ${cur} to ${r.version}?` : `Go back from ${cur} to the older ${r.version}?`;
  if (!confirm(`${what}\n\nIt is installed into the other system slot while this one keeps running (a few minutes); then the controller restarts into it by itself. If it doesn't start, the controller goes back to ${cur} by itself.`)) return;
  const res = await Live.L.sys.install(r.version);
  if (!res.ok) msg(`Install ${r.version}: ${res.err}`, "warn");
}

function sysEnable(on) {
  sys.on = on;
  sysPlace();
  sysCard().hidden = !on;
  if (!on) $("ch-update").hidden = true;
  if (on) Live.L.sys.status();
}

function sysIn(m) {
  if (m.status !== undefined) sys.status = m.status;
  if (m.check) sys.check = m.check;
  if (m.restarting !== undefined) sys.restarting = m.restarting;
  sysRender();
}

// versions are dotted numbers (2026.10.26.3): is a older than b?
function sysOlder(a, b) {
  const x = String(a || "").split(/[.-]/), y = String(b || "").split(/[.-]/);
  for (let i = 0; i < Math.max(x.length, y.length); i++) {
    const p = +x[i] || 0, q = +y[i] || 0;
    if (p !== q) return p < q;
  }
  return false;
}

// the release notes are Markdown: headings, bullets, `code`
function sysNotes(md) {
  const inl = (s) => esc(s).replace(/`([^`]+)`/g, "<code>$1</code>").replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>");
  let out = "", list = false;
  for (const line of String(md || "").split("\n")) {
    const h = line.match(/^#{1,4}\s+(.*)/), li = line.match(/^\s*[-*]\s+(.*)/);
    if (!li && list) { out += "</ul>"; list = false; }
    if (h) out += `<h4>${inl(h[1])}</h4>`;
    else if (li) { if (!list) { out += "<ul>"; list = true; } out += `<li>${inl(li[1])}</li>`; }
    else if (line.trim()) out += `<p>${inl(line)}</p>`;
  }
  return out + (list ? "</ul>" : "");
}

function sysRender() {
  if (!sys.on) return;
  sysPlace();
  const s = sys.status || {}, k = sys.check, ins = s.install || null;
  const last = s.last;
  const waiting = ins && ins.state === "installed" && last && last.outcome === "pending";  // installed, the restart to come
  const progRuns = typeof busy === "function" && busy();  // app.js: a program runs
  const locked = !!(ins && ins.state === "running") || waiting || !!sys.restarting;
  // this system
  $("sys-cur").innerHTML = s.error ? `<span class="pill bad">helper</span> ${esc(s.error)}`
    : s.version ? `Running <b>${esc(s.version)}</b> (slot ${esc(s.slot || "?")}, built ${esc(s.date || "?")})` +
      (s.components ? `<br><span class="note">${Object.entries(s.components).map(([a, b]) => `${esc(a)} ${esc(b)}`).join(" · ")}</span>` : "")
    : "…";
  $("sys-server").textContent = s.server ? `server ${s.server}` : "";
  // how the last update ended (once per outcome, also as an event)
  let lastHtml = "";
  if (last && last.outcome === "updated") lastHtml = `<span class="pill ok">${sysOlder(last.to, last.from) ? "went back" : "updated"}</span> from ${esc(last.from || "?")} to ${esc(last.to)}`;
  else if (last && last.outcome === "rolled_back") lastHtml = `<span class="pill bad">rolled back</span> ${esc(last.to)} didn't start: the controller went back to ${esc(s.version || "?")}`;
  $("sys-last").innerHTML = lastHtml; $("sys-last").hidden = !lastHtml;
  if (last && last.outcome !== "pending" && sys.said !== last.time + last.outcome) {
    if (sys.said !== null) msg(last.outcome === "updated" ? `System ${sysOlder(last.to, last.from) ? "back at" : "updated to"} ${last.to}` : `System ${last.to} didn't start: back to ${s.version}`, last.outcome === "updated" ? "ok" : "bad");
    sys.said = last.time + last.outcome;
  }
  // a running or finished install
  $("sys-prog").hidden = !ins || ins.state === undefined;
  if (ins) {
    $("sys-bar").style.width = `${ins.percent || 0}%`;
    $("sys-prog-t").textContent = sys.restarting ? `Restarting into ${sys.restarting}: the console is back in about a minute…`
      : ins.state === "running" ? `Installing ${ins.version}: ${ins.percent || 0} % ${ins.message || ""}`
      : waiting ? `${ins.version} installed into slot ${ins.slot}: the controller restarts ${progRuns ? "when the program has ended" : "now"}…`
      : ins.state === "installed" ? `${ins.version} installed into slot ${ins.slot}`
      : ins.state === "failed" ? `Installing ${ins.version} failed: ${String(ins.error || "").split("\n").slice(-1)[0]}` : "";
  }
  // the server's releases
  const avail = !!(k && k.ok && k.update_available);
  $("sys-pill").className = "pill " + (locked ? "info" : avail ? "warn" : k && k.ok ? "ok" : "off");
  $("sys-pill").textContent = sys.restarting || waiting ? "RESTARTING" : locked ? "INSTALLING" : avail ? "UPDATE AVAILABLE" : k && k.ok ? "UP TO DATE" : "—";
  $("ch-update").hidden = !avail || locked;
  $("sys-latest").hidden = !avail || locked;
  $("sys-check").disabled = locked;
  $("sys-list").hidden = locked;
  if (locked) return;
  if (avail) $("sys-latest").textContent = `Upgrade to ${k.latest}`;
  if (!k) { $("sys-list").innerHTML = '<p class="note">Check for updates to see the releases.</p>'; return; }
  if (!k.ok) { $("sys-list").innerHTML = `<p class="note">${esc(k.error || "no answer")}</p>`; return; }
  $("sys-list").innerHTML = k.releases.map((r) => {
    const tags = (r.current ? '<span class="pill ok">running</span>' : "") + (r.in_other_slot ? '<span class="pill off">in the other slot</span>' : "") +
      (r.newer ? '<span class="pill warn">newer</span>' : "");
    const btn = r.current ? "" : `<button class="btn sm" data-install="${esc(r.version)}"${locked ? " disabled" : ""}>${r.newer ? "Upgrade" : "Downgrade"}</button>`;
    const open = sys.open.has(r.version) || (r === k.releases[0] && r.newer && !sys.open.size);
    return `<div class="sys-rel"><div class="sys-rel-h"><b>${esc(r.version)}</b><span class="note">${esc(r.date || "")}${r.size ? " · " + (r.size / 1e6).toFixed(0) + " MB" : ""}</span>${tags}<span class="spacer"></span>` +
      `<button class="btn sm" data-notes="${esc(r.version)}">${open ? "Hide" : "What's new"}</button>${btn}</div>` +
      (open ? `<div class="sys-notes">${sysNotes(r.notes)}</div>` : "") + "</div>";
  }).join("");
}

$("ch-update").onclick = () => showPage(document.body.classList.contains("sup-mode") ? "config" : "set");
