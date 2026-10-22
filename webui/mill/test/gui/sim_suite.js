// The console's every page and control, on its built-in simulated machine
// (index.html from disk). Each case starts from the console as it boots: OP1
// held mid-cut, machine on and homed.
//   node test/gui/sim_suite.js [-k WORD]... [--list] [--shots DIR] [-v]
const path = require("path");
const http = require("http");
const fs = require("fs");
const { test, run } = require("./runner");

const MILL = path.resolve(__dirname, "../..");
const URL = "file://" + path.join(MILL, "index.html");
const PAGES = ["main", "file", "wiz", "off", "tool", "probe", "dyn", "stat", "set"];

// ------------------------------------------------------------ helpers
const idle = (t, ms = 15000) => t.until(() => (st.run === "idle" || st.run === "done") && !st.motion, null, ms);
const stop = async (t) => { await t.click("#b-stop"); return idle(t, 3000); };
async function mdi(t, cmd) {
  await t.nav("main");
  await t.click("#st-mdi");
  await t.page.fill("#mdi", cmd);
  await t.page.press("#mdi", "Enter");
  await t.sleep(60);
  return idle(t);
}
const near = (a, b, tol = 1e-3) => Math.abs(a - b) < tol;
const pos = (t) => t.ev(() => ({ ...st.pos }));
const setRange = (t, id, v) => t.ev(([id, v]) => { const e = document.getElementById(id); e.value = v; e.dispatchEvent(new Event("input")); }, [id, v]);
const logHas = (t, s) => t.ev((s) => events.some((e) => e.text.includes(s)), s);
const ffwd = (t, secsBefore) => t.ev((s) => { st.t = Math.max(0, s); }, secsBefore);  // the sim's program clock

// ------------------------------------------------------------ boot, navigation
test("boot: the simulation, held mid-cut", async (t) => {
  t.check(await t.until(() => st.run === "hold" && st.motion === prog), "held in OP1");
  t.check((await t.text("#ch-run")).includes("FEED HOLD"), "chip: FEED HOLD");
  t.check((await t.text("#ch-estop")).includes("E-STOP OK"), "chip: E-STOP OK");
  t.check((await t.text("#ch-on")).includes("MACHINE ON"), "chip: MACHINE ON");
  t.check((await t.text("#ch-home")) === "HOMED", "chip: HOMED");
  t.check((await t.text("#ch-mode")).includes("AUTO"), "chip: AUTO");
  t.check((await t.text("#h-prog")) === "BRACKET-AL6082_OP1.ngc", "the program in the header");
  t.check(await t.hidden("#ch-link") && await t.hidden("#b-control"), "no LIVE chip, no control button");
  t.check((await t.text("#f-src")).includes("simulated"), "footer: simulated");
  t.check(await t.hidden("#vfallback"), "the 3D view runs (WebGL)");
  t.check(await t.ev(() => !!R && document.getElementById("scene").width > 0), "the 3D scene");
  const n = await t.ev(() => [document.querySelectorAll("#gcode div").length, prog.lines.length]);
  t.check(n[0] === n[1] && n[0] > 200, `the G-code listing (${n[0]} lines)`);
  t.check(await t.ev(() => document.querySelectorAll("#gcode .cur").length === 1), "the current line marked");
  t.check(await t.ev(() => document.querySelectorAll("#events li").length >= 6), "the event log");
  t.check((await t.text("#msg")).includes("Simulation"), "message: simulation");
  t.check(await t.until(() => +document.getElementById("sp-rpm").textContent.replace(/,/g, "") > 17000), "spindle at 18 000 rpm");
  t.check((await t.attr("#cl-flood", "aria-pressed")) === "true", "flood on");
});

test("navigation: every page; remembered; #hash", async (t) => {
  for (const p of PAGES) {
    await t.nav(p);
    const vis = await t.ev(() => [...document.querySelectorAll("#page > section, [id^=p-]")].filter((e) => /^p-/.test(e.id) && !e.hidden).map((e) => e.id));
    t.check(vis.length === 1 && vis[0] === "p-" + p, `${p}: only its pane (${vis})`);
    t.check((await t.attr(`#nav button[data-p="${p}"]`, "aria-current")) === "page", `${p}: aria-current`);
  }
  await t.nav("tool");
  await t.page.reload();
  t.check(await t.until(() => st.page === "tool" && !document.getElementById("p-tool").hidden), "reload: back on TOOLS");
  await t.page.goto(URL + "#probe");
  t.check(await t.until(() => st.page === "probe"), "#probe opens PROBE");
});

// ------------------------------------------------------------ cycle
test("cycle: stop, start, feed hold, resume, single block", async (t) => {
  t.check(await stop(t), "Stop: idle");
  t.check((await t.text("#ch-run")) === "IDLE" && (await t.text("#ch-mode")).includes("MANUAL"), "chips: IDLE, MANUAL");
  t.check(await t.msgHas("Stopped"), "message: stopped");
  await t.click("#b-start");
  t.check(await t.until(() => st.run === "run"), "Cycle start: running");
  t.check((await t.text("#ch-run")) === "RUNNING", "chip: RUNNING");
  const t0 = await t.ev(() => st.t);
  await t.sleep(600);
  t.check((await t.ev(() => st.t)) > t0, "the program advances");
  await t.click("#b-hold");
  t.check(await t.until(() => st.run === "hold"), "Feed hold");
  const t1 = await t.ev(() => st.t);
  await t.sleep(400);
  t.check(near(await t.ev(() => st.t), t1), "held: the program stands still");
  await t.click("#b-hold");
  t.check(await t.until(() => st.run === "run"), "Hold again: resumed");
  await t.click("#b-step");
  t.check(await t.until(() => st.run === "step" || st.run === "hold"), "Single block");
  t.check(await t.until(() => st.run === "hold", null, 15000), "...then holds after the block");
  await t.click("#b-start");
  t.check(await t.until(() => st.run === "run"), "Cycle start resumes from single block");
  await stop(t);
});

test("cycle: runs to the end; progress, time, events", async (t) => {
  await stop(t);
  await t.click("#b-start");
  await t.until(() => st.run === "run");
  await ffwd(t, await t.ev(() => prog.total - 0.4));
  t.check(await t.until(() => st.run === "done", null, 5000), "DONE");
  t.check((await t.text("#ch-run")) === "DONE", "chip: DONE");
  t.check(await t.msgHas("finished"), "message: finished");
  t.check(await t.until(() => document.getElementById("pg-bar").style.width === "100%"), "progress bar full");
  t.check((await t.text("#t-rem")) === "00:00", "nothing remaining");
});

test("cycle: run from a selected line", async (t) => {
  await stop(t);
  await t.click("#gl-60");
  t.check(await t.ev(() => document.getElementById("gl-60").classList.contains("sel")), "the line is selected");
  t.check(!(await t.page.isDisabled("#b-runfrom")) && (await t.text("#b-runfrom")) === "Run from N0060", "Run from N0060 enabled");
  await t.click("#b-runfrom");
  t.check(await t.until(() => st.run === "run"), "running");
  t.check(await logHas(t, "Run from N0060"), "event: run from");
  t.check(await t.until(() => st.seg > 0 && prog.segs[st.seg].line >= 60), "it starts at the line");
  t.check(await t.ev(() => prog.segs[st.seg].line < 70), "...not further on");
  await stop(t);
});

test("cycle: M1 optional stop holds; switched off, it runs through", async (t) => {
  const m1 = await t.ev(() => prog.segs.find((s) => s.k === "m1").t0);
  await stop(t);
  await t.click("#b-start");
  await ffwd(t, m1 - 0.3);
  t.check(await t.until(() => st.run === "hold", null, 5000), "M1: hold");
  t.check(await t.msgHas("M1 optional stop"), "message: M1");
  await stop(t);
  await t.nav("set");
  await t.click("#set-m1");
  t.check((await t.text("#set-m1")) === "Off" && !(await t.ev(() => st.m1)), "setting: M1 off");
  await t.click("#b-start");
  await ffwd(t, m1 - 0.3);
  await t.sleep(1500);
  t.check(await t.ev((m1) => st.run === "run" && st.t > m1, m1), "runs past M1");
  await stop(t);
});

test("cycle: start asks for confirmation when set", async (t) => {
  await stop(t);
  await t.nav("set");
  await t.click("#set-confirm");
  t.check((await t.text("#set-confirm")) === "On", "setting: confirm on");
  await t.click("#b-start");
  t.check(await t.msgHas("Press Cycle start again"), "asks to confirm");
  t.check(await t.ev(() => st.run === "idle"), "still idle");
  await t.click("#b-start");
  t.check(await t.until(() => st.run === "run"), "second press: running");
  await stop(t);
});

test("feed hold lifts Z 5 mm with the spindle on (setting)", async (t) => {
  t.check(await t.until(() => Math.abs(+document.getElementById("d-z").textContent.replace("−", "-") - (st.pos.z + 5)) < 0.01, null, 4000), "held: the tool 5 mm up");
  await t.nav("set");
  await t.click("#set-lift");
  t.check((await t.text("#set-lift")) === "Off", "setting: lift off");
  t.check(await t.until(() => Math.abs(+document.getElementById("d-z").textContent.replace("−", "-") - st.pos.z) < 0.01, null, 4000), "the tool back down");
});

// ------------------------------------------------------------ machine state
test("E-STOP and power: buttons, F1/F2, what they block", async (t) => {
  await t.click("#b-estop");
  t.check(await t.until(() => st.estop && !st.on && st.run === "idle"), "E-STOP: off, idle");
  t.check((await t.text("#ch-estop")) === "E-STOP" && (await t.text("#ch-on")) === "MACHINE OFF", "chips");
  t.check((await t.attr("#b-estop", "aria-pressed")) === "true", "E-STOP button pressed");
  t.check(await t.ev(() => st.spindle.dir === 0), "spindle stopped");
  t.check(await t.msgHas("E-STOP"), "message");
  await t.click("#b-power");
  t.check(await t.msgHas("Release E-STOP first"), "power refused in E-STOP");
  await t.page.fill("#mdi", "G0 X0");
  await t.page.press("#mdi", "Enter");
  t.check(await t.msgHas("E-STOP is active"), "MDI refused in E-STOP");
  await t.page.locator("body").click({ position: { x: 5, y: 5 } }).catch(() => {});
  await t.page.keyboard.press("F1");
  t.check(await t.until(() => !st.estop), "F1: released");
  t.check(await t.msgHas("Switch the machine on"), "message: switch on");
  await t.click("#b-start");
  t.check(await t.msgHas("machine is off"), "cycle start refused while off");
  await t.page.keyboard.press("F2");
  t.check(await t.until(() => st.on), "F2: machine on");
  t.check((await t.attr("#b-power", "aria-pressed")) === "true", "power button pressed");
  await t.click("#b-power");
  t.check(await t.until(() => !st.on), "power button: off");
});

test("homing: REF per axis, home all, unhomed blocks moves", async (t) => {
  await stop(t);
  await t.click('#dro button[data-home="x"]');
  t.check(await t.until(() => !st.homed.x), "REF X: X unhomed while homing");
  t.check((await t.text("#ch-home")) === "NOT HOMED", "chip: NOT HOMED");
  await t.page.fill("#mdi", "G0 X1");
  await t.page.press("#mdi", "Enter");
  t.check(await t.msgHas("Home all axes first"), "MDI refused unhomed");
  t.check(await t.until(() => st.homed.x), "X homed");
  t.check(await t.ev(() => document.getElementById("h-x").classList.contains("on")), "X's homed mark");
  await t.ev(() => { st.homed = { x: false, y: false, z: false }; updateChips(); });
  await t.click("#b-homeall");
  t.check(await t.until(() => st.homed.x && st.homed.y && st.homed.z, null, 4000), "Home all");
  t.check((await t.text("#ch-home")) === "HOMED", "chip: HOMED");
});

// ------------------------------------------------------------ DRO
test("DRO: work, machine, distance to go", async (t) => {
  await stop(t);
  t.check(await mdi(t, "G0 X10 Y20 Z5"), "MDI move");
  // after a feed hold the tool comes back down from its 5 mm lift
  t.check(await t.until(() => ["x", "y", "z"].map((a) => document.getElementById("d-" + a).textContent.trim()).join(" ") === "10.000 20.000 5.000"), "work: 10 20 5");
  t.check((await t.text("#ds-x")).startsWith("machine 450.000"), "sub line: machine");
  await t.click('#dro-mode button[data-c="mach"]');
  t.check(await t.until(() => document.getElementById("d-x").textContent.includes("450.000")), "machine: X 450");
  t.check(near(await t.dro("z"), -190), "machine: Z -190 (" + await t.dro("z") + ")");
  t.check((await t.text("#ds-x")).startsWith("G54 10.000"), "sub line: G54");
  await t.click('#dro-mode button[data-c="dtg"]');
  t.check(await t.until(() => document.getElementById("d-y").textContent.trim() === "0.000"), "distance to go: 0");
  await t.click('#dro-mode button[data-c="work"]');
  t.check(await t.until(() => document.getElementById("d-x").textContent.includes("10.000")), "back to work");
  t.check((await t.text("#dro-wcs")) === "G54" && (await t.text("#f-wcs")) === "G54", "G54 shown");
});

test("DRO: ZERO, SET (dialog), zero all, go to zero, park", async (t) => {
  await stop(t);
  await mdi(t, "G0 X10 Y20 Z5");
  await t.click('#dro button[data-zero="x"]');
  t.check(await t.until(() => Math.abs(st.pos.x) < 1e-9 && Math.abs(st.offsets[0].x - 450) < 1e-6), "ZERO X: G54 X 450");
  t.check(await t.msgHas("G10 L20 P1 X0.000"), "message: G10 L20");
  await t.click('#dro button[data-touch="y"]');
  t.check(!(await t.hidden("#dlg")) && (await t.text("#dlg-t")) === "Touch off Y", "SET Y: the dialog");
  await t.page.fill("#dlg-in", "5");
  await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => Math.abs(st.pos.y - 5) < 1e-9) && await t.hidden("#dlg"), "Y = 5");
  await t.click('#dro button[data-touch="z"]');
  await t.page.keyboard.press("Escape");
  t.check(await t.hidden("#dlg") && near((await pos(t)).z, 5), "Esc closes the dialog, Z unchanged");
  await t.click('#dro button[data-touch="z"]');
  await t.click("#dlg-cancel");
  t.check(await t.hidden("#dlg"), "Cancel closes it");
  await t.click("#b-zeroall");
  const p = await pos(t);
  t.check(near(p.x, 0) && near(p.y, 0) && near(p.z, 0), "zero all");
  await mdi(t, "G0 X15 Y-15 Z15");
  await t.click("#b-gotozero");
  t.check(await idle(t) && await t.until(() => Math.abs(st.pos.x) < 1e-6 && Math.abs(st.pos.y) < 1e-6 && Math.abs(st.pos.z - 5) < 1e-6), "go to zero: X0 Y0 Z5");
  await t.click("#b-park");
  await t.sleep(100);
  t.check(await idle(t) && await t.ev(() => { const m = toMach(st.pos); return Math.abs(m.x - 500) < 1e-6 && Math.abs(m.y - 980) < 1e-6 && Math.abs(m.z) < 1e-6; }), "park: machine X500 Y980 Z0");
});

// ------------------------------------------------------------ MDI
test("MDI: moves, offsets, tools, spindle, coolant, history", async (t) => {
  await stop(t);
  await t.page.keyboard.press("Control+m");
  t.check(await t.ev(() => document.activeElement.id === "mdi") && (await t.attr("#st-mdi", "aria-selected")) === "true", "Ctrl+M: the MDI line");
  t.check(await mdi(t, "G0 X10 Y20 Z5"), "G0");
  t.check(near((await pos(t)).x, 10) && (await t.page.inputValue("#mdi")) === "", "moved; the line cleared");
  t.check((await t.text("#mdi-hist li:first-child")) === "G0 X10 Y20 Z5", "history");
  const t0 = Date.now();
  await mdi(t, "G1 X40 F3000");
  t.check(near((await pos(t)).x, 40) && Date.now() - t0 > 400, "G1 at F3000 takes its time");
  await mdi(t, "G55");
  t.check((await t.text("#dro-wcs")) === "G55", "G55");
  await mdi(t, "G54");
  await mdi(t, "T5 M6");
  t.check(await t.ev(() => st.tool === 5) && await t.msgHas("T5 M6"), "T5 M6");
  await mdi(t, "M3 S9000");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.startsWith("S9000 · CW")), "M3 S9000");
  await mdi(t, "M4");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.includes("CCW")), "M4");
  await mdi(t, "M5");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.includes("stopped")), "M5");
  await mdi(t, "M8");
  t.check((await t.attr("#cl-flood", "aria-pressed")) === "true", "M8: flood");
  await mdi(t, "M7");
  t.check((await t.attr("#cl-mist", "aria-pressed")) === "true" && (await t.attr("#cl-flood", "aria-pressed")) === "false", "M7: mist");
  await mdi(t, "M9");
  t.check((await t.attr("#cl-mist", "aria-pressed")) === "false", "M9: off");
  await mdi(t, "G10 L20 P1 X0 Y0");
  const p = await pos(t);
  t.check(near(p.x, 0) && near(p.y, 0), "G10 L20 P1 X0 Y0");
  await mdi(t, "G64 P0.1");
  t.check(await t.msgHas("accepted (simulated)"), "other codes: accepted");
  await mdi(t, "G0 X2000");
  t.check(await t.msgHas("beyond the travel"), "beyond the travel: refused");
  await t.click("#mdi-hist li:nth-child(3)");
  t.check((await t.page.inputValue("#mdi")) === "G0 X2000".replace("G0 X2000", await t.text("#mdi-hist li:nth-child(3)")), "history click fills the line");
  await t.click("#b-start");
  await t.page.fill("#mdi", "G0 X0");
  await t.page.press("#mdi", "Enter");
  t.check(await t.msgHas("MDI waits"), "refused while the program runs");
  await stop(t);
});

// ------------------------------------------------------------ jog
test("jog: increments from the pad, soft limit, rate", async (t) => {
  await stop(t);
  await mdi(t, "G0 X10 Y10 Z5");
  await t.click("#st-jog");
  const jog = (j) => t.page.dispatchEvent(`#jogpad button[data-j="${j}"]`, "pointerdown");
  for (const [inc, j, axis, d] of [["1", "y+", "y", 1], ["0.1", "x-", "x", -0.1], ["10", "z-", "z", -10], ["0.01", "x+", "x", 0.01], ["0.001", "z+", "z", 0.001]]) {
    await t.click(`#jog-inc button[data-v="${inc}"]`);
    t.check((await t.attr(`#jog-inc button[data-v="${inc}"]`, "aria-pressed")) === "true", `increment ${inc} selected`);
    const a = (await pos(t))[axis];
    await jog(j);
    t.check(near((await pos(t))[axis] - a, d, 1e-6), `${j} by ${inc}`);
  }
  await t.ev(() => { st.pos.z = toWork({ x: 0, y: 0, z: 0 }).z; });
  await t.click('#jog-inc button[data-v="1"]');
  await jog("z+");
  t.check(await t.msgHas("Soft limit"), "soft limit: Z can't go up");
  await setRange(t, "jog-rate", 4000);
  t.check((await t.text("#jog-rate-v")) === "4000 mm/min" && await t.ev(() => st.jogRate === 4000), "jog rate");
  await jog("home");
  t.check(await idle(t) && near((await pos(t)).x, 0) && near((await pos(t)).y, 0), "XY0: to the work zero");
});

test("jog: continuous while held", async (t) => {
  await stop(t);
  await mdi(t, "G0 X10 Y10 Z5");
  await t.click("#st-jog");
  await t.click('#jog-inc button[data-v="0"]');
  const x0 = (await pos(t)).x;
  await t.page.dispatchEvent('#jogpad button[data-j="x+"]', "pointerdown");
  await t.sleep(700);
  const x1 = (await pos(t)).x;
  await t.page.dispatchEvent("body", "pointerup");
  await t.sleep(150);
  const x2 = (await pos(t)).x;
  await t.sleep(400);
  t.check(x1 - x0 > 10, `moved while held (${(x1 - x0).toFixed(1)} mm)`);
  t.check(near((await pos(t)).x, x2), "stops on release");
});

test("jog: keyboard arrows and page keys; off in settings and in fields", async (t) => {
  await stop(t);
  await mdi(t, "G0 X10 Y10 Z5");
  await t.click("#st-jog");
  await t.click('#jog-inc button[data-v="1"]');
  await t.page.locator("#dro").click({ position: { x: 2, y: 2 } });
  const p0 = await pos(t);
  for (const k of ["ArrowRight", "ArrowUp", "PageDown"]) await t.page.keyboard.press(k);
  const p1 = await pos(t);
  t.check(near(p1.x - p0.x, 1) && near(p1.y - p0.y, 1) && near(p1.z - p0.z, -1), "→ ↑ PgDn: X+1 Y+1 Z−1");
  for (const k of ["ArrowLeft", "ArrowDown", "PageUp"]) await t.page.keyboard.press(k);
  const p2 = await pos(t);
  t.check(near(p2.x, p0.x) && near(p2.y, p0.y) && near(p2.z, p0.z), "← ↓ PgUp: back");
  await t.click("#st-mdi");
  await t.page.focus("#mdi");
  await t.page.keyboard.press("ArrowRight");
  t.check(near((await pos(t)).x, p0.x), "typing in a field doesn't jog");
  await t.nav("set");
  await t.click("#set-kbd");
  await t.page.locator("#p-set").click({ position: { x: 2, y: 2 } });
  await t.page.keyboard.press("ArrowRight");
  t.check((await t.text("#set-kbd")) === "Off" && near((await pos(t)).x, p0.x), "keyboard jog off");
});

// ------------------------------------------------------------ overrides, spindle, coolant
test("overrides: sliders, readouts, reset", async (t) => {
  for (const [id, key, v] of [["o-feed", "feed", 150], ["o-rapid", "rapid", 25], ["o-spin", "spin", 80], ["o-maxv", "maxv", 60]]) {
    await setRange(t, id, v);
    t.check((await t.text(`#${id}-v`)) === `${v} %` && near(await t.ev((k) => st.ovr[k], key), v / 100), `${key} ${v} %`);
    await t.click(`[data-reset="${id}"]`);
    t.check((await t.text(`#${id}-v`)) === "100 %" && near(await t.ev((k) => st.ovr[k], key), 1), `${key} reset to 100 %`);
  }
  await setRange(t, "o-spin", 50);
  t.check(await t.until(() => +document.getElementById("sp-rpm").textContent.replace(/,/g, "") < 9500, null, 6000), "spindle override slows the spindle");
});

test("spindle and coolant buttons", async (t) => {
  await stop(t);
  await t.click("#sp-stop");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.includes("stopped")) && await t.msgHas("M5"), "stop");
  t.check(await t.until(() => +document.getElementById("sp-rpm").textContent.replace(/,/g, "") < 2000, null, 5000), "it runs down");
  await t.click("#sp-ccw");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.includes("CCW")) && await t.msgHas("M4"), "CCW");
  await t.click("#sp-cw");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.includes("at speed"), null, 10000), "CW, at speed");
  t.check(await t.ev(() => document.getElementById("sp-led").classList.contains("on")), "the at-speed light");
  for (const k of ["flood", "mist", "air"]) {
    const was = (await t.attr("#cl-" + k, "aria-pressed")) === "true";
    await t.click("#cl-" + k);
    t.check((await t.attr("#cl-" + k, "aria-pressed")) === String(!was), `${k} toggles`);
    await t.click("#cl-" + k);
    t.check((await t.attr("#cl-" + k, "aria-pressed")) === String(was), `${k} back`);
  }
});

// ------------------------------------------------------------ FILE, WIZARDS
test("file page: programs, preflight, load, edit", async (t) => {
  await t.nav("file");
  t.check(await t.ev(() => document.querySelectorAll("#files .file").length === 4), "four programs");
  t.check((await t.text("#fl-name")) === "BRACKET-AL6082_OP1.ngc", "the loaded one selected");
  const cards = await t.text("#fl-cards");
  t.check(cards.includes("FITS") && cards.includes("ALL IN THE TABLE") && cards.includes("Tool changes"), "preflight: travel, tools, time");
  t.check((await t.text("#fl-code")).startsWith("%"), "the code");
  await t.click('#files .file[data-i="1"] b');
  t.check((await t.text("#fl-name")) === "HOUSING-7075_OP2.ngc", "select another");
  t.check((await t.attr('#files .file[data-i="1"]', "aria-pressed")) === "true", "marked selected");
  await t.click('#files .file[data-i="1"] .btn');
  t.check(await t.msgHas("Stop the program"), "load refused while held");
  await stop(t);
  await t.click('#files .file[data-i="1"] .btn');
  t.check(await t.until(() => prog.name === "HOUSING-7075_OP2.ngc") && (await t.text("#h-prog")) === "HOUSING-7075_OP2.ngc", "loaded");
  t.check(await t.msgHas("Loaded HOUSING"), "message: loaded");
  t.check((await t.text('#files .file[data-i="1"] .btn')) === "Loaded", "its button: Loaded");
  await t.nav("main");
  t.check(await t.ev(() => document.querySelectorAll("#gcode div").length === prog.lines.length), "the listing follows");
  await t.click("#b-edit");
  t.check(await t.until(() => st.page === "file") && (await t.text("#fl-name")) === "HOUSING-7075_OP2.ngc", "Edit: the file page on it");
});

test("wizards: facing, bolt circle, circular pocket", async (t) => {
  await t.nav("wiz");
  const opts = await t.ev(() => [...document.querySelectorAll("#fz-t option")].map((o) => o.value));
  t.check(opts.length === 5 && !opts.includes("99"), `tool lists without the probe (${opts})`);
  await t.click("#fz-go");
  t.check(await t.msgHas("Stop the program"), "refused while held");
  await stop(t);
  await t.nav("wiz");
  await t.page.fill("#fz-w", "100");
  await t.click("#fz-go");
  t.check(await t.until(() => prog.name === "FACING_100x80.ngc" && st.page === "main"), "facing: loaded, back to MAIN");
  t.check(await t.ev(() => prog.lines.includes("T12 M6") && Math.abs(prog.ext.x[1] - 150) < 1e-6), "facing: T12, 100 mm and a diameter beyond");
  await t.nav("wiz");
  await t.page.fill("#bc-n", "6");
  await t.page.fill("#bc-d", "-8");
  await t.click("#bc-go");
  t.check(await t.until(() => prog.name === "BOLTCIRCLE_6x_R28.ngc"), "bolt circle: loaded");
  t.check(await t.ev(() => prog.segs.filter((s) => s.k === "mv" && Math.abs(s.z + 8) < 1e-9).length === 6), "six holes to Z-8");
  await t.nav("wiz");
  await t.page.fill("#cp-dia", "30");
  await t.page.selectOption("#cp-t", "5");
  await t.click("#cp-go");
  t.check(await t.until(() => prog.name === "CPOCKET_D30.ngc"), "circular pocket: loaded");
  t.check(await t.ev(() => prog.lines.includes("T5 M6") && Math.abs(prog.ext.x[1] - 60 - 12.5) < 0.01), "with T5: its radius inside the wall");
  await t.click("#b-start");
  t.check(await t.until(() => st.run === "run"), "a wizard's program runs");
  await stop(t);
});

// ------------------------------------------------------------ OFFSETS
test("offsets page: table, edit, use, set here, G92, touch plate", async (t) => {
  await stop(t);
  await mdi(t, "G0 X10 Y20 Z5");
  await t.nav("off");
  t.check(await t.ev(() => document.querySelectorAll("#off-t tbody tr").length === 10), "G54…G59.3 and G92");
  t.check((await t.text("#off-t tbody tr.active td")).includes("G54 · active"), "G54 active");
  const m0 = await t.ev(() => toMach(st.pos));
  await t.page.fill("#of-0-x", "400");
  await t.page.press("#of-0-x", "Tab");
  t.check(await t.until(() => st.offsets[0].x === 400), "G54 X = 400");
  t.check(await t.ev((m) => Math.abs(toMach(st.pos).x - m.x) < 1e-9 && Math.abs(st.pos.x - (m.x - 400)) < 1e-9, m0), "the machine stays; the DRO moves");
  t.check(await t.msgHas("G10 L2 P1 X400.000"), "message");
  await t.click('#off-t button[data-act="1"]');
  t.check(await t.until(() => st.wcs === 1) && (await t.text("#dro-wcs")) === "G55", "Use G55");
  t.check((await t.text("#off-t tbody tr.active td")).includes("G55 · active"), "G55 marked");
  await t.click("#off-set");
  t.check(await t.ev((m) => st.pos.x === 0 && st.pos.y === 0 && st.pos.z === 0 && Math.abs(st.offsets[1].x - m.x) < 1e-3, m0), "Set here: G55 = this position");
  await t.ev(() => { const m = toMach(st.pos); st.g92 = { x: 5, y: 5, z: 0 }; st.pos = toWork(m); renderOffsets(); });
  await t.click("#off-g92");
  t.check(await t.ev(() => st.g92.x === 0 && st.g92.y === 0) && await t.msgHas("G92 cleared"), "G92 cleared");
  await t.click('#off-t button[data-act="0"]');
  await t.page.fill("#tp-th", "12");
  await t.click("#tp-go");
  t.check(await t.until(() => document.getElementById("tp-res").textContent.includes("−195.000"), null, 15000), "touch plate: G54 Z set");
  t.check((await t.text("#tp-res")).includes("12 mm"), "...with the plate's thickness");
});

// ------------------------------------------------------------ TOOLS
test("tools page: select, load, add, delete, edit, measure", async (t) => {
  await stop(t);
  await t.nav("tool");
  t.check(await t.ev(() => document.querySelectorAll("#tool-t tbody tr").length === 6), "six tools");
  t.check((await t.text("#tool-t tr.active td")).includes("T3 ● in spindle"), "T3 in the spindle");
  await t.click('#tool-t tr[data-t="5"] td:first-child');
  t.check((await t.attr('#tool-t tr[data-t="5"]', "class")) === "sel", "select T5");
  await t.click("#tl-load");
  t.check(await t.until(() => st.tool === 5) && (await t.text("#tool-t tr.active td")).includes("T5"), "load T5");
  await t.click("#tl-del");
  t.check(await t.msgHas("Unload the tool"), "can't delete the tool in the spindle");
  await t.click("#tl-add");
  t.check(await t.ev(() => st.tools.some((x) => x.t === 13) && st.toolSel === 13), "add: T13, selected");
  t.check(await t.ev(() => !![...document.querySelectorAll("#fz-t option")].find((o) => o.value === "13")), "wizards offer T13");
  await t.click("#tl-del");
  t.check(await t.ev(() => !st.tools.some((x) => x.t === 13)), "delete T13");
  await t.page.fill('#tool-t input[aria-label="T1 diameter"]', "11.5");
  await t.page.press('#tool-t input[aria-label="T1 diameter"]', "Tab");
  t.check(await t.ev(() => st.tools.find((x) => x.t === 1).d === 11.5) && await t.msgHas("T1 diameter updated"), "edit a diameter");
  await t.page.fill('#tool-t input[aria-label="T1 comment"]', "renamed");
  await t.page.press('#tool-t input[aria-label="T1 comment"]', "Tab");
  t.check(await t.ev(() => st.tools.find((x) => x.t === 1).c === "renamed"), "edit a comment");
  await setRange(t, "o-feed", 200);
  await t.click("#tl-meas");
  t.check(await t.msgHas("Measuring T5"), "measure: on the way to the setter");
  t.check(await t.until(() => document.getElementById("msg").textContent.includes("length"), null, 20000), "measured");
  t.check(await t.ev(() => Math.abs(st.tools.find((x) => x.t === 5).l - 71.902) < 0.02), "length near the table's");
});

// ------------------------------------------------------------ PROBE
test("probe page: cycles need the probe; results; apply", async (t) => {
  await stop(t);
  await t.nav("probe");
  t.check(await t.ev(() => document.querySelectorAll("#probe-ops .op").length === 8), "eight cycles");
  await t.click('#probe-ops .op[data-op="corner"]');
  t.check((await t.attr('#probe-ops .op[data-op="corner"]', "aria-pressed")) === "true" && (await t.attr('#probe-ops .op[data-op="z"]', "aria-pressed")) === "false", "select corner");
  await t.click("#pr-go");
  t.check(await t.msgHas("Load the probe first"), "needs T99");
  await t.ev(() => toolChange(99));
  await t.ev(() => { st.offsets[0].x = 430; st.offsets[0].y = 450; });
  await t.click("#pr-go");
  t.check(await t.until(() => document.getElementById("pr-res").textContent.includes("Corner X"), null, 15000), "corner: result");
  t.check((await t.text("#pr-op")) === "Outside corner" && !(await t.page.isDisabled("#pr-apply")), "named; Apply enabled");
  await t.click("#pr-apply");
  t.check(await t.ev(() => Math.abs(st.offsets[0].x - 440) < 0.01 && Math.abs(st.offsets[0].y - 460) < 0.01) && await t.msgHas("set from the probe"), "applied to G54");
  for (const [op, key] of [["z", "Surface Z"], ["bore", "Diameter"], ["edgex", "Edge X"], ["angle", "Edge angle"]]) {
    await t.click(`#probe-ops .op[data-op="${op}"]`);
    await t.click("#pr-go");
    t.check(await t.until((k) => document.getElementById("pr-res").textContent.includes(k), key, 15000), `${op}: ${key}`);
  }
  await t.click('#probe-ops .op[data-op="tool"]');
  await t.click("#pr-go");
  t.check(await t.msgHas("Measuring T99"), "tool setter from the probe page");
  await stop(t);
});

// ------------------------------------------------------------ DYNAMICS, STATUS, SETTINGS
test("dynamics page: pcomp switches, readouts, plots, re-measure", async (t) => {
  await t.nav("dyn");
  t.check((await t.text("#pc-pill")) === "ON" && (await t.text("#pc-delay")) === "24 ms", "pcomp on, 24 ms");
  await t.click("#pc-shaper");
  t.check((await t.attr("#pc-shaper", "aria-pressed")) === "false" && (await t.text("#pc-delay")) === "10 ms", "shaper off: 10 ms");
  for (const k of ["defl", "shrink", "jerk"]) await t.click("#pc-" + k);
  t.check((await t.text("#pc-pill")) === "OFF" && (await t.text("#pc-delay")) === "0 ms", "all off");
  t.check(await t.msgHas("pcomp jerk off"), "message");
  t.check(await t.until(() => /Hz/.test(document.getElementById("pc-modes").textContent) && /V/.test(document.getElementById("pw-bus").textContent)), "modes, bus");
  t.check(await t.until(() => ["X", "Y1", "Y2", "Z", "S"].every((d) => document.getElementById("dc-" + d) && document.getElementById("dc-" + d).textContent !== "")), "the drive table");
  const ink = await t.ev(() => ["pl-err", "pl-beam", "pl-yaw", "pl-spec"].map((id) => {
    const c = document.getElementById(id), g = c.getContext("2d"), d = g.getImageData(0, 0, c.width, c.height).data;
    let n = 0; for (let i = 3; i < d.length; i += 4) if (d[i]) n++;
    return n;
  }));
  t.check(ink.every((n) => n > 200), `four plots drawn (${ink})`);
  await t.click("#b-recal");
  t.check((await t.text("#recal-note")).includes("Stop the program"), "re-measure refused while held");
  await stop(t);
  await t.click("#b-recal");
  t.check((await t.text("#recal-note")).includes("Queued") && await t.msgHas("Re-measure"), "re-measure queued");
});

test("status page: events, filter, clear, pins, stats", async (t) => {
  await t.nav("stat");
  const n = await t.ev(() => document.querySelectorAll("#events li").length);
  t.check(n >= 6, `events (${n})`);
  t.check((await t.text("#nav-stat-badge")) === "1", "badge: one warning");
  await t.click('#ev-filter button[data-f="warn"]');
  t.check(await t.ev(() => [...document.querySelectorAll("#events li")].every((l) => /warn|bad/.test(l.className))), "filter: warnings");
  await t.click('#ev-filter button[data-f="bad"]');
  t.check(await t.ev(() => [...document.querySelectorAll("#events li")].every((l) => l.className === "bad" || l.textContent.includes("No events"))), "filter: faults");
  await t.click('#ev-filter button[data-f="all"]');
  t.check((await t.ev(() => document.querySelectorAll("#events li").length)) === n, "filter: all");
  await t.click("#ev-clear");
  t.check((await t.text("#events")).includes("No events.") && (await t.text("#nav-stat-badge")) === "", "clear");
  t.check(await t.until(() => document.querySelectorAll("#pins tbody tr").length === 11 && document.getElementById("pin-0").textContent !== ""), "eleven pins with values");
  t.check(await t.until(() => document.querySelectorAll("#mstats dt").length === 6), "machine stats");
  await t.click("#b-estop");
  await t.nav("stat");
  t.check(await t.until(() => document.querySelector("#events li").className === "bad"), "a new fault at the top");
});

test("settings: units, decimals, exaggeration, switches", async (t) => {
  await stop(t);
  await mdi(t, "G0 X10 Y20 Z-5");
  await t.nav("set");
  await t.click('#set-units button[data-u="in"]');
  t.check((await t.text("#f-units")) === "in", "footer: in");
  t.check(await t.until(() => document.getElementById("d-x").textContent.trim() === "0.3937"), "DRO in inches, 4 decimals");
  t.check(await t.until(() => document.getElementById("d-z").textContent.trim() === "−0.1969"), "negative with the console's minus");
  await t.click('#set-dec button[data-d="4"]');
  t.check(await t.until(() => document.getElementById("d-x").textContent.trim() === "0.39370"), "4 decimals: 5 in inches");
  await t.click('#dro button[data-touch="x"]');
  t.check((await t.text("#dlg-n")).includes("in in"), "SET asks in inches");
  await t.page.fill("#dlg-in", "1");
  await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => Math.abs(st.pos.x - 25.4) < 1e-9), "SET 1 in = 25.4 mm");
  await t.click('#set-units button[data-u="mm"]');
  t.check(await t.until(() => document.getElementById("d-x").textContent.trim() === "25.4000"), "back in mm, 4 decimals");
  await t.click('#set-exag button[data-x="50"]');
  t.check((await t.text("#defl-btn")) === "deflection ×50" && await t.ev(() => st.exag === 50), "exaggeration ×50");
  for (const id of ["set-kbd", "set-m1", "set-bd", "set-lift", "set-confirm"]) {
    const a = await t.text("#" + id);
    await t.click("#" + id);
    const b = await t.text("#" + id);
    t.check(a !== b && (b === "On" || b === "Off") && (await t.attr("#" + id, "aria-pressed")) === String(b === "On"), `${id}: ${a} → ${b}`);
  }
});

test("3D view: cameras, colours, layers", async (t) => {
  for (const cam of ["top", "front", "side", "part", "follow", "iso"]) {
    await t.click(`[data-cam="${cam}"]`);
    t.check(await t.ev((c) => [...document.querySelectorAll("[data-cam]")].every((b) => (b.getAttribute("aria-pressed") === "true") === (b.dataset.cam === c)), cam), `camera ${cam}`);
  }
  for (const col of ["feed", "op", "err"]) {
    await t.click(`[data-col="${col}"]`);
    t.check(await t.ev((c) => st.color === c, col), `colour by ${col}`);
  }
  for (const k of ["machine", "work", "path", "limits", "defl"]) {
    const was = await t.ev((k) => st.show[k], k);
    await t.click(`[data-show="${k}"]`);
    t.check(await t.ev(([k, w]) => st.show[k] === !w && document.querySelector(`[data-show="${k}"]`).getAttribute("aria-pressed") === String(!w), [k, was]), `layer ${k}`);
  }
  // the loop above left the workpiece off: stock and vise hidden in the scene, not the G54 origin
  t.check(await t.until(() => !st.show.work && R.work.length === 4 && R.work.every((m) => !m.visible) && R.fixture.visible && R.fixture.children.some((c) => c.visible)), "workpiece off: stock and vise hidden, the origin stays");
  await t.click('[data-show="work"]');
  t.check(await t.until(() => R.work.every((m) => m.visible)), "workpiece on again");
  const box = await t.page.locator("#scene").boundingBox();
  await t.page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await t.page.mouse.down();
  await t.page.mouse.move(box.x + box.width / 2 + 80, box.y + box.height / 2 + 30, { steps: 5 });
  await t.page.mouse.up();
  t.check(await t.ev(() => [...document.querySelectorAll("[data-cam]")].every((b) => b.getAttribute("aria-pressed") === "false")), "orbiting by hand leaves the presets");
});

test("keyboard: Ctrl+R, Esc holds then stops, Esc closes a dialog", async (t) => {
  await stop(t);
  await t.page.locator("#dro").click({ position: { x: 2, y: 2 } });
  await t.page.keyboard.press("Control+r");
  t.check(await t.until(() => st.run === "run"), "Ctrl+R: cycle start");
  await t.page.keyboard.press("Escape");
  t.check(await t.until(() => st.run === "hold"), "Esc: feed hold");
  await t.page.keyboard.press("Escape");
  t.check(await t.until(() => st.run === "idle"), "Esc again: stop");
  await t.click('#dro button[data-touch="x"]');
  await t.page.keyboard.press("Escape");
  t.check(await t.hidden("#dlg") && await t.ev(() => st.run === "idle"), "Esc closes the dialog only");
});

test("layout: no sideways scrolling on any page (phone)", async (t) => {
  for (const p of PAGES) {
    await t.ev((p) => showPage(p), p);
    await t.sleep(80);
    const w = await t.ev(() => [document.documentElement.scrollWidth, innerWidth]);
    t.check(w[0] <= w[1] + 1, `${p}: ${w[0]} ≤ ${w[1]}`);
  }
}, { viewport: { width: 390, height: 844 } });

test("layout: no sideways scrolling on any page (tablet)", async (t) => {
  for (const p of PAGES) {
    await t.ev((p) => showPage(p), p);
    await t.sleep(80);
    const w = await t.ev(() => [document.documentElement.scrollWidth, innerWidth]);
    t.check(w[0] <= w[1] + 1, `${p}: ${w[0]} ≤ ${w[1]}`);
  }
}, { viewport: { width: 1024, height: 768 } });

// ------------------------------------------------------------ served over HTTP without millweb
let server = null;
async function serve() {
  if (server) return server.address().port;
  server = http.createServer((req, res) => {
    const p = path.join(MILL, decodeURIComponent(req.url.split("?")[0].split("#")[0]).replace(/\/$/, "/index.html"));
    if (!p.startsWith(MILL) || !fs.existsSync(p) || !fs.statSync(p).isFile()) { res.writeHead(404); res.end(); return; }
    const type = { ".html": "text/html", ".js": "text/javascript", ".css": "text/css" }[path.extname(p)] || "application/octet-stream";
    res.writeHead(200, { "Content-Type": type }); res.end(fs.readFileSync(p));
  });
  await new Promise((r) => server.listen(0, "127.0.0.1", r));
  return server.address().port;
}

test("over HTTP with no millweb: the simulation after 2.5 s", async (t) => {
  const port = await serve();
  await t.page.goto(`http://127.0.0.1:${port}/`);
  await t.sleep(1000);
  t.check(await t.ev(() => st.run !== "hold"), "waits for a machine first");
  t.check(await t.until(() => st.run === "hold" && document.getElementById("msg").textContent.includes("No machine answered"), null, 6000), "then the simulation, saying why");
  t.check(await t.ev(() => LIVE === null), "not live");
}, { open: false, allowErrors: true });

test("over HTTP with ?sim: the simulation at once", async (t) => {
  const port = await serve();
  await t.page.goto(`http://127.0.0.1:${port}/?sim`);
  t.check(await t.until(() => st.run === "hold", null, 1500), "at once");
}, { open: false });

run({
  open: async (t, c) => { if (c.opts.open !== false) { await t.page.goto(URL); await t.page.waitForFunction(() => typeof st !== "undefined" && st.run === "hold" && events.length > 5); } },
}).then((rc) => { if (server) server.close(); process.exit(rc); });
