// A small test runner for the console in headless Chromium (Playwright).
// Each case gets a fresh browser context and page; a case fails on a failed
// check, a thrown error, or a script error on the page, and leaves a
// screenshot in the shots folder.
//   suite.js [-k WORD]... [--list] [--shots DIR] [--headed] [-v]
const { chromium } = require("playwright");
const fs = require("fs");
const path = require("path");

const cases = [];
const test = (name, fn, opts = {}) => cases.push({ name, fn, opts });

function args() {
  const a = process.argv.slice(2), out = { k: [], list: false, shots: null, headed: false, verbose: false, rest: [] };
  for (let i = 0; i < a.length; i++) {
    if (a[i] === "-k") out.k.push(a[++i].toLowerCase());
    else if (a[i] === "--list") out.list = true;
    else if (a[i] === "--shots") out.shots = a[++i];
    else if (a[i] === "--headed") out.headed = true;
    else if (a[i] === "-v") out.verbose = true;
    else out.rest.push(a[i]);
  }
  return out;
}

// the page's own state: classic scripts share their top-level names, so
// page.evaluate sees st, prog, LIVE ...
function helpers(page, ctx, opt) {
  const t = {
    page, ctx, fails: [], errors: [],
    check(cond, what) {
      if (opt.verbose || !cond) console.log(`    ${cond ? "ok  " : "FAIL"} ${what}`);
      if (!cond) t.fails.push(what);
      return !!cond;
    },
    async until(fn, arg, ms = 8000) {
      try { await page.waitForFunction(fn, arg, { timeout: ms, polling: 50 }); return true; } catch (e) { return false; }
    },
    ev: (fn, arg) => page.evaluate(fn, arg),
    text: (sel) => page.textContent(sel).then((s) => (s || "").trim()),
    attr: (sel, a) => page.getAttribute(sel, a),
    click: (sel, o) => page.click(sel, o),
    sleep: (ms) => page.waitForTimeout(ms),
    hidden: (sel) => page.$eval(sel, (e) => e.hidden || getComputedStyle(e).display === "none"),
    nav: (p) => page.click(`#nav button[data-p="${p}"]`),
    async msgHas(s, ms = 4000) { return t.until((s) => document.getElementById("msg").textContent.includes(s), s, ms); },
    // the DRO's value for an axis, as a number ("−" is the console's minus)
    dro: (a) => page.textContent("#d-" + a).then((s) => +s.replace("−", "-").trim()),
    skipped: null,
    skip(why) { t.skipped = why; },  // a case that can't run here: return t.skip("why")
  };
  return t;
}

async function run(opts) {
  const o = args();
  const sel = cases.filter((c) => !o.k.length || o.k.some((k) => c.name.toLowerCase().includes(k)));
  if (o.list) { sel.forEach((c) => console.log(c.name)); return 0; }
  if (o.shots) fs.mkdirSync(o.shots, { recursive: true });
  const browser = await chromium.launch({ headless: !o.headed, args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
  const results = [];
  const t0 = Date.now();
  if (opts.warmup && sel.length) {  // the browser's first page is slow on a small computer: not a case's time
    const ctx = await browser.newContext(), page = await ctx.newPage(), w0 = Date.now();
    try { await opts.warmup(page); console.log(`(warm-up: the first page in ${((Date.now() - w0) / 1000).toFixed(1)} s)`); }
    catch (e) { console.log("(warm-up: " + String(e).split("\n")[0] + ")"); }
    await ctx.close();
  }
  for (const c of sel) {
    const vp = c.opts.viewport || opts.viewport || { width: 1600, height: 1000 };
    const ctx = await browser.newContext({ viewport: vp, ...(c.opts.context || opts.context || {}) });  // context: e.g. hasTouch, isMobile
    const page = await ctx.newPage();
    const t = helpers(page, ctx, o);
    page.on("pageerror", (e) => t.errors.push(String(e)));
    page.on("console", (m) => { if (m.type() === "error" && !/Failed to load resource|WebSocket connection/.test(m.text())) t.errors.push("console: " + m.text()); });
    const s0 = Date.now();
    let err = null;
    try {
      if (opts.open) await opts.open(t, c);
      await Promise.race([c.fn(t), new Promise((_, rej) => setTimeout(() => rej(new Error("timed out")), c.opts.timeout || 90000))]);
    } catch (e) { err = e; }
    if (err) t.fails.push(String(err.stack || err).split("\n").slice(0, 3).join(" | "));
    if (t.errors.length && !c.opts.allowErrors) { t.fails.push("script errors: " + t.errors.join(" | ")); console.log("    FAIL script errors:\n      " + t.errors.slice(0, 5).join("\n      ")); }
    const ok = !t.fails.length;
    console.log(`${ok ? (t.skipped ? "SKIP" : "PASS") : "FAIL"} ${c.name.padEnd(64)} ${((Date.now() - s0) / 1000).toFixed(1).padStart(5)}s`);
    if (ok && t.skipped) console.log("    " + t.skipped);
    if (!ok) {
      if (err) console.log("    " + String(err.message || err).split("\n")[0]);
      if (o.shots) {
        const f = path.join(o.shots, c.name.replace(/[^\w]+/g, "_").slice(0, 60) + ".png");
        await page.screenshot({ path: f }).catch(() => {});
        console.log("    screenshot: " + f);
      }
      const m = await page.textContent("#msg").catch(() => "");
      if (m) console.log("    last message: " + m.trim());
    }
    results.push({ name: c.name, ok, skipped: ok && !!t.skipped });
    if (opts.after) await opts.after(t, c).catch(() => {});
    await ctx.close();
  }
  await browser.close();
  const bad = results.filter((r) => !r.ok);
  const sk = results.filter((r) => r.skipped).length;
  console.log(`\n${results.length - bad.length - sk}/${results.length - sk} cases passed${sk ? `, ${sk} skipped` : ""} in ${Math.round((Date.now() - t0) / 1000)} s`);
  bad.forEach((r) => console.log("  FAIL " + r.name));
  return bad.length ? 1 : 0;
}

module.exports = { test, run, args };
