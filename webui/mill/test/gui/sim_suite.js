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
const PAGES = ["main", "file", "wiz", "off", "tool", "probe", "dyn", "meas", "stat", "debug", "config", "set"];

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

test("loading screen: shown while the console loads, gone after", async (t) => {
  let release; const held = new Promise((r) => (release = r));
  await t.page.route("**/js/app.js", async (route) => { await held; route.continue(); });  // a slow start
  const nav = t.page.goto(URL);
  await t.page.waitForSelector("#boot");
  await t.sleep(300);
  t.check(!(await t.hidden("#boot")) && (await t.text("#boot-msg")) === "Loading the console…", "while loading: the loading screen, saying so");
  t.check(await t.ev(() => getComputedStyle(document.querySelector(".boot-bar i")).animationName === "bootbar"), "its bar moving");
  release(); await nav;
  t.check(await t.until(() => !document.getElementById("boot") && typeof st !== "undefined" && st.run === "hold"), "started: gone, the console there");
}, { open: false });

test("loading screen: offline, the web fonts fail and the console still starts", async (t) => {
  await t.page.route(/fonts\.(googleapis|gstatic)\.com/, (route) => route.abort());
  await t.page.goto(URL);
  t.check(await t.until(() => !document.getElementById("boot") && typeof st !== "undefined" && st.run === "hold"), "no fonts: the loading screen gone, the console there");
}, { open: false, allowErrors: true });

test("loading screen: a script that doesn't load is said, not spun", async (t) => {
  await t.page.route("**/js/app.js", (route) => route.abort());
  await t.page.goto(URL);
  t.check(await t.until(() => document.getElementById("boot").classList.contains("failed") && /didn't start.*app\.js/.test(document.getElementById("boot-msg").textContent)), "the loading screen: the console didn't start, which file");
}, { open: false, allowErrors: true });

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
  await t.ev(() => { st.m1 = false; });  // the jump to the end would stop at OP1's M1 (as it should)
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
  // held until it has gone past an increment's 10 mm (the simulation's time runs slow at a software renderer's 5 fps)
  await t.until((x0) => st.pos.x - x0 > 12, x0, 4000);
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
  t.check((await t.text("#pg-name")) === "HOUSING-7075_OP2.ngc" && /^\d+ lines/.test(await t.text("#pg-sub")), "the program view's title: its name");
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
  t.check(await t.until(() => document.getElementById("msg").textContent.includes("length"), null, 40000), "measured");  // simulated time runs slow at a software renderer's 5 fps
  t.check(await t.ev(() => Math.abs(st.tools.find((x) => x.t === 5).l - 71.902) < 0.02), "length near the table's");
});

// ------------------------------------------------------------ PROBE
test("probe page: the 3D view below the cycles; back on MAIN after", async (t) => {
  await t.nav("probe");
  t.check(await t.until(() => document.getElementById("viewer").parentNode.id === "probe-view"), "the view is on the probe page");
  t.check(await t.until(() => { const c = document.getElementById("scene"); return c.width > 300 && c.height > 250; }), "at a usable size");
  t.check(await t.until(() => document.querySelector('[data-cam="part"]').getAttribute("aria-pressed") === "true"), "close on the stock (PART)");
  await t.nav("tool");
  await t.nav("main");
  t.check(await t.until(() => document.getElementById("viewer").parentNode.id === "p-main" && document.getElementById("scene").width > 300), "back on MAIN, full size");
});

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

test("Debug page, HAL tab (simulation): its pins, filter, watch, trend, remembered", async (t) => {
  await t.ev(() => localStorage.removeItem("pmc.halwatch"));
  await t.nav("debug");
  t.check(await t.until(() => /11 pins/.test(document.getElementById("hal-count").textContent)), "the simulation's 11 pins");
  await t.page.fill("#hal-q", "spindle");
  t.check(await t.until(() => document.querySelectorAll("#hal-list .hal-row").length === 2), "filter: spindle.0.speed-in, spindle.0.at-speed");
  await t.click('.hal-row[data-name="spindle.0.speed-in"]');
  await t.click('.hal-row[data-name="spindle.0.at-speed"]');
  t.check(await t.until(() => hal.watch.length === 2 && document.querySelectorAll("#hal-watch tbody tr").length === 2), "both watched");
  t.check(await t.until(() => +document.getElementById("hw-v-0").textContent > 250 && hal.hist.get("spindle.0.speed-in").length > 5), "the value moves in, with its trend (rev/s)");
  t.check(await t.until(() => document.getElementById("hw-led-1") && document.getElementById("hw-led-1").classList.contains("on")), "a bit as a light: at speed");
  t.check(await t.until(() => +document.querySelector('.hal-row[data-name="spindle.0.speed-in"] .v').textContent > 250, null, 3000), "the list's rows refresh too");
  t.check((await t.text("#hal-detail")).includes("spindle.0.at-speed"), "details of the last one clicked");
  t.check(!(await t.ev(() => !!document.querySelector("#hal-watch [data-set]"))), "the simulation's pins can't be set");
  await t.click('#hal-watch [data-rm="0"]');
  t.check(await t.ev(() => hal.watch.length === 1 && hal.watch[0].name === "spindle.0.at-speed"), "×: no longer watched");
  await t.page.reload();
  await t.page.waitForFunction(() => typeof st !== "undefined" && st.page === "debug" && hal.watch.length === 1);
  t.check(await t.until(() => document.querySelectorAll("#hal-watch tbody tr").length === 1), "the watch list remembered across a reload");
  await t.click("#hal-clear");
  t.check(await t.ev(() => hal.watch.length === 0), "Clear");
});

test("Debug page, scope (simulation): channels, auto, roll, a trigger, cursor, single", async (t) => {
  await stop(t);
  await mdi(t, "M5");
  await t.until(() => st.spindle.rpm < 600, null, 15000);  // from rest: a rising edge to come
  await t.ev(() => { localStorage.removeItem("pmc.halwatch"); localStorage.removeItem("pmc.views"); });
  await t.nav("debug");
  await t.page.fill("#hal-q", "spindle.0.speed");
  await t.click('.hal-row[data-name="spindle.0.speed-in"]');
  t.check(await t.until(() => document.querySelector('#hal-watch [data-scope="0"]')), "a watched number has a scope button");
  await t.click('#hal-watch [data-scope="0"]');
  t.check(await t.ev(() => scope.ch.length === 1 && scope.ch[0].name === "spindle.0.speed-in"), "∿: on the scope");
  await t.click('#dbg-tab button[data-t="scope"]');
  t.check(await t.ev(() => !document.getElementById("dbg-scope").hidden && document.getElementById("dbg-hal").hidden), "the Scope tab");
  await t.page.fill("#sc-add", "motion.current-vel");
  await t.click("#sc-add-btn");
  t.check(await t.ev(() => scope.ch.length === 2 && document.querySelectorAll("#sc-ch [data-i]").length === 2 && scope.ch[0].color !== scope.ch[1].color), "a second channel by name, its own colour");
  await t.click("#sc-run");
  t.check(await t.until(() => scope.state === "RUNNING" && scope.disp && scope.disp.data[0].length === 1000, null, 4000), "auto: no edge, a screen anyway (100 ms/div at 1 kHz: 1000 samples)");
  t.check(await t.ev(() => /samples\/s/.test(document.getElementById("sc-info").textContent) && document.querySelector('#dbg-tab [data-t="scope"]').textContent.includes("●")), "rate shown, the tab says it runs");
  await t.click('#sc-mode button[data-m="roll"]');
  t.check(await t.until(() => scope.state === "ROLL" && scope.disp && scope.disp.trig === null), "roll");
  // a rising edge through 50 rev/s on the spindle: start it from another page while the scope waits
  await t.click('#sc-mode button[data-m="normal"]');
  await t.page.selectOption("#sc-src", "0"); await t.page.fill("#sc-level", "50"); await t.page.press("#sc-level", "Tab");
  t.check(await t.until(() => scope.state === "WAITING FOR TRIGGER"), "normal: waits for the edge");
  await mdi(t, "M3 S9000");
  await t.nav("debug");
  t.check(await t.until(() => scope.disp && scope.disp.trig === 200, null, 8000), "triggered while on another page: the edge at 20 % " + await t.ev(() => JSON.stringify({ st: scope.state, run: scope.running, mode: scope.mode, trig: scope.trig, total: scope.total, armed: scope.armedAt, from: scope.searchFrom, disp: scope.disp && scope.disp.trig, rpm: st.spindle.rpm, last: scope.ring[0][(scope.total - 1) % scope.size] })));
  t.check(await t.ev(() => { const a = scope.disp.data[0]; return a[150] < 50 && a[400] >= 50; }), "below the level before, above after");
  t.check(await t.ev(() => scope.ch[0].scale > 0 && scope.ch[0].auto === false), "scaled on the first capture");
  const box = await t.page.locator("#sc-cv").boundingBox();
  await t.page.mouse.move(box.x + box.width * 0.5, box.y + box.height / 2);
  t.check(/^t \+\d/.test(await t.text("#sc-read")) && (await t.text("#sc-read")).includes("spindle.0.speed-in"), "the cursor: time after the trigger, the values");
  await t.click('#sc-mode button[data-m="single"]');
  await mdi(t, "M5");
  t.check(await t.until(() => st.spindle.rpm < 600, null, 15000), "the spindle down below the level (no rising edge otherwise)");
  await t.nav("debug");
  if (!(await t.ev(() => scope.running))) await t.click("#sc-run");
  await mdi(t, "M3 S9000"); await t.nav("debug");
  t.check(await t.until(() => !scope.running && scope.state === "STOPPED" && scope.disp.trig === 200, null, 8000), "single: one capture, then stopped");
  await mdi(t, "M5");
  t.check(t.errors.length === 0, "no script errors");
});

test("Debug page, views: save, open with a click and Alt+1, delete, remembered", async (t) => {
  await t.nav("debug");
  await t.click('#dbg-tab button[data-t="scope"]');
  await t.ev(() => { localStorage.removeItem("pmc.views"); viewsLoad([]); hal.watch = [{ kind: "pin", name: "spindle.0.speed-in" }]; renderHalWatch(); scope.ch = []; scopeAdd("spindle.0.speed-in"); scopeAdd("motion.current-vel"); });
  t.check(await t.ev(() => scope.ch.length === 2 && hal.watch.length === 1), "a set-up");
  await t.page.selectOption("#sc-tdiv", "0.01");
  await t.click("#dbg-save");
  await t.page.fill("#dlg-in", "spindle");
  await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => views.length === 1 && document.querySelector('#dbg-views .view[aria-current="true"]')), "saved, and the current view");
  t.check(await t.ev(() => JSON.parse(localStorage.getItem("pmc.views"))[0].scope.ch.length === 2), "kept in this browser");
  await t.ev(() => { hal.watch = []; renderHalWatch(); scope.ch = []; scope.tdiv = 0.1; renderScopeSide(); });
  await t.click("#dbg-save");
  await t.page.fill("#dlg-in", "empty");
  await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => views.length === 2 && document.querySelectorAll("#dbg-views .view").length === 2), "a second view");
  await t.click('#dbg-views [data-view="0"]');
  t.check(await t.until(() => scope.ch.length === 2 && hal.watch.length === 1 && scope.tdiv === 0.01 && document.getElementById("sc-tdiv").value === "0.01"), "click: the watch list and the scope back");
  await t.page.keyboard.press("Alt+2");
  t.check(await t.until(() => scope.ch.length === 0 && hal.watch.length === 0), "Alt+2: the empty one");
  await t.page.keyboard.press("Alt+1");
  t.check(await t.until(() => scope.ch.length === 2 && hal.watch.length === 1), "Alt+1");
  await t.page.reload();
  await t.page.waitForFunction(() => typeof st !== "undefined" && st.page === "debug");
  t.check(await t.until(() => document.querySelectorAll("#dbg-views .view").length === 2), "remembered across a reload");
  await t.click('#dbg-views [data-vdel="1"]');
  await t.click('#dlg-form [type=submit]');
  t.check(await t.until(() => views.length === 1 && views[0].name === "spindle"), "×: deleted, after asking");
  await t.ev(() => { localStorage.removeItem("pmc.views"); localStorage.removeItem("pmc.halwatch"); });
});

test("Debug page, drives (simulation): cards, the console, history, a command that asks first", async (t) => {
  await t.nav("debug");
  await t.click('#dbg-tab button[data-t="drives"]');
  t.check(await t.until(() => document.querySelectorAll("#drv-cards .drv-card").length === 4), "four drives: X, Y1, Y2, Z");
  t.check(await t.until(() => /ENABLED|DISABLED/.test(document.querySelector('.drv-card[data-n="X"] [data-k="state"]').textContent) && /V$/.test(document.querySelector('.drv-card[data-n="X"] [data-k="dc"]').textContent)), "a card: state, bus voltage");
  t.check((await t.text("#dc-name")) === "X" && (await t.text("#dc-mode")) === "READ + WRITE", "the console on X");
  await t.page.fill("#dc-in", "about"); await t.page.press("#dc-in", "Enter");
  t.check(await t.until(() => /software info/.test(document.getElementById("dc-out").textContent)), "about answers");
  await t.page.fill("#dc-in", "conf0.max_vel = 300"); await t.page.press("#dc-in", "Enter");
  await t.page.fill("#dc-in", "conf0.max_vel"); await t.page.press("#dc-in", "Enter");
  t.check(await t.until(() => /conf0\.max_vel = 300\.000000/.test(document.getElementById("dc-out").textContent)), "a pin set, read back");
  await t.page.press("#dc-in", "ArrowUp");
  t.check((await t.ev(() => document.getElementById("dc-in").value)) === "conf0.max_vel", "↑: the last line");
  await t.page.press("#dc-in", "ArrowUp");
  t.check((await t.ev(() => document.getElementById("dc-in").value)) === "conf0.max_vel = 300", "↑ again: the one before");
  await t.page.fill("#dc-in", "jogl"); await t.page.press("#dc-in", "Enter");
  t.check(await t.until(() => !document.getElementById("dlg").hidden && /jogl/.test(document.getElementById("dlg-t").textContent)), "a jog asks first");
  await t.click('#dlg-form [type="submit"]');
  t.check(await t.until(() => /doesn't run that/.test(document.getElementById("dc-out").textContent)), "then goes");
  t.check((await t.ev(() => document.getElementById("dc-in").maxLength)) === 63, "63 characters at most, as the drive");
  await t.click('.drv-card[data-n="Z"] [data-con]');
  t.check(await t.until(() => document.getElementById("dc-name").textContent === "Z" && document.querySelector('.drv-card[data-n="Z"]').classList.contains("sel") && !/jogl/.test(document.getElementById("dc-out").textContent)), "Console on Z's card: Z's console");
  await t.click('#dc-quick [data-c="showconf"]');
  t.check(await t.until(() => /linrev0\.scale=10/.test(document.getElementById("dc-out").textContent)), "a quick button: showconf");
  await t.click('#dbg-tab button[data-t="hal"]');
  t.check(t.errors.length === 0, "no script errors");
});

test("Measure (simulation): four sensors, levels, the spectrum with its markers, tap to name, a crash, the console", async (t) => {
  await t.nav("meas");
  t.check(await t.until(() => document.querySelectorAll("#ms-cards .ms-card").length === 4), "four sensor cards");
  t.check((await t.text("#ms-link")) === "SIMULATED", "SIMULATED");
  t.check(await t.until(() => /mm\/s/.test(document.querySelector('.ms-card[data-node="4"] [data-k="vel"]').textContent)), "a card: velocity in mm/s");
  t.check(await t.ev(() => document.querySelector('.ms-card[data-node="1"] [data-k="vel"]').classList.contains("ok")), "the base sensor's level: ok (green)");
  t.check(await t.until(() => (ms.trend[4] || []).length > 3), "a trend builds up");
  await t.click('.ms-card[data-node="4"] [data-spec]');
  t.check(await t.until(() => ms.spec[4] && /peak/.test(document.getElementById("ms-spec-info").textContent)), "Spectrum: the spindle's arrives");
  const pk = +(await t.ev(() => document.getElementById("ms-spec-cv").dataset.peak));
  t.check(Math.abs(pk - 150) < 15, `its peak at the 1st order, 150 Hz at 9000 rpm (${pk})`);
  await t.click('#ms-spec-range button[data-r="1000"]');
  t.check(await t.ev(() => ms.range === 1000), "range 1 kHz");
  await t.click("#ms-ident");
  t.check(await t.until(() => ms.tapped && document.querySelector("#ms-sensors tr.tapped")), "Identify, a tap: its row lights up");
  const row = '#ms-sensors tr.tapped';
  await t.page.fill(`${row} [data-k="name"]`, "main spindle");
  await t.click(`${row} [data-assign]`);
  t.check(await t.until(() => /main spindle/.test(document.querySelector('.ms-card[data-node="4"] .h b').textContent)), "Save: the card has the name");
  await t.click("#ms-ident");
  await t.ev(() => vibSim.crash(4));
  t.check(await t.until(() => /CRASH/.test(document.getElementById("ms-alarms").textContent) && document.querySelector('#nav button[data-p="meas"]').classList.contains("alarm")), "a crash: the alarm, the nav button red");
  await t.click("#ms-reset");
  t.check(await t.until(() => !/CRASH/.test(document.getElementById("ms-alarms").textContent)), "Reset crash");
  await t.page.selectOption("#ms-con-node", "2");
  await t.page.fill("#ms-con-in", "vib0.fft_n = 4096"); await t.page.press("#ms-con-in", "Enter");
  t.check(await t.until(() => /> vib0\.fft_n = 4096\nOK/.test(document.getElementById("ms-con-out").textContent)), "the console: a line, its answer");
  t.check(t.errors.length === 0, "no script errors");
});

test("Configure (simulation): an example machine, forms and text, joints, problems, save in this browser", async (t) => {
  await t.ev(() => { localStorage.removeItem("pmc.machine"); localStorage.removeItem("pmc.custom"); });
  await t.nav("config");
  t.check(await t.until(() => cfg.loaded && document.getElementById("cf-state").textContent === "SIMULATED"), "the example machine, LinuxCNC: SIMULATED");
  t.check((await t.text("#cf-dirty")) === "No changes", "no changes");
  await t.page.fill('#cf-body [data-path="name"]', "Test mill"); await t.page.press('#cf-body [data-path="name"]', "Tab");
  t.check((await t.text("#cf-dirty")) === "Changed: not saved", "a change: not saved");
  await t.click('#cf-tabs button[data-t="axes"]');
  const n = await t.ev(() => document.querySelectorAll(".cf-joint").length);
  await t.click("#cf-addjoint");
  t.check(await t.ev((n) => document.querySelectorAll(".cf-joint").length === n + 1, n), "Add joint");
  await t.page.selectOption(`.cf-joint[data-joint="${n}"] [data-path="joints.${n}.drive.type"]`, "stmbl_can");
  t.check(await t.until((n) => document.querySelector(`[data-path="joints.${n}.drive.node"]`), n), "a CAN drive: bus and node fields");
  await t.click('#cf-tabs button[data-t="hw"]');
  t.check(/CAN \(stmblcan\)/.test(await t.text("#cf-body")), "Hardware: the CAN section appears");
  await t.click('#cf-tabs button[data-t="axes"]');
  await t.click(`[data-rmjoint="${n}"]`);
  await t.click('[data-rmaxis="Z"]');
  await t.click("#cf-check");
  t.check(await t.until(() => /joints\.3\.axis/.test(document.getElementById("cf-problems").textContent)), "Z gone but a joint on it: a problem");
  await t.click("#cf-problems li");
  t.check(await t.ev(() => cfg.tab === "axes" && document.querySelector('[data-f="joints.3.axis"]').classList.contains("bad")), "a click: its tab, the field marked");
  await t.click('#cf-tabs button[data-t="json"]');
  await t.page.fill("#cf-json", "{ not json");
  await t.click('#cf-tabs button[data-t="machine"]');
  t.check(await t.until(() => cfg.tab === "json" && /isn't JSON/.test(document.getElementById("cf-body").textContent)), "text that isn't JSON: stays on the text, says so");
  await t.click("#cf-discard"); await t.click('#dlg-form [type="submit"]');
  t.check(await t.until(() => document.getElementById("cf-dirty").textContent === "No changes"), "Discard: back to the saved one");
  await t.click('#cf-tabs button[data-t="machine"]');
  await t.page.fill('#cf-body [data-path="name"]', "Test mill"); await t.page.press('#cf-body [data-path="name"]', "Tab");
  await t.click("#cf-save");
  t.check(await t.until(() => !document.getElementById("dlg").hidden && /Save machine\.json \(name\)/.test(document.getElementById("dlg-t").textContent)), "Save asks: name changed");
  await t.page.fill("#dlg-in", "renamed"); await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => JSON.parse(localStorage.getItem("pmc.machine")).name === "Test mill" && document.getElementById("cf-dirty").textContent === "No changes"), "saved in this browser");
  await t.click('#cf-tabs button[data-t="hist"]');
  t.check(/renamed/.test(await t.text("#cf-body")), "History: the save");
  await t.ev(() => { localStorage.removeItem("pmc.machine"); localStorage.removeItem("pmc.custom"); });
  t.check(t.errors.length === 0, "no script errors");
});


// an STL box (binary) for the 3D model tab: x0 y0 z0 x1 y1 z1 in mm
function stlBox(x0, y0, z0, x1, y1, z1) {
  const v = []; for (const x of [x0, x1]) for (const y of [y0, y1]) for (const z of [z0, z1]) v.push([x, y, z]);
  const f = [[0, 1, 3], [0, 3, 2], [4, 6, 7], [4, 7, 5], [0, 4, 5], [0, 5, 1], [2, 3, 7], [2, 7, 6], [0, 2, 6], [0, 6, 4], [1, 5, 7], [1, 7, 3]];
  const b = Buffer.alloc(84 + 50 * f.length); b.writeUInt32LE(f.length, 80);
  f.forEach((t, i) => t.forEach((k, j) => v[k].forEach((c, n) => b.writeFloatLE(c, 84 + i * 50 + 12 + j * 12 + n * 4))));
  return b;
}

test("Configure (simulation): the machine's 3D model: upload parts, they move with their axes, the built-in one hidden", async (t) => {
  await t.ev(() => { localStorage.removeItem("pmc.machine"); localStorage.removeItem("pmc.custom"); });
  await t.nav("config");
  await t.click('#cf-tabs button[data-t="model"]');
  t.check(await t.until(() => document.getElementById("cf-view") && document.getElementById("cf-view").contains(document.getElementById("viewer"))), "the 3D view beside the form");
  await t.page.setInputFiles("#cf-mfile", { name: "frame.stl", mimeType: "model/stl", buffer: stlBox(-100, -100, -300, 1100, 1100, -240) });
  t.check(await t.until(() => cfg.models.some((m) => m.name === "frame.stl")), "frame.stl uploaded");
  await t.page.setInputFiles("#cf-mfile", { name: "ram.stl", mimeType: "model/stl", buffer: stlBox(-60, 40, 0, 60, 140, 400) });
  t.check(await t.until(() => cfg.models.length === 2), "ram.stl uploaded");
  await t.click("#cf-addpart");
  await t.click("#cf-addpart");
  await t.page.selectOption('[data-path="model.parts.1.file"]', "ram.stl");
  await t.page.selectOption('[data-path="model.parts.1.moves"]', "XYZ");
  await t.page.uncheck('[data-path="model.builtin"]');
  t.check(await t.until(() => mm3d.nodes.length === 2 && mm3d.nodes.every(Boolean) && !mm3d.builtin), "two parts in the view, the built-in gantry off");
  t.check(await t.ev(() => { const keep = new Set(); R.toolG.traverse((o) => keep.add(o)); R.coolant.traverse((o) => keep.add(o)); let vis = 0, tool = 0; R.machine.traverse((o) => { if (o.isMesh && o.visible) { if (keep.has(o)) tool++; else vis++; } }); return vis === 0 && tool > 0; }), "its meshes hidden, the tool still drawn");
  // the built-in carriage and ram stay where the tool tip is (hidden): the ram part with them
  t.check(await t.until(() => { const p = mm3d.nodes[1].holder.position; return Math.abs(p.x - R.carriage.position.x) < 0.01 && Math.abs(p.y - (R.bridge.position.y - 220)) < 0.01 && Math.abs(p.z - R.ram.position.z) < 0.01; }), "the ram (XYZ) at the tool tip");
  t.check(await t.ev(() => mm3d.nodes[0].holder.position.x === 0 && mm3d.nodes[0].holder.position.z === ZTOP), "the frame (fixed) where it was put");
  await t.click("#cf-save"); await t.until(() => !document.getElementById("dlg").hidden); await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => JSON.parse(localStorage.getItem("pmc.machine") || "{}").model?.parts?.length === 2), "saved");
  await t.nav("main");
  t.check(await t.until(() => document.getElementById("viewer").closest("#p-main") && mm3d.nodes.filter(Boolean).length === 2 && !mm3d.builtin), "the main page: the saved model, the view back home");
  await t.ev(() => { localStorage.removeItem("pmc.machine"); });
  t.check(t.errors.length === 0, "no script errors");
});

test("Configure (simulation): a fixed-bridge machine (CN 720): the type draws it; the table and the work move with Y, the tool stays under the bridge", async (t) => {
  await t.ev(() => { localStorage.removeItem("pmc.machine"); localStorage.removeItem("pmc.custom"); });
  await t.nav("config");
  t.check(await t.ev(() => R.type === "gantry"), "the gantry to start with");
  await t.page.selectOption('[data-path="type"]', "bridge");
  t.check(await t.until(() => R.type === "bridge" && R.tableG && R.tableG.children.length > 10), "Machine: fixed bridge: the built-in bridge, its table");
  await t.click("#cf-check");
  t.check(await t.until(() => cfg.errors.some(([w, e]) => w === "axes.Y" && /one joint \(2 here/.test(e))), "two Y joints (the example's gantry pair): a problem for a bridge");
  // the CN 720 (machines/cn720) as the saved machine: the main page draws it
  const cn720 = require("fs").readFileSync(path.join(MILL, "machines", "cn720", "machine.json"), "utf8");
  await t.ev((x) => localStorage.setItem("pmc.machine", x), cn720);
  await t.page.reload();
  t.check(await t.until(() => typeof R !== "undefined" && R && R.type === "bridge" && R.tableG), "the saved CN 720: the bridge on the main page");
  t.check(await t.ev(() => TRAVEL.x[1] === 700 && TRAVEL.y[1] === 430 && TRAVEL.z[0] === -200), "its travel from machine.json (700 x 430 x 200)");
  await t.nav("main");
  // the tool at the bridge's line (y 0 in the view); the work (and the table) moved by -Y, so in the work's frame the tool is at machine Y
  const pose = () => t.ev(() => { const w = new THREE.Vector3(); R.toolG.getWorldPosition(w); const l = R.workG.worldToLocal(w.clone()); return { wy: w.y, ly: l.y, lx: l.x, table: R.tableG.position.y, work: R.workG.position.y, bridge: R.bridge.position.y, cx: R.carriage.position.x }; });
  let p = await pose();
  t.check(Math.abs(p.wy) < 0.5 && p.bridge === 0 && p.table === p.work, `the tool under the fixed bridge (y ${p.wy.toFixed(2)}), table and work together`);
  t.check(Math.abs(p.ly + p.work) < 0.5 && Math.abs(p.lx - p.cx) < 0.5, `in the work's frame the tool is at machine X ${p.lx.toFixed(1)} Y ${p.ly.toFixed(1)}`);
  await t.ev(() => setCam("top", true));
  await t.nav("config");
  await t.page.selectOption('[data-path="type"]', "gantry");
  t.check(await t.until(() => R.type === "gantry" && !R.tableG), "back to the gantry");
  t.check(await t.until(() => R.workG.position.y === 0), "its work stands still");
  await t.ev(() => { localStorage.removeItem("pmc.machine"); });
  t.check(t.errors.length === 0, "no script errors");
});

test("messages: a warning as a large card that goes by itself; a fault stays until a tap; the footer opens the last again", async (t) => {
  await t.ev(() => msg("Just so you know", "info"));
  t.check(await t.ev(() => document.getElementById("popmsg").hidden), "info: only the footer");
  await t.ev(() => msg("Soft limit: X can't go further", "warn"));
  t.check(await t.ev(() => !document.getElementById("popmsg").hidden && document.getElementById("popmsg").className.includes("warn")), "a warning: the card");
  t.check(/Soft limit/.test(await t.text("#pm-t")) && (await t.text("#pm-k")) === "WARNING", "its text, WARNING");
  t.check(await t.ev(() => parseFloat(getComputedStyle(document.getElementById("pm-t")).fontSize) >= 20), "large text");
  await t.ev(() => msg("Soft limit: X can't go further", "warn"));
  t.check((await t.text("#pm-k")) === "WARNING ×2", "the same again: counted, not stacked");
  await t.click("#b-hold");  // the card takes no clicks: what is under it works, and a tap closes it
  t.check(await t.ev(() => document.getElementById("popmsg").hidden), "a tap anywhere closes it");
  await t.ev(() => msg("Drive Y1 fault: overcurrent", "bad"));
  t.check((await t.text("#pm-k")) === "FAULT", "a fault: FAULT");
  await t.sleep(10500);
  t.check(await t.ev(() => !document.getElementById("popmsg").hidden), "a fault stays (a warning goes after 10 s)");
  await t.page.keyboard.press("Shift");
  t.check(await t.ev(() => document.getElementById("popmsg").hidden), "a key closes it");
  await t.click("#msg");
  t.check(await t.ev(() => !document.getElementById("popmsg").hidden) && /overcurrent/.test(await t.text("#pm-t")), "the footer's line opens the last again");
  await t.page.mouse.click(5, 5);
  t.check(t.errors.length === 0, "no script errors");
});

test("run statistics: STATS takes the MDI/JOG place while a cycle runs: run time, pace, line rate, velocity, following errors; back after", async (t) => {
  await stop(t);
  await t.click("#st-jog");
  await t.click("#b-start");
  t.check(await t.until(() => st.run === "run"), "running");
  t.check(await t.until(() => !document.getElementById("sp-stats").hidden && document.getElementById("sp-jog").hidden), "STATS in place of JOG");
  await ffwd(t, await t.ev(() => prog.total * 0.3));  // past the tool change and the spindle's start: cutting
  await t.sleep(2500);
  const cells = () => t.ev(() => Object.fromEntries([...document.querySelectorAll("#rstats > div")].map((d) => [d.querySelector("span").textContent, d.querySelector("b").textContent])));
  const c = await cells();
  t.check(/^Running: /.test(await t.text("#rs-head")), "the head: running, the program");
  t.check(["RUN TIME", "PACE", "G-CODE", "MAX VELOCITY", "MEAN FEED", "CONTOUR ERR", "FERROR X", "FERROR Y1", "FERROR Y2", "FERROR Z", "Y1−Y2 SKEW", "SPINDLE LOAD"].every((k) => k in c), "every number: " + Object.keys(c).join(", "));
  const m = await t.ev(() => ({ v: rs.velMax, fe: rs.feMax, n: rs.lines.size }));
  t.check(m.v > 0 && m.fe.some((v) => v > 0) && m.n > 0, "measured: velocity, following error, lines " + JSON.stringify(m));
  t.check(/l\/s$/.test(c["G-CODE"]) && /µm$/.test(c["FERROR X"]), "units: l/s, µm");
  await stop(t);
  t.check(await t.until(() => !document.getElementById("sp-jog").hidden), "stopped: JOG back");
  await t.click("#st-stats");
  t.check(/^Last run, stopped/.test(await t.text("#rs-head")), "STATS keeps the last run");
  await t.click("#rs-reset");
  t.check(/No run yet/.test(await t.text("#rs-head")), "Reset");
  await t.click("#st-mdi");
  t.check(t.errors.length === 0, "no script errors");
});

test("Configure (simulation): Spindle and Tools & probe tabs, a rack changer, HAL pins from the pin browser, a screw's lead and ratio", async (t) => {
  await t.ev(() => { localStorage.removeItem("pmc.machine"); localStorage.removeItem("pmc.custom"); });
  await t.nav("config");
  await t.click('#cf-tabs button[data-t="spindle"]');
  await t.page.selectOption('[data-path="spindle.type"]', "hal");
  t.check(await t.until(() => !!document.querySelector('[data-path="spindle.on"]')), "Spindle: its pins with type hal");
  await t.click('[data-pinpick="spindle.on"]');
  t.check(await t.until(() => !document.getElementById("pinpick").hidden && document.querySelectorAll("#pp-list [data-pin]").length > 10), "the pin browser opens, with pins");
  t.check(/isn't running/.test(await t.text("#pp-n")), "the simulation: the names guessed, said so");
  t.check(await t.ev(() => [...document.querySelectorAll("#pp-list [data-pin]")].every((b) => /IN|IO/.test(b.children[2].textContent))), "an output to the machine: pins that take a value (IN)");
  await t.page.fill("#pp-q", "outm 03");
  t.check(await t.until(() => document.querySelectorAll("#pp-list [data-pin]").length === 1), "the filter: one pin");
  await t.page.press("#pp-q", "Enter");
  t.check(await t.until(() => document.getElementById("pinpick").hidden && cfg.m.spindle.on === "hm2_7i96s.0.outm.00.out-03"), "Enter picks it: spindle.on set");
  t.check((await t.ev(() => document.querySelector('[data-path="spindle.on"]').value)) === "hm2_7i96s.0.outm.00.out-03", "and the field shows it");
  await t.click('#cf-tabs button[data-t="tools"]');
  t.check(await t.until(() => !!document.querySelector('[data-path="probe.type"]') && !!document.getElementById("cf-ts")), "Tools & probe: the probe and the tool setter");
  await t.click('[data-pinpick="probe.input"]');
  t.check(await t.until(() => document.querySelectorAll("#pp-list [data-pin]").length > 10), "the probe's pin: the browser");
  t.check(await t.ev(() => [...document.querySelectorAll("#pp-list [data-pin]")].every((b) => /OUT|IO/.test(b.children[2].textContent))), "an input from the machine: pins that give a value (OUT)");
  t.check(await t.ev(() => document.querySelector('#pp-list [aria-selected="true"]')?.dataset.pin === "hm2_7i96s.0.inm.00.input-01"), "its pin now marked");
  await t.page.keyboard.press("Escape");
  t.check(await t.ev(() => document.getElementById("pinpick").hidden), "Escape closes it");
  await t.page.selectOption('[data-path="atc.type"]', "rack");
  t.check(await t.until(() => !!document.querySelector('[data-path="atc.pockets"]') && !!document.querySelector('[data-pinpick="atc.unclamp"]')), "a rack: its pockets, positions and pins");
  await t.click('#cf-tabs button[data-t="axes"]');
  await t.page.fill('[data-path="joints.0.mech.lead"]', "10");
  await t.page.press('[data-path="joints.0.mech.lead"]', "Tab");
  await t.page.fill('[data-path="joints.0.mech.ratio"]', "2");
  await t.page.press('[data-path="joints.0.mech.ratio"]', "Tab");
  t.check(await t.until(() => cfg.m.joints[0].mech && cfg.m.joints[0].mech.lead === 10 && cfg.m.joints[0].mech.ratio === 2), "joint 0: lead 10, ratio 2");
  t.check(/5 mm a motor turn · 6,600 rpm at 550 mm\/s/.test(await t.ev(() => document.querySelector('[data-f="joints.0.mech.ratio"] .h').textContent)), "its help: mm a motor turn, rpm at top speed");
  await t.ev(() => { localStorage.removeItem("pmc.machine"); cfg.loaded = false; });
  t.check(t.errors.length === 0, "no script errors");
});

test("Tools page: a tool's 3D model (STL) replaces the drawn cylinder; back to the drawn one", async (t) => {
  await t.nav("tool");
  await t.ev(() => { st.toolSel = st.tool; renderTools(); });
  t.check(/^T\d+$/.test(await t.text("#tm-t")), "the panel: the selected tool");
  await t.page.setInputFiles("#tm-file", { name: "holder.stl", mimeType: "model/stl", buffer: stlBox(-10, -10, 0, 10, 10, 60) });
  t.check(await t.until(() => setup.tools[String(st.tool)] && setup.tools[String(st.tool)].src === "model:holder.stl"), "uploaded: the tool's model");
  t.check(await t.ev(() => R.toolG.children.length === 1 && R.toolG.children[0].geometry === setup.geoms.get("holder.stl")), "the 3D view: the model in place of the cylinder");
  await t.page.fill("#tm-rx", "90"); await t.page.press("#tm-rx", "Tab");
  t.check(await t.until(() => Math.abs(R.toolG.children[0].rotation.x - Math.PI / 2) < 1e-6), "turned 90° about X");
  await t.page.selectOption("#tm-src", "");
  t.check(await t.until(() => !setup.tools[String(st.tool)] && R.toolG.children.length >= 2), "none: drawn from its Ø and length again");
  t.check(t.errors.length === 0, "no script errors");
});

test("3D view: see-through objects draw in a fixed order (no flicker); the chips and the trail are never culled", async (t) => {
  await t.nav("main");
  await t.ev(() => { st.show.fix = "glass"; st.show.work = "glass"; applyLooks(); });
  const o = await t.ev(() => [...setup.nodes.values()].map((n) => { const r = []; n.node.traverse((x) => { if (x.isMesh) r.push(x.renderOrder); }); return { r, off: n.material.polygonOffset, dw: n.material.depthWrite }; }));
  const all = o.flatMap((x) => x.r);
  t.check(all.length > 1 && new Set(all).size === all.length && all.every((v) => v > 0), `every see-through mesh its own draw order (${all.length})`);
  t.check(o.every((x) => x.off && !x.dw), "a depth offset; no depth writes");
  t.check(await t.ev(() => R.trail.frustumCulled === false && R.workG.children.some((c) => c.isPoints && c.frustumCulled === false)), "the trail and the chips: never culled by a stale bounding sphere");
  await t.ev(() => { st.show.fix = true; st.show.work = true; applyLooks(); });
  t.check(t.errors.length === 0, "no script errors");
});

test("lathe (simulation, the Emco Compact 5's machine.json): Ø DRO, X Z jog, the turret's tools, wizards, a program to its end, back to a mill", async (t) => {
  const emco = require("fs").readFileSync(path.join(MILL, "machines", "emco_compact5", "machine.json"), "utf8");
  await t.ev((x) => localStorage.setItem("pmc.machine", x), emco);
  await t.page.reload();
  t.check(await t.until(() => typeof R !== "undefined" && R && R.type === "lathe" && LATHE && document.body.classList.contains("lathe")), "machine.json's type lathe: the console is a lathe's");
  t.check(await t.ev(() => LATHE.post === "front" && LATHE.dia && TRAVEL.x[1] === 50 && TRAVEL.z[0] === -260), "front post, diameter mode, its travel");
  t.check(await t.ev(() => document.getElementById("d-y").closest(".axis").hidden && document.getElementById("b-zeroall").textContent === "Zero XZ"), "no Y in the DRO");
  t.check(/^Ø50\.000$/.test(await t.text("#d-x")) && /^r 25\.000/.test(await t.text("#ds-x")), "X as a diameter (Ø50, r 25)");
  t.check(await t.ev(() => prog.name === "SHAFT-M20_OP1.ngc" && prog.lines.includes("G18 G7 G21 G90 G40 G54 G95") && prog.segs.every((s) => s.y0 === 0 && s.y === 0)), "a lathe's program: G18 G7, no Y");
  t.check(await t.ev(() => prog.lines.some((l) => /^G1 Z-40\.000 F0\.22$/.test(l)) && prog.lines.some((l) => /^G0 X20\.500$/.test(l)) && Math.max(...prog.segs.map((s) => s.x)) <= 25.01), "X words diameters (X20.500), positions radii");
  t.check(await t.ev(() => R.pathMirror && R.pathMirror.visible && R.spinG && R.toolG.children.length >= 2), "the path mirrored across the axis, the chuck, the tool's insert and holder");
  // jog: left/right Z, up/down X (up: away from the operator on a front post)
  await t.click("#st-jog");
  await t.ev(() => { st.jogInc = 1; });
  const p0 = await pos(t);
  await t.click('#jogpad [data-j="x+"]'); await t.click('#jogpad [data-j="y+"]');
  const p1 = await pos(t);
  t.check(near(p1.z - p0.z, 1) && near(p1.x - p0.x, -1) && near(p1.y, 0), `jog: Z+1, X−1 (the "up" arrow on a front post): ${JSON.stringify(p1)}`);
  t.check((await t.text('#jogpad [data-j="x+"]')) === "Z+" && await t.ev(() => document.querySelector('#jogpad [data-j="z+"]').hidden), "the pad's labels; no Z± for Y");
  await t.click("#st-mdi");
  // the turret's tools: X / Z offsets, nose radius, orientation
  await t.nav("tool");
  t.check(await t.until(() => /X off/.test(document.querySelector("#tool-t thead").textContent) && /Nose R/.test(document.querySelector("#tool-t thead").textContent)), "the tool table: X off, Z off, Nose R, Q");
  await t.page.fill('#tool-t [data-k="d"][data-i="0"]', "0.8"); await t.page.press('#tool-t [data-k="d"][data-i="0"]', "Tab");
  t.check(await t.until(() => st.tools[0].d === 1.6), "nose R 0.8: D 1.6 in the table");
  await t.page.selectOption('#tool-t [data-k="q"][data-i="0"]', "2");
  t.check(await t.until(() => st.tools[0].q === 2), "the orientation");
  // a wizard: turning to Ø20 over 30
  await t.nav("wiz");
  t.check(await t.ev(() => document.querySelector(".lathe-cards") && !document.querySelector(".lathe-cards").hidden && [...document.querySelectorAll("#p-wiz > .cards")].filter((c) => !c.hidden).length === 1), "the lathe's wizards, not the mill's");
  await t.page.fill("#lt-d1", "20"); await t.page.fill("#lt-l", "30");
  await t.click("#lt-go");
  t.check(await t.until(() => /^TURN_D20x30/.test(prog.name) && st.page === "main"), "Turning: made and loaded");
  t.check(await t.ev(() => prog.lines.some((l) => /^G1 X20\.000/.test(l) || /^G0 X20\.000/.test(l)) && Math.min(...prog.segs.filter((s) => s.k === "mv" && !s.rapid).map((s) => s.x)) >= 9.99), "its finishing pass at Ø20, nothing below");
  await stop(t);
  await t.ev(() => { st.m1 = false; });
  await t.click("#b-start");
  await t.until(() => st.run === "run");
  await ffwd(t, await t.ev(() => prog.total - 0.3));
  t.check(await t.until(() => st.run === "done", null, 8000), "it runs to its end");
  // back to a mill: the Configure page's type
  await t.nav("config");
  await t.page.selectOption('[data-path="type"]', "gantry");
  t.check(await t.until(() => R.type === "gantry" && !LATHE && !document.body.classList.contains("lathe")), "type gantry: the mill's console again");
  t.check(await t.ev(() => library.some((p) => /BRACKET/.test(p.name)) && st.tools.some((x) => x.t === 12) && !document.getElementById("d-y").closest(".axis").hidden), "its programs, tools and Y back");
  await t.page.selectOption('[data-path="type"]', "lathe");
  t.check(await t.until(() => R.type === "lathe" && LATHE && !!document.querySelector('[data-path="lathe.post"]')), "type lathe in Configure: a lathe, its tool post's field");
  await t.page.selectOption('[data-path="lathe.post"]', "back");
  t.check(await t.until(() => LATHE && LATHE.post === "back" && LATHE.s === 1 && (document.querySelector('#jogpad [data-j="y+"]').textContent === "X+")), "a back post: +X away from the operator, the pad's up arrow X+");
  await t.page.selectOption('[data-path="type"]', "gantry");
  t.check(await t.until(() => R.type === "gantry" && !LATHE), "and a gantry again");
  await t.ev(() => localStorage.removeItem("pmc.machine"));
  t.check(t.errors.length === 0, "no script errors");
});

test("Debug page, drives (simulation): firmware versions; a config backed up, patched for this firmware and restored", async (t) => {
  await t.nav("debug");
  await t.click('#dbg-tab button[data-t="drives"]');
  t.check(await t.until(() => /firmware v1 · HV v1 · config unversioned/.test(document.querySelector('.drv-card[data-n="X"] [data-k="ver"]').textContent)), "the card: firmware v1, HV v1, config unversioned");
  t.check(/Patch config gives it its version line/.test(await t.text('.drv-card[data-n="X"] [data-k="problem"]')), "a problem: patch the config");
  await t.click('.drv-card[data-n="X"] [data-cfgp]');
  t.check(await t.until(() => !document.getElementById("drv-cfg").hidden && /None yet/.test(document.getElementById("drv-cfg").textContent)), "the config panel: no backups yet");
  await t.click('#drv-cfg [data-dcf="backup"]');
  t.check(await t.until(() => document.querySelectorAll("#drv-cfg [data-restore]").length === 1), "Back up now: one backup");
  await t.click('#drv-cfg [data-dcf="plan"]');
  t.check(await t.until(() => /Patch v0 → v1/.test(document.getElementById("drv-cfg").textContent)), "the patch: v0 → v1, what it does");
  await t.click('#drv-cfg [data-dcf="patch"]');
  t.check(await t.until(() => !document.getElementById("dlg").hidden && /Patch drive X/.test(document.getElementById("dlg-t").textContent)), "asks first");
  await t.click('#dlg-form [type="submit"]');
  t.check(await t.until(() => /config v1/.test(document.querySelector('.drv-card[data-n="X"] [data-k="ver"]').textContent) && !document.querySelector('.drv-card[data-n="X"] [data-k="problem"]')), "patched: config v1, no problem");
  t.check(await t.until(() => document.querySelectorAll("#drv-cfg [data-restore]").length === 2), "the one before kept");
  await t.click("#drv-cfg li:nth-child(2) [data-restore]");
  await t.click('#dlg-form [type="submit"]');
  t.check(await t.until(() => /config unversioned/.test(document.querySelector('.drv-card[data-n="X"] [data-k="ver"]').textContent)), "restored: unversioned again");
  await t.click("#dcf-close");
  t.check(t.errors.length === 0, "no script errors");
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
  // fixtures and workpiece: solid, see-through, hidden, solid again (the vise is a fixture, the stock the workpiece)
  const look = (id) => t.ev((id) => { const n = setup.nodes.get(id); return [n.node.visible, n.material.transparent, +n.material.opacity.toFixed(2)]; }, id);
  for (const [k, id, name] of [["fix", "vise", "fixtures"], ["work", "stock", "workpiece"]]) {
    for (const [label, want] of [[name + " · see-through", [true, true, 0.28]], [name, [false, false, 1]], [name, [true, false, 1]]]) {
      await t.click(`[data-show="${k}"]`);
      t.check(await t.until(([k, label]) => document.querySelector(`[data-show="${k}"]`).textContent === label, [k, label]) && JSON.stringify(await look(id)) === JSON.stringify(want),
        `${name}: ${label} ${JSON.stringify(await look(id))}`);
    }
  }
  await t.click('[data-show="work"]'); await t.click('[data-show="work"]');  // the workpiece hidden
  t.check(await t.until(() => !setup.nodes.get("stock").node.visible && setup.nodes.get("vise").node.visible && R.triad.visible), "workpiece hidden: the vise and the work origin stay");
  await t.click('[data-show="work"]');
  t.check(await t.until(() => setup.nodes.get("stock").node.visible), "workpiece on again");
  for (const k of ["machine", "path", "limits", "defl"]) {
    const was = await t.ev((k) => st.show[k], k);
    await t.click(`[data-show="${k}"]`);
    t.check(await t.ev(([k, w]) => st.show[k] === !w && document.querySelector(`[data-show="${k}"]`).getAttribute("aria-pressed") === String(!w), [k, was]), `layer ${k}`);
    await t.click(`[data-show="${k}"]`);
  }
  const box = await t.page.locator("#scene").boundingBox();
  await t.page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await t.page.mouse.down();
  await t.page.mouse.move(box.x + box.width / 2 + 80, box.y + box.height / 2 + 30, { steps: 5 });
  await t.page.mouse.up();
  t.check(await t.ev(() => [...document.querySelectorAll("[data-cam]")].every((b) => b.getAttribute("aria-pressed") === "false")), "orbiting by hand leaves the presets");
});

test("3D view: select a fixture or the workpiece by clicking; its panel moves, turns, hides, removes", async (t) => {
  await t.click('[data-cam="part"]');
  await t.sleep(1300);
  // a point of an object on the screen (its bounding box's centre, nudged)
  const onScreen = (id, dy = 0) => t.ev(([id, dy]) => {
    const v = new THREE.Vector3(); new THREE.Box3().setFromObject(setup.nodes.get(id).node).getCenter(v); v.y += dy; v.project(R.cam);
    const r = document.getElementById("scene").getBoundingClientRect(); return [r.left + (v.x + 1) / 2 * r.width, r.top + (1 - v.y) / 2 * r.height];
  }, [id, dy]);
  let [x, y] = await onScreen("stock");
  await t.page.mouse.click(x, y);
  t.check(await t.until(() => setup.sel === "stock" && !document.getElementById("objpanel").hidden), "click the stock: selected, its panel");
  t.check((await t.text("#ob-name")).startsWith("Stock") && (await t.page.inputValue("#ob-role")) === "workpiece" && (await t.page.inputValue("#ob-frame")) === "work", "panel: name, workpiece, in the work offset");
  t.check(await t.ev(() => !!R.selBox && setup.nodes.get("stock").material.emissiveIntensity > 0), "highlighted, with a box around it");
  [x, y] = await onScreen("vise", -50);
  await t.page.mouse.click(x, y);
  t.check(await t.until(() => setup.sel === "vise") && (await t.page.inputValue("#ob-role")) === "fixture", "click the vise: the fixture selected");
  // move it, turn it, see-through
  const nodeX = () => t.ev(() => setup.nodes.get("vise").node.position.x);
  const x0 = await nodeX();
  await t.page.fill("#ob-x", "15"); await t.page.press("#ob-x", "Tab");
  t.check(await t.until((x0) => Math.abs(setup.nodes.get("vise").node.position.x - x0 - 15) < 1e-6, x0), "X 15: it moves 15 mm");
  await t.click("#ob-turn");
  t.check(await t.ev(() => objOf("vise").rot[2] === 90 && Math.abs(setup.nodes.get("vise").node.rotation.z - Math.PI / 2) < 1e-6) && (await t.page.inputValue("#ob-rz")) === "90", "Turn 90°");
  await t.click('#ob-look button[data-look="glass"]');
  t.check(JSON.stringify(await t.ev(() => { const m = setup.nodes.get("vise").material; return [m.transparent, +m.opacity.toFixed(2)]; })) === "[true,0.28]", "see-through, this one");
  // a frame change keeps it in place
  const before = await t.ev(() => setup.nodes.get("vise").node.position.toArray());
  await t.page.selectOption("#ob-frame", "machine");
  const after = await t.ev(() => setup.nodes.get("vise").node.position.toArray());
  t.check(before.every((v, i) => Math.abs(v - after[i]) < 1e-6) && (await t.page.inputValue("#ob-x")) !== "15", "placed in machine: the same place, machine numbers");
  // the helpers
  await t.click("#ob-table");
  t.check(await t.until(() => Math.abs(new THREE.Box3().setFromObject(setup.nodes.get("vise").node).min.z) < 1e-3), "On the table: its bottom on the table");
  await t.click('[data-show="work"]'); await t.click('[data-show="work"]');
  await t.ev(() => selectObject("stock"));
  await t.page.fill("#ob-z", "-7"); await t.page.press("#ob-z", "Tab");
  await t.click("#ob-zero");
  t.check(await t.until(() => { const b = new THREE.Box3().setFromObject(setup.nodes.get("stock").node), o = off(); return Math.abs(b.max.z - (o.z + ZTOP)) < 1e-3 && Math.abs(b.min.x - o.x) < 1e-3 && Math.abs(b.min.y - o.y) < 1e-3; }), "Top at work zero: the stock's top at Z0, its corner at X0 Y0");
  // remove (asks), and clicking empty space deselects
  await t.ev(() => selectObject("vise"));
  await t.click("#ob-del");
  await t.click('#dlg-form [type="submit"]');
  t.check(await t.until(() => !setup.objects.some((o) => o.id === "vise") && !setup.nodes.has("vise") && document.getElementById("objpanel").hidden), "Remove: the vise gone from the setup");
  const box = await t.page.locator("#scene").boundingBox();
  await t.ev(() => selectObject("stock"));
  await t.page.mouse.click(box.x + 20, box.y + box.height - 60);
  t.check(await t.until(() => setup.sel === null), "a click on empty space: nothing selected");
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

test("over HTTP with no millweb: the simulation after 6 s", async (t) => {
  const port = await serve();
  await t.page.goto(`http://127.0.0.1:${port}/`);
  await t.sleep(1000);
  t.check(await t.ev(() => st.run !== "hold"), "waits for a machine first");
  t.check(/Connecting to 127\.0\.0\.1/.test(await t.text("#boot-msg")), "the loading screen: connecting");
  t.check(await t.until(() => st.run === "hold" && document.getElementById("msg").textContent.includes("No machine answered"), null, 9000), "then the simulation, saying why");
  t.check(await t.until(() => !document.getElementById("boot")), "the loading screen gone");
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
