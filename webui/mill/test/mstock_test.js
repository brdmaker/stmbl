// node test/mstock_test.js: the mill's material removal (js/mstock.js)
"use strict";
const M = require("../js/mstock.js");
let fails = 0, n = 0;
const ok = (c, what, got) => { n++; if (!c) { fails++; console.log("FAIL " + what + (got !== undefined ? ": " + JSON.stringify(got) : "")); } else console.log("ok   " + what); };
const near = (a, b, tol) => Math.abs(a - b) <= tol;
const BOX = { boxes: [{ x0: 0, y0: 0, z0: -25, x1: 120, y1: 80, z1: 0 }], cell: 0.25 };
const top = (S, x, y) => S.top[Math.floor((y - S.y0) / S.cell) * S.nx + Math.floor((x - S.x0) / S.cell)];
const T = [
  { t: 1, d: 50, fl: 4, ty: "flat", cl: 6 },
  { t: 3, d: 6, fl: 3, ty: "flat", cl: 18 },
  { t: 4, d: 6, ty: "ball", cl: 18 },
  { t: 5, d: 5, ty: "drill", cl: 40 },
  { t: 6, d: 6, ty: "flat", cl: 10 },
  { t: 7, d: 6, ty: "flat", cl: 22, so: 22, hd: 30 },
  { t: 99, d: 2, c: "3D touch probe" },
];
const mv = (a, b, o = {}) => ({ k: "mv", x0: a[0], y0: a[1], z0: a[2], x: b[0], y: b[1], z: b[2], f: o.rapid ? 0 : 600, rapid: !!o.rapid, probe: !!o.probe, line: o.line || 1, tool: o.tool || 3 });
const plunge = (x, y, z, tool, line) => [mv([x, y, 5], [x, y, z], { tool, line })];

ok(M.toolShape({ d: 5, c: "5 mm carbide drill" }).ty === "drill" && M.toolShape({ t: 99, d: 2 }).probe && M.toolShape({ d: 10, c: "10 mm 90° chamfer mill" }).ty === "vee", "types guessed from the comment: drill, probe, chamfer");
// facing: Ø50 at Z-0.5 across
{
  const R = M.simulate(BOX, [mv([-30, 40, -0.5], [150, 40, -0.5], { tool: 1 })], T), S = R.S;
  ok(near(top(S, 60, 40), -0.5, 1e-6) && near(top(S, 60, 64), -0.5, 1e-6) && near(top(S, 60, 66), 0, 1e-6), "faced to -0.5 over a 50 mm band");
  ok(near(R.stat.vol, 120 * 50 * 0.5, 120 * 50 * 0.5 * 0.02), "its volume", R.stat.vol);
  ok(R.find.length === 0, "no findings", R.find);
}
// a slot: Ø6, 3 deep
{
  const R = M.simulate(BOX, [...plunge(10, 40, -3, 3), mv([10, 40, -3], [110, 40, -3])], T), S = R.S;
  ok(near(top(S, 60, 40), -3, 1e-6) && near(top(S, 60, 43.4), 0, 1e-6) && near(top(S, 60, 42.6), -3, 1e-6), "a slot 6 wide, 3 deep");
  const want = (100 * 6 + Math.PI * 9) * 3;
  ok(near(R.stat.vol, want, want * 0.03), "its volume", [R.stat.vol, want]);
}
// a ball: its bottom at 1.5 off its axis
{
  const R = M.simulate(BOX, [mv([10, 40, 5], [10, 40, -3], { tool: 4 }), mv([10, 40, -3], [110, 40, -3], { tool: 4 })], T), S = R.S;
  const yc = 41.375;  // a cell's centre, 1.375 off the axis
  ok(near(top(S, 60, yc), -3 + 3 - Math.sqrt(9 - Math.pow(yc - 40, 2)), 1e-4), "a ball's groove: its round bottom", top(S, 60, yc));
}
// a ramp: a flat end mill down 4 over 40, exact at its leading edge
{
  const R = M.simulate(BOX, [mv([10, 10, 1], [10, 10, 0]), mv([10, 10, 0], [50, 10, -4])], T), S = R.S;
  ok(near(top(S, 30.1, 10.1), -(30.125 + 3 - 10) / 40 * 4, 0.02), "a ramp's floor: its lowest end over the cell", top(S, 30.1, 10.1));
}
// a drill: its point
{
  const R = M.simulate(BOX, plunge(20, 20, -10, 5), T), S = R.S;
  ok(near(top(S, 20.1, 20.1), -10 + Math.hypot(0.125, 0.125) / Math.tan(59 * Math.PI / 180), 0.01) && near(top(S, 22.1, 20.1), -10 + 2.125 / Math.tan(59 * Math.PI / 180), 0.02), "a drill's point, 118°");
}
// a rapid into the stock
{
  const R = M.simulate(BOX, [mv([60, 40, 5], [60, 40, 2], { rapid: true, line: 3 }), mv([60, 40, 2], [60, 40, -1], { rapid: true, line: 4 })], T);
  ok(R.find.length === 1 && R.find[0].kind === "rapid" && R.find[0].line === 4, "G0 into the stock: line 4", R.find);
}
// deeper than the flutes; the holder
{
  let R = M.simulate(BOX, [...plunge(10, 40, -15, 6, 5), mv([10, 40, -15], [60, 40, -15], { tool: 6, line: 6 })], T);
  ok(R.find.some((f) => f.kind === "flutes" && f.line === 5), "15 deep with 10 mm flutes: the shank in", R.find);
  R = M.simulate(BOX, [...plunge(10, 40, -23, 7, 7), mv([10, 40, -23], [60, 40, -23], { tool: 7, line: 8 })], T);
  ok(R.find.some((f) => f.kind === "holder" && f.line === 8), "23 deep with 22 out of a Ø30 holder: the holder in the stock", R.find);
}
// a fixture: a vise jaw along the stock's front
{
  // the vise: its base under the stock, its jaws in front and behind (the stock between them)
  const o = { ...BOX, fixtures: { boxes: [{ name: "Vise", x0: -35, y0: -20, z0: -39, x1: 155, y1: 100, z1: -25 }, { name: "Vise", x0: -35, y0: -22, z0: -39, x1: 155, y1: 0, z1: -5 }, { name: "Vise", x0: -35, y0: 80, z0: -39, x1: 155, y1: 102, z1: -5 }] } };
  let R = M.simulate(o, [mv([-10, -5, 5], [-10, -5, -10], { line: 9 }), mv([-10, -5, -10], [130, -5, -10], { line: 10 })], T);
  ok(R.find.some((f) => f.kind === "fixture" && f.line === 10) && /cutter into Vise/.test(R.find.find((f) => f.kind === "fixture").text), "a contour at Z-10 along the jaw: into the vise", R.find);
  R = M.simulate(o, [mv([-10, -5, 5], [-10, -5, -3], { line: 9 }), mv([-10, -5, -3], [130, -5, -3], { line: 10 })], T);
  ok(!R.find.some((f) => f.kind === "fixture"), "at Z-3, over the jaw (its top Z-5): clear", R.find);
  R = M.simulate(o, [...plunge(60, 40, -24, 5, 11)], T);
  ok(!R.find.some((f) => f.kind === "fixture"), "a hole 24 deep between the jaws: clear of the vise", R.find);
  R = M.simulate(o, [...plunge(60, 40, -27, 5, 11)], T);
  ok(R.find.some((f) => f.kind === "fixture" && f.line === 11) && R.find.some((f) => f.kind === "through"), "27 deep: through the stock, into the vise's base", R.find);
}
// a probe: touching on a probing move, not off one
{
  let R = M.simulate(BOX, [mv([60, 40, 5], [60, 40, -0.2], { tool: 99, probe: true, line: 11 })], T);
  ok(R.find.length === 0 && R.stat.vol === 0, "a probing move: no finding, nothing cut", R.find);
  R = M.simulate(BOX, [mv([60, 40, 5], [60, 40, -2], { tool: 99, line: 12 })], T);
  ok(R.find.some((f) => f.kind === "probe"), "a probe fed into the stock: a crash", R.find);
}
// the spindle stopped
{
  const R = M.simulate(BOX, [{ k: "m5", line: 13 }, ...plunge(60, 40, -2, 3, 14)], T);
  ok(R.find.some((f) => f.kind === "spindle" && f.line === 14), "cutting after M5", R.find);
}
// a model's triangles: a 40 x 30 block, 10 high
{
  const q = (a, b, c, d) => [...a, ...b, ...c, ...a, ...c, ...d];
  const tris = new Float32Array([...q([0, 0, 0], [40, 0, 0], [40, 30, 0], [0, 30, 0]), ...q([0, 0, -10], [0, 30, -10], [40, 30, -10], [40, 0, -10])]);
  const S = M.stock({ tris: [tris], cell: 0.5 });
  ok(near(S.top[S.nx * 10 + 10], 0, 1e-6) && near(S.bot[S.nx * 10 + 10], -10, 1e-6) && near(S.vol0, 12000, 1), "a model: its top and bottom", [S.vol0]);
}
// a move in parts: the same
{
  const segs = [...plunge(10, 40, -3, 3), mv([10, 40, -3], [110, 60, -5]), mv([110, 60, -5], [20, 20, -1])];
  const A = M.simulate(BOX, segs, T), B = M.begin(BOX);
  for (let k = 0; k <= segs.length; k++) for (const u of [0.3, 0.7]) M.run(B, segs, T, k, u);
  M.run(B, segs, T, segs.length);
  let same = true; for (let i = 0; i < A.S.top.length; i++) if (Math.abs(A.S.top[i] - B.S.top[i]) > 1e-5) { same = false; break; }
  ok(same, "a move taken in parts: the same stock");
}
console.log(`${n - fails}/${n} passed`);
process.exit(fails ? 1 : 0);
