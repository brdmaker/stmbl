// node test/armmodel_test.js: the arm's model and kinematics in the browser (js/armmodel.js)
"use strict";
const fs = require("fs"), path = require("path");
const A = require("../js/armmodel.js");
const ROOT = path.resolve(__dirname, "../../..");
let fails = 0, n = 0;
const ok = (c, what, got) => { n++; if (!c) { fails++; console.log("FAIL " + what + (got !== undefined ? ": " + JSON.stringify(got) : "")); } else console.log("ok   " + what); };
// the files as the server's arm_model gives them: names relative to the model's folder
function bundle(model, cell, tools) {
  const root = path.dirname(model), files = {}, rel = (p) => path.relative(root, p).split(path.sep).join("/");
  const add = (p) => {
    const name = rel(p); if (name in files) return;
    if (p.endsWith(".stl")) { files[name] = { b64: "" }; return; }
    const text = fs.readFileSync(p, "utf8"); files[name] = text;
    for (const line of text.split("\n")) {
      const w = line.split("#")[0].trim().split(/\s+/);
      const ref = w[0] === "include" ? w[1] : w[0] === "part" && w[2] === "mesh" ? w[3] : null;
      if (ref) { const q = path.resolve(path.dirname(p), ref); if (fs.existsSync(q)) add(q); }
    }
  };
  add(model); if (cell) add(cell); if (tools) add(tools);
  return { files, model: rel(model), cell: cell && rel(cell), tools: tools && rel(tools) };
}
const vis = path.join(ROOT, "linuxcnc/vis3d/models");
const B = bundle(path.join(vis, "arm6_changer.vis"), path.join(ROOT, "hwtest/arcmate-sim/cell_changer.txt"), path.join(ROOT, "host/arm/arcmate/tools.txt"));
const v = A.load(B);
ok(v.errors.length === 0, "arm6_changer.vis, its cell and tools: read", v.errors);
ok(v.n === 6 && v.q_theta, "six joints, the DH theta as the joints (LinuxCNC's)", [v.n, v.q_theta]);
ok(v.parts.length > 20 && v.parts.some((p) => p.body && p.shape === "capsule") && v.parts.some((p) => p.shape === "tube"), "parts and the arm's bodies", v.parts.length);
ok(Object.keys(v.tools).length === 2 && v.tools[1].name === "torch" && v.tools[2].dock && v.tools[1].tcp && Math.abs(v.tools[1].tcp[2] - 0.29) < 1e-9, "two tools: their names, tool points, docks", Object.values(v.tools).map((t) => t.name));
ok(v.tools[1].bodies.length > 0, "a tool's bodies (drawn without parts)");
ok(v.obs.length >= 1 && v.obs[0].shape === "box", "the cell's obstacles", v.obs.map((o) => o.name));
ok(v.prevent === 10, "the guard's clearance limit (prevent 10 mm)", v.prevent);
// the dock: the flange pose with the tool in its dock, mm and degrees
const dk = v.tools[1].dock; ok(Math.abs(dk[12] - 1.0) < 1e-9 && Math.abs(dk[13] - 0.4) < 1e-9, "tool 1's dock at 1000 400 1000 mm", [dk[12], dk[13], dk[14]]);
// against LinuxCNC's genserkins (host/tests/robot/genserkins_ref.txt): the flange's X Y Z, A B C
let ep = 0, ea = 0, m = 0;
const pairs = fs.readFileSync(path.join(ROOT, "host/tests/robot/genserkins_ref.txt"), "utf8").split("\n").filter((l) => l.startsWith("pair ")).map((l) => l.split(/\s+/).slice(1).map(Number));
for (const p of pairs) {
  const r = A.pose(v, p.slice(0, 6));
  for (let i = 0; i < 3; i++) { ep = Math.max(ep, Math.abs(r.xyz[i] - p[6 + i])); ea = Math.max(ea, Math.abs(((r.abc[i] - p[9 + i]) % 360 + 540) % 360 - 180)); }
  m++;
}
ok(m === 8 && ep < 1e-5, "the flange = LinuxCNC genserkins, 8 poses (mm)", ep);
ok(ea < 1e-5, "A B C = genserkins' (deg)", ea);
// IK: back to each pose from 5 degrees off
let ikok = 0, eq = 0;
for (const p of pairs) {
  const r = A.pose(v, p.slice(0, 6)), q0 = p.slice(0, 6).map((x) => x + 5);
  const s = A.ik(v, r, q0);
  if (s.ok) ikok++;
  const back = A.pose(v, s.q); eq = Math.max(eq, ...back.xyz.map((x, i) => Math.abs(x - r.xyz[i])));
}
ok(ikok === 8 && eq < 0.01, "IK: each pose again from 5° off (within 0.01 mm)", [ikok, eq]);
// with a tool point: the TCP moves, the flange follows
const t1 = v.tools[1].tcp, home = pairs[0].slice(0, 6), r1 = A.pose(v, home, t1), s1 = A.ik(v, { xyz: [r1.xyz[0] + 20, r1.xyz[1], r1.xyz[2]], abc: r1.abc }, home, t1);
const r2 = A.pose(v, s1.q, t1);
ok(s1.ok && Math.abs(r2.xyz[0] - r1.xyz[0] - 20) < 0.01 && Math.abs(r2.xyz[1] - r1.xyz[1]) < 0.01, "IK on the torch's tool point: +20 in X", r2.xyz);
// includes resolve from the including file
ok(A.norm("../../host/arm/arcmate/../arcmate/x.arm") === "../../host/arm/arcmate/x.arm", "paths: .. and . folded");
console.log(`${n - fails}/${n} passed`);
process.exit(fails ? 1 : 0);
