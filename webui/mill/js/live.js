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
  let files = [], dirs = [], slowSt = {}, jogTimer = null, jogAxis = null, toolsTimer = null;
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
  // onSent(frac): how much of a large message has left the browser (the socket's buffer drains)
  function send(op, args = {}, onSent) {
    return new Promise((resolve) => {
      if (!ws || ws.readyState !== 1) { resolve({ ok: false, err: "not connected" }); return; }
      const id = ++nid;
      pending.set(id, resolve);
      const text = JSON.stringify({ op, id, ...args }), base = ws.bufferedAmount;
      ws.send(text);
      if (onSent) {
        const sock = ws, tick = () => {
          if (!pending.has(id) || sock !== ws) return;
          const left = Math.max(0, sock.bufferedAmount - base);
          onSent(1 - left / text.length);
          if (left > 0) setTimeout(tick, 120);
        };
        tick();
      }
      setTimeout(() => { if (pending.has(id)) { pending.delete(id); resolve({ ok: false, err: "no answer" }); } }, 130000);
    });
  }
  async function act(op, args, label, onSent) {
    const r = await send(op, args, onSent);
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
      case "files": files = m.files; dirs = m.dirs || []; if (st.page === "file") L.renderFiles(); return;
      case "setup": lastSetup = m.objects; lastTools = m.tools; return setupLoad(m.objects, m.tools, m.lathe);
      case "hal": if (st.page === "debug") halShow(m.v); return;
      case "scope": return scopeIn(m);
      case "views": return viewsLoad(m.views);
      case "drv": return drivesShow(m.drives);
      case "vib": return vibShow(m.up, m.status);
      case "vibev": return vibEvent(m.event);
      case "vibspectrum": return vibSpectrumIn(m.node, m.spectrum);
      case "vibconsole": return vibConsoleIn(m.node, m.text);
      case "sup": if (L.kind) supBrand(m.status); return cfgStatus(m.status);
      case "dout": if (m.n === drv.watching) drvText(m.text); return;
      case "control": holder = m.holder; return controlUi();
      case "ev": return event(m);
      case "reply": { const p = pending.get(m.id); if (p) { pending.delete(m.id); p(m); } return; }
      default: return;
    }
  }
  function hello(m) {
    // millweb (LinuxCNC running) or millweb-supervisor (it isn't): a change between them starts the page afresh
    const sup = !m.machine && !!m.supervisor;
    if (L.kind !== undefined && L.kind !== sup) { location.reload(); return; }
    L.kind = sup;
    if (sup) return supHello(m);
    connected = true; me = m.you; holder = m.control; info = m.machine;
    const lim = info.limits;
    TRAVEL.x = [lim.X.min, lim.X.max]; TRAVEL.y = [lim.Y.min, lim.Y.max]; TRAVEL.z = [lim.Z.min, lim.Z.max];
    if (R) { if (R.type !== (info.type || "gantry")) buildBuiltin(info.type || "gantry"); else sizeLimits(); }  // a gantry or a fixed bridge ([MILLWEB] MACHINE_TYPE)
    document.querySelector(".brand b").textContent = info.name.toUpperCase();
    document.querySelector(".brand span").textContent = `${info.coords} · LinuxCNC · millweb ${info.version}`;
    $("f-src").textContent = `live · ${location.host}`;
    events.length = 0;
    for (const e of m.events.slice().reverse()) events.push({ t: e.time, lvl: e.lvl, text: e.text });
    renderEvents();
    buildLiveTables();
    if (info.ovr_max) {  // the INI's override limits: beyond them LinuxCNC would ignore the slider
      $("o-feed").max = Math.round(info.ovr_max.feed * 100);
      $("o-spin").max = Math.round(info.ovr_max.spin * 100); $("o-spin").min = Math.round(info.ovr_max.spin_min * 100);
    }
    linkChip(); controlUi();
    if (info.supervisor) cfgStatus(info.supervisor);
    if (onFirst) { const f = onFirst; onFirst = null; f(); }
    else if (st.page === "debug") halWatchSend();  // reconnected: the watch again
    else if (st.page === "config") cfgLoad(dirty());  // reconnected (LinuxCNC restarted): what is saved and running now
    if (!onFirst && R) mm3dLoad();  // reconnected: the machine's model as saved now
    msg(`Connected to ${info.name}`, "ok");
  }
  // LinuxCNC down: the supervisor serves the page, only the Configure page works
  function supHello(m) {
    connected = true; me = m.you; holder = m.control;
    info = { name: "LinuxCNC", supervised: true, token_required: m.token_required, drive_links: [] };
    document.body.classList.add("sup-mode");
    supBrand(m.supervisor);
    $("f-src").textContent = `supervisor · ${location.host}`;
    events.length = 0;
    for (const e of (m.events || []).slice().reverse()) events.push({ t: e.time, lvl: e.lvl, text: e.text });
    renderEvents();
    linkChip(); controlUi();
    cfgStatus(m.supervisor);
    if (onFirst) { const f = onFirst; onFirst = null; f(); }
    msg(`LinuxCNC ${m.supervisor.state}: ${m.supervisor.error || "Configure it, or start it"}`, m.supervisor.state === "failed" ? "bad" : "info");
  }
  function supBrand(s) {
    document.querySelector(".brand b").textContent = "LINUXCNC " + String(s.state).toUpperCase();
    document.querySelector(".brand span").textContent = `millweb-supervisor ${s.version || ""} · ${s.dir || ""}`;
  }
  function state(m) {
    if ("estop" in m) st.estop = m.estop;
    if ("on" in m) st.on = m.on;
    if (m.homed) st.homed = { x: m.homed[0], y: m.homed[1], z: m.homed[2] };
    if (m.pos) st.pos = { x: m.pos[0], y: m.pos[1], z: m.pos[2] };
    if ("wcs" in m) st.wcs = m.wcs;
    if ("wcs_ok" in m) L.wcsOk = m.wcs_ok;  // false: LinuxCNC hasn't published the work offset yet
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
    // the line motion is on; the interpreter's (read ahead) only until motion has one
    if ("mline" in m) L.mline = m.mline;
    if ("line" in m) L.iline = m.line;
    if ("mline" in m || "line" in m) L.line = L.mline || L.iline || L.line;
    if (m.gcodes !== undefined && typeof LATHE !== "undefined" && LATHE) LATHE.dia = !/\bG8\b/.test(m.gcodes);  // G7: X as a diameter
    if (m.gcodes !== undefined || m.mcodes !== undefined) {
      L.gcodes = m.gcodes ?? L.gcodes; L.mcodes = m.mcodes ?? L.mcodes;
      L.modal = `${L.gcodes || ""} ${L.mcodes || ""}`.trim();
    }
    if ("file" in m && !m.file) { prog = emptyProgram(); renderProgram(); rebuildPath(); }
    updateChips();
  }
  function slow() {
    if (slowSt.load != null) st.spindle.load = slowSt.load;
    st.turret = slowSt.turret || null;
    if (typeof LATHE !== "undefined" && LATHE) latheTurret();
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
  // the path: [kind, x, y, z, feed, line, tool] per segment, each from where the one before ended
  // (async: a long path in slices, the page painting between them; onFrac(0..1))
  async function toProgram(m, onFrac) {
    const segs = [], used = new Set(m.stats ? m.stats.tools : []), d = m.path || [];
    let [x0, y0, z0] = m.start || [0, 0, 0];
    for (let i = 0; i < d.length; i += 7) {
      if (i && i % 280000 === 0) { if (onFrac) onFrac(i / d.length); await nextFrame(); }
      const k = d[i], x = d[i + 1], y = d[i + 2], z = d[i + 3], f = d[i + 4], line = d[i + 5], tool = d[i + 6];
      if (k === 3) { segs.push({ k: "tc", tool, line, x0, y0, z0, x, y, z }); used.add(tool); }
      else {
        segs.push({ k: "mv", x0, y0, z0, x, y, z, f: k === 0 ? 0 : f, rapid: k === 0, line, tool, probe: k === 2 });
        if (tool > 0) used.add(tool);
      }
      x0 = x; y0 = y; z0 = z;
    }
    const p = { name: m.name, lines: m.lines, line0: m.line0 || 1, nlines: m.nlines || m.lines.length, segs, tools: used,
      server: true, error: m.error, truncated: m.truncated, stats: m.stats };
    plan(p);
    if (m.stats) { p.total = m.stats.time; p.cutLen = m.stats.cut; if (m.stats.moves) p.ext = m.stats.ext; }  // every move, not only those drawn
    if (!segs.length) p.ext = { x: [0, 0], y: [0, 0], z: [0, 0] };
    return p;
  }
  // a window of the listing around line n
  let linesBusy = false;
  L.lines = async (n) => {
    if (linesBusy || !prog.server || !prog.nlines) return;
    linesBusy = true;
    const want = prog.name, start = Math.max(1, n - 300);
    const r = await send("program_lines", { name: want, start, count: 2000 });
    linesBusy = false;
    if (r.ok && prog.name === want) { prog.lines = r.lines; prog.line0 = r.start; prog.nlines = r.nlines; renderListing(); }
  };
  let progSeq = 0;
  async function program(m) {
    const seq = ++progSeq, moves = Math.round((m.path || []).length / 7), label = `${m.name}: drawing ${moves.toLocaleString("en-US")} moves`;
    busyShow(label, moves > 40000 ? 0 : null, "program"); await nextFrame();
    const p = await toProgram(m, (f) => busyShow(label, 0.9 * f, "program"));
    if (seq !== progSeq) return;  // another came meanwhile
    prog = p;
    renderProgram(); rebuildPath();
    busyHide("program"); busyHide("load");
    if (m.error) msg(`${m.name}: ${m.error}`, "warn");
    else if (m.truncated) msg(`${m.name}: ${m.truncated}`, "warn");
  }
  function tools(list) {
    st.tools = list.map((t) => ({ t: t.t, p: t.p, d: t.d, l: t.z, x: t.x, q: t.q, i: t.i, j: t.j, fl: t.fl, ins: t.ins, w: t.w, cl: t.cl, wear: 0, life: 100, c: t.c }));
    if (typeof latheVer !== "undefined") latheVer++;  // the lathe's checks with these tools
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
    if (m.lvl === "warn" || m.lvl === "bad") popMsg(m.text, m.lvl);
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
    if (st.page === "debug" && typeof renderHalWatch === "function") { hal.writable = L.halWritable(); renderHalWatch(); }
    if (st.page === "debug" && dbgTab === "drives") drvMode();
  }
  $("b-control").addEventListener("click", () => {
    if (holder === me) { act("control", { take: false }); return; }
    const go = (token) => act("control", { take: true, force: !!holder, token }, "Take control").then((r) => { if (r.ok) msg("This browser has control", "ok"); });
    if (info && info.token_required) askText("Take control", "The machine's control token ([MILLWEB] TOKEN).", go);
    else go();
  });
  function askText(title, note, cb) { ask(title, note, "", cb, "password"); }
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
      // from where it was: a long program moves on a little each frame (a loop's lines repeat: then from the top)
      const S = prog.segs, from = st.seg < S.length && S[st.seg].line <= L.line ? st.seg : 0;
      let i = from; while (i < S.length && S[i].line < L.line) i++;
      if (i >= S.length) i = S.length - 1;
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
    const lathe = (t) => Object.fromEntries(["x", "q", "i", "j", "fl", "ins", "w", "cl"].filter((k) => t[k] != null && t[k] !== "").map((k) => [k, t[k]]));  // a lathe's; the tag's
    toolsTimer = setTimeout(() => act("tools_save", { tools: st.tools.map((t) => ({ t: t.t, p: t.p, d: t.d, z: t.l, c: t.c, ...lathe(t) })) }, "Tool table"), 400);
  };
  L.toolAdd = () => {
    const n = Math.max(0, ...st.tools.filter((t) => t.t < 99).map((t) => t.t)) + 1;
    st.tools.push({ t: n, p: n, d: 8, l: 50, wear: 0, life: 100, c: "new tool" }); st.toolSel = n; renderTools(); L.toolsSave();
  };
  L.toolDelete = (t) => {
    if (t === st.tool) { msg("Unload the tool before deleting it.", "warn"); return; }
    st.tools = st.tools.filter((x) => x.t !== t); renderTools(); L.toolsSave();
  };
  const modelBufs = new Map();  // path -> {mtime, buf}
  L.modelGet = async (path) => {
    const f = files.find((x) => x.name === path), c = modelBufs.get(path);
    if (c && f && c.mtime === f.mtime) return c.buf;
    const r = await act("model_get", { name: path }, "3D model " + path);
    if (!r.ok) return null;
    const bin = atob(r.data), buf = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) buf[i] = bin.charCodeAt(i);
    modelBufs.set(path, { mtime: r.mtime, buf: buf.buffer });
    return buf.buffer;
  };
  let lastSetup = null, lastTools = null;  // the machine's: back to them when a change can't be saved
  L.setupSave = (objects, tools, lathe) => act("setup_save", { objects: objects.map(({ fresh, ...o }) => o), tools, lathe }, "3D setup").then((r) => { if (!r.ok && lastSetup) setupLoad(lastSetup, lastTools); });
  L.modelFiles = () => files.filter((f) => f.kind === "model").map((f) => f.name);
  L.modelUpload = async (name, buf) => { const r = await act("model_save", { name, data: b64(buf) }, "Upload " + name); if (r.ok) modelBufs.delete(name); return r.ok; };
  // the Debug page's HAL tab: the machine's HAL
  L.hal = {
    list: async () => { const r = await send("hal_list"); if (!r.ok) throw new Error(r.err); return { pins: r.pins, sigs: r.sigs, params: r.params }; },
    watch: async (names) => { const r = await send("hal_watch", { names }); if (r.ok && r.values) halShow(r.values); return r; },
    set: (name, value, kind) => send("hal_set", { name, value, kind }),
    values: async (names) => { const r = await send("hal_values", { names }); return r.ok ? r.values : {}; },
  };
  // the scope: millweb's sampler streams "scope" frames (float32, base64)
  L.scope = { start: (names, rate) => send("scope_start", { names, rate }), stop: () => send("scope_stop") };
  function scopeIn(m) {
    if (m.error) { scope.info = m.error; scopeStop(); scopeStateUi(); return; }
    const bin = atob(m.d || ""), b = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) b[i] = bin.charCodeAt(i);
    scopeData(m.n, new Float32Array(b.buffer), m.rate, m.missed);
  }
  L.viewsSave = (views) => send("views_save", { views });
  // the Configure page: machine.json, bitstreams, LinuxCNC's restarts (millweb-supervisor)
  L.supervised = () => !!(info && info.supervised);
  L.info = () => info || {};
  L.conf = {
    get: () => send("config_get"),
    check: (text, custom) => send("config_check", { text, custom }),
    save: (text, custom, note, restart) => send("config_save", { text, custom, note, restart: !!restart, start: !!restart }),
    version: (version) => send("config_version", { version }),
    bitstreamSave: (name, data) => send("bitstream_save", { name, data }),
    bitstreamDelete: (name) => send("bitstream_delete", { name }),
    log: (which) => send("log_get", { which }),
    restart: () => send("lcnc_restart"), start: () => send("lcnc_start"), stop: () => send("lcnc_stop"),
    status: () => send("sup_status"),
    modelSave: (name, data) => send("mmodel_save", { name, data }),
    modelDelete: (name) => send("mmodel_delete", { name }),
    modelGet: async (name) => {  // a machine model file's bytes
      const r = await send("mmodel_get", { name });
      if (!r.ok) { msg(`3D model ${name}: ${r.err}`, "warn"); return null; }
      const bin = atob(r.data), u = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) u[i] = bin.charCodeAt(i);
      return u.buffer;
    },
  };
  // the Debug page's drives: their cards ("drv"), consoles ("dout")
  L.drives = {
    watch: (name) => send("drive_watch", { name }),
    cmd: (name, cmd, sure) => send("drive_cmd", { name, cmd, sure: !!sure }),
    detach: (name, on) => send("drive_detach", { name, on, seconds: 600 }),
    conf: (name, action, x = {}) => send("drive_conf", { name, action, ...x }),
    writable: () => !!(info && info.drive_write) && holder === me,
  };
  // the Measure page: the vibration sensors through vibmon ("vib", "vibev", "vibspectrum", "vibconsole")
  L.hasVib = () => !!(info && info.vibmon);
  L.vib = {
    spectrum: (node) => send("vib_spectrum", { node }),
    trend: (node) => send("vib_trend", { node }),
    console: (node, line) => send("vib_console", { node, line }),
    consoleRead: (node) => send("vib_console_read", { node }),
    reset: (node) => send("vib_reset", { node }),
    identify: (on) => send("vib_identify", { on }),
    assign: (uid, name, position) => send("vib_assign", { uid, name, position }),
    replace: (old, nu) => send("vib_replace", { old, new: nu }),
    rescan: () => send("vib_rescan", {}),
  };
  L.halWritable = () => !!(info && info.hal_write) && holder === me;
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
    return loadOnMachine(p.name);
  };
  // LinuxCNC opens it, the server makes its preview, then the program message draws it
  async function loadOnMachine(name) {
    busyShow(`Loading ${name}: LinuxCNC reads it, the preview is made…`, null, "load");
    const r = await act("program_load", { name }, "Load " + name);
    if (!r.ok) busyHide("load");
    else setTimeout(() => busyHide("load"), 30000);  // the preview never came (an unchanged program isn't sent again)
    return r.ok;
  }
  // the File page: the program folder's folders and programs, one folder at a time
  L.dir = ""; L.sel = null;  // the folder shown ("" the program folder); the selected {kind: "file" | "dir", path}
  const inDir = (path, d) => { const pre = d ? d + "/" : ""; return path.startsWith(pre) && !path.slice(pre.length).includes("/"); };
  const baseName = (p) => p.slice(p.lastIndexOf("/") + 1);
  L.renderFiles = () => {
    $("fl-tools").hidden = false;
    if (info) $("fl-dir").textContent = info.nc_dir;
    if (L.dir && !dirs.includes(L.dir)) L.dir = "";  // gone (renamed or deleted elsewhere)
    const d = L.dir, parts = d ? d.split("/") : [];
    $("fl-crumbs").innerHTML = `<button data-go="" aria-current="${!d}">programs</button>` +
      parts.map((p, i) => `/<button data-go="${esc(parts.slice(0, i + 1).join("/"))}" aria-current="${i === parts.length - 1}">${esc(p)}</button>`).join("");
    const sub = dirs.filter((x) => inDir(x, d));
    const here = files.map((f, i) => [f, i]).filter(([f]) => inDir(f.name, d));
    const sel = (kind, path) => L.sel && L.sel.kind === kind && L.sel.path === path;
    $("files").innerHTML =
      (d ? `<button class="file dir" data-up="1"><span><b>..</b><br><span class="muted">up a folder</span></span><span></span><span></span></button>` : "") +
      sub.map((x) => { const n = files.filter((f) => f.name.startsWith(x + "/")).length;
        return `<button class="file dir" data-dir="${esc(x)}" aria-pressed="${sel("dir", x)}"><span><b>${esc(baseName(x))}/</b><br><span class="muted">${n} program${n === 1 ? "" : "s"}</span></span><span></span><span class="btn sm" data-open="1">Open</span></button>`; }).join("") +
      here.map(([f, i]) => f.kind === "model"
        ? `<button class="file model" data-i="${i}" aria-pressed="${sel("file", f.name)}"><span><b>${esc(baseName(f.name))}</b><br><span class="muted">3D model · ${f.name.split(".").pop().toUpperCase()} · ${(f.size / 1024).toFixed(1)} kB${inSetup(f.name) ? " · in the setup" : ""}</span></span><span class="num muted"></span><span class="adds"><span class="btn sm" data-add="fixture">+ fixture</span><span class="btn sm" data-add="workpiece">+ workpiece</span></span></button>`
        : `<button class="file" data-i="${i}" aria-pressed="${sel("file", f.name)}"><span><b>${esc(baseName(f.name))}</b><br><span class="muted">${(f.size / 1024).toFixed(1)} kB · ${new Date(f.mtime * 1000).toLocaleString()}</span></span><span class="num muted"></span><span class="btn sm ${f.name === prog.name ? "accent" : ""}">${f.name === prog.name ? "Loaded" : "Load"}</span></button>`).join("") ||
      `<p class="note">No programs in ${d ? esc(d) : esc(info ? info.nc_dir : "the program folder")}. Upload some, or drop them here.</p>`;
    if (L.sel && !(L.sel.kind === "dir" ? dirs.includes(L.sel.path) : files.some((f) => f.name === L.sel.path))) L.sel = null;
    if (!L.sel && inDir(prog.name, d) && files.some((f) => f.name === prog.name)) L.sel = { kind: "file", path: prog.name, auto: true };  // the loaded one
    $("fl-rename").disabled = $("fl-delete").disabled = !L.sel;
    if (L.sel && L.sel.kind === "file") {
      const i = files.findIndex((f) => f.name === L.sel.path);
      if (i !== fileSel || L.sel.auto) { delete L.sel.auto; L.selectFile(i); }
      else $$("#files .file").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.i !== undefined && +b.dataset.i === i)));
    }
  };
  const inSetup = (path) => setup.objects.some((o) => o.src === "model:" + path);
  L.selectFile = async (i) => {
    if (!files[i]) return;
    fileSel = i; L.sel = { kind: "file", path: files[i].name };
    $$("#files .file").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.i !== undefined && +b.dataset.i === fileSel)));
    $("fl-rename").disabled = $("fl-delete").disabled = false;
    const name = files[i].name;
    if (files[i].kind === "model") return modelPreflight(name);
    busyShow(`${name}: reading the preview…`, null, "preflight");
    const r = await send("program_get", { name });
    const p = r.ok && await toProgram(r.program, (f) => busyShow(`${name}: preparing the preview`, f, "preflight"));
    busyHide("preflight");
    if (p && files[fileSel] && files[fileSel].name === name) renderPreflight(p);
  };
  const openDir = (d) => { L.dir = d; L.sel = null; L.renderFiles(); };
  // the File page's clicks: the server's programs, not the simulation's library
  $("files").addEventListener("click", (e) => {
    if (!LIVE) return;
    const b = e.target.closest(".file"); if (!b) return;
    e.stopImmediatePropagation();
    if (b.dataset.up) return openDir(L.dir.includes("/") ? L.dir.slice(0, L.dir.lastIndexOf("/")) : "");
    if (b.dataset.dir !== undefined) {
      if (e.target.closest("[data-open]") || (L.sel && L.sel.kind === "dir" && L.sel.path === b.dataset.dir)) return openDir(b.dataset.dir);  // Open, or a second tap (no double-click on a tablet)
      L.sel = { kind: "dir", path: b.dataset.dir };
      $$("#files .file").forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
      $("fl-rename").disabled = $("fl-delete").disabled = false;
      return;
    }
    const i = +b.dataset.i, add = e.target.closest("[data-add]");
    if (add && files[i]) return addModel(files[i].name, add.dataset.add);
    if (e.target.closest(".btn") && files[i] && files[i].kind !== "model" && files[i].name !== prog.name) loadOnMachine(files[i].name);
    L.selectFile(i);
  }, true);
  // a 3D model: its size and triangles where a program's preflight goes
  async function modelPreflight(name) {
    $("fl-name").textContent = name;
    $("fl-cards").innerHTML = `<div class="card"><h3>3D model <span class="pill info">READING</span></h3></div>`; $("fl-code").textContent = "";
    const buf = await L.modelGet(name);
    if (!buf || !L.sel || L.sel.path !== name) return;
    let g; try { g = parseModel(name, buf); } catch (e) { $("fl-cards").innerHTML = `<div class="card"><h3>3D model <span class="pill bad">UNREADABLE</span></h3><p class="note">${esc(e.message)}</p></div>`; return; }
    setup.geoms.set(name, g);
    const m = modelInfo(g), big = Math.max(...m.size);
    $("fl-cards").innerHTML = `<div class="card"><h3>3D model <span class="pill ${inSetup(name) ? "ok" : "info"}">${inSetup(name) ? "IN THE SETUP" : "NOT IN THE SETUP"}</span></h3><dl class="kv">
      <dt>Size X × Y × Z</dt><dd>${m.size.map((v) => v.toFixed(1)).join(" × ")} mm</dd><dt>Triangles</dt><dd>${m.tris.toLocaleString("en-US")}</dd></dl>
      ${big < 5 ? `<p class="note">Under 5 mm across: drawn in inches? Set its scale to 25.4 in the 3D view.</p>` : ""}
      <div class="row"><button class="btn sm" data-addsel="fixture">Add as fixture</button><button class="btn sm" data-addsel="workpiece">Add as workpiece</button></div></div>`;
    $("fl-code").textContent = "Added to the setup it appears in the 3D view: a workpiece with its top at work Z0 and its front-left corner at X0 Y0, a fixture on the table under the work zero. Click it there to move, turn, scale or hide it.";
  }
  $("fl-cards").addEventListener("click", (e) => { const b = e.target.closest("[data-addsel]"); if (b && LIVE && L.sel) addModel(L.sel.path, b.dataset.addsel); });
  function addModel(path, role) {
    if (holder !== me) { msg("Take control to change the 3D setup", "warn"); return; }
    const id = setupAddModel(path, role);
    msg(`${path} added as a ${role}: in the 3D view`, "ok");
    showPage("main"); setTimeout(() => selectObject(id), 600);
  }
  $("files").addEventListener("dblclick", (e) => { const b = e.target.closest(".file.dir"); if (LIVE && b && b.dataset.dir) openDir(b.dataset.dir); });
  $("fl-crumbs").addEventListener("click", (e) => { const b = e.target.closest("button[data-go]"); if (b) openDir(b.dataset.go); });
  const here = (n) => (L.dir ? L.dir + "/" : "") + n;
  $("fl-mkdir").addEventListener("click", () => ask("New folder", `In ${L.dir || "the program folder"}.`, "", (v) => {
    v = v.trim(); if (!v) return;
    act("folder_create", { name: here(v) }, "New folder").then((r) => { if (r.ok) msg(`Folder ${here(v)} made`, "ok"); });
  }, "text"));
  $("fl-rename").addEventListener("click", () => {
    const s = L.sel; if (!s) return;
    ask(s.kind === "dir" ? "Rename or move the folder" : "Rename or move the program",
      "A new name, or a path in the program folder to move it to (folder/name).", s.path, (v) => {
        let to = v.trim().replace(/^\/+|\/+$/g, ""); if (!to || to === s.path) return;
        const ext = s.path.match(/\.(ngc|nc|tap|gcode)$/i);
        if (s.kind === "file" && ext && !/\.(ngc|nc|tap|gcode)$/i.test(to)) to += ext[0];  // keeps its extension
        act("program_rename", { name: s.path, to }, "Rename").then((r) => {
          if (!r.ok) return;
          msg(`${s.path} is now ${to}`, "ok");
          L.sel = { kind: s.kind, path: to };
          const d = to.includes("/") ? to.slice(0, to.lastIndexOf("/")) : "";
          if (d !== L.dir && (s.kind === "file" || d)) L.dir = d;  // follow it
        });
      }, "text");
  });
  $("fl-delete").addEventListener("click", () => {
    const s = L.sel; if (!s) return;
    ask(`Delete ${s.path}${s.kind === "dir" ? "/" : ""}?`, s.kind === "dir" ? "Only an empty folder can be deleted." : "The program is removed from the machine.", "", () => {
      act(s.kind === "dir" ? "folder_delete" : "program_delete", { name: s.path }, "Delete").then((r) => { if (r.ok) { msg(`Deleted ${s.path}`, "info"); L.sel = null; } });
    }, "confirm");
  });
  // upload: the button, or files dropped on the list, into the folder shown
  const confirmP = (title, note) => new Promise((res) => ask(title, note, "", () => res(true), "confirm", () => res(false)));
  async function upload(list) {
    for (const f of list) {
      const model = /\.(stl|obj)$/i.test(f.name);
      if (!model && !/\.(ngc|nc|tap|gcode)$/i.test(f.name)) { msg(`${f.name}: not a G-code program (.ngc .nc .tap .gcode) or a 3D model (.stl .obj)`, "warn"); continue; }
      const lim = model ? 5 : 6;
      if (f.size > lim * 1024 * 1024) { msg(`${f.name}: ${(f.size / 1048576).toFixed(1)} MB; ${model ? "3D models" : "uploads"} stop at ${lim} MB: copy it into the program folder`, "warn"); continue; }
      const name = here(f.name.replace(/[^\w.\- ]/g, "_"));
      if (files.some((x) => x.name === name) && !(await confirmP(`Replace ${name}?`, "A file of that name is there already."))) continue;
      const label = `Uploading ${name} (${(f.size / 1048576).toFixed(1)} MB)`, sent = (fr) => busyShow(fr < 1 ? label : `${name}: the machine saves it…`, fr < 1 ? 0.2 + 0.8 * fr : null, "upload");
      busyShow(`Reading ${f.name}…`, 0, "upload");
      const data = await readFile(f, model, (fr) => busyShow(`Reading ${f.name}`, 0.2 * fr, "upload"));
      const r = data == null ? { ok: false } : model ? await act("model_save", { name, data: b64(data) }, "Upload " + f.name, sent)
        : await act("program_save", { name, text: data }, "Upload " + f.name, sent);
      busyHide("upload");
      if (data == null) { msg(`${f.name}: could not be read`, "warn"); continue; }
      if (r.ok && model) modelBufs.delete(name);
      if (r.ok) {
        msg(`Uploaded ${name}`, "ok"); L.sel = { kind: "file", path: name };
        const i = files.findIndex((x) => x.name === name);  // the file list may have come before this reply
        if (i >= 0 && st.page === "file") { L.renderFiles(); L.selectFile(i); }
      }
    }
  }
  // the file's text (a program) or bytes (a model), with the reading's progress
  function readFile(f, bytes, onFrac) {
    return new Promise((res) => {
      const rd = new FileReader();
      rd.onprogress = (e) => { if (e.lengthComputable) onFrac(e.loaded / e.total); };
      rd.onload = () => res(rd.result); rd.onerror = () => res(null);
      if (bytes) rd.readAsArrayBuffer(f); else rd.readAsText(f);
    });
  }
  function b64(buf) {
    const u = new Uint8Array(buf); let s = "";
    for (let i = 0; i < u.length; i += 32768) s += String.fromCharCode.apply(null, u.subarray(i, i + 32768));
    return btoa(s);
  }
  $("fl-upload").addEventListener("click", () => $("fl-input").click());
  $("fl-input").addEventListener("change", (e) => { upload([...e.target.files]); e.target.value = ""; });
  const panel = $("files").closest(".panel");
  panel.addEventListener("dragover", (e) => { if (!LIVE) return; e.preventDefault(); panel.classList.add("drop"); });
  panel.addEventListener("dragleave", () => panel.classList.remove("drop"));
  panel.addEventListener("drop", (e) => { if (!LIVE) return; e.preventDefault(); panel.classList.remove("drop"); upload([...e.dataTransfer.files]); });

  return { connect, L, emptyProgram };
})();

// ------------------------------------------------------------ boot: live when millweb serves the page
(function boot() {
  const want = /^https?:$/.test(location.protocol) && !/[?&]sim(=|&|$)/.test(location.search);
  if (!want) { bootMsg("Starting the simulation…"); bootSim(); return; }
  let booted = false;
  bootMsg(`Connecting to ${location.host}…`);
  // a cold browser on a small computer can take seconds: wait 6 s before showing the simulation
  const fallback = setTimeout(() => {
    if (booted) return;
    booted = true;
    bootMsg("No machine answered: starting the simulation…");
    bootSim("No machine answered at " + location.host + ": this is the simulation (add ?sim to skip the wait).");
  }, 6000);
  Live.connect(() => {
    if (booted) {  // the machine answered after all: show it (once; not if it keeps coming and going)
      let again = false; try { again = sessionStorage.getItem("pmc.relive") === "1"; sessionStorage.setItem("pmc.relive", "1"); } catch (e) { again = true; }
      if (!again) location.reload();
      return;
    }
    try { sessionStorage.removeItem("pmc.relive"); } catch (e) { /* storage blocked */ }
    booted = true; clearTimeout(fallback);
    bootMsg(`Connected to ${location.host}: loading the machine…`);
    LIVE = Live.L;
    prog = Live.emptyProgram(); library.length = 0;
    st.on = false; st.estop = true; st.run = "idle"; st.motion = null; st.spindle.dir = 0; st.spindle.rpm = 0;
    st.homed = { x: false, y: false, z: false };
    bootCommon();
    if (Live.L.kind) showPage("config");  // LinuxCNC down: the Configure page
  });
})();
