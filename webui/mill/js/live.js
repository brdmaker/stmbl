/* Portal Mill Console: the live machine. When millweb serves the page it
   connects over a WebSocket (/ws), fills the console's state (st, prog,
   offsets, tools, files, events) from what the server sends, and turns the
   console's actions into protocol messages (server/millweb.py). Opened from
   disk or with ?sim, or with no server answering, the simulation runs. */
"use strict";

const Live = (() => {
  let ws = null, nid = 0, me = null, holder = null, info = null, connected = false, onFirst = null;
  const pending = new Map();
  const L = { modal: "" };
  let files = [], slowSt = {}, jogTimer = null, jogAxis = null, toolsTimer = null;
  const ovrTimer = {}, ovrLast = {};
  let lastTool = null, rpmTarget = 0;

  // ------------------------------------------------------------ transport
  function url() { return (location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/ws"; }
  function connect(first) {
    onFirst = onFirst || first;
    try { ws = new WebSocket(url()); } catch (e) { setTimeout(() => connect(), 2000); return; }
    ws.onmessage = (ev) => { let m; try { m = JSON.parse(ev.data); } catch (e) { return; } handle(m); };
    ws.onclose = () => {
      const was = connected; connected = false;
      for (const [, p] of pending) p({ ok: false, err: "connection lost" });
      pending.clear();
      if (was) { msg("Connection to the machine lost: reconnecting…", "bad"); linkChip(); stopJog(false); }
      setTimeout(() => connect(), 2000);
    };
  }
  function send(op, args = {}) {
    return new Promise((resolve) => {
      if (!ws || ws.readyState !== 1) { resolve({ ok: false, err: "not connected" }); return; }
      const id = ++nid;
      pending.set(id, resolve);
      ws.send(JSON.stringify({ op, id, ...args }));
      setTimeout(() => { if (pending.has(id)) { pending.delete(id); resolve({ ok: false, err: "no answer" }); } }, 130000);
    });
  }
  async function act(op, args, label) {
    const r = await send(op, args);
    if (!r.ok) msg(`${label || op}: ${r.err}`, "warn");
    return r;
  }

  // ------------------------------------------------------------ server -> console
  function handle(m) {
    switch (m.t) {
      case "hello": return hello(m);
      case "st": return state(m);
      case "slow": Object.assign(slowSt, m); return slow();
      case "program": return program(m);
      case "tools": return tools(m.tools);
      case "offsets": return offsets(m);
      case "files": files = m.files; if (st.page === "file") L.renderFiles(); return;
      case "control": holder = m.holder; return controlUi();
      case "ev": return event(m);
      case "reply": { const p = pending.get(m.id); if (p) { pending.delete(m.id); p(m); } return; }
      default: return;
    }
  }
  function hello(m) {
    connected = true; me = m.you; holder = m.control; info = m.machine;
    const lim = info.limits;
    TRAVEL.x = [lim.X.min, lim.X.max]; TRAVEL.y = [lim.Y.min, lim.Y.max]; TRAVEL.z = [lim.Z.min, lim.Z.max];
    document.querySelector(".brand b").textContent = info.name.toUpperCase();
    document.querySelector(".brand span").textContent = `${info.coords} · LinuxCNC · millweb ${info.version}`;
    $("f-src").textContent = `live · ${location.host}`;
    events.length = 0;
    for (const e of m.events.slice().reverse()) events.push({ t: e.time, lvl: e.lvl, text: e.text });
    renderEvents();
    buildLiveTables();
    linkChip(); controlUi();
    if (onFirst) { const f = onFirst; onFirst = null; f(); }
    msg(`Connected to ${info.name}`, "ok");
  }
  function state(m) {
    if ("estop" in m) st.estop = m.estop;
    if ("on" in m) st.on = m.on;
    if (m.homed) st.homed = { x: m.homed[0], y: m.homed[1], z: m.homed[2] };
    if (m.pos) st.pos = { x: m.pos[0], y: m.pos[1], z: m.pos[2] };
    if ("wcs" in m) st.wcs = m.wcs;
    if (m.g5x) { const o = st.offsets[st.wcs]; const moved = o.x !== m.g5x[0] || o.y !== m.g5x[1] || o.z !== m.g5x[2]; o.x = m.g5x[0]; o.y = m.g5x[1]; o.z = m.g5x[2]; if (moved) { rebuildPath(); if (st.page === "off") renderOffsets(); } }
    if (m.g92) st.g92 = { x: m.g92[0], y: m.g92[1], z: m.g92[2] };
    if ("rot" in m) st.rot = m.rot;
    if ("tool" in m && m.tool !== lastTool) { lastTool = m.tool; st.tool = m.tool; buildTool(); if (st.page === "tool") renderTools(); }
    if (m.spin) {
      st.spindle.dir = m.spin.dir; st.spindle.s = Math.round(m.spin.s); st.ovr.spin = m.spin.ovr;
      rpmTarget = m.spin.enabled ? m.spin.s * m.spin.ovr : 0;
      slider("o-spin", m.spin.ovr);
    }
    if (m.ovr) { st.ovr.feed = m.ovr.feed; st.ovr.rapid = m.ovr.rapid; st.ovr.maxv = m.ovr.maxv; slider("o-feed", m.ovr.feed); slider("o-rapid", m.ovr.rapid); slider("o-maxv", m.ovr.maxv); }
    if ("flood" in m) setCool("flood", m.flood);
    if ("mist" in m) setCool("mist", m.mist);
    if ("m1" in m) settingBtn("set-m1", "m1", m.m1);
    if ("bd" in m) settingBtn("set-bd", "bd", m.bd);
    if (m.fe) st.fe = [m.fe.X || 0, m.fe.Y1 ?? m.fe.Y ?? 0, m.fe.Y2 ?? m.fe.Y ?? 0, m.fe.Z || 0];
    if ("vel" in m) { st.velNow = m.vel; st.feedNow = m.vel; }
    if ("mode" in m) st.lmode = m.mode;
    if ("run" in m) {
      st.run = m.run === "mdi" ? "run" : m.run;
      st.motion = (m.run === "run" || m.run === "hold") ? prog : null;
    }
    if ("mline" in m || "line" in m) L.line = m.mline || m.line || L.line;
    if (m.gcodes !== undefined || m.mcodes !== undefined) {
      L.gcodes = m.gcodes ?? L.gcodes; L.mcodes = m.mcodes ?? L.mcodes;
      L.modal = `${L.gcodes || ""} ${L.mcodes || ""}`.trim();
    }
    if ("file" in m && !m.file) { prog = emptyProgram(); renderProgram(); rebuildPath(); }
    updateChips();
  }
  function slow() {
    if (slowSt.load != null) st.spindle.load = slowSt.load;
    if (slowSt.bus != null) st.bus = slowSt.bus;
    if (slowSt.yaw != null) st.yaw = Math.abs(slowSt.yaw);
    if (slowSt.contour != null) st.err = slowSt.contour;
    if (slowSt.pcomp) {
      const p = slowSt.pcomp;
      for (const [k, pin] of [["shaper", "shaper"], ["defl", "defl"], ["shrink", "shrink"], ["jerk", "jerk-ms"]]) {
        if (p[pin] == null) continue;
        st.pc[k] = !!p[pin]; $("pc-" + k).setAttribute("aria-pressed", String(st.pc[k]));
      }
    }
  }
  function toProgram(m) {
    const segs = [], used = new Set();
    for (const s of m.segs) {
      const [k, x0, y0, z0, x, y, z, f, line, tool] = s;
      if (k === 3) { segs.push({ k: "tc", tool, line, x0, y0, z0, x, y, z }); used.add(tool); continue; }
      segs.push({ k: "mv", x0, y0, z0, x, y, z, f: k === 0 ? 0 : f, rapid: k === 0, line, tool, probe: k === 2 });
      if (tool > 0) used.add(tool);
    }
    const p = { name: m.name, lines: m.lines, segs, tools: used, server: true, error: m.error };
    plan(p);
    if (!segs.length) p.ext = { x: [0, 0], y: [0, 0], z: [0, 0] };
    return p;
  }
  function program(m) {
    prog = toProgram(m);
    renderProgram(); rebuildPath();
    if (m.error) msg(`${m.name}: ${m.error}`, "warn");
  }
  function tools(list) {
    st.tools = list.map((t) => ({ t: t.t, p: t.p, d: t.d, l: t.z, wear: 0, life: 100, c: t.c }));
    if (st.page === "tool") renderTools();
    fillToolSelects(); buildTool();
  }
  function offsets(m) {
    m.offsets.forEach((o, i) => { if (st.offsets[i]) { st.offsets[i].x = o[0]; st.offsets[i].y = o[1]; st.offsets[i].z = o[2]; } });
    st.g92 = { x: m.g92[0], y: m.g92[1], z: m.g92[2] };
    if (m.wcs != null) st.wcs = m.wcs;
    rebuildPath(); if (st.page === "off") renderOffsets();
  }
  function event(m) {
    events.unshift({ t: m.time, lvl: m.lvl, text: m.text });
    if (events.length > 300) events.pop();
    $("msg").textContent = m.text;
    $("msg").style.color = m.lvl === "warn" ? C.warn : m.lvl === "bad" ? C.stop : C.ink;
    renderEvents();
  }

  // ------------------------------------------------------------ console bits
  function slider(id, v) {
    const el = $(id); if (!el || document.activeElement === el) return;
    const pctv = Math.round(v * 100); if (+el.value !== pctv) { el.value = pctv; $(id + "-v").textContent = pctv + " %"; }
  }
  function settingBtn(id, key, v) { st[key] = !!v; $(id).setAttribute("aria-pressed", String(!!v)); $(id).textContent = v ? "On" : "Off"; }
  function linkChip() {
    const e = $("ch-link"); e.hidden = false;
    e.className = "chip-st " + (connected ? "ok" : "bad");
    e.lastChild.textContent = connected ? "LIVE" : "OFFLINE";
  }
  function controlUi() {
    const b = $("b-control"); b.hidden = false;
    const mine = holder === me;
    b.innerHTML = mine ? "IN<br>CONTROL" : holder ? "WATCHING<br>TAKE OVER" : "TAKE<br>CONTROL";
    b.setAttribute("aria-pressed", String(mine));
    b.title = mine ? "This browser controls the machine. Click to hand it back." : holder ? "Another browser controls the machine." : "Nobody controls the machine.";
  }
  $("b-control").addEventListener("click", () => {
    if (holder === me) { act("control", { take: false }); return; }
    const go = (token) => act("control", { take: true, force: !!holder, token }, "Take control").then((r) => { if (r.ok) msg("This browser has control", "ok"); });
    if (info && info.token_required) askText("Take control", "The machine's control token ([MILLWEB] TOKEN).", go);
    else go();
  });
  function askText(title, note, cb) {
    const inp = $("dlg-in"); inp.type = "password"; inp.step = "";
    ask(title, note, "", () => { const v = inp.value; inp.type = "number"; inp.step = "0.001"; cb(v); });
  }
  function emptyProgram() {
    const p = { name: "—", lines: [], segs: [], tools: new Set(), server: true, total: 0, cutLen: 0, ext: { x: [0, 0], y: [0, 0], z: [0, 0] } };
    return p;
  }
  function buildLiveTables() {
    const names = info.drives.length ? info.drives : [];
    $("drv-t").innerHTML = `<thead><tr><th>Drive</th><th class="n">A</th><th class="n">W</th><th class="n">Fault</th></tr></thead><tbody>` +
      (names.length ? names.map((d, i) => `<tr><td><span class="led" id="ldl-${i}"></span>${esc(d)}</td><td class="n" id="ldc-${i}">—</td><td class="n" id="ldp-${i}">—</td><td class="n" id="ldf-${i}">—</td></tr>`).join("")
        : `<tr><td colspan="4" class="muted">No drives configured ([MILLWEB] DRIVE).</td></tr>`) + "</tbody>";
    $("pins").innerHTML = `<thead><tr><th>Pin</th><th class="n">Value</th></tr></thead><tbody>` +
      (info.pins.length ? info.pins.map((p, i) => `<tr><td class="num" style="font-size:11.5px">${esc(p)}</td><td class="n" id="lpin-${i}">—</td></tr>`).join("")
        : `<tr><td colspan="2" class="muted">No pins configured ([MILLWEB] PIN).</td></tr>`) + "</tbody>";
  }

  // ------------------------------------------------------------ the LIVE interface (app.js calls these)
  L.step = (dt) => {
    st.spindle.rpm += (rpmTarget - st.spindle.rpm) * Math.min(1, dt * 3);
    st.motion = (st.run === "run" || st.run === "hold") && st.lmode === "auto" ? prog : null;
    let seg = null;
    if (prog.segs.length && L.line) {
      let i = prog.segs.findIndex((s) => s.line >= L.line);
      if (i < 0) i = prog.segs.length - 1;
      seg = prog.segs[i]; st.seg = i; st.t = seg.t0;
    }
    const moving = st.velNow > 1;
    return { seg, moving, cutting: !!(moving && st.pos.z < 0.5 && st.spindle.rpm > 100), pTot: null };
  };
  L.renderDyn = () => {
    const d = slowSt.drives || [];
    d.forEach((x, i) => {
      if (!$("ldc-" + i)) return;
      $("ldc-" + i).textContent = x.cur == null ? "—" : (+x.cur).toFixed(2);
      $("ldp-" + i).textContent = x.pwr == null ? "—" : Math.round(x.pwr);
      $("ldf-" + i).textContent = x.fault == null ? "—" : x.fault ? "FAULT" : "ok";
      $("ldl-" + i).classList.toggle("on", !!(st.on && !x.fault));
    });
    $("pw-bus").textContent = slowSt.bus == null ? "—" : Math.round(slowSt.bus) + " V";
    $("pw-p").textContent = "—"; $("pw-ch").textContent = "—";
    $("pc-pill").textContent = info && info.pcomp ? ($("pc-pill").textContent) : "NOT LOADED";
  };
  L.renderPins = () => {
    const p = slowSt.pins || {};
    (info ? info.pins : []).forEach((n, i) => { const e = $("lpin-" + i); if (e) e.textContent = p[n] == null ? "—" : String(p[n]); });
  };

  L.estop = () => act("estop", { on: !st.estop }, "E-STOP");
  L.power = () => act("power", { on: !st.on }, "Machine power");
  L.home = (a) => act("home", { axis: a === "all" ? "all" : a.toUpperCase() }, "Home");
  L.zero = (a, v) => act("zero", { axis: a, value: v || 0 }, "Zero " + a.toUpperCase());
  L.zeroAll = async () => { for (const a of ["x", "y", "z"]) { const r = await L.zero(a, 0); if (!r.ok) return; } };
  L.wcs = (i) => act("wcs", { index: i }, "Work offset");
  L.offset = (i, a, v) => act("offset", { index: i, [a]: v }, "G10 L2");
  L.g92clear = () => act("g92_clear", {}, "G92.1");
  L.moveTo = async (points, label, feed) => {
    for (const q of points) {  // LinuxCNC checks too; this names the move that won't fit
      const m = toMach(q);
      if (!inside(m)) { msg(`${label}: beyond the travel (machine ${m.x.toFixed(1)}, ${m.y.toFixed(1)}, ${m.z.toFixed(1)})`, "warn"); return false; }
    }
    for (const q of points) {
      const r = await act("mdi", { cmd: `G${feed ? 1 : 0} X${q.x.toFixed(4)} Y${q.y.toFixed(4)} Z${q.z.toFixed(4)}${feed ? " F" + feed : ""}`, wait: true }, label);
      if (!r.ok) return false;
    }
    return true;
  };
  L.cycleStart = (line) => act("cycle", st.run === "hold" ? { action: "resume" } : { action: "start", line: line || 0 }, "Cycle start");
  L.cycle = (action) => act("cycle", { action }, { pause: "Feed hold", resume: "Resume", step: "Single block", stop: "Stop" }[action] || action);
  L.spindle = (dir, s) => act("spindle", { dir, speed: s || st.spindle.s || 12000 }, "Spindle");
  L.coolant = (k, v) => act("coolant", { k, on: v }, k);
  L.override = (k, v) => {  // at most 10 per second per override
    ovrLast[k] = v;
    if (ovrTimer[k]) return;
    act("override", { k, v }, "Override");
    ovrTimer[k] = setTimeout(() => { ovrTimer[k] = null; if (ovrLast[k] !== v) L.override(k, ovrLast[k]); }, 100);
  };
  L.opt = (k, v) => act("opt", { [k]: v }, k === "m1" ? "Optional stop" : "Block delete");
  L.pcomp = (k, on) => act("pcomp", { k: k === "jerk" ? "jerk-ms" : k, v: k === "jerk" ? (on ? 20 : 0) : on ? 1 : 0 }, "pcomp");
  L.mdi = (c) => act("mdi", { cmd: c }, "MDI");
  L.toolLoad = (t) => act("tool_load", { t }, `T${t} M6`);
  L.toolsSave = () => {  // after edits settle
    clearTimeout(toolsTimer);
    toolsTimer = setTimeout(() => act("tools_save", { tools: st.tools.map((t) => ({ t: t.t, p: t.p, d: t.d, z: t.l, c: t.c })) }, "Tool table"), 400);
  };
  L.toolAdd = () => {
    const n = Math.max(0, ...st.tools.filter((t) => t.t < 99).map((t) => t.t)) + 1;
    st.tools.push({ t: n, p: n, d: 8, l: 50, wear: 0, life: 100, c: "new tool" }); st.toolSel = n; renderTools(); L.toolsSave();
  };
  L.toolDelete = (t) => {
    if (t === st.tool) { msg("Unload the tool before deleting it.", "warn"); return; }
    st.tools = st.tools.filter((x) => x.t !== t); renderTools(); L.toolsSave();
  };
  L.toolMeasure = () => act("tool_measure", {}, "Tool setter");
  L.probe = async (op) => {
    const name = op === "plate" ? "Touch plate Z" : (probeOps.find((o) => o[0] === op) || [op, op])[1];
    $("pr-op").textContent = name;
    const r = await act("probe", { kind: op, travel: +$("pr-tr").value, feed: +$("pr-f").value, plate: +$("tp-th").value, clearance: +$("pr-cl").value }, name);
    if (!r.ok) return;
    probeRes = { op, mach: r.mach };
    $("pr-res").innerHTML = `<dt>Touched at machine Z</dt><dd>${r.mach[2].toFixed(4)}</dd><dt>X / Y</dt><dd>${r.mach[0].toFixed(3)} / ${r.mach[1].toFixed(3)}</dd>`;
    $("pr-apply").disabled = op === "plate";
    if (op === "plate") $("tp-res").textContent = `${off().n} Z0 set on the stock top (plate ${$("tp-th").value} mm)`;
  };
  L.probeApply = (res) => {
    if (res.op !== "z" || !res.mach) { msg("Only the Z surface result can be applied in this version.", "warn"); return; }
    return L.offset(st.wcs, "z", +(res.mach[2] - st.g92.z).toFixed(4));
  };

  // continuous jog: the server stops it unless it hears from us every 300 ms
  L.jog = (dir) => {
    if (dir === "home") { $("b-gotozero").click(); return; }
    const a = dir[0], s = dir[1] === "+" ? 1 : -1;
    if (st.jogInc === 0) {
      jogHeld = { a, s }; jogAxis = a;
      const go = () => send("jog", { axis: a, dir: s, inc: 0, rate: st.jogRate * st.ovr.maxv });
      act("jog", { axis: a, dir: s, inc: 0, rate: st.jogRate * st.ovr.maxv }, "Jog");
      clearInterval(jogTimer); jogTimer = setInterval(go, 100);
      return;
    }
    act("jog", { axis: a, dir: s, inc: st.jogInc, rate: st.jogRate }, "Jog");
  };
  function stopJog(tell = true) {
    clearInterval(jogTimer); jogTimer = null;
    if (tell && jogAxis) act("jog_stop", { axis: jogAxis }, "Jog");
    jogAxis = null;
  }
  L.jogEnd = () => stopJog(true);

  // programs: the server's files; preflight from the server's preview
  L.loadProgram = async (p) => {
    if (!p.server) {  // made by a wizard: save it on the machine, then load it
      const r = await act("program_save", { name: p.name, text: p.lines.join("\n") }, "Save " + p.name);
      if (!r.ok) return false;
    }
    const r = await act("program_load", { name: p.name }, "Load " + p.name);
    return r.ok;
  };
  L.renderFiles = () => {
    $("files").innerHTML = files.length ? files.map((f, i) => `<button class="file" data-i="${i}" aria-pressed="${i === fileSel}"><span><b>${esc(f.name)}</b><br><span class="muted">${(f.size / 1024).toFixed(1)} kB · ${new Date(f.mtime * 1000).toLocaleString()}</span></span><span class="num muted"></span><span class="btn sm ${f.name === prog.name ? "accent" : ""}">${f.name === prog.name ? "Loaded" : "Load"}</span></button>`).join("")
      : `<p class="note">No programs in ${esc(info ? info.nc_dir : "the program folder")}.</p>`;
    if (files.length) L.selectFile(fileSel);
  };
  L.selectFile = async (i) => {
    if (!files.length) return;
    fileSel = clamp(i, 0, files.length - 1);
    $$("#files .file").forEach((b) => b.setAttribute("aria-pressed", String(+b.dataset.i === fileSel)));
    const name = files[fileSel].name;
    const r = await send("program_get", { name });
    if (r.ok && files[fileSel] && files[fileSel].name === name) renderPreflight(toProgram(r.program));
  };
  // the File page's Load buttons: the server's file list, not the sim's library
  $("files").addEventListener("click", (e) => {
    if (!LIVE) return;
    const b = e.target.closest(".file"); if (!b) return;
    e.stopImmediatePropagation();
    const i = +b.dataset.i;
    if (e.target.closest(".btn") && files[i] && files[i].name !== prog.name) act("program_load", { name: files[i].name }, "Load");
    L.selectFile(i);
  }, true);

  return { connect, L, emptyProgram };
})();

// ------------------------------------------------------------ boot: live when millweb serves the page
(function boot() {
  const want = /^https?:$/.test(location.protocol) && !/[?&]sim(=|&|$)/.test(location.search);
  if (!want) { bootSim(); return; }
  let booted = false;
  const fallback = setTimeout(() => {
    if (booted) return;
    booted = true;
    bootSim("No machine answered at " + location.host + ": this is the simulation (add ?sim to skip the wait).");
  }, 2500);
  Live.connect(() => {
    if (booted) return;
    booted = true; clearTimeout(fallback);
    LIVE = Live.L;
    prog = Live.emptyProgram(); library.length = 0;
    st.on = false; st.estop = true; st.run = "idle"; st.motion = null; st.spindle.dir = 0; st.spindle.rpm = 0;
    st.homed = { x: false, y: false, z: false };
    bootCommon();
  });
})();
