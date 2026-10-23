// The Configure page on a machine run by millweb-supervisor (test/machine's
// machine.json): forms, Check, Save & restart, a configuration LinuxCNC
// refuses (the page served by the supervisor), Stop and Start.
//   python3 test/harness.py --supervised -- node test/gui/config_suite.js [-k WORD] [--shots DIR]
const { test, run } = require("./runner");
const { Client } = require("./wsclient");

const PORT = +process.env.MILLWEB_PORT;
if (!PORT) { console.error("MILLWEB_PORT: run through test/harness.py --supervised"); process.exit(2); }
const BASE = `http://127.0.0.1:${PORT}/`;
let ORIGINAL = null;  // test/machine's machine.json, put back after each case

// the page reloads when LinuxCNC goes down and comes back: wait across that
async function pageUntil(t, fn, arg, ms = 60000) {
  const end = Date.now() + ms;
  while (Date.now() < end) {
    try { if (await t.page.evaluate(fn, arg)) return true; } catch (e) { /* reloading */ }
    await t.sleep(200);
  }
  return false;
}
// in the page: live again after a start later than the one marked (window.__starts)
const backLive = () => typeof LIVE !== "undefined" && LIVE && !document.body.classList.contains("sup-mode") && typeof cfg !== "undefined" && cfg.loaded && cfg.status && cfg.status.state === "running" && (cfg.status.starts || 0) > (window.__starts || 0);
const mark = () => { window.__starts = cfg.status ? cfg.status.starts || 0 : 0; };
const isLive = () => typeof LIVE !== "undefined" && LIVE && !document.body.classList.contains("sup-mode") && typeof cfg !== "undefined" && cfg.loaded && cfg.status && cfg.status.state === "running";
const isSup = (state) => typeof LIVE !== "undefined" && LIVE && document.body.classList.contains("sup-mode") && typeof cfg !== "undefined" && cfg.status && cfg.status.state === state;
async function take(t) {
  await t.click("#b-control");
  return t.until(() => /IN/.test(document.getElementById("b-control").textContent), null, 5000);
}
async function protocol() {  // a client for the supervisor or millweb, whichever answers
  for (let i = 0; i < 100; i++) { try { return await Client.open(PORT); } catch (e) { await new Promise((r) => setTimeout(r, 300)); } }
  throw new Error("nothing answers on " + PORT);
}
async function restore() {  // test/machine's machine.json, LinuxCNC running with it
  const c = await protocol();
  await c.call("control", { take: true, force: true });
  const r = await c.call("config_get");
  if (r.text !== ORIGINAL || r.custom || (c.hello.supervisor && c.hello.supervisor.state !== "running")) {
    await c.call("config_save", { text: ORIGINAL, custom: "", restart: true, start: true });
  }
  c.ws.close();
  for (let i = 0; i < 200; i++) {  // until millweb (not the supervisor) answers
    await new Promise((r) => setTimeout(r, 300));
    try { const d = await Client.open(PORT); const ok = !!d.hello.machine; d.ws.close(); if (ok) return; } catch (e) { /* down */ }
  }
  throw new Error("LinuxCNC didn't come back");
}

test("Configure: the running machine's machine.json in the forms; Check makes its INI and HAL", async (t) => {
  await t.nav("config");
  t.check(await t.until(() => cfg.loaded && document.getElementById("cf-state").textContent === "RUNNING", null, 10000), "LinuxCNC: RUNNING");
  t.check(await t.ev(() => document.querySelector('#cf-body [data-path="name"]').value === "PORTAL-M sim"), "the machine's name in the form");
  t.check((await t.text("#cf-dirty")) === "No changes" && await t.hidden("#cf-pending"), "nothing changed, what runs is what is saved");
  await t.click('#cf-tabs button[data-t="axes"]');
  t.check(await t.ev(() => document.querySelectorAll("#cf-body .cf-joint").length === 4), "four joint cards (X Y Y Z)");
  await t.click("#cf-check");
  t.check(await t.msgHas("no problems"), "Check: no problems");
  await t.click('#cf-tabs button[data-t="gen"]');
  t.check(/\[AXIS_X\][\s\S]*MAX_VELOCITY = 500/.test(await t.text("#cf-body")) && /net probe probe-and\.out => motion\.probe-input/.test(await t.text("#cf-body")), "Generated: the INI and the HAL");
  await t.click('#cf-tabs button[data-t="log"]');
  t.check(await t.until(() => /LINUXCNC/.test(document.getElementById("cf-log").textContent)), "Log: LinuxCNC's output");
  // a mistake shows where it is
  await take(t);
  await t.click('#cf-tabs button[data-t="axes"]');
  await t.page.fill('#cf-body [data-path="axes.X.min"]', "2000"); await t.page.press('#cf-body [data-path="axes.X.min"]', "Tab");
  t.check((await t.text("#cf-dirty")) === "Changed: not saved", "changed: not saved");
  await t.click("#cf-check");
  t.check(await t.until(() => !document.getElementById("cf-problems").hidden && /axes\.X/.test(document.getElementById("cf-problems").textContent)), "Problems: axes.X");
  t.check(await t.ev(() => document.querySelector('#cf-body [data-f="axes.X.min"]').classList.contains("bad")), "the field marked");
  await t.click("#cf-save");
  t.check(await t.until(() => !document.getElementById("cf-problems").hidden) && await t.hidden("#dlg"), "Save with problems: not saved, not asked");
  await t.click("#cf-discard"); await t.click('#dlg-form [type="submit"]');
  t.check(await t.until(() => document.getElementById("cf-dirty").textContent === "No changes"), "Discard");
});

test("Configure: Save & restart; the page follows LinuxCNC down and back, with the new machine", async (t) => {
  await t.nav("config");
  await t.until(() => cfg.loaded);
  await take(t);
  await t.click('#cf-tabs button[data-t="axes"]');
  await t.page.fill('#cf-body [data-path="axes.X.max_vel"]', "450"); await t.page.press('#cf-body [data-path="axes.X.max_vel"]', "Tab");
  await t.click("#cf-saverestart");
  t.check(await t.until(() => !document.getElementById("dlg").hidden && /Save machine\.json \(axes\.X\.max_vel\)/.test(document.getElementById("dlg-t").textContent) && /restarts/.test(document.getElementById("dlg-n").textContent)), "asks: axes.X.max_vel changed, LinuxCNC restarts");
  await t.ev(mark);
  await t.page.fill("#dlg-in", "X 450");
  await t.page.press("#dlg-in", "Enter");
  const t0 = Date.now();
  t.check(await pageUntil(t, backLive, null, 60000), `back, live, in ${((Date.now() - t0) / 1000).toFixed(1)} s`);
  t.check(await pageUntil(t, () => st.page === "config" && document.querySelector('#cf-body [data-path="axes.X.max_vel"]') && document.querySelector('#cf-body [data-path="axes.X.max_vel"]').value === "450"), "the Configure page again, X at 450");
  const c = await protocol();
  t.check(c.hello.machine.limits.X.vel === 450 * 60, "LinuxCNC runs it: X 450 mm/s");
  const h = await c.call("config_get");
  t.check(h.history[0].note === "X 450", "History: the note");
  c.ws.close();
  await t.click('#cf-tabs button[data-t="hist"]');
  t.check(await t.until(() => /X 450/.test(document.getElementById("cf-body").textContent)), "History tab");
  t.check(await t.ev(() => document.querySelectorAll("#cf-body [data-hshow]").length === 2 && /as it was/.test(document.getElementById("cf-body").textContent)), "two versions: this save, and the file as it was");
  await t.click("#cf-body li:nth-child(2) [data-hshow]");
  t.check(await t.until(() => /max_vel/.test(document.getElementById("cf-body").textContent)), "Show: the change against the editor");
});

test("Configure: a configuration LinuxCNC refuses: the supervisor's page says why, a fix starts it again", async (t) => {
  await t.nav("config");
  await t.until(() => cfg.loaded);
  await take(t);
  await t.click('#cf-tabs button[data-t="custom"]');
  await t.page.fill("#cf-custom", "net bogus nosuch.pin\n");
  await t.click("#cf-saverestart");
  await t.until(() => !document.getElementById("dlg").hidden);
  await t.page.fill("#dlg-in", "broken"); await t.page.press("#dlg-in", "Enter");
  t.check(await pageUntil(t, isSup, "failed", 60000), "the supervisor's page: FAILED");
  t.check(await t.until(() => /nosuch\.pin/.test(document.getElementById("cf-error").textContent)), `the error: ${await t.text("#cf-error")}`);
  t.check(/LINUXCNC FAILED/.test(await t.text(".brand b")), "the header says it");
  t.check(await t.ev(() => getComputedStyle(document.querySelector('#nav button[data-p="main"]')).pointerEvents === "none" && getComputedStyle(document.getElementById("p-main")).display === "none"), "the other pages: not while LinuxCNC is down");
  t.check((await t.text("#cf-saverestart")) === "Save & start" && !(await t.hidden("#cf-start")), "Save & start, Start");
  await t.click('#cf-tabs button[data-t="log"]');
  t.check(await t.until(() => /custom\.hal:1: Pin 'nosuch\.pin' does not exist/.test(document.getElementById("cf-log").textContent)), "Log: LinuxCNC's error");
  await take(t);
  await t.click('#cf-tabs button[data-t="custom"]');
  await t.page.fill("#cf-custom", "");
  await t.click("#cf-saverestart");
  await t.until(() => !document.getElementById("dlg").hidden);  // after the page's Check
  await t.ev(mark);
  await t.page.press("#dlg-in", "Enter");
  t.check(await pageUntil(t, backLive, null, 60000), "fixed: LinuxCNC runs, the page is live again");
});


// an STL box (binary) for the 3D model tab: x0 y0 z0 x1 y1 z1 in mm
function stlBox(x0, y0, z0, x1, y1, z1) {
  const v = []; for (const x of [x0, x1]) for (const y of [y0, y1]) for (const z of [z0, z1]) v.push([x, y, z]);
  const f = [[0, 1, 3], [0, 3, 2], [4, 6, 7], [4, 7, 5], [0, 4, 5], [0, 5, 1], [2, 3, 7], [2, 7, 6], [0, 2, 6], [0, 6, 4], [1, 5, 7], [1, 7, 3]];
  const b = Buffer.alloc(84 + 50 * f.length); b.writeUInt32LE(f.length, 80);
  f.forEach((t, i) => t.forEach((k, j) => v[k].forEach((c, n) => b.writeFloatLE(c, 84 + i * 50 + 12 + j * 12 + n * 4))));
  return b;
}

test("Configure: the machine's 3D model on the machine: uploaded, saved, a part following a move", async (t) => {
  await t.nav("config");
  await t.until(() => cfg.loaded);
  await take(t);
  await t.click('#cf-tabs button[data-t="model"]');
  await t.page.setInputFiles("#cf-mfile", { name: "ram.stl", mimeType: "model/stl", buffer: stlBox(-60, 40, 0, 60, 140, 400) });
  t.check(await t.until(() => cfg.models.some((m) => m.name === "ram.stl")), "uploaded to the machine's models/");
  await t.click("#cf-addpart");
  await t.page.selectOption('[data-path="model.parts.0.moves"]', "XYZ");
  await t.click("#cf-save"); await t.until(() => !document.getElementById("dlg").hidden); await t.page.press("#dlg-in", "Enter");
  t.check(await t.until(() => !dirty()), "saved");
  const c = await protocol();
  await c.call("control", { take: true, force: true });
  for (const [op, a] of [["estop", { on: false }], ["power", { on: true }], ["home", { axis: "all" }]]) await c.call(op, a);
  await new Promise((r) => setTimeout(r, 500));
  await c.call("mdi", { cmd: "G53 G0 X300 Y200" });
  await t.nav("main");
  t.check(await t.until(() => mm3d.nodes[0] && Math.abs(mm3d.nodes[0].holder.position.x - 300) < 0.5 && Math.abs(mm3d.nodes[0].holder.position.y - 200) < 0.5, null, 20000), "the part rides with the machine to X300 Y200");
  const other = await t.ctx.browser().newPage();
  try {
    await other.goto(BASE + "#main");
    t.check(await other.waitForFunction(() => typeof mm3d !== "undefined" && mm3d.nodes.filter(Boolean).length === 1, null, { timeout: 20000 }).then(() => true, () => false), "another browser loads it from the machine");
  } finally { await other.close(); }
  c.ws.close();
});

test("Configure: Stop and Start", async (t) => {
  await t.nav("config");
  await t.until(() => cfg.loaded);
  await take(t);
  await t.click("#cf-stop");
  await t.click('#dlg-form [type="submit"]');
  t.check(await pageUntil(t, isSup, "stopped", 40000), "stopped: the supervisor's page");
  await take(t);
  await t.ev(mark);
  await t.click("#cf-start");
  t.check(await pageUntil(t, backLive, null, 60000), "Start: live again");
});

(async () => {
  const c = await protocol();
  ORIGINAL = (await c.call("config_get")).text;
  c.ws.close();
  const rc = await run({
    warmup: async (page) => { await page.goto(BASE + "#main"); await page.waitForFunction(() => typeof LIVE !== "undefined" && LIVE, null, { timeout: 90000 }); },
    open: async (t) => {
      await restore();
      await t.page.goto(BASE + "#config");
      await t.page.waitForFunction(() => typeof LIVE !== "undefined" && LIVE && typeof cfg !== "undefined" && cfg.loaded, null, { timeout: 30000 });
    },
  });
  await restore().catch(() => {});
  process.exit(rc);
})().catch((e) => { console.log("FAIL " + e); process.exit(1); });
