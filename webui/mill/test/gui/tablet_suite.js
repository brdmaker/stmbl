// The console on tablets: every page and Debug tab at the usual iPad and
// Android tablet sizes, both ways round, as a touch screen (Chromium's
// emulation: hasTouch, isMobile, coarse pointer). The browser's own bars take
// 64 px of the height.
//   node test/gui/tablet_suite.js [-k ipad] [--shots DIR]          the simulation
//   TABLET_URL=http://board:8080/ node test/gui/tablet_suite.js     a machine
// Each page: no sideways scrolling; every control on screen can be tapped
// (nothing clipped or covered) and is at least 30 px both ways; the header on
// one row with the E-STOP when held sideways; in portrait (the page scrolls)
// a floating E-STOP when the header's is out of sight. Safari itself isn't
// here: double-tap zoom and text selection are checked as the CSS that stops
// them (touch-action, user-select).
const path = require("path");
const { test, run } = require("./runner");

const MILL = path.join(__dirname, "..", "..");
const URL = process.env.TABLET_URL || "file://" + path.join(MILL, "index.html");
const BAR = 64;
const TABLETS = [["iPad mini", 744, 1133], ["iPad 10.2", 810, 1080], ["iPad Air", 820, 1180], ["iPad Pro 11", 834, 1194], ["iPad Pro 12.9", 1024, 1366], ["Android 10 in", 800, 1280]];
const PAGES = ["main", "file", "wiz", "off", "tool", "probe", "dyn", "stat", "debug:hal", "debug:scope", "debug:drives", "set"];

// in the page: what a finger can't reach
function audit() {
  const W = innerWidth, H = innerHeight, out = { hscroll: document.documentElement.scrollWidth - W, bad: [], small: [] };
  const name = (e) => (e.id ? "#" + e.id : e.tagName.toLowerCase() + (typeof e.className === "string" && e.className ? "." + e.className.trim().split(/\s+/).join(".") : "")) + (e.textContent ? ` "${e.textContent.trim().slice(0, 16)}"` : "");
  const els = [...document.querySelectorAll("button, input, select, textarea, [role=tab], .file, summary, .hal-row")].filter((e) => {
    if (e.closest("[hidden]") || e.closest(".dialog")) return false;
    const r = e.getBoundingClientRect(); return r.width > 0 && r.height > 0 && getComputedStyle(e).visibility !== "hidden";
  });
  for (const e of els) {
    const r = e.getBoundingClientRect(), cx = r.left + r.width / 2, cy = r.top + r.height / 2;
    if (r.right > W + 1) out.bad.push("off the right: " + name(e));
    if (!/range|checkbox|radio/.test(e.type) && (r.height < 30 || r.width < 30)) out.small.push(`${name(e)} ${Math.round(r.width)}x${Math.round(r.height)}`);
    let skip = false, clip = null;
    for (let a = e.parentElement; a && a !== document.body; a = a.parentElement) {
      const cs = getComputedStyle(a), ar = a.getBoundingClientRect();
      if (!(cx < ar.left || cx > ar.right || cy < ar.top || cy > ar.bottom)) continue;
      if (/auto|scroll/.test(cs.overflowX + cs.overflowY)) { skip = true; break; }  // scrolled out of its box: fine
      if (/hidden|clip/.test(cs.overflowX + cs.overflowY)) { clip = a; break; }
    }
    if (skip) continue;
    if (clip) { out.bad.push(`clipped: ${name(e)} by ${name(clip).slice(0, 30)}`); continue; }
    if (cy < 0 || cy > H || cx < 0 || cx > W) continue;
    const hit = document.elementFromPoint(cx, cy);
    if (hit && !e.contains(hit) && !hit.contains(e) && hit.id !== "b-estop2") out.bad.push(`covered: ${name(e)} by ${name(hit).slice(0, 30)}`);
  }
  return out;
}

for (const [dev, w, h] of TABLETS) for (const orient of ["portrait", "landscape"]) {
  const vp = orient === "portrait" ? { width: w, height: h - BAR } : { width: h, height: w - BAR };
  test(`${dev} ${orient} ${vp.width}x${vp.height}: every page reachable by touch`, async (t) => {
    for (const pg of PAGES) {
      const [page, tab] = pg.split(":");
      await t.ev(([page, tab]) => { showPage(page); if (tab) debugTab(tab); scrollTo(0, 0); }, [page, tab]);
      await t.sleep(350);
      const a = await t.ev(audit);
      t.check(a.hscroll <= 1, `${pg}: no sideways scrolling (${a.hscroll} px)`);
      t.check(!a.bad.length, `${pg}: every control can be tapped${a.bad.length ? ": " + a.bad.slice(0, 4).join(" | ") : ""}`);
      t.check(!a.small.length, `${pg}: controls at least 30 px${a.small.length ? ": " + a.small.slice(0, 4).join(" | ") : ""}`);
    }
    await t.ev(() => { showPage("main"); scrollTo(0, 0); });
    const head = await t.ev(() => { const h = document.querySelector(".head").getBoundingClientRect(), e = document.getElementById("b-estop").getBoundingClientRect(); return { h: h.height, eTop: e.top - h.top, eRight: innerWidth - e.right }; });
    if (vp.width > 960) t.check(head.h < 90 && head.eRight < 60, `held sideways: the header on one row (${Math.round(head.h)} px), the E-STOP at its right end`);
    t.check(await t.ev(() => document.getElementById("b-estop2").hidden), "the header's E-STOP in sight: no floating one");
    const scrolls = await t.ev(() => document.documentElement.scrollHeight > innerHeight + 200);
    if (scrolls) {
      await t.ev(() => scrollTo(0, document.documentElement.scrollHeight));
      t.check(await t.until(() => !document.getElementById("b-estop2").hidden), "scrolled down: a floating E-STOP");
      const r = await t.ev(() => { const b = document.getElementById("b-estop2").getBoundingClientRect(); return [b.left + b.width / 2, b.top + b.height / 2, b.right <= innerWidth && b.bottom <= innerHeight]; });
      t.check(r[2], "on screen");
      await t.page.touchscreen.tap(r[0], r[1]);
      t.check(await t.until(() => st.estop === true), "a tap on it: E-STOP");
      await t.ev(() => { if (st.estop) toggleEstop(); scrollTo(0, 0); });
    }
  }, { viewport: vp, context: { hasTouch: true, isMobile: true }, timeout: 120000 });
}

test("touch: no double-tap zoom, no text selection on controls, text where it is worth copying", async (t) => {
  const cs = await t.ev(() => {
    const g = (s, k) => getComputedStyle(document.querySelector(s))[k];
    return { btn: g("#b-power", "touchAction"), seg: g("#dbg-tab button", "touchAction"), jog: g("#jogpad button", "touchAction"),
      canvas: g(".viewer canvas", "touchAction"), body: g("body", "userSelect"), btnSel: g("#b-power", "userSelect"),
      gcode: g("#gcode", "userSelect"), input: g("#mdi", "userSelect"), out: g("#dc-out", "userSelect"), events: g("#events", "userSelect") };
  });
  t.check(cs.btn === "manipulation" && cs.seg === "manipulation" && cs.jog === "manipulation", `buttons: touch-action manipulation (no double-tap zoom; ${cs.btn})`);
  t.check(cs.canvas === "none", "the 3D view keeps its own gestures (touch-action none)");
  t.check(cs.body === "none" && cs.btnSel === "none", "controls don't select text");
  t.check(cs.gcode === "text" && cs.input === "text" && cs.out === "text" && cs.events === "text", "G-code, inputs, the drive console and the event log do");
  t.check(await t.ev(() => { const e = new MouseEvent("contextmenu", { bubbles: true, cancelable: true }); document.querySelector("#jogpad button").dispatchEvent(e); return e.defaultPrevented; }), "a held jog button: no long-press menu");
  const vp = await t.ev(() => document.querySelector('meta[name="viewport"]').content);
  t.check(!/user-scalable\s*=\s*no|maximum-scale\s*=\s*1\b/.test(vp), "pinch zoom still allowed (viewport: " + vp + ")");
}, { viewport: { width: 1080, height: 746 }, context: { hasTouch: true, isMobile: true } });

run({
  open: async (t) => { await t.page.goto(URL); await t.page.waitForFunction(() => typeof st !== "undefined" && typeof events !== "undefined" && events.length > 3, null, { timeout: 30000 }); },
}).then((rc) => process.exit(rc));
