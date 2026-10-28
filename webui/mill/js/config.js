/* Portal Mill Console: the Configure page. The machine in one file,
   machine.json (board, bitstream, axes, joints and their drives, I/O, the
   console), edited in forms or as text, checked and made into LinuxCNC's INI
   and HAL by millweb (Check, the Generated tab), saved with a history, and
   LinuxCNC restarted with it. On a machine run by millweb-supervisor:
   millweb while LinuxCNC runs, the supervisor itself while it doesn't
   (sup-mode: only this page). In the simulation: an example, kept in this
   browser. Classic script after drives.js. */
"use strict";

const cfg = { tab: "machine", text: "", custom: "", m: null, def: {}, saved: "", savedCustom: "", files: null, errors: [], diff: "",
  history: [], bitstreams: [], models: [], status: null, loaded: false, jsonBad: null, view: null, dir: "" };

// ------------------------------------------------------------ paths in the machine object
const cget = (o, p) => p.split(".").reduce((a, k) => (a == null ? undefined : a[k]), o);
function cset(o, p, v) {
  const ks = p.split(".");
  let a = o;
  for (let i = 0; i < ks.length - 1; i++) {
    if (a[ks[i]] == null || typeof a[ks[i]] !== "object") a[ks[i]] = /^\d+$/.test(ks[i + 1]) ? [] : {};
    a = a[ks[i]];
  }
  if (v === undefined) delete a[ks[ks.length - 1]]; else a[ks[ks.length - 1]] = v;
}
const cdef = (p) => cget(cfg.def, p);

// ------------------------------------------------------------ fields
// kind: text | num | bool | sel | lines (a list, a line each) | kv (KEY = value lines)
function fld(path, label, kind = "text", o = {}) {
  const v = cget(cfg.m, path), d = o.def !== undefined ? o.def : cdef(path);
  const ph = d === undefined || d === null || d === "" ? "" : ` placeholder="${esc(String(d))}"`;
  const help = o.help ? `<span class="h">${o.help}</span>` : "";
  const attrs = `data-path="${path}" data-kind="${kind}"${o.re ? ' data-re="1"' : ""}`;
  if (kind === "bool") return `<label class="cf chk${o.wide ? " wide" : ""}" data-f="${path}"><input type="checkbox" ${attrs}${(v ?? d) ? " checked" : ""}> ${label}${help}</label>`;
  let input;
  if (kind === "sel") input = `<select ${attrs}>${o.choices.map(([c, l]) => `<option value="${esc(String(c))}"${String(v ?? d) === String(c) ? " selected" : ""}>${esc(l || String(c))}</option>`).join("")}</select>`;
  else if (kind === "lines") input = `<textarea ${attrs} spellcheck="false" rows="4">${esc((v || []).join("\n"))}</textarea>`;
  else if (kind === "kv") input = `<textarea ${attrs} spellcheck="false" rows="3">${esc(Object.entries(v || {}).map(([k, x]) => `${k} = ${x}`).join("\n"))}</textarea>`;
  else if (kind === "color") input = `<input type="color" ${attrs} value="${esc(String(v ?? d ?? "#8a99ab"))}">`;
  else input = `<input ${attrs} ${kind === "num" ? 'inputmode="decimal"' : 'spellcheck="false"'} value="${v === undefined || v === null ? "" : esc(String(v))}"${ph}>`;
  return `<label class="cf${o.wide ? " wide" : ""}" data-f="${path}">${label}${input}${help}</label>`;
}
function readField(el) {
  const k = el.dataset.kind, raw = el.type === "checkbox" ? el.checked : el.value;
  if (k === "bool") return raw;
  if (k === "num") { const s = String(raw).trim(); if (s === "") return undefined; const n = Number(s); return isFinite(n) ? n : s; }
  if (k === "lines") return String(raw).split("\n").map((x) => x.trim()).filter(Boolean);
  if (k === "kv") return Object.fromEntries(String(raw).split("\n").map((x) => x.split("=")).filter((x) => x.length >= 2 && x[0].trim()).map(([a, ...b]) => [a.trim(), b.join("=").trim()]));
  if (k === "text" && raw === "" && el.dataset.keep !== "1") return undefined;
  return raw;
}

// ------------------------------------------------------------ the tabs
const BOARDS = [["none", "none: no hardware (simulated joints, stmbl over CAN)"], ["hm2_eth", "Mesa over Ethernet (hm2_eth)"], ["hm2_spix", "Mesa over SPI (hm2_spix)"], ["hm2_rpspi", "Mesa over SPI, Raspberry Pi (hm2_rpspi)"], ["hm2_soc_ol", "Mesa in an SoC FPGA, Zynq (hm2_soc_ol)"]];
const DRIVE_T = [["sim", "simulated"], ["stmbl_sserial", "stmbl, smart-serial"], ["stmbl_can", "stmbl, CAN"], ["stmbl_sim", "stmbl, simulated drive"]];
const HOME_T = [["absolute", "absolute (at once)"], ["immediate", "here (no move)"], ["switch", "switch"], ["index", "index"]];
const TABS = {
  machine: () => `<div class="cf-grid">
      ${fld("name", "Name")}
      ${fld("type", "Machine", "sel", { def: "gantry", live: true, choices: [["gantry", "gantry: the bridge moves in Y over a fixed table"], ["bridge", "fixed bridge: the table moves in Y (CN 720)"]], help: "the 3D view's machine; a fixed bridge has one joint an axis" })}
      ${fld("servo_period_ns", "Servo period (ns)", "num", { help: "1000000: 1 kHz, 500000: 2 kHz" })}
      ${fld("spindle.type", "Spindle", "sel", { choices: [["none", "none"], ["sim", "simulated (answers at once)"]] })}
      ${fld("spindle.max_rpm", "Spindle max (rpm)", "num")}
      ${fld("display.max_feed_override", "Feed override max", "num", { help: "2: 200 %" })}
      ${fld("display.max_spindle_override", "Spindle override max", "num")}
      ${fld("display.min_spindle_override", "Spindle override min", "num")}
    </div>
    <h3>Tool setter</h3>
    <label class="cf chk"><input type="checkbox" id="cf-ts"${cfg.m.toolsetter ? " checked" : ""}> A tool setter (the Tools page's Measure)</label>
    ${cfg.m.toolsetter ? `<div class="cf-grid">${fld("toolsetter.x", "X (machine)", "num")}${fld("toolsetter.y", "Y (machine)", "num")}${fld("toolsetter.z_above", "Z above it", "num")}${fld("toolsetter.z_ref", "Z of the reference tool's tip", "num", { help: "tool length = touched Z − this" })}${fld("toolsetter.travel", "Search travel", "num")}</div>` : ""}`,
  axes: () => {
    const axes = cfg.m.axes || {}, J = cfg.m.joints || [];
    const ax = Object.keys(axes).map((a) => `<tr data-axis="${a}"><td><b>${a}</b></td>${["min", "max", "max_vel", "max_acc"].map((k) => `<td data-f="axes.${a}.${k}">${fldIn(`axes.${a}.${k}`, "num")}</td>`).join("")}<td><button class="btn sm" data-rmaxis="${a}" title="Remove the axis">×</button></td></tr>`).join("");
    const free = "XYZABCUVW".split("").filter((a) => !axes[a]);
    const jr = J.map((j, i) => {
      const d = j.drive || {}, h = j.home || {}, t = d.type || "sim", hm = h.method || "absolute", P = `joints.${i}`;
      const drv = t === "stmbl_sserial" ? fld(`${P}.drive.port`, "Smart-serial port", "num", { def: 0 }) + fld(`${P}.drive.channel`, "Channel", "num", { def: 0 })
        : t === "stmbl_can" ? fld(`${P}.drive.bus`, "CAN bus", "num", { def: 0 }) + fld(`${P}.drive.node`, "Node", "num", { def: 1 })
        : t === "stmbl_sim" ? fld(`${P}.drive.conf`, "Drive config (in the folder)", "text", { def: "drive.conf" }) + fld(`${P}.drive.sim`, "Sim commands file", "text", { def: "" }) : "";
      const homing = hm === "switch" ? fld(`${P}.home.switch`, "Switch pin", "text", { def: "", re: true, help: t === "sim" ? "sim: a simulated switch" : "" }) + (h.switch === "sim" ? fld(`${P}.home.sim_at`, "Switch from the start", "num", { def: 10, help: "in the search direction" }) : "") + fld(`${P}.home.search_vel`, "Search speed", "num", { def: "", help: "its sign: the direction" }) + fld(`${P}.home.latch_vel`, "Latch speed", "num", { def: "" }) + fld(`${P}.home.offset`, "Offset", "num", { def: 0 })
        : hm === "index" ? fld(`${P}.home.latch_vel`, "Latch speed", "num", { def: "" }) + fld(`${P}.home.offset`, "Offset", "num", { def: 0 }) : "";
      return `<div class="cf-joint" data-joint="${i}"><div class="h"><b>Joint ${i}</b><span class="muted">${esc(j.axis || "?")} · ${esc((DRIVE_T.find((x) => x[0] === t) || [t, t])[1])}</span><span class="spacer"></span><button class="btn sm" data-rmjoint="${i}" title="Remove the joint">Remove</button></div>
        <div class="cf-grid">
          ${fld(`${P}.axis`, "Axis", "sel", { re: true, choices: Object.keys(axes).map((a) => [a, a]) })}
          ${fld(`${P}.drive.type`, "Drive", "sel", { re: true, choices: DRIVE_T })}
          ${drv}
          ${t !== "sim" ? fld(`${P}.drive.console`, "Its console on the Drives tab", "bool", { def: true }) : ""}
          ${fld(`${P}.ferror`, "Following error", "num", { def: 1 })}${fld(`${P}.min_ferror`, "… at rest", "num", { def: 0.25 })}
          ${t !== "sim" ? fld(`${P}.phasing_ferror`, "… while it phases", "num", { def: "off", help: "until homed and on a while: a drive that autophases at enable" }) + fld(`${P}.phasing_time`, "… for (s) after enable", "num", { def: 1.5 }) : ""}
          ${fld(`${P}.home.method`, "Homing", "sel", { re: true, choices: HOME_T })}${fld(`${P}.home.pos`, "Home at", "num", { def: 0 })}${fld(`${P}.home.sequence`, "Sequence", "num", { def: 0 })}
          ${homing}
          ${fld(`${P}.min`, "Joint min", "num", { def: "axis − 1" })}${fld(`${P}.max`, "Joint max", "num", { def: "axis + 1" })}${fld(`${P}.max_vel`, "Joint max vel", "num", { def: "axis × 1.1" })}${fld(`${P}.max_acc`, "Joint max acc", "num", { def: "axis × 1.1" })}
        </div></div>`;
    }).join("");
    return `<h3>Axes <small class="muted">(the machine's travel and speeds: mm, mm/s, mm/s²)</small></h3>
      <div class="wrapx"><table class="cf-t"><thead><tr><th>Axis</th><th>Min</th><th>Max</th><th>Max vel</th><th>Max acc</th><th></th></tr></thead><tbody>${ax}</tbody></table></div>
      ${free.length ? `<div class="row"><select id="cf-newaxis">${free.map((a) => `<option>${a}</option>`).join("")}</select><button class="btn sm" id="cf-addaxis">Add axis</button></div>` : ""}
      <h3>Joints <small class="muted">(a motor each; two on one axis make a gantry, homed together)</small></h3>
      ${jr}
      <div class="row"><button class="btn sm" id="cf-addjoint">Add joint</button></div>`;
  },
  hw: () => {
    const b = cfg.m.board || {}, bs = cfg.m.bitstream || {}, hm2 = String(b.type || "none").startsWith("hm2_"), ld = bs.loader || "none";
    const files = cfg.bitstreams.map((x) => [x.name, `${x.name} (${fmtSize(x.size)})`]);
    const anyCan = (cfg.m.joints || []).some((j) => j.drive && j.drive.type === "stmbl_can");
    return `<h3>Board</h3><div class="cf-grid">
        ${fld("board.type", "Interface", "sel", { choices: BOARDS, re: true })}
        ${hm2 ? fld("board.name", "Board name", "text", { help: "as in its pins: 7i96s, 7c81 (hm2_<name>.0…)" }) : ""}
        ${hm2 ? fld("board.params", "Driver parameters", "text", { help: "e.g. board_ip=10.10.10.10, spiclk_rate=35714" }) : ""}
        ${hm2 ? fld("board.config", "config=", "text", { help: "e.g. num_encoders=0 sserial_port_0=00000000" }) : ""}
        ${hm2 ? fld("board.watchdog_ms", "Watchdog (ms)", "num") : ""}</div>
      <h3>FPGA bitstream <small class="muted">(loaded before each start)</small></h3><div class="cf-grid">
        ${fld("bitstream.loader", "Loader", "sel", { re: true, choices: [["none", "none (the FPGA keeps its own)"], ["fpga_manager", "Linux FPGA manager (Zynq: .bit.bin)"], ["command", "a command"]] })}
        ${ld !== "none" ? fld("bitstream.file", "File (bitstreams/)", "sel", { choices: [["", "—"], ...files] }) : ""}
        ${ld === "fpga_manager" ? fld("bitstream.fpga", "FPGA manager", "text", { help: "/sys/class/fpga_manager/<this>" }) : ""}
        ${ld === "command" ? fld("bitstream.command", "Command", "text", { wide: true, help: "{file}: the bitstream's path; runs as the machine's user (sudo -n for root)" }) : ""}
        ${ld !== "none" ? fld("bitstream.reload", "Load", "sel", { choices: [["changed", "when changed or after a boot"], ["always", "at every start"]] }) : ""}</div>
      <div class="row"><input type="file" id="cf-bsfile" hidden><button class="btn sm" id="cf-bsup">Upload a bitstream…</button>
        ${cfg.bitstreams.map((x) => `<button class="btn sm" data-bsdel="${esc(x.name)}" title="Delete ${esc(x.name)}">× ${esc(x.name)}</button>`).join("")}</div>
      ${anyCan || (cfg.m.can && (cfg.m.can.spi || []).length) ? `<h3>CAN (stmblcan)</h3><div class="cf-grid">${fld("can.spi", "SPI devices, a line a bus", "lines", { help: "/dev/spidev0.0, or sim" })}${fld("can.discover", "Ask the drives for their pins at load", "bool")}${fld("can.spi_hz", "SPI clock (Hz)", "num")}</div>` : ""}`;
  },
  io: () => `<h3>E-STOP</h3><div class="cf-grid">
      ${fld("estop.input", "Input pin (true: chain closed)", "text", { help: "blank: no E-STOP input (a Mesa board's watchdog still trips it)" })}
      ${fld("estop.invert", "Inverted", "bool")}</div>
    <h3>Probe</h3><div class="cf-grid">
      ${fld("probe.type", "Probe", "sel", { re: true, choices: [["none", "none"], ["input", "an input pin"], ["sim", "simulated (trips below a Z)"]] })}
      ${(cfg.m.probe || {}).type === "input" ? fld("probe.input", "Input pin", "text") + fld("probe.invert", "Inverted", "bool") : ""}
      ${(cfg.m.probe || {}).type === "sim" ? fld("probe.sim_z", "Trips below machine Z", "num") : ""}</div>`,
  console: () => `<div class="cf-grid">
      ${fld("console.port", "Port", "num")}${fld("console.host", "Address", "text")}
      ${fld("console.token", "Token for control", "text", { help: "blank: none" })}
      ${fld("console.on_control_loss", "Controlling browser gone", "sel", { choices: [["continue", "the machine carries on"], ["hold", "feed hold"]] })}
      ${fld("console.cpus", "CPU cores for millweb", "text", { help: "off LinuxCNC's real-time core: 0, 0-1" })}
      ${fld("console.hal_write", "HAL page may set pins", "bool")}${fld("console.drive_write", "Drive consoles may write", "bool")}
      ${fld("console.pins", "Status page pins", "lines", { wide: true })}
      ${fld("console.extra", "More [MILLWEB] settings (KEY = value)", "kv", { wide: true, help: "POLL_HZ, HAL_HZ, PREVIEW_SEGS, CERT, KEY, SPINDLE_LOAD_PIN, …" })}</div>`,
  model: () => {
    const md = cfg.m.model || {}, parts = md.parts || [], files = cfg.models.map((x) => [x.name, `${x.name} (${fmtSize(x.size)})`]);
    const cards = parts.map((p, i) => {
      const P = `model.parts.${i}`;
      return `<div class="cf-joint" data-part="${i}"><div class="h"><b>${esc(p.name || "Part " + (i + 1))}</b><span class="muted">${esc(p.file || "no file")}</span><span class="spacer"></span><button class="btn sm" data-rmpart="${i}">Remove</button></div>
        <div class="cf-grid">
          ${fld(`${P}.name`, "Name", "text", { def: "" })}
          ${fld(`${P}.file`, "File (models/)", "sel", { choices: [["", "—"], ...files], live: true })}
          ${fld(`${P}.moves`, "Moves", "sel", { choices: MOVES_T, def: "" })}
          ${["X", "Y", "Z"].map((a, k) => fld(`${P}.pos.${k}`, `${a} (mm)`, "num", { def: 0 })).join("")}
          ${["X", "Y", "Z"].map((a, k) => fld(`${P}.rot.${k}`, `Turn about ${a} (°)`, "num", { def: 0 })).join("")}
          ${fld(`${P}.scale`, "Scale", "num", { def: 1, help: "25.4: drawn in inches" })}
          ${fld(`${P}.color`, "Colour", "color", { def: "#8a99ab" })}
          ${fld(`${P}.opacity`, "Opacity", "num", { def: 1, help: "0 to 1" })}
        </div></div>`;
    }).join("");
    return `<div class="cf-model"><div class="cf-body">
        <p class="note">The machine in the 3D view: its parts as STL or OBJ files, each placed as it is with the machine at X0 Y0 Z0 and the <b>tool tip at the origin</b>, moving with the axes it rides on. Changes show in the view at once; Save keeps them.</p>
        ${fld("model.builtin", "The built-in machine, a gantry or a fixed bridge as the Machine tab says (hide it when your parts are the whole machine)", "bool", { def: true, wide: true })}
        ${cards || `<p class="note">No parts yet: upload a file, then Add part.</p>`}
        <div class="row"><button class="btn sm" id="cf-addpart">Add part</button><input type="file" id="cf-mfile" accept=".stl,.obj" hidden><button class="btn sm" id="cf-mup">Upload a model…</button>
          ${cfg.models.map((x) => `<button class="btn sm" data-mdel="${esc(x.name)}" title="Delete ${esc(x.name)}">× ${esc(x.name)}</button>`).join("")}</div>
      </div><div class="cf-view" id="cf-view"></div></div>`;
  },
  custom: () => `<p class="note">More HAL, loaded after the generated machine.hal (see Generated): what the form doesn't cover. Blank: none.</p>
    <textarea class="cf-code" id="cf-custom" spellcheck="false">${esc(cfg.custom)}</textarea>`,
  json: () => `<p class="note">machine.json as text: the forms show the same. ${cfg.jsonBad ? `<b class="bad">${esc(cfg.jsonBad)}</b>` : ""}</p>
    <textarea class="cf-code" id="cf-json" spellcheck="false">${esc(cfg.text)}</textarea>`,
  gen: () => cfg.files ? `<p class="note">What LinuxCNC runs, made from the editor's machine.json (Check makes it again).</p>
      ${cfg.diff ? `<h3>Changes against the saved files</h3><pre class="cf-code cf-diff">${diffHtml(cfg.diff)}</pre>` : ""}
      ${Object.entries(cfg.files).map(([n, t]) => `<h3>run/${esc(n)}</h3><pre class="cf-code">${esc(t)}</pre>`).join("")}`
    : `<p class="note">Check to see the INI and HAL millweb makes of this machine.json.</p>`,
  hist: () => `<p class="note">Every save, newest first. Show compares a version with the editor; Load puts it in the editor (then Save it to go back to it).</p>
    <ul class="cf-hist">${cfg.history.map((h) => `<li><span>${esc(h.time || h.id)} <span class="muted">${esc(h.who || "")}${h.note ? " · " + esc(h.note) : ""}</span></span><button class="btn sm" data-hshow="${esc(h.id)}">Show</button><button class="btn sm" data-hload="${esc(h.id)}">Load</button></li>`).join("") || "<li class='muted'>No saves yet.</li>"}</ul>
    ${cfg.view ? `<h3>${esc(cfg.view.id)} → the editor</h3><pre class="cf-code cf-diff">${diffHtml(cfg.view.diff) || "(the same)"}</pre>` : ""}`,
  log: () => `<div class="row"><div class="seg" id="cf-logw"><button data-w="0" aria-pressed="${cfg.logw === 0}">last start</button><button data-w="1" aria-pressed="${cfg.logw === 1}">the one before</button><button data-w="2" aria-pressed="${cfg.logw === 2}">before that</button></div><button class="btn sm" id="cf-logref">Refresh</button></div>
    <pre class="cf-code" id="cf-log">${esc(cfg.log || "")}</pre>`,
};
cfg.logw = 0;
function fldIn(path, kind, ph = "", cls = "") {
  const v = cget(cfg.m, path);
  return `<input class="${cls === "w" ? "w" : ""}" style="${cls === "s" ? "width:46px" : ""}" data-path="${path}" data-kind="${kind}" value="${v === undefined || v === null ? "" : esc(String(v))}" placeholder="${esc(String(ph))}" ${kind === "num" ? 'inputmode="decimal"' : 'spellcheck="false"'}>`;
}
function selIn(path, choices, re) {
  const v = cget(cfg.m, path) ?? choices[0][0];
  return `<select data-path="${path}" data-kind="text"${re ? ' data-re="1"' : ""} data-keep="1">${choices.map(([c, l]) => `<option value="${esc(c)}"${String(v) === String(c) ? " selected" : ""}>${esc(l)}</option>`).join("")}</select>`;
}
function diffHtml(d) {
  return esc(d || "").split("\n").map((l) => (/^\+(?!\+\+)/.test(l) ? `<span class="a">${l}</span>` : /^-(?!--)/.test(l) ? `<span class="d">${l}</span>` : /^@@/.test(l) ? `<span class="h">${l}</span>` : l)).join("\n");
}

// ------------------------------------------------------------ drawing
function renderCfg() {
  if (!cfg.loaded) { $("cf-body").innerHTML = `<p class="note">Reading the machine…</p>`; return; }
  if (!cfg.m && !["json", "custom", "hist", "log"].includes(cfg.tab)) cfg.tab = "json";
  $$("#cf-tabs button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.t === cfg.tab)));
  $("cf-body").innerHTML = TABS[cfg.tab]();
  markErrors();
  if (cfg.tab === "model") { placeViewer("config"); mm3dApply(cfg.m.model, cfgSrc().modelGet, cfg.m.type || "gantry"); }
  renderCfgBar();
}
function cfgText() { return cfg.m ? JSON.stringify(cfg.m, null, 2) + "\n" : cfg.text; }
// changed: the settings, not the text's layout (the forms write it their way)
function canon(text) { try { return JSON.stringify(sortKeys(JSON.parse(text))); } catch (e) { return text; } }
function sortKeys(v) { return Array.isArray(v) ? v.map(sortKeys) : v && typeof v === "object" ? Object.fromEntries(Object.keys(v).sort().map((k) => [k, sortKeys(v[k])])) : v; }
function dirty() { return canon(cfgText()) !== canon(cfg.saved) || cfg.custom !== cfg.savedCustom; }
function changedPaths(a, b, p = "") {  // the settings that differ, as paths
  if (JSON.stringify(sortKeys(a)) === JSON.stringify(sortKeys(b))) return [];
  if (a && b && typeof a === "object" && typeof b === "object" && Array.isArray(a) === Array.isArray(b)) {
    return [...new Set([...Object.keys(a), ...Object.keys(b)])].flatMap((k) => changedPaths(a[k], b[k], p ? `${p}.${k}` : k));
  }
  return [p || "machine.json"];
}
function renderCfgBar() {
  const s = cfg.status || {}, live = !!LIVE, sup = cfgSupervised();
  const st2 = s.state || (live ? (sup ? "running" : "not supervised") : "simulated");
  $("cf-state").textContent = st2.toUpperCase();
  $("cf-state").className = "pill " + ({ running: "ok", failed: "bad", stopped: "off", starting: "info", loading: "info", restarting: "warn", stopping: "warn" }[st2] || "off");
  $("cf-since").textContent = s.since ? `since ${new Date(s.since * 1000).toLocaleTimeString()}${s.starts ? ` · start ${s.starts}` : ""}` : "";
  $("cf-error").hidden = !s.error; $("cf-error").textContent = s.error || "";
  $("cf-pending").hidden = !(sup && s.state === "running" && cfg.running && cfg.savedHash && cfg.running !== cfg.savedHash);
  const down = sup && ["stopped", "failed"].includes(s.state);
  $("cf-start").hidden = !down; $("cf-stop").hidden = !(sup && s.state === "running" && !document.body.classList.contains("sup-mode"));
  $("cf-restart").hidden = !(sup && s.state === "running" && !document.body.classList.contains("sup-mode")) && !(!live);
  $("cf-saverestart").textContent = down ? "Save & start" : "Save & restart";
  const d = dirty();
  $("cf-dirty").textContent = d ? "Changed: not saved" : "No changes";
  $("cf-dirty").className = d ? "pill warn" : "muted";
}
function markErrors() {
  $$("#cf-body .bad").forEach((e) => e.classList.remove("bad"));
  for (const [where] of cfg.errors) {
    if (!where) continue;
    $$("#cf-body [data-f]").forEach((e) => { const f = e.dataset.f; if (f === where || where.startsWith(f + ".") || f.startsWith(where + ".")) e.classList.add("bad"); });
  }
  const p = $("cf-problems");
  p.hidden = !cfg.errors.length;
  p.innerHTML = cfg.errors.length ? `<h2>Problems <small>${cfg.errors.length}</small></h2><ul>${cfg.errors.map(([w, t]) => `<li data-go="${esc(w)}"><b>${esc(w || "machine.json")}</b>: ${esc(t)}</li>`).join("")}</ul>` : "";
}
const tabOf = (w) => (/^model/.test(w) ? "model" : /^(axes|joints)/.test(w) ? "axes" : /^(board|bitstream|can)/.test(w) ? "hw" : /^(estop|probe)/.test(w) ? "io" : /^console/.test(w) ? "console" : /^custom/.test(w) ? "custom" : w ? "machine" : "json");

// ------------------------------------------------------------ the source: millweb or the supervisor, or the simulation's example
const cfgSupervised = () => (LIVE ? LIVE.supervised() : false);
const cfgSrc = () => (LIVE ? LIVE.conf : cfgSim);
async function cfgLoad(keep) {
  const r = await cfgSrc().get();
  if (!r.ok) { cfg.loaded = true; cfg.m = null; cfg.text = ""; $("cf-body").innerHTML = `<p class="note">${esc(r.err)}</p>`; renderCfgBar(); return; }
  cfg.saved = r.text; cfg.savedCustom = r.custom; cfg.history = r.history || []; cfg.bitstreams = r.bitstreams || []; cfg.models = r.models || []; cfg.dir = r.dir;
  cfg.running = r.running; cfg.savedHash = r.saved;
  try { cfg.def = JSON.parse(r.default); } catch (e) { cfg.def = {}; }
  if (!keep) { cfg.text = r.text; cfg.custom = r.custom; parseText(); cfg.errors = []; cfg.files = null; }
  cfg.loaded = true;
  renderCfg();
}
function parseText() {
  try { cfg.m = JSON.parse(cfg.text); cfg.jsonBad = null; if (typeof cfg.m !== "object" || !cfg.m) throw new Error("not an object"); }
  catch (e) { cfg.m = null; cfg.jsonBad = "machine.json isn't JSON: " + e.message; }
}
function syncFromEditors() {  // the text tabs into the object
  if ($("cf-json")) { cfg.text = $("cf-json").value; parseText(); }
  if ($("cf-custom")) cfg.custom = $("cf-custom").value;
  if (cfg.m) cfg.text = cfgText();
}
async function cfgCheck() {
  syncFromEditors();
  if (!cfg.m) { cfg.errors = [["", cfg.jsonBad]]; renderCfg(); return false; }
  const r = await cfgSrc().check(cfgText(), cfg.custom);
  if (!r.ok && !r.errors) { msg("Check: " + r.err, "warn"); return false; }
  cfg.errors = r.errors || []; cfg.files = r.files && Object.keys(r.files).length ? r.files : cfg.files; cfg.diff = r.diff || "";
  renderCfg();
  if (cfg.errors.length) msg(`machine.json: ${cfg.errors.length} problem${cfg.errors.length > 1 ? "s" : ""}`, "warn");
  return !cfg.errors.length;
}
async function cfgSave(then) {
  if (!(await cfgCheck())) return;
  const changed = dirty();
  let paths = [];
  try { paths = changedPaths(JSON.parse(cfg.saved), cfg.m); } catch (e) { paths = ["machine.json"]; }
  if (cfg.custom !== cfg.savedCustom) paths.push("custom.hal");
  const down = cfgSupervised() && cfg.status && ["stopped", "failed"].includes(cfg.status.state);
  const note = then === "restart" ? (down ? " Then LinuxCNC starts with it." : " Then LinuxCNC restarts with it: the machine goes off, the console reconnects.") : cfgSupervised() ? " It runs from LinuxCNC's next start (Restart)." : "";
  if (!changed && then !== "restart") { msg("Nothing to save", "info"); return; }
  const what = paths.length > 4 ? `${paths.length} settings` : paths.join(", ");
  ask(changed ? `Save machine.json (${what})?` : "Restart LinuxCNC?", (changed ? "Every saved version stays in History." : "") + note, "", async (n) => {
    if (!changed) return cfgRestart(true);
    const r = await cfgSrc().save(cfgText(), cfg.custom, String(n || "").trim(), then === "restart");
    if (!r.ok) { msg("Save: " + r.err, "warn"); return; }
    msg(then === "restart" ? "Saved: LinuxCNC restarts…" : "machine.json saved", "ok");
    cfgLoad(true);
    cfg.saved = cfgText(); cfg.savedCustom = cfg.custom; renderCfgBar();
  }, changed ? "text" : "confirm");
  if (changed) $("dlg-in").placeholder = "a note for the history (optional)";
}
function cfgRestart(sure) {
  const go = async () => { const r = await cfgSrc().restart(); if (!r.ok) msg("Restart: " + r.err, "warn"); else msg("LinuxCNC restarts…", "info"); };
  if (sure) return go();
  ask("Restart LinuxCNC?", "The machine goes off, LinuxCNC starts again with the saved machine.json (the bitstream first, if it changed). Browsers reconnect by themselves.", "", go, "confirm");
}

// ------------------------------------------------------------ events
$("cf-tabs").addEventListener("click", (e) => { const b = e.target.closest("button[data-t]"); if (!b) return; syncFromEditors(); cfg.tab = b.dataset.t; if (cfg.tab === "log") cfgLog(); renderCfg(); });
$("cf-body").addEventListener("change", (e) => {
  const el = e.target;
  if (el.id === "cf-ts") { cfg.m.toolsetter = el.checked ? { x: 0, y: 0, z_above: 0, z_ref: 0, travel: 50 } : null; renderCfg(); return; }
  if (!el.dataset.path || !cfg.m) return;
  cset(cfg.m, el.dataset.path, readField(el));
  cfg.text = cfgText();
  if (el.dataset.re || el.dataset.path.endsWith(".file")) renderCfg(); else renderCfgBar();
  if (el.dataset.path.startsWith("model") || el.dataset.path === "type") mm3dApply(cfg.m.model, cfgSrc().modelGet, cfg.m.type || "gantry");  // the view follows the form
});
$("cf-body").addEventListener("input", (e) => { if (e.target.id === "cf-json" || e.target.id === "cf-custom") { syncFromEditors(); renderCfgBar(); } });
$("cf-body").addEventListener("click", async (e) => {
  const t = e.target;
  if (t.id === "cf-addaxis") { const a = $("cf-newaxis").value; cfg.m.axes[a] = { min: 0, max: 100, max_vel: 50, max_acc: 500 }; renderCfg(); }
  if (t.dataset.rmaxis) { delete cfg.m.axes[t.dataset.rmaxis]; renderCfg(); }
  if (t.id === "cf-addjoint") { const free = Object.keys(cfg.m.axes)[0] || "X"; (cfg.m.joints = cfg.m.joints || []).push({ axis: free, drive: { type: "sim" }, home: { method: "absolute", pos: 0, sequence: 0 } }); renderCfg(); }
  if (t.dataset.rmjoint) { cfg.m.joints.splice(+t.dataset.rmjoint, 1); renderCfg(); }
  if (t.id === "cf-bsup") $("cf-bsfile").click();
  if (t.id === "cf-mup") $("cf-mfile").click();
  if (t.id === "cf-addpart") { const md = cfg.m.model = cfg.m.model || { builtin: true, parts: [] }; (md.parts = md.parts || []).push({ name: "Part " + (md.parts.length + 1), file: (cfg.models[0] || {}).name || "", moves: "", pos: [0, 0, 0], rot: [0, 0, 0], scale: 1, color: "#8a99ab", opacity: 1 }); cfg.text = cfgText(); renderCfg(); }
  if (t.dataset.rmpart) { cfg.m.model.parts.splice(+t.dataset.rmpart, 1); cfg.text = cfgText(); renderCfg(); }
  if (t.dataset.mdel) ask(`Delete ${t.dataset.mdel}?`, "From the machine's models/ folder.", "", async () => { const r = await cfgSrc().modelDelete(t.dataset.mdel); if (!r.ok) msg(r.err, "warn"); else { cfg.models = r.models; renderCfg(); } }, "confirm");
  if (t.dataset.bsdel) ask(`Delete ${t.dataset.bsdel}?`, "From the machine's bitstreams/ folder.", "", async () => { const r = await cfgSrc().bitstreamDelete(t.dataset.bsdel); if (!r.ok) msg(r.err, "warn"); else { cfg.bitstreams = r.bitstreams; renderCfg(); } }, "confirm");
  if (t.dataset.hshow || t.dataset.hload) {
    const id = t.dataset.hshow || t.dataset.hload, r = await cfgSrc().version(id);
    if (!r.ok) { msg(r.err, "warn"); return; }
    syncFromEditors();
    if (t.dataset.hload) { cfg.text = r.text; cfg.custom = r.custom; parseText(); cfg.view = null; msg(`Version ${id} in the editor: Save to go back to it`, "info"); renderCfg(); return; }
    cfg.view = { id, diff: cfgDiff(r.text, cfgText()) + cfgDiff(r.custom || "", cfg.custom, "custom.hal") };
    renderCfg();
  }
  if (t.dataset.w !== undefined && t.closest("#cf-logw")) { cfg.logw = +t.dataset.w; cfgLog(); }
  if (t.id === "cf-logref") cfgLog();
});
$("cf-body").addEventListener("change", (e) => {
  if (e.target.id !== "cf-bsfile") return;
  const f = e.target.files[0]; e.target.value = ""; if (!f) return;
  if (f.size > 6e6) { msg("Up to 6 MB through the page: put larger ones into the machine's bitstreams/ folder", "warn"); return; }
  const rd = new FileReader();
  rd.onload = async () => {
    const r = await cfgSrc().bitstreamSave(f.name, String(rd.result).split(",")[1] || "");
    if (!r.ok) { msg("Upload: " + r.err, "warn"); return; }
    cfg.bitstreams = r.bitstreams; if (cfg.m && cfg.m.bitstream && !cfg.m.bitstream.file) cfg.m.bitstream.file = f.name;
    msg(`${f.name} uploaded`, "ok"); renderCfg();
  };
  rd.readAsDataURL(f);
});
$("cf-body").addEventListener("change", (e) => {
  if (e.target.id !== "cf-mfile") return;
  const f = e.target.files[0]; e.target.value = ""; if (!f) return;
  if (f.size > 6e6) { msg("Up to 6 MB a file: fewer triangles, or the machine in several parts", "warn"); return; }
  const rd = new FileReader();
  rd.onload = async () => {
    const r = await cfgSrc().modelSave(f.name, String(rd.result).split(",")[1] || "", rd.result);
    if (!r.ok) { msg("Upload: " + r.err, "warn"); return; }
    cfg.models = r.models; mm3d.geoms.delete(f.name);
    msg(`${f.name} uploaded: Add part to put it in the machine`, "ok"); renderCfg();
  };
  rd.readAsDataURL(f);
});
$("cf-problems").addEventListener("click", (e) => { const li = e.target.closest("li[data-go]"); if (!li) return; syncFromEditors(); cfg.tab = tabOf(li.dataset.go); renderCfg(); });
$("cf-check").addEventListener("click", async () => { if (await cfgCheck()) msg("machine.json: no problems (Generated has its INI and HAL)", "ok"); });
$("cf-save").addEventListener("click", () => cfgSave());
$("cf-saverestart").addEventListener("click", () => cfgSave("restart"));
$("cf-discard").addEventListener("click", () => { if (!dirty()) return; ask("Discard the changes?", "Back to the saved machine.json.", "", () => { cfg.text = cfg.saved; cfg.custom = cfg.savedCustom; parseText(); cfg.errors = []; renderCfg(); }, "confirm"); });
$("cf-restart").addEventListener("click", () => (LIVE ? cfgRestart() : msg("The simulation has no LinuxCNC to restart", "info")));
$("cf-start").addEventListener("click", async () => { const r = await cfgSrc().start(); if (!r.ok) msg("Start: " + r.err, "warn"); });
$("cf-stop").addEventListener("click", () => ask("Stop LinuxCNC?", "The machine goes off and LinuxCNC ends; this page stays, to configure and start it again.", "", async () => { const r = await cfgSrc().stop(); if (!r.ok) msg("Stop: " + r.err, "warn"); }, "confirm"));
async function cfgLog() {
  const r = await cfgSrc().log(cfg.logw);
  cfg.log = r.ok ? (r.lines.join("\n") || "(empty)") : r.err;
  if (cfg.tab === "log" && $("cf-log")) { $("cf-log").textContent = cfg.log; $("cf-log").scrollTop = 1e9; $$("#cf-logw button").forEach((b) => b.setAttribute("aria-pressed", String(+b.dataset.w === cfg.logw))); }
}
function cfgDiff(a, b, name = "machine.json") {  // a small line diff for the history (the server's is unified)
  const A = a.split("\n"), B = b.split("\n"), out = [];
  const n = A.length, m = B.length, L = Array.from({ length: n + 1 }, () => new Int32Array(m + 1));
  for (let i = n - 1; i >= 0; i--) for (let j = m - 1; j >= 0; j--) L[i][j] = A[i] === B[j] ? L[i + 1][j + 1] + 1 : Math.max(L[i + 1][j], L[i][j + 1]);
  let i = 0, j = 0;
  while (i < n || j < m) {
    if (i < n && j < m && A[i] === B[j]) { i++; j++; continue; }
    if (j < m && (i >= n || L[i][j + 1] >= L[i + 1][j])) out.push("+" + B[j++]); else out.push("-" + A[i++]);
  }
  return out.length ? `@@ ${name}\n` + out.join("\n") + "\n" : "";
}
function cfgEnter() {
  if (!cfg.loaded) cfgLoad(); else renderCfg();
  if (LIVE && cfgSupervised()) cfgSrc().status().then((r) => { if (r.ok) cfgStatus(r.status); });
}
function cfgStatus(s) { cfg.status = s; if (s && s.config) cfg.running = s.config; if (st.page === "config") renderCfgBar(); }

// ------------------------------------------------------------ the simulation's example machine
const cfgSim = (() => {
  const EX = { version: 1, name: "PORTAL-M", units: "mm", servo_period_ns: 1000000,
    board: { type: "hm2_eth", name: "7i96s", params: "board_ip=10.10.10.10", config: "num_encoders=0 sserial_port_0=00000000", watchdog_ms: 50 },
    bitstream: { loader: "none" },
    axes: { X: { min: 0, max: 1000, max_vel: 500, max_acc: 3000 }, Y: { min: 0, max: 1000, max_vel: 500, max_acc: 3000 }, Z: { min: -215, max: 0, max_vel: 200, max_acc: 2000 } },
    joints: [
      { axis: "X", drive: { type: "stmbl_sserial", port: 0, channel: 0 }, ferror: 2, min_ferror: 0.5, home: { method: "index", pos: 500, sequence: 0, latch_vel: 5 } },
      { axis: "Y", drive: { type: "stmbl_sserial", port: 0, channel: 1 }, ferror: 2, min_ferror: 0.5, home: { method: "index", pos: 500, sequence: 1, latch_vel: 5 } },
      { axis: "Y", drive: { type: "stmbl_sserial", port: 0, channel: 2 }, ferror: 2, min_ferror: 0.5, home: { method: "index", pos: 500, sequence: 1, latch_vel: 5 } },
      { axis: "Z", drive: { type: "stmbl_sserial", port: 0, channel: 3 }, ferror: 2, min_ferror: 0.5, home: { method: "index", pos: 0, sequence: 0, latch_vel: 2 } }],
    estop: { input: "hm2_7i96s.0.inm.00.input-00" }, probe: { type: "input", input: "hm2_7i96s.0.inm.00.input-01" }, spindle: { type: "sim", max_rpm: 24000 },
    toolsetter: { x: 980, y: 20, z_above: -40, z_ref: -245, travel: 200 }, console: { port: 8080 } };
  let saved = JSON.stringify(EX, null, 2) + "\n", custom = "", hist = [];
  const files = new Map();  // uploaded models: this page only (a reload forgets them)
  try { saved = localStorage.getItem("pmc.machine") || saved; custom = localStorage.getItem("pmc.custom") || ""; } catch (e) { /* storage blocked */ }
  const ok = (x) => Promise.resolve(Object.assign({ ok: true }, x));
  return {
    get: () => ok({ text: saved, custom, default: saved, history: hist, bitstreams: [], models: [...files].map(([name, b]) => ({ name, size: b.byteLength })), dir: "(this browser)" }),
    modelSave: (name, b64, dataUrl) => { const bin = atob(b64), u = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) u[i] = bin.charCodeAt(i); files.set(name, u.buffer); return ok({ models: [...files].map(([n, b]) => ({ name: n, size: b.byteLength })) }); },
    modelGet: async (name) => files.get(name) || null,
    modelDelete: (name) => { files.delete(name); return ok({ models: [...files].map(([n, b]) => ({ name: n, size: b.byteLength })) }); },
    check: (text, c) => {  // only what the browser can see; millweb checks the rest
      let m; try { m = JSON.parse(text); } catch (e) { return ok({ errors: [["", "machine.json isn't JSON: " + e.message]], files: {} }); }
      const errs = [];
      if (!m.name) errs.push(["name", "a name"]);
      for (const [i, j] of (m.joints || []).entries()) if (!(m.axes || {})[j.axis]) errs.push([`joints.${i}.axis`, "one of the axes"]);
      for (const a of Object.keys(m.axes || {})) if (!(m.joints || []).some((j) => j.axis === a)) errs.push([`axes.${a}`, "no joint moves it"]);
      if (m.type && !["gantry", "bridge"].includes(m.type)) errs.push(["type", "gantry or bridge"]);
      if (m.type === "bridge") for (const a of ["X", "Y"]) { const n = (m.joints || []).filter((j) => j.axis === a).length; if (n > 1) errs.push([`axes.${a}`, `a fixed-bridge machine moves ${a} with one joint (${n} here: that's a gantry)`]); }
      return ok({ errors: errs, diff: cfgDiff(saved, text) + cfgDiff(custom, c, "custom.hal"), files: errs.length ? {} : { "mill.ini": "(the simulation doesn't make the INI: on a machine, millweb does)", "machine.hal": "(nor the HAL)" } });
    },
    save: (text, c, note) => { hist.unshift({ id: String(Date.now()), time: new Date().toLocaleString(), who: "this browser", note, text: saved, custom }); saved = text; custom = c; try { localStorage.setItem("pmc.machine", text); localStorage.setItem("pmc.custom", c); } catch (e) { /* storage blocked */ } return ok({}); },
    version: (id) => { const h = hist.find((x) => x.id === id); return ok(h ? { text: h.text, custom: h.custom } : {}); },
    bitstreamSave: () => ok({ bitstreams: [], err: "the simulation keeps no bitstreams" }).then((r) => Object.assign(r, { ok: false })),
    bitstreamDelete: () => ok({ bitstreams: [] }),
    log: () => ok({ lines: ["(the simulation has no LinuxCNC: on a machine run by millweb-supervisor, its output is here)"] }),
    restart: () => ok({}), start: () => ok({}), stop: () => ok({}), status: () => ok({ status: { state: "simulated" } }),
  };
})();
