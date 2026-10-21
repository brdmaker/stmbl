// The console on real LinuxCNC (test/sim with millweb), through the page's
// own controls. Each case starts from the same machine (wsclient.baseline:
// on, homed, idle, G54 at its test values, nobody in control).
//   python3 test/harness.py -- node test/gui/live_suite.js [-k WORD]... [--shots DIR]
// (the harness starts LinuxCNC and sets MILLWEB_PORT and MILLWEB_DIR)
const fs = require("fs");
const path = require("path");
const { test, run } = require("./runner");
const { Client } = require("./wsclient");

const PORT = +process.env.MILLWEB_PORT;
const DIR = process.env.MILLWEB_DIR;
if (!PORT) { console.error("MILLWEB_PORT: run through test/harness.py"); process.exit(2); }
const BASE = `http://127.0.0.1:${PORT}/`;
let M = null;  // the suite's own protocol client

const near = (a, b, tol = 1e-3) => Math.abs(a - b) < tol;
const droIs = (t, a, s, ms) => t.until(([a, s]) => document.getElementById("d-" + a).textContent.trim() === s, [a, s], ms);
async function take(t, page) {
  page = page || t.page;
  await page.click("#b-control");
  return page.waitForFunction(() => document.getElementById("b-control").textContent.includes("IN"), null, { timeout: 5000 }).then(() => true, () => false);
}
async function mdi(t, cmd) {
  await t.nav("main");
  await t.click("#st-mdi");
  await t.page.fill("#mdi", cmd);
  await t.page.press("#mdi", "Enter");
  await new Promise((r) => setTimeout(r, 150));
  return M.idle();
}
const logHas = (t, s, ms = 5000) => t.until((s) => events.some((e) => e.text.includes(s)), s, ms);

// ------------------------------------------------------------ connecting, control
test("connects: LIVE, the machine's name, tables from the INI", async (t) => {
  t.check((await t.text("#ch-link")).includes("LIVE"), "chip: LIVE");
  t.check((await t.text(".brand b")) === "PORTAL-M SIM", "the machine's name");
  t.check((await t.text(".brand span")).startsWith("XYYZ · LinuxCNC · millweb"), "coordinates, millweb version");
  t.check((await t.text("#f-src")) === `live · 127.0.0.1:${PORT}`, "footer: live");
  t.check((await t.text("#b-control")).includes("TAKE"), "nobody has control: TAKE CONTROL");
  t.check((await t.text("#ch-on")).includes("MACHINE ON") && (await t.text("#ch-home")) === "HOMED", "the machine's state");
  t.check(await t.ev(() => TRAVEL.z[0] === -215 && TRAVEL.x[1] === 1000), "travel from the INI");
  t.check(await t.ev(() => events.some((e) => e.text.includes("Browser connected"))), "events from the server");
  await t.nav("stat");
  t.check(await t.until(() => document.querySelectorAll("#pins tbody tr").length === 5 && document.getElementById("lpin-0").textContent !== "—"), "the INI's five pins, with values");
  await t.nav("dyn");
  t.check((await t.text("#drv-t")).includes("No drives configured"), "no drives configured");
  t.check(await t.until(() => document.getElementById("pc-pill").textContent === "NOT LOADED"), "pcomp: not loaded");
  await t.nav("tool");
  t.check(await t.ev(() => st.tools.map((x) => x.t).join(",") === "1,3,5,12,99"), "the machine's tool table");
});

test("control: take it, a second browser watches and can only E-STOP, hand it back", async (t) => {
  t.check(await take(t), "Take control: IN CONTROL");
  t.check(await logHas(t, "This browser has control"), "message");
  const p2 = await t.ctx.browser().newPage({ viewport: { width: 1280, height: 900 } });
  try {
    await p2.goto(BASE + "#main");
    await p2.waitForFunction(() => document.getElementById("ch-link").textContent.includes("LIVE"));
    t.check((await p2.textContent("#b-control")).includes("WATCHING"), "the other browser: WATCHING");
    await p2.fill("#mdi", "G0 X0");
    await p2.press("#mdi", "Enter");
    t.check(await p2.waitForFunction(() => document.getElementById("msg").textContent.includes("doesn't have control"), null, { timeout: 4000 }).then(() => true, () => false), "its MDI is refused");
    await p2.click("#b-estop");
    t.check(await t.until(() => st.estop && !st.on), "its E-STOP stops the machine; this browser sees it");
    t.check(await t.msgHas("E-STOP"), "message: E-STOP");
    await t.click("#b-control");
    t.check(await p2.waitForFunction(() => document.getElementById("b-control").textContent.includes("TAKE"), null, { timeout: 4000 }).then(() => true, () => false), "handed back: the other sees TAKE CONTROL");
    t.check(await take(t, p2), "the other takes it");
    t.check(await t.until(() => document.getElementById("b-control").textContent.includes("WATCHING")), "this one watches");
    t.check(await take(t), "and takes it over");
  } finally { await p2.close(); }
});

test("E-STOP, power and homing from the header and the DRO", async (t) => {
  await take(t);
  await t.click("#b-estop");
  t.check(await t.until(() => st.estop) && (await t.text("#ch-estop")) === "E-STOP", "E-STOP");
  await t.click("#b-estop");
  t.check(await t.until(() => !st.estop && !st.on), "E-STOP reset: off");
  await t.click("#b-power");
  t.check(await t.until(() => st.on) && (await t.text("#ch-on")) === "MACHINE ON", "power on");
  await M.call("control", { take: true, force: true });
  await M.call("unhome", { axis: "all" });
  await M.call("control", { take: false });
  t.check(await t.until(() => !st.homed.x && !st.homed.y && !st.homed.z), "unhomed (by the suite)");
  t.check((await t.text("#ch-home")) === "NOT HOMED", "chip: NOT HOMED");
  await take(t);
  await t.click('#dro button[data-home="z"]');
  t.check(await t.until(() => st.homed.z && !st.homed.x), "REF Z");
  await t.click("#b-homeall");
  t.check(await t.until(() => st.homed.x && st.homed.y && st.homed.z, null, 20000), "Home all");
  t.check(await logHas(t, "All axes homed"), "event: all axes homed");
  await t.page.keyboard.press("F2");
  t.check(await t.until(() => !st.on), "F2: off");
  await t.page.keyboard.press("F1");
  t.check(await t.until(() => st.estop), "F1: E-STOP");
});

// ------------------------------------------------------------ MDI, DRO, jog
test("MDI: the DRO follows; machine and work coordinates; history", async (t) => {
  await take(t);
  await mdi(t, "G0 X25 Y35 Z10");
  t.check(await droIs(t, "x", "25.000") && await droIs(t, "y", "35.000") && await droIs(t, "z", "10.000"), "DRO: 25 35 10");
  t.check((await t.text("#ds-x")).startsWith("machine 465.000"), "machine X 465");
  await t.click('#dro-mode button[data-c="mach"]');
  t.check(await droIs(t, "z", "−185.000"), "machine Z −185");
  await t.click('#dro-mode button[data-c="work"]');
  t.check((await t.text("#mdi-hist li:first-child")) === "G0 X25 Y35 Z10", "history");
  t.check(await t.until(() => /G54/.test(document.getElementById("modal").textContent) && /M5/.test(document.getElementById("modal").textContent)), "modal codes from LinuxCNC");
  await t.page.fill("#mdi", "G999");
  await t.page.press("#mdi", "Enter");
  t.check(await t.until(() => events.some((e) => e.lvl === "bad")), "an interpreter error in the log");
  await mdi(t, "M3 S8000");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.startsWith("S8000 · CW")), "M3 S8000 on the spindle panel");
  await mdi(t, "M5");
});

test("jog: increments from the pad, continuous until released, keyboard", async (t) => {
  await take(t);
  await t.nav("main");
  await t.click("#st-jog");
  await t.click('#jog-inc button[data-v="1"]');
  await t.page.dispatchEvent('#jogpad button[data-j="y+"]', "pointerdown");
  t.check(await droIs(t, "y", "11.000"), "Y+ 1 mm");
  await t.click('#jog-inc button[data-v="0.1"]');
  await t.page.dispatchEvent('#jogpad button[data-j="x-"]', "pointerdown");
  t.check(await droIs(t, "x", "9.900"), "X− 0.1 mm");
  await t.click('#jog-inc button[data-v="0"]');
  await t.page.dispatchEvent('#jogpad button[data-j="x+"]', "pointerdown");
  await t.sleep(1200);
  const x1 = M.st.pos[0];
  await t.page.dispatchEvent("body", "pointerup");
  await M.idle();
  const x2 = M.st.pos[0];
  await t.sleep(500);
  t.check(x1 > 20, `held 1.2 s: moved (X ${x1.toFixed(2)}; the keep-alive kept it going)`);
  t.check(near(M.st.pos[0], x2), "released: stopped");
  await t.click('#jog-inc button[data-v="1"]');
  await t.page.locator("#dro").click({ position: { x: 2, y: 2 } });
  const y0 = M.st.pos[1];
  await t.page.keyboard.press("ArrowDown");
  t.check(await M.until((s) => near(s.pos[1], y0 - 1)), "↓: Y−1 on the machine");
});

test("zero, SET, zero all: G10 L20 on the machine", async (t) => {
  await take(t);
  await t.click('#dro button[data-zero="x"]');
  t.check(await droIs(t, "x", "0.000") && await M.until((s) => near(s.g5x[0], 450)), "ZERO X: G54 X 450 on the machine");
  await t.click('#dro button[data-touch="y"]');
  await t.page.fill("#dlg-in", "5");
  await t.page.press("#dlg-in", "Enter");
  t.check(await droIs(t, "y", "5.000") && await M.until((s) => near(s.g5x[1], 465)), "SET Y 5: G54 Y 465");
  await t.click("#b-gotozero");  // three moves, each after the one before
  t.check(await M.until((s) => near(s.pos[0], 0) && near(s.pos[1], 0) && near(s.pos[2], 5) && s.interp === "idle", 20000), "go to zero: X0 Y0 Z5");
  t.check(!(await t.ev(() => events.some((e) => e.text.includes("running")))), "no move refused on the way");
  await t.click("#b-park");
  t.check(await M.until((s) => near(s.mach[1], 980) && near(s.mach[2], 0) && s.interp === "idle", 20000), "park: Y 980, Z up");
  await mdi(t, "G53 G0 Z0");
  await t.click("#b-zeroall");
  t.check(await droIs(t, "z", "0.000") && await droIs(t, "y", "0.000"), "zero all");
  t.check(await M.until((s) => near(s.g5x[2], 0)), "G54 Z = machine Z 0");
  await t.click("#b-gotozero");
  t.check(await t.msgHas("beyond the travel"), "go to zero with Z5 above the top: the console refuses");
});

// ------------------------------------------------------------ pages
test("offsets page: the machine's table, edit, use G55, G92 clear", async (t) => {
  await take(t);
  await t.nav("off");
  t.check(await t.until(() => document.getElementById("of-1-x") && +document.getElementById("of-1-x").value === 640), "G55 X 640 from the machine");
  await t.page.fill("#of-1-z", "-190");
  await t.page.press("#of-1-z", "Tab");
  t.check(await t.until(() => st.offsets[1].z === -190), "G55 Z −190 (G10 L2) back from the machine");
  await t.click('#off-t button[data-act="1"]');
  t.check(await M.until((s) => s.wcs === 1) && await t.until(() => document.getElementById("dro-wcs").textContent === "G55"), "Use G55");
  t.check(await t.until(() => document.querySelector("#off-t tbody tr.active td").textContent.includes("G55")), "G55 marked active");
  await mdi(t, "G92 X0");
  t.check(await M.until((s) => Math.abs(s.g92[0]) > 1), "G92 X0 by MDI");
  await t.nav("off");
  await t.click("#off-g92");
  t.check(await M.until((s) => near(s.g92[0], 0)), "G92 cleared");
});

test("tools page: add, edit, delete reach tool.tbl; load; measure", async (t) => {
  await take(t);
  await t.nav("tool");
  const tbl = () => fs.readFileSync(path.join(DIR, "tool.tbl"), "utf8");
  await t.click("#tl-add");
  t.check(await t.until(() => st.tools.some((x) => x.t === 13)), "T13 added");
  await t.sleep(1200);
  t.check(/^T13 P13 D8\.0000 Z\+50\.0000 ;new tool$/m.test(tbl()), "...in tool.tbl");
  await t.page.fill('#tool-t input[aria-label="T13 diameter"]', "9");
  await t.page.press('#tool-t input[aria-label="T13 diameter"]', "Tab");
  await t.sleep(1200);
  t.check(/^T13 P13 D9\.0000/m.test(tbl()), "edited diameter in tool.tbl");
  await t.click('#tool-t tr[data-t="13"] td:first-child');
  await t.click("#tl-del");
  await t.sleep(1200);
  t.check(!/^T13/m.test(tbl()), "deleted from tool.tbl");
  await t.click('#tool-t tr[data-t="3"] td:first-child');
  await t.click("#tl-load");
  t.check(await M.until((s) => s.tool === 3 && near(s.tlo, 48.13), 20000), "T3 M6 G43 on the machine");
  t.check(await t.until(() => document.querySelector("#tool-t tr.active td").textContent.includes("T3 ● in spindle")), "T3 in the spindle");
  await t.click("#tl-meas");
  t.check(await t.until(() => events.some((e) => /T3 length 49\.99|T3 length 50\.00/.test(e.text)), null, 60000), "the tool setter: T3 50 mm");
  t.check(await t.until(() => Math.abs(st.tools.find((x) => x.t === 3).l - 50) < 0.01), "the table shows it");
  await M.call("control", { take: true, force: true });
  await M.call("tools_save", { tools: M.last.tools.tools.map((x) => (x.t === 3 ? { ...x, z: 48.13 } : x)) });
});

test("probe page: Z surface with T99, apply to G54; unsupported cycles say so", async (t) => {
  await take(t);
  await mdi(t, "T99 M6 G43");
  await mdi(t, "G0 X60 Y40 Z5");
  await M.call("control", { take: true, force: true });
  await M.call("offset", { index: 0, z: -190 });
  await M.call("control", { take: false });
  await take(t);
  await t.nav("probe");
  await t.click('#probe-ops .op[data-op="z"]');
  await t.click("#pr-go");
  t.check(await t.until(() => document.getElementById("pr-res").textContent.includes("−195") || document.getElementById("pr-res").textContent.includes("-195"), null, 30000), "touched at machine Z −195");
  await t.click("#pr-apply");
  t.check(await M.until((s) => near(s.g5x[2], -195, 0.01)), "applied: G54 Z −195");
  await t.click('#probe-ops .op[data-op="bore"]');
  await t.click("#pr-go");
  t.check(await t.msgHas("not in the backend yet"), "bore: not in the backend yet");
  await mdi(t, "T0 M6 G49");
});

test("file page: the machine's programs, preflight, load, run with the cycle buttons", async (t) => {
  await take(t);
  await t.nav("file");
  t.check(await t.until(() => [...document.querySelectorAll("#files .file b")].some((b) => b.textContent === "bracket-op1.ngc")), "bracket-op1.ngc listed");
  const i = await t.ev(() => [...document.querySelectorAll("#files .file b")].findIndex((b) => b.textContent === "bracket-op1.ngc"));
  await t.click(`#files .file[data-i="${i}"] b`);
  t.check(await t.until(() => document.getElementById("fl-name").textContent === "bracket-op1.ngc" && document.getElementById("fl-cards").textContent.includes("FITS")), "preflight from the machine's interpreter");
  t.check((await t.text("#fl-cards")).includes("T3"), "...with its tool");
  await t.click(`#files .file[data-i="${i}"] .btn`);
  t.check(await t.until(() => document.getElementById("h-prog").textContent === "bracket-op1.ngc"), "loaded");
  await t.nav("main");
  t.check(await t.until(() => document.querySelectorAll("#gcode div").length === prog.lines.length && prog.lines.length > 20), "its listing");
  await t.click("#b-start");
  t.check(await M.until((s) => s.run === "run") && await t.until(() => document.getElementById("ch-run").textContent === "RUNNING"), "Cycle start: RUNNING");
  t.check(await t.until(() => document.getElementById("ch-mode").textContent.includes("AUTO")), "AUTO");
  await t.sleep(1500);
  await t.click("#b-hold");
  t.check(await M.until((s) => s.run === "hold") && await t.until(() => document.getElementById("ch-run").textContent === "FEED HOLD"), "Feed hold");
  await t.click("#b-hold");
  t.check(await M.until((s) => s.run === "run"), "resume");
  t.check(await t.until(() => document.querySelectorAll("#gcode .cur").length === 1), "the current line marked");
  await t.click("#b-stop");
  t.check(await M.until((s) => s.interp === "idle") && await t.until(() => document.getElementById("ch-run").textContent === "IDLE" || document.getElementById("ch-run").textContent === "DONE"), "Stop");
  await t.click("#gl-16");  // G0 Z5 after the pocket: a line it can start from
  await t.click("#b-runfrom");
  t.check(await logHas(t, "Cycle start from line 16"), "Run from N0016");
  t.check(await M.until((s) => s.run === "done", 90000), "...runs to the end");
  t.check(await logHas(t, "Program finished") && !(await t.ev(() => events.some((e) => e.lvl === "bad"))), "finished without an error");
  t.check(await t.until(() => document.getElementById("ch-run").textContent === "DONE"), "DONE");
});

test("wizard: its program is saved on the machine and loaded", async (t) => {
  await take(t);
  await t.nav("wiz");
  await t.page.fill("#bc-n", "5");
  await t.click("#bc-go");
  t.check(await t.until(() => document.getElementById("h-prog").textContent === "BOLTCIRCLE_5x_R28.ngc"), "loaded on the machine");
  t.check(fs.existsSync(path.join(DIR, "nc_files", "BOLTCIRCLE_5x_R28.ngc")), "saved in the program folder");
  t.check(await M.until((s) => (s.file || "").endsWith("BOLTCIRCLE_5x_R28.ngc")), "LinuxCNC has it open");
  t.check(await t.until(() => prog.server && prog.segs.filter((s) => s.k === "tc").length === 1), "the preview from LinuxCNC's interpreter");
});

test("overrides, spindle, coolant, M1/block delete reach the machine", async (t) => {
  await take(t);
  const range = (id, v) => t.ev(([id, v]) => { const e = document.getElementById(id); e.value = v; e.dispatchEvent(new Event("input")); }, [id, v]);
  await range("o-feed", 150);
  t.check(await M.until((s) => near(s.ovr.feed, 1.5)), "feed 150 %");
  await range("o-rapid", 40);
  t.check(await M.until((s) => near(s.ovr.rapid, 0.4)), "rapid 40 %");
  await range("o-maxv", 70);
  t.check(await M.until((s) => near(s.ovr.maxv, 0.7)), "max velocity 70 %");
  await t.click('[data-reset="o-feed"]');
  t.check(await M.until((s) => near(s.ovr.feed, 1)), "reset: feed 100 %");
  await t.click("#sp-ccw");
  t.check(await M.until((s) => s.spin.dir === -1), "spindle CCW");
  await t.click("#sp-cw");
  t.check(await M.until((s) => s.spin.dir === 1), "spindle CW");
  await range("o-spin", 80);
  t.check(await M.until((s) => near(s.spin.ovr, 0.8)), "spindle 80 %");
  await t.click("#sp-stop");
  t.check(await M.until((s) => s.spin.dir === 0), "spindle stop");
  await t.click("#cl-flood");
  t.check(await M.until((s) => s.flood) && (await t.attr("#cl-flood", "aria-pressed")) === "true", "flood on");
  await t.click("#cl-mist");
  t.check(await M.until((s) => s.mist), "mist on");
  await t.click("#cl-flood");
  await t.click("#cl-mist");
  t.check(await M.until((s) => !s.flood && !s.mist), "both off");
  await t.click("#cl-air");
  t.check(await t.msgHas("no air"), "air: not configured on this machine");
  await t.nav("set");
  await t.click("#set-m1");
  t.check(await M.until((s) => s.m1), "M1 on");
  await t.click("#set-bd");
  t.check(await M.until((s) => s.bd), "block delete on");
  await t.click("#set-m1");
  await t.click("#set-bd");
  t.check(await M.until((s) => !s.m1 && !s.bd), "both off");
});

test("the machine changed by another browser shows here", async (t) => {
  await M.call("control", { take: true, force: true });
  await M.mdi("G0 X77 Y66 Z12");
  await M.call("spindle", { dir: 1, speed: 15000 });
  await M.call("coolant", { k: "flood", on: true });
  await M.call("override", { k: "feed", v: 0.6 });
  t.check(await droIs(t, "x", "77.000") && await droIs(t, "y", "66.000") && await droIs(t, "z", "12.000"), "the DRO");
  t.check(await t.until(() => document.getElementById("sp-cmd").textContent.startsWith("S15000 · CW")), "the spindle");
  t.check(await t.until(() => document.getElementById("cl-flood").getAttribute("aria-pressed") === "true"), "flood");
  t.check(await t.until(() => document.getElementById("o-feed-v").textContent === "60 %"), "the feed override slider");
  t.check((await t.text("#b-control")).includes("WATCHING"), "this browser watches");
  await M.call("spindle", { dir: 0 });
});

test("no script errors on any page", async (t) => {
  for (const p of ["main", "file", "wiz", "off", "tool", "probe", "dyn", "stat", "set"]) { await t.nav(p); await t.sleep(400); }
  t.check(t.errors.length === 0, "none");
});

// last: millweb goes away
test("millweb stops: OFFLINE, and the page says so", async (t) => {
  const pids = fs.readdirSync("/proc").filter((p) => /^\d+$/.test(p)).filter((p) => {
    try { return fs.readFileSync(`/proc/${p}/cmdline`, "utf8").split("\0").some((c) => c.endsWith("webui/mill/millweb")); } catch (e) { return false; }
  });
  t.check(pids.length === 1, "one millweb");
  pids.forEach((p) => process.kill(+p, "SIGTERM"));
  t.check(await t.until(() => document.getElementById("ch-link").textContent.includes("OFFLINE"), null, 10000), "chip: OFFLINE");
  t.check(await t.until(() => events.some((e) => e.text.includes("Connection to the machine lost"))), "event: connection lost");
}, { allowErrors: true });

(async () => {
  M = await Client.open(PORT);
  const rc = await run({
    viewport: { width: 1600, height: 1000 },
    open: async (t, c) => {
      if (!M.closed) await M.baseline();
      await t.page.goto(BASE + "#main");
      await t.page.waitForFunction(() => document.getElementById("ch-link").textContent.includes("LIVE") && typeof st !== "undefined" && st.on, null, { timeout: 15000 });
    },
  });
  M.close();
  process.exit(rc);
})().catch((e) => { console.log("FAIL " + e); process.exit(1); });
