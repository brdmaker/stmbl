// The console on an arm (js/arm.js), against LinuxCNC running hwtest/arcmate-sim with its tool changer:
//   python3 test/harness.py --arm -- node test/gui/arm_suite.js     (make test-gui-arm)
// as linuxcnc/arm6hmi's self-test: the model and its panels; E-STOP reset, power, home; a joint jog (joint mode), an
// axis jog (world mode); MDI; the guard armed; Record point; tool changes (armtool, LinuxCNC and the kinematics
// agreeing); the gripper on the flange; E-STOP.
"use strict";
const fs = require("fs");
const path = require("path");
const { test, run } = require("./runner");

const PORT = +process.env.MILLWEB_PORT, DIR = process.env.MILLWEB_DIR, BASE = `http://127.0.0.1:${PORT}/`;
const near = (a, b, tol) => Math.abs(a - b) <= tol;

test("an arm: its model (25 parts, 2 tools, the cell's obstacle), the joints, A B C, the guard", async (t) => {
  t.check(await t.until(() => typeof ARM !== "undefined" && ARM && ARM.built && R.type === "arm", null, 20000), "the console an arm's, its 3D model built");
  t.check(await t.ev(() => ARM.v.parts.length >= 20 && Object.keys(ARM.v.tools).length === 2 && ARM.v.obs.length >= 1 && ARM.v.prevent === 10 && !ARM.v.errors.length), "the model, its tools, the cell (prevent 10)");
  t.check(await t.ev(() => document.querySelectorAll("#arm-joints .armjrow").length === 6 && !!document.getElementById("d-c")), "six joint rows; A B C in the DRO");
  t.check(await t.until(() => st.armJ && st.armJ.length === 6 && Math.abs(st.armJ[1] + 90) < 0.5), "the joints from LinuxCNC (J2 at -90°)");
  t.check(await t.ev(() => document.getElementById("spin-panel").offsetParent === null && document.querySelector('#nav button[data-p="probe"]').hidden), "no spindle panel, no probing");
  t.check(t.errors.length === 0, "no script errors");
});

test("E-STOP reset, power, home; J1 +10° (joint mode), X −10 (world mode), MDI Z; the guard armed", async (t) => {
  await t.click("#b-control");
  t.check(await t.until(() => document.getElementById("b-control").textContent.includes("IN")), "control taken");
  if (await t.ev(() => st.estop)) await t.click("#b-estop");
  t.check(await t.until(() => !st.estop), "E-STOP reset");
  if (!(await t.ev(() => st.on))) await t.click("#b-power");
  t.check(await t.until(() => st.on, null, 10000), "power on");
  await t.click("#b-homeall");
  t.check(await t.until(() => st.armJH && st.armJH.every(Boolean), null, 60000), "all six joints homed");
  await t.click("#st-jog");
  await t.click('#jog-inc button[data-v="10"]');
  await t.click('#armpad-frame button[data-f="joint"]');
  const j0 = await t.ev(() => st.armJ[0]);
  await t.page.locator('#armpad button[data-ax="j0"][data-s="1"]').dispatchEvent("pointerdown");
  t.check(await t.until((j) => Math.abs(st.armJ[0] - j - 10) < 0.05 && st.armJmode === "joint", j0, 15000), "J1 +10° in joint mode");
  t.check(await t.until(() => /^10\.\d\d°$/.test(document.querySelector("#arm-joints .armjrow .num").textContent.trim()), null, 3000), "the joint's row shows it");
  await t.click('#armpad-frame button[data-f="world"]');
  const x0 = await t.ev(() => st.armAx[0]);
  await t.page.locator('#armpad button[data-ax="x"][data-s="-1"]').dispatchEvent("pointerdown");
  t.check(await t.until((x) => Math.abs(st.armAx[0] - x + 10) < 0.05 && st.armJmode === "world", x0, 15000), "X −10 mm in world mode");
  // MDI: Z down 20 at the TCP
  const z0 = await t.ev(() => st.armAx[2]);
  await t.click("#st-mdi");
  await t.page.fill("#mdi", `G1 Z${(z0 - 20).toFixed(3)} F3000`); await t.page.press("#mdi", "Enter");
  t.check(await t.until((z) => Math.abs(st.armAx[2] - z + 20) < 0.05, z0, 20000), "MDI G1 Z−20: the DRO follows");
  t.check(await t.until(() => /^ARMED|^NEAR/.test(document.getElementById("arm-guard").textContent) && /limit 10/.test(document.getElementById("arm-clear").textContent), null, 5000), "the guard armed, the cell's 10 mm");
  // the 3D arm follows: its flange where LinuxCNC says
  t.check(await t.ev(() => { const p = ARMMODEL.pose(ARM.v, st.armJ, armTcp()).xyz; return Math.abs(p[0] - st.armAx[0]) < 0.5 && Math.abs(p[2] - st.armAx[2]) < 0.5; }), "the 3D arm's tool point = LinuxCNC's position");
  t.check(t.errors.length === 0, "no script errors");
}, { timeout: 150000 });

test("Record point: a G1 line in nc_files/teach.ngc, the count", async (t) => {
  await t.click("#b-control");
  await t.until(() => document.getElementById("b-control").textContent.includes("IN"));
  await t.click("#arm-rec");
  t.check(await t.until(() => /1 point/.test(document.getElementById("arm-teach").textContent)), "1 point");
  const f = path.join(DIR, "nc_files", "teach.ngc");
  t.check(fs.existsSync(f) && /^G1 X-?\d+\.\d{3} Y-?\d+\.\d{3} Z-?\d+\.\d{3} A-?\d+\.\d{3} B-?\d+\.\d{3} C-?\d+\.\d{3} F\d+$/m.test(fs.readFileSync(f, "utf8")), "the line in the teach file");
  t.check(t.errors.length === 0, "no script errors");
});

test("tools: mount T1 (torch), change to T2 (gripper), close it, put it back; armtool, LinuxCNC, the kinematics agree", async (t) => {
  await t.click("#b-control");
  await t.until(() => document.getElementById("b-control").textContent.includes("IN"));
  await t.nav("tool");
  t.check(await t.until(() => document.querySelectorAll('#tl-arm tr[data-armtool]').length === 2), "the changer's two tools");
  const agree = (n) => t.until((n) => st.arm && st.arm.tool && Math.round(st.arm.tool.mounted) === n && st.tool === n && Math.round(st.arm.kins_tool) === n && st.run === "idle", n, 90000);
  await t.click('#tl-arm [data-mount="1"]');
  t.check(await agree(1), "T1 on the flange: armtool, LinuxCNC, the kinematics");
  t.check(await t.until(() => /ON FLANGE/.test(document.querySelector('#tl-arm tr[data-armtool="1"]').textContent)), "its row: ON FLANGE");
  t.check(await t.ev(() => ARM.mountedShown === 1 && ARM.toolG[1].parent === ARM.links[ARM.v.n]), "the 3D view: T1 on the flange");
  await t.click('#tl-arm [data-mount="2"]');
  t.check(await agree(2), "T2 (the gripper) on the flange");
  t.check(await t.ev(() => ARM.toolG[1].parent === R.armRoot), "T1 back in its dock in the view");
  await t.nav("main");
  await t.click("#arm-gclose");
  t.check(await t.until(() => st.arm.grip && [2, 3, 4].includes(Math.round(st.arm.grip.state)), null, 15000), "the gripper closes (on nothing: EMPTY)");
  await t.click("#arm-gopen");
  t.check(await t.until(() => st.arm.grip && Math.round(st.arm.grip.state) === 0, null, 15000), "and opens");
  await t.nav("tool");
  await t.click('#tl-arm [data-mount="0"]');
  t.check(await agree(0), "put back: nothing on the flange");
  t.check(t.errors.length === 0, "no script errors");
}, { timeout: 300000 });

test("E-STOP", async (t) => {
  await t.click("#b-estop");
  t.check(await t.until(() => st.estop), "E-STOP");
});

(async () => {
  const rc = await run({
    viewport: { width: 1600, height: 1000 },
    warmup: async (page) => { await page.goto(BASE + "#main"); await page.waitForFunction(() => document.getElementById("ch-link").textContent.includes("LIVE"), null, { timeout: 90000 }); },
    open: async (t) => {
      await t.page.goto(BASE + "#main");
      await t.page.waitForFunction(() => document.getElementById("ch-link").textContent.includes("LIVE") && typeof ARM !== "undefined" && ARM && ARM.built, null, { timeout: 30000 });
    },
  });
  process.exit(rc);
})().catch((e) => { console.log("FAIL " + e); process.exit(1); });
