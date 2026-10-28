// The console on real LinuxCNC (test/sim with millweb), through the page's
// own controls. Each case starts from the same machine (wsclient.baseline:
// on, homed, idle, G54 at its test values, nobody in control).
//   python3 test/harness.py -- node test/gui/live_suite.js [-k WORD]... [--shots DIR]
// (the harness starts LinuxCNC and sets MILLWEB_PORT and MILLWEB_DIR), or from
// another computer against a board running `harness.py --serve`:
//   MILLWEB_REMOTE=board:8080 node test/gui/live_suite.js
// Remote, the machine's files are checked through the protocol instead of on
// disk, and the case that stops millweb is skipped.
const fs = require("fs");
const path = require("path");
const { test, run } = require("./runner");
const { Client } = require("./wsclient");

const REMOTE = process.env.MILLWEB_REMOTE || "";
const HOST = REMOTE ? REMOTE.replace(/:\d+$/, "") : "127.0.0.1";
const PORT = REMOTE ? +(REMOTE.match(/:(\d+)$/) || [0, 8080])[1] : +process.env.MILLWEB_PORT;
const DIR = REMOTE ? null : process.env.MILLWEB_DIR;
if (!PORT) { console.error("MILLWEB_PORT: run through test/harness.py (or MILLWEB_REMOTE=host:port)"); process.exit(2); }
const BASE = `http://${HOST}:${PORT}/`;
// the machine's files: on disk beside us, or through the protocol from another computer
const onMachine = {
  async files() { return (await M.call("files")); },
  async exists(rel) { if (DIR) return fs.existsSync(path.join(DIR, "nc_files", rel)); const r = await this.files(); return r.files.some((f) => f.name === rel) || r.dirs.includes(rel); },
  async text(rel) {
    if (DIR) return fs.readFileSync(path.join(DIR, "nc_files", rel), "utf8");
    const r = await M.call("program_lines", { name: rel, start: 1, count: 5000 }); return r.ok ? r.lines.join("\n") + "\n" : "";
  },
  async size(rel) { if (DIR) return fs.statSync(path.join(DIR, "nc_files", rel)).size; const f = (await this.files()).files.find((x) => x.name === rel); return f ? f.size : -1; },
  async toolTable() {  // tool.tbl, or the table as millweb writes it
    if (DIR) return fs.readFileSync(path.join(DIR, "tool.tbl"), "utf8");
    const z = (v) => (v >= 0 ? "+" : "-") + Math.abs(v).toFixed(4);
    return M.last.tools.tools.map((t) => `T${t.t} P${t.p} D${t.d.toFixed(4)} Z${z(t.z)} ;${t.c}`).join("\n") + "\n";
  },
  async setup() { return DIR ? JSON.parse(fs.readFileSync(path.join(DIR, "millweb-setup.json"), "utf8")).objects : M.last.setup.objects; },
};
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
  t.check(await t.until(() => !document.getElementById("boot")), "the loading screen gone");
  t.check((await t.text("#ch-link")).includes("LIVE"), "chip: LIVE");
  t.check((await t.text(".brand b")) === "PORTAL-M SIM", "the machine's name");
  t.check((await t.text(".brand span")).startsWith("XYYZ · LinuxCNC · millweb"), "coordinates, millweb version");
  t.check((await t.text("#f-src")) === `live · ${HOST}:${PORT}`, "footer: live");
  t.check((await t.text("#b-control")).includes("TAKE"), "nobody has control: TAKE CONTROL");
  t.check((await t.text("#ch-on")).includes("MACHINE ON") && (await t.text("#ch-home")) === "HOMED", "the machine's state");
  t.check(await t.ev(() => TRAVEL.z[0] === -215 && TRAVEL.x[1] === 1000), "travel from the INI");
  t.check(await t.ev(() => document.getElementById("o-feed").max === "200" && document.getElementById("o-spin").max === "150" && document.getElementById("o-spin").min === "50"), "override sliders: the INI's limits");
  t.check(await t.ev(() => events.some((e) => e.text.includes("Browser connected"))), "events from the server");
  await t.nav("stat");
  t.check(await t.until(() => document.querySelectorAll("#pins tbody tr").length === 5 && document.getElementById("lpin-0").textContent !== "—"), "the INI's five pins, with values");
  await t.nav("dyn");
  const nd = (M.hello.machine.drives || []).length;  // the test machine has none; with harness.py --drive, X
  t.check(nd ? await t.until((n) => document.querySelectorAll("#drv-t tbody tr").length === n, nd) : (await t.text("#drv-t")).includes("No drives configured"), nd ? "the configured drives" : "no drives configured");
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
  const tbl = () => onMachine.toolTable();
  await t.click("#tl-add");
  t.check(await t.until(() => st.tools.some((x) => x.t === 13)), "T13 added");
  await t.sleep(1200);
  t.check(/^T13 P13 D8\.0000 Z\+50\.0000 ;new tool$/m.test(await tbl()), "...in tool.tbl");
  await t.page.fill('#tool-t input[aria-label="T13 diameter"]', "9");
  await t.page.press('#tool-t input[aria-label="T13 diameter"]', "Tab");
  await t.sleep(1200);
  t.check(/^T13 P13 D9\.0000/m.test(await tbl()), "edited diameter in tool.tbl");
  await t.click('#tool-t tr[data-t="13"] td:first-child');
  await t.click("#tl-del");
  await t.sleep(1200);
  t.check(!/^T13/m.test(await tbl()), "deleted from tool.tbl");
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
  await t.ev(() => {  // the progress bar while it loads (seen by an observer: it may come and go between polls)
    window.busySeen = [];
    const b = document.getElementById("busy");
    new MutationObserver(() => { if (!b.hidden) window.busySeen.push(document.getElementById("busy-t").textContent); }).observe(b, { attributes: true, childList: true, subtree: true });
  });
  await t.click(`#files .file[data-i="${i}"] .btn`);
  t.check(await t.until(() => document.getElementById("h-prog").textContent === "bracket-op1.ngc"), "loaded");
  t.check(await t.ev(() => window.busySeen.some((x) => /^Loading bracket-op1\.ngc/.test(x))), "a progress bar while it loads: " + (await t.ev(() => window.busySeen.slice(0, 3).join(" | "))));
  t.check(await t.until(() => document.getElementById("busy").hidden), "...gone once it is drawn");
  t.check(await t.until(() => document.getElementById("pg-name").textContent === "bracket-op1.ngc"), "the program view's title: its name");
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
  t.check(await t.until(() => !document.getElementById("sp-stats").hidden && rs && rs.velMax > 0 && rs.lines.size > 1), "STATS while it runs: the machine's velocity and lines");
  await t.click("#b-stop");
  t.check(await M.until((s) => s.interp === "idle") && await t.until(() => document.getElementById("ch-run").textContent === "IDLE" || document.getElementById("ch-run").textContent === "DONE"), "Stop");
  await t.click("#gl-16");  // G0 Z5 after the pocket: a line it can start from
  await t.ev(() => { window.evMark = events.length; });  // events are newest first
  await t.click("#b-runfrom");
  t.check(await logHas(t, "Cycle start from line 16"), "Run from N0016");
  t.check(await M.until((s) => s.run === "done", 90000), "...runs to the end");
  t.check(await logHas(t, "Program finished") && !(await t.ev(() => events.slice(0, events.length - window.evMark).some((e) => e.lvl === "bad"))), "finished without an error");
  t.check(await t.until(() => document.getElementById("ch-run").textContent === "DONE"), "DONE");
});

// a board keeps its files between runs (the harness starts each run afresh): a case's own leftovers go first
async function cleanUp(names, dirs = []) {
  await M.call("control", { take: true, force: true });
  for (const n of names) await M.call("program_delete", { name: n });
  for (const d of dirs) await M.call("folder_delete", { name: d });
  await M.call("control", { take: false });
}

test("file page: upload, folders, rename and move, delete", async (t) => {
  await cleanUp(["uploaded one.ngc", "moved.ngc", "jobs/dropped.ngc", "jobs/renamed.ngc", ...Array.from({ length: 14 }, (_, i) => `many/p${String(i).padStart(2, "0")}.ngc`)], ["jobs", "many"]);
  await take(t);
  await t.nav("file");
  const rows = () => t.ev(() => [...document.querySelectorAll("#files .file b")].map((b) => b.textContent));
  const dlg = async (value, ok = true) => {  // answer the console's dialog
    await t.page.waitForSelector("#dlg:not([hidden])");
    if (value !== null) await t.page.fill("#dlg-in", value);
    await t.click(ok ? '#dlg-form [type="submit"]' : "#dlg-cancel");
  };
  t.check(!(await t.hidden("#fl-tools")) && (await t.text("#fl-crumbs")) === "programs", "the tools, at the program folder");
  // a long folder: every row stays clickable (the list scrolls; it once ran under the next panel)
  await M.call("control", { take: true, force: true });
  await M.call("folder_create", { name: "many" });
  for (let i = 0; i < 14; i++) await M.call("program_save", { name: `many/p${String(i).padStart(2, "0")}.ngc`, text: "M2" });
  await M.call("control", { take: false });
  await take(t);
  await t.click('#files .file[data-dir="many"] [data-open]');
  await t.until(() => document.querySelectorAll("#files .file").length === 15);
  const covered = await t.ev(() => [...document.querySelectorAll("#files .file")].filter((r) => {
    r.scrollIntoView({ block: "center" }); const b = r.getBoundingClientRect(), e = document.elementFromPoint(b.x + b.width / 2, b.y + b.height / 2);
    return !(e && r.contains(e)); }).length);
  t.check(covered === 0, `a folder of 14: every row clickable (${covered} covered)`);
  await t.click('#files .file[data-up]');
  await M.call("control", { take: true, force: true });
  for (let i = 0; i < 14; i++) await M.call("program_delete", { name: `many/p${String(i).padStart(2, "0")}.ngc` });
  await M.call("folder_delete", { name: "many" });
  await M.call("control", { take: false });
  await take(t);
  // upload with the button
  await t.page.setInputFiles("#fl-input", [{ name: "uploaded one.ngc", mimeType: "text/plain", buffer: Buffer.from("G0 X5 Y5 Z5\nG1 X15 F600\nM2\n") }]);
  t.check(await t.until(() => [...document.querySelectorAll("#files .file b")].some((b) => b.textContent === "uploaded one.ngc")), "uploaded: listed");
  t.check((await onMachine.text("uploaded one.ngc")).startsWith("G0 X5 Y5 Z5"), "on the machine");
  t.check(await t.until(() => document.getElementById("fl-name").textContent === "uploaded one.ngc"), "selected, with its preflight");
  // the same name again: asks first; Cancel keeps the old one
  await t.page.setInputFiles("#fl-input", [{ name: "uploaded one.ngc", mimeType: "text/plain", buffer: Buffer.from("M2\n") }]);
  await dlg(null, false);
  await t.sleep(500);
  t.check((await onMachine.text("uploaded one.ngc")).startsWith("G0 X5"), "replace? Cancel: unchanged");
  // too large, and not G-code
  await t.page.setInputFiles("#fl-input", [{ name: "huge.ngc", mimeType: "text/plain", buffer: Buffer.alloc(7 * 1024 * 1024, 65) }]);
  t.check(await t.msgHas("uploads stop at 6 MB"), "7 MB: refused, says why");
  await t.page.setInputFiles("#fl-input", [{ name: "notes.txt", mimeType: "text/plain", buffer: Buffer.from("hello") }]);
  t.check(await t.msgHas("not a G-code program"), "a .txt: refused");
  // a new folder, opened, a file dropped into it
  await t.click("#fl-mkdir");
  await dlg("jobs");
  t.check(await t.until(() => [...document.querySelectorAll("#files .file.dir b")].some((b) => b.textContent === "jobs/")), "new folder jobs/");
  await t.click('#files .file[data-dir="jobs"] [data-open]');
  t.check(await t.until(() => document.getElementById("fl-crumbs").textContent === "programs/jobs"), "opened: programs/jobs");
  await t.page.evaluate(() => {
    const dt = new DataTransfer(); dt.items.add(new File(["G0 X1\nM2\n"], "dropped.ngc", { type: "text/plain" }));
    document.getElementById("files").dispatchEvent(new DragEvent("drop", { dataTransfer: dt, bubbles: true, cancelable: true }));
  });
  t.check(await t.until(() => [...document.querySelectorAll("#files .file b")].some((b) => b.textContent === "dropped.ngc")), "dropped into jobs/");
  t.check(await onMachine.exists("jobs/dropped.ngc"), "...on the machine");
  // rename (it keeps its extension), then move it up
  await t.click('#files .file[data-i] b');
  await t.click("#fl-rename");
  await dlg("jobs/renamed");
  t.check(await t.until(() => [...document.querySelectorAll("#files .file b")].some((b) => b.textContent === "renamed.ngc")), "renamed: renamed.ngc (the extension kept)");
  await t.click("#fl-rename");
  await dlg("moved.ngc");
  t.check(await t.until(() => document.getElementById("fl-crumbs").textContent === "programs" && [...document.querySelectorAll("#files .file b")].some((b) => b.textContent === "moved.ngc")), "moved to the program folder; the page follows");
  t.check(await onMachine.exists("moved.ngc") && !(await onMachine.exists("jobs/renamed.ngc")), "...on the machine");
  // delete: asks; the empty folder too; the loaded program can't be
  await t.click("#fl-delete");
  await dlg(null);
  t.check(await t.until(() => ![...document.querySelectorAll("#files .file b")].some((b) => b.textContent === "moved.ngc")) && !(await onMachine.exists("moved.ngc")), "deleted after asking");
  await t.click('#files .file[data-dir="jobs"] b');
  t.check(!(await t.page.isDisabled("#fl-delete")), "a folder selected");
  await t.click("#fl-delete");
  await dlg(null);
  t.check(await t.until(() => !document.querySelector('#files .file[data-dir="jobs"]')) && !(await onMachine.exists("jobs")), "the empty folder deleted");
  await t.click('#files .file[data-i] b');
  const loaded = await t.ev(() => prog.name);
  await t.ev((n) => { const i = [...document.querySelectorAll("#files .file[data-i]")].findIndex((b) => b.querySelector("b").textContent === n); if (i >= 0) document.querySelectorAll("#files .file[data-i]")[i].click(); }, loaded);
  await t.click("#fl-delete");
  await dlg(null);
  t.check(await t.msgHas("loaded"), `the loaded program (${loaded}) can't be deleted`);
  await cleanUp(["uploaded one.ngc"]);
});

// a binary STL of a box: 12 triangles
function stlBox(w, d, h) {
  const v = [[0, 0, 0], [w, 0, 0], [w, d, 0], [0, d, 0], [0, 0, h], [w, 0, h], [w, d, h], [0, d, h]];
  const f = [[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]];
  const b = Buffer.alloc(84 + 50 * f.length); b.writeUInt32LE(f.length, 80);
  f.forEach((t, i) => t.forEach((k, j) => v[k].forEach((c, a) => b.writeFloatLE(c, 84 + i * 50 + 12 + j * 12 + a * 4))));
  return b;
}

test("3D models: upload, add to the setup, select and move in the view, saved on the machine", async (t) => {
  await cleanUp(["part blank.stl"]);
  await take(t);
  await t.nav("file");
  const setupFile = () => onMachine.setup();
  await t.page.setInputFiles("#fl-input", [{ name: "part blank.stl", mimeType: "model/stl", buffer: stlBox(60, 40, 20) }]);
  t.check(await t.until(() => [...document.querySelectorAll("#files .file.model b")].some((b) => b.textContent === "part blank.stl")), "uploaded: listed as a 3D model");
  t.check(await onMachine.size("part blank.stl") === 84 + 50 * 12, "on the machine, the whole file");
  t.check(await t.until(() => /60\.0 × 40\.0 × 20\.0 mm/.test(document.getElementById("fl-cards").textContent) && /Triangles\s*12/.test(document.getElementById("fl-cards").textContent)), "its size and triangles");
  await t.click('#fl-cards [data-addsel="workpiece"]');
  t.check(await t.until(() => st.page === "main" && setup.objects.some((o) => o.src === "model:part blank.stl") && setup.sel && objOf(setup.sel).src === "model:part blank.stl", null, 10000), "added as a workpiece: on MAIN, selected");
  t.check(await t.until(() => { const n = [...setup.nodes.values()].find((x) => x.node.userData.objId === setup.sel); if (!n) return false;
    const b = new THREE.Box3().setFromObject(n.node), o = off(); return Math.abs(b.max.z - (o.z + st.g92.z + ZTOP)) < 1e-3 && Math.abs(b.min.x - o.x - st.g92.x) < 1e-3; }, null, 10000), "placed: its top at work Z0, its corner at X0 Y0");
  t.check(await t.until(() => true, null, 100) && await (async () => { for (let k = 0; k < 40; k++) { try { if ((await setupFile()).some((o) => o.src === "model:part blank.stl")) return true; } catch (e) { /* not yet */ } await t.sleep(100); } return false; })(), "the machine's setup has it");
  const id = await t.ev(() => setup.sel);
  await t.page.fill("#ob-y", "12.5"); await t.page.press("#ob-y", "Tab");
  t.check(await (async () => { for (let k = 0; k < 40; k++) { const o = (await setupFile()).find((o) => o.id === id); if (o && o.pos[1] === 12.5) return true; await t.sleep(100); } return false; })(), "Y 12.5 in the panel: saved on the machine");
  // a second browser sees the same setup
  const p2 = await t.ctx.browser().newPage();
  try {
    await p2.goto(BASE + "#main");
    t.check(await p2.waitForFunction((id) => typeof setup !== "undefined" && setup.nodes.has(id) && objOf(id).pos[1] === 12.5, id, { timeout: 15000 }).then(() => true, () => false), "another browser: the model, where it was put");
  } finally { await p2.close(); }
  // used in the setup: the file can't go
  await t.nav("file");
  await t.click('#files .file.model b');
  await t.click("#fl-delete"); await t.click('#dlg-form [type="submit"]');
  t.check(await t.msgHas("3D setup"), "deleting it while the setup uses it: refused");
  // out of the setup, then deleted
  await t.nav("main");
  await t.ev((id) => selectObject(id), id);
  await t.click("#ob-del"); await t.click('#dlg-form [type="submit"]');
  t.check(await (async () => { for (let k = 0; k < 40; k++) { if (!(await setupFile()).some((o) => o.id === id)) return true; await t.sleep(100); } return false; })(), "removed from the setup on the machine");
  await t.nav("file");
  await t.click('#files .file.model b');
  await t.click("#fl-delete"); await t.click('#dlg-form [type="submit"]');
  t.check(await t.until(() => !document.querySelector("#files .file.model")) && !(await onMachine.exists("part blank.stl")), "then deleted");
});

test("wizard: its program is saved on the machine and loaded", async (t) => {
  await cleanUp(["BOLTCIRCLE_5x_R28.ngc"]);
  await take(t);
  await t.nav("wiz");
  await t.page.fill("#bc-n", "5");
  await t.click("#bc-go");
  t.check(await t.until(() => document.getElementById("h-prog").textContent === "BOLTCIRCLE_5x_R28.ngc"), "loaded on the machine");
  t.check(await onMachine.exists("BOLTCIRCLE_5x_R28.ngc"), "saved in the program folder");
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
  // the machine first, then the page: its own connection may hear it a moment later
  t.check(await M.until((s) => s.flood) && await t.until(() => document.getElementById("cl-flood").getAttribute("aria-pressed") === "true"), "flood on");
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

test("Debug page, HAL tab: the machine's HAL, links, a watch that follows a jog", async (t) => {
  await t.ev(() => localStorage.removeItem("pmc.halwatch"));
  await take(t);
  await t.nav("debug");
  t.check(await t.until(() => /^\d{3,} pins · \d+ signals · \d+ parameters$/.test(document.getElementById("hal-count").textContent), null, 15000), `listed: ${await t.text("#hal-count")}`);
  await t.page.fill("#hal-q", "joint.0.motor-pos");
  t.check(await t.until(() => document.querySelectorAll("#hal-list .hal-row").length === 2), "filter: motor-pos-cmd and -fb");
  await t.click('.hal-row[data-name="joint.0.motor-pos-fb"]');
  t.check(await t.until(() => /Signal\s*j0/.test(document.getElementById("hal-detail").textContent) && /Its driver\s*joint\.0\.motor-pos-cmd/.test(document.getElementById("hal-detail").textContent)), "details: its signal j0, driven by motor-pos-cmd");
  await t.click('#hal-detail [data-go="j0"]');
  t.check(await t.until(() => hal.sel.kind === "sig" && /Read by.*joint\.0\.motor-pos-fb/.test(document.getElementById("hal-detail").textContent)), "the signal: driver and readers, and it's watched");
  const x0 = await t.ev(() => hal.last["joint.0.motor-pos-fb"]);
  await t.nav("main"); await t.click("#st-jog"); await t.click('#jog-inc button[data-v="10"]');
  await t.page.dispatchEvent('#jogpad button[data-j="x+"]', "pointerdown");
  await t.nav("debug");
  t.check(await t.until((x0) => Math.abs(hal.last["joint.0.motor-pos-fb"] - x0 - 10) < 1e-3, x0), "jog X+10: the watched pin follows");
  t.check(await t.until(() => hal.hist.get("joint.0.motor-pos-fb").length > 5), "and its trend fills");
  t.check(!(await t.ev(() => !!document.querySelector("#hal-watch [data-set]"))), "no Set: HAL_WRITE is off");
  await t.click('#hal-kind button[data-k="param"]'); await t.page.fill("#hal-q", "tmax");
  t.check(await t.until(() => document.querySelectorAll("#hal-list .hal-row").length > 2), "parameters: the thread timings");
  await t.click("#hal-clear");
});

test("Debug page, scope: a jog captured on a trigger; a view shared with another browser", async (t) => {
  await M.call("views_save", { views: [] });
  await take(t);
  await t.nav("debug");
  await t.click('#dbg-tab button[data-t="scope"]');
  await t.ev(() => { scope.ch = []; scopeChanged(); });
  await t.page.fill("#sc-add", "joint.0.motor-pos-fb"); await t.click("#sc-add-btn");
  await t.page.fill("#sc-add", "motion.in-position"); await t.click("#sc-add-btn");
  const x0 = (await M.call("hal_values", { names: ["joint.0.motor-pos-fb"] })).values["joint.0.motor-pos-fb"];
  await t.page.selectOption("#sc-tdiv", "0.1");
  await t.click('#sc-mode button[data-m="single"]');
  await t.page.fill("#sc-level", String(x0 + 5)); await t.page.press("#sc-level", "Tab");
  await t.click("#sc-run");
  t.check(await t.until(() => scope.state === "WAITING FOR TRIGGER" && scope.total > 500, null, 5000), "single: waiting, samples coming in");
  t.check(await t.until(() => /^\d+ samples\/s/.test(document.getElementById("sc-info").textContent) && Math.abs(parseInt(document.getElementById("sc-info").textContent) - 1000) < 80), `the sampler's rate: ${await t.text("#sc-info")}`);
  await t.nav("main"); await t.click("#st-jog"); await t.click('#jog-inc button[data-v="10"]');
  await t.page.dispatchEvent('#jogpad button[data-j="x+"]', "pointerdown");
  await M.idle();
  await t.nav("debug");
  t.check(await t.until(() => !scope.running && scope.disp && scope.disp.trig === 200, null, 8000), "one capture, stopped");
  t.check(await t.ev((x0) => { const a = scope.disp.data[0]; return a[0] < x0 + 5 && a[199] < x0 + 5 && a[200] >= x0 + 5 && a[999] > a[200]; }, x0), "X through the level at the trigger, moving on");
  t.check(await t.ev(() => scope.disp.data[1].every((v) => v === 0 || v === 1)), "a bit channel: 0 and 1");
  await t.click("#dbg-save");
  await t.page.fill("#dlg-in", "x jog");
  await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => views.length === 1 && views[0].scope.ch.length === 2), "saved on the machine");
  const p2 = await t.ctx.browser().newPage({ viewport: { width: 1280, height: 900 } });
  try {
    await p2.goto(BASE + "#debug");
    t.check(await p2.waitForFunction(() => typeof views !== "undefined" && document.querySelectorAll("#dbg-views .view").length === 1, null, { timeout: 15000 }).then(() => true, () => false), "another browser has the view");
    await p2.keyboard.press("Alt+1");
    t.check(await p2.waitForFunction(() => scope.ch.length === 2 && scope.ch[0].name === "joint.0.motor-pos-fb" && scope.mode === "single", null, { timeout: 5000 }).then(() => true, () => false), "Alt+1 there: the same scope set-up");
  } finally { await p2.close(); }
  await t.click('#dbg-views [data-vdel="0"]'); await t.click('#dlg-form [type=submit]');
  t.check(await t.until(() => views.length === 0), "deleted");
  t.check(t.errors.length === 0, "no script errors");
});

test("Debug page, drives: the simulated drive on X, its card through a move and a stop, its console", async (t) => {
  if (!(M.hello.machine.drive_links || []).length) return t.skip("no drive on this machine (make test-gui-drive runs it with one)");
  await take(t);
  await t.nav("debug");
  await t.click('#dbg-tab button[data-t="drives"]');
  const card = '.drv-card[data-n="X"]';
  t.check(await t.until((c) => document.querySelector(c) && /ENABLED/.test(document.querySelector(c + ' [data-k="state"]').textContent) && /LINK OK/.test(document.querySelector(c + ' [data-k="link"]').textContent), card, 15000), "X: ENABLED, link ok");
  t.check(await t.until((c) => /STMBL/.test(document.querySelector(c + " .fw").textContent) && /^\d{3} V$/.test(document.querySelector(c + ' [data-k="dc"]').textContent), card, 10000), `firmware and bus: ${await t.text(card + " .fw")}, ${await t.text(card + ' [data-k="dc"]')}`);
  t.check((await t.text("#dc-mode")) === "READING", "the console: reading only (DRIVE_WRITE off)");
  await t.page.fill("#dc-in", "fault0.dc_volt"); await t.page.press("#dc-in", "Enter");
  t.check(await t.until(() => /fault0\.dc_volt <= hv0\.dc_volt = \d+/.test(document.getElementById("dc-out").textContent), null, 8000), "a pin read over the smart-serial console");
  await t.page.fill("#dc-in", "conf0.max_vel = 1"); await t.page.press("#dc-in", "Enter");
  t.check(await t.until(() => /! only reading/.test(document.getElementById("dc-out").textContent)), "a write: refused, said in the console");
  // firmware versions, and the config panel: a backup is made, a patch can't be written (DRIVE_WRITE off)
  t.check(await t.until((c) => /firmware v1 · HV v1 · config unversioned/.test((document.querySelector(c + ' [data-k="ver"]') || {}).textContent || ""), card, 15000), "the card: firmware v1, HV v1, config unversioned");
  await t.click(card + " [data-cfgp]");
  await t.click('#drv-cfg [data-dcf="backup"]');
  t.check(await t.until(() => document.querySelectorAll("#drv-cfg [data-restore]").length >= 1, null, 20000), "Back up now: the drive's config kept on the machine");
  await t.click('#drv-cfg [data-dcf="plan"]');
  t.check(await t.until(() => /Patch v0 → v1/.test(document.getElementById("drv-cfg").textContent), null, 20000), "the patch v0 → v1, read from the drive");
  await t.click('#drv-cfg [data-dcf="patch"]');
  t.check(await t.until(() => /DRIVE_WRITE/.test(document.getElementById("drv-cfg").textContent), null, 10000), "writing it: refused, DRIVE_WRITE is off");
  await t.click("#dcf-close");
  // a stop from the machine: the card follows, the drive's message in the log
  await t.click("#b-power");
  t.check(await t.until((c) => /DISABLED/.test(document.querySelector(c + ' [data-k="state"]').textContent), card, 10000), "machine off: DISABLED");
  t.check(await logHas(t, "Drive X: Disabled", 8000), "the drive's own message in the event log");
  // another browser sees the same console
  const p2 = await t.ctx.browser().newPage({ viewport: { width: 1280, height: 900 } });
  try {
    await p2.goto(BASE + "#debug");
    await p2.waitForFunction(() => typeof dbgTab !== "undefined" && st.page === "debug", null, { timeout: 15000 });
    await p2.click('#dbg-tab button[data-t="drives"]');
    t.check(await p2.waitForFunction(() => /fault0\.dc_volt <= hv0/.test(document.getElementById("dc-out").textContent), null, { timeout: 10000 }).then(() => true, () => false), "another browser: the console so far");
  } finally { await p2.close(); }
  t.check(t.errors.length === 0, "no script errors");
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
  for (const p of ["main", "file", "wiz", "off", "tool", "probe", "dyn", "stat", "debug", "config", "set"]) { await t.nav(p); await t.sleep(400); }
  t.check(t.errors.length === 0, "none");
});

// last: millweb goes away
test("millweb stops: OFFLINE, and the page says so", async (t) => {
  if (REMOTE) return t.skip("remote: this would stop the board's millweb (and LinuxCNC with it)");
  const pids = fs.readdirSync("/proc").filter((p) => /^\d+$/.test(p)).filter((p) => {
    try { return fs.readFileSync(`/proc/${p}/cmdline`, "utf8").split("\0").some((c) => c.endsWith("webui/mill/millweb")); } catch (e) { return false; }
  });
  t.check(pids.length === 1, "one millweb");
  pids.forEach((p) => process.kill(+p, "SIGTERM"));
  t.check(await t.until(() => document.getElementById("ch-link").textContent.includes("OFFLINE"), null, 10000), "chip: OFFLINE");
  t.check(await t.until(() => events.some((e) => e.text.includes("Connection to the machine lost"))), "event: connection lost");
}, { allowErrors: true });

(async () => {
  M = await Client.open(PORT, HOST);
  // these tests take control, home, jog and run programs: only on the test machine (test/sim)
  if (M.hello.machine.name !== "PORTAL-M sim") {
    console.log(`refusing: ${HOST}:${PORT} is "${M.hello.machine.name}", not the test machine (test/sim, "PORTAL-M sim"); these tests move it`);
    process.exit(2);
  }
  const rc = await run({
    viewport: { width: 1600, height: 1000 },
    warmup: async (page) => {
      await page.goto(BASE + "#main");
      await page.waitForFunction(() => document.getElementById("ch-link").textContent.includes("LIVE"), null, { timeout: 90000 });
    },
    open: async (t, c) => {
      if (!M.closed) await M.baseline();
      await t.page.goto(BASE + "#main");
      await t.page.waitForFunction(() => document.getElementById("ch-link").textContent.includes("LIVE") && typeof st !== "undefined" && st.on, null, { timeout: 15000 });
    },
  });
  M.close();
  process.exit(rc);
})().catch((e) => { console.log("FAIL " + e); process.exit(1); });
