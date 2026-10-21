// The live console in a real browser (Playwright, headless Chromium):
// the page connects to millweb, takes control with its own button, resets
// E-STOP, switches the machine on, homes, sends MDI, and the DRO follows.
//   node browser_live.js PORT [SCREENSHOT]
const { chromium } = require("playwright");

(async () => {
  const port = process.argv[2], shot = process.argv[3];
  const browser = await chromium.launch({ args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader"] });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const errors = [];
  page.on("pageerror", (e) => errors.push(String(e)));
  let failed = 0;
  const check = (cond, what) => { console.log((cond ? "ok   " : "FAIL ") + "browser: " + what); if (!cond) failed++; };
  const until = async (fn, ms = 15000) => { try { await page.waitForFunction(fn, null, { timeout: ms }); return true; } catch (e) { return false; } };
  await page.goto(`http://127.0.0.1:${port}/#main`);
  check(await until(() => document.querySelector("#ch-link") && document.querySelector("#ch-link").textContent.includes("LIVE")), "the page says LIVE");
  check((await page.textContent(".brand b")).includes("PORTAL-M SIM"), "the machine's name in the header");
  await page.click("#b-control");
  check(await until(() => document.querySelector("#b-control").textContent.includes("IN")), "Take control: IN CONTROL");
  if (await page.$eval("#ch-estop", (e) => e.textContent.includes("E-STOP OK")) === false) {
    await page.click("#b-estop");
    check(await until(() => document.querySelector("#ch-estop").textContent.includes("E-STOP OK")), "E-STOP button: released");
  }
  await page.click("#b-power");
  check(await until(() => document.querySelector("#ch-on").textContent.includes("MACHINE ON")), "MACHINE ON button");
  await page.click("#b-homeall");
  check(await until(() => document.querySelector("#ch-home").textContent.trim() === "HOMED"), "Home all");
  await page.fill("#mdi", "G0 X25 Y35 Z10");
  await page.press("#mdi", "Enter");
  check(await until(() => document.querySelector("#d-x").textContent.includes("25.000") && document.querySelector("#d-z").textContent.includes("10.000")), "MDI G0 X25 Y35 Z10: the DRO follows");
  // jog with the pad: Y+ by the 1 mm increment
  await page.click("#st-jog");
  await page.click('#jog-inc button[data-v="1"]');
  await page.dispatchEvent('#jogpad button[data-j="y+"]', "pointerdown");
  check(await until(() => document.querySelector("#d-y").textContent.includes("36.000")), "jog Y+ 1 mm from the pad");
  // a wizard's program is saved on the machine and loaded
  await page.click('#nav button[data-p="wiz"]');
  await page.click("#bc-go");
  check(await until(() => document.querySelector("#h-prog").textContent.startsWith("BOLTCIRCLE")), "a bolt-circle wizard program saved and loaded on the machine");
  await page.click('#nav button[data-p="file"]');
  check(await until(() => [...document.querySelectorAll("#files .file b")].some((b) => b.textContent.startsWith("BOLTCIRCLE"))), "the File page lists the machine's programs");
  check(await until(() => document.querySelector("#fl-cards") && document.querySelector("#fl-cards").textContent.includes("FITS")), "preflight from the machine's preview");
  await page.click('#nav button[data-p="main"]');
  check(errors.length === 0, "no script errors" + (errors.length ? ": " + errors.join(" | ") : ""));
  if (shot) { await page.waitForTimeout(800); await page.screenshot({ path: shot }); console.log("screenshot: " + shot); }
  await browser.close();
  process.exit(failed ? 1 : 0);
})().catch((e) => { console.log("FAIL browser: " + e); process.exit(1); });
