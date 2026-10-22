// The console with a million-line program while the machine runs it (from
// stress_suite.py, which loaded it and started it from line 500 000).
//   node test/gui/stress_page.js PORT [HOST]
const { chromium } = require("playwright");

(async () => {
  const port = process.argv[2], host = process.argv[3] || "127.0.0.1";
  const browser = await chromium.launch({ args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader"] });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const errors = [];
  page.on("pageerror", (e) => errors.push(String(e)));
  page.on("console", (m) => { if (m.type() === "error" && !/Failed to load resource/.test(m.text())) errors.push("console: " + m.text()); });
  let failed = 0;
  const check = (c, what) => { console.log((c ? "ok   " : "FAIL ") + what); if (!c) failed++; };
  const until = (fn, ms, arg) => page.waitForFunction(fn, arg, { timeout: ms, polling: 100 }).then(() => true, () => false);
  const t0 = Date.now();
  await page.goto(`http://${host}:${port}/#main`);
  const shown = await until(() => typeof prog !== "undefined" && prog.name === "surface-1m.ngc" && prog.segs.length > 1000, 60000);
  check(shown, `the program drawn ${((Date.now() - t0) / 1000).toFixed(1)} s after opening the page`);
  if (!shown) console.log("     page state: " + JSON.stringify(await page.evaluate(() => ({ name: prog.name, segs: prog.segs.length, live: !!LIVE, msg: document.getElementById("msg").textContent }))) + " errors: " + errors.join(" | "));
  const info = await page.evaluate(() => ({ segs: prog.segs.length, nlines: prog.nlines, sub: document.getElementById("pg-sub").textContent }));
  check(info.nlines === 1001661 && info.sub.startsWith("1,001,661 lines"), `1,001,661 lines (${info.sub})`);
  check(info.segs <= 100002, `${info.segs} segments drawn`);
  // the listing follows the machine's line, far past the first window
  check(await until(() => { const c = document.querySelector("#gcode .cur"); return c && +c.dataset.l >= 500000; }, 20000), "the listing shows the running line (≥ 500 000)");
  const cur = () => page.evaluate(() => { const c = document.querySelector("#gcode .cur"); return c ? +c.dataset.l : 0; });
  const l1 = await cur();
  await page.waitForTimeout(2500);
  let l2 = await cur();
  for (let k = 0; k < 10 && !l2; k++) { await page.waitForTimeout(100); l2 = await cur(); }  // between two windows
  check(l2 > l1, `...and moves on (${l1} → ${l2})`);
  // the console's own work per frame (scripts; the 3D drawing itself is the GPU's,
  // here software WebGL on shared cores, so frames per second are only printed)
  const perf = await page.evaluate(() => new Promise((res) => {
    const frames = []; let gl = 0; const tick0 = window.tick, render0 = R.renderer.render;
    window.tick = function (now) { const t = performance.now(); gl = 0; try { return tick0(now); } finally { frames.push([performance.now() - t - gl, gl]); } };
    R.renderer.render = function (...a) { const t = performance.now(); try { return render0.apply(this, a); } finally { gl += performance.now() - t; } };
    const t0 = performance.now();
    setTimeout(() => {
      window.tick = tick0; R.renderer.render = render0;
      const med = (k) => { const v = frames.map((f) => f[k]).sort((a, b) => a - b); return v[v.length >> 1] || 0; };
      res({ ms: med(0), gl: med(1), fps: frames.length / ((performance.now() - t0) / 1000) });
    }, 4000);
  }));
  check(perf.ms < 8, `the console's scripts: ${perf.ms.toFixed(1)} ms a frame (median) with ${info.segs} segments (+ ${perf.gl.toFixed(1)} ms in WebGL calls; ${perf.fps.toFixed(0)} frames/s here)`);
  const x1 = await page.textContent("#d-x");
  await page.waitForTimeout(500);
  check((await page.textContent("#d-x")) !== x1 || (await page.textContent("#d-y")) !== "", "the DRO moves");
  const heap = await page.evaluate(() => (performance.memory ? performance.memory.usedJSHeapSize / 1e6 : 0));
  check(heap < 600, `JS heap ${heap.toFixed(0)} MB`);
  // the File page: the preflight from every move
  await page.click('#nav button[data-p="file"]');
  await page.locator("#files .file b", { hasText: /^surface-1m\.ngc$/ }).click();  // by name: the folder may hold more
  check(await until(() => /Moves.*1,0\d\d,\d\d\d.*every \d+th point/.test(document.getElementById("fl-cards").textContent), 60000), "preflight: every move counted, the drawn points named");
  check((await page.textContent("#fl-cards")).includes("FITS"), "...and it fits");
  check(errors.length === 0, "no script errors" + (errors.length ? ": " + errors.join(" | ") : ""));
  await browser.close();
  process.exit(failed ? 1 : 0);
})().catch((e) => { console.log("FAIL " + e); process.exit(1); });
