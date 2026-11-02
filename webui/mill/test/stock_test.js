// node test/stock_test.js: the lathe's material removal (js/stock.js)
"use strict";
const L = require("../js/stock.js");
let fails = 0, n = 0;
const ok = (c, what, got) => { n++; if (!c) { fails++; console.log("FAIL " + what + (got !== undefined ? ": " + JSON.stringify(got) : "")); } else console.log("ok   " + what); };
const near = (a, b, tol) => Math.abs(a - b) <= tol;
const STOCK = { d: 32, stick: 60, face: 0, chuckD: 100, dz: 0.05 };
const at = (S, z) => Math.min(S.n - 1, Math.max(0, Math.floor((z - S.z0) / S.dz)));
const T = [
  { t: 1, q: 2, d: 0.8, ins: "80", cl: 9 },      // OD turning, R0.4
  { t: 4, q: 2, d: 0.2, ins: "groove", w: 2, cl: 20 },
  { t: 5, q: 2, d: 0.2, ins: "groove", w: 2, cl: 12 },  // too short a blade for Ø32
  { t: 6, q: 7, d: 6, ins: "drill", cl: 40 },
  { t: 7, q: 2, d: 0.8 },                         // no insert data: an 80° one
  { t: 8, q: 2, d: 0.8, i: 95, j: 155 },          // the table's angles
];
const mv = (x0, z0, x, z, o = {}) => ({ k: "mv", x0, y0: 0, z0, x, y: 0, z, f: o.rapid ? 0 : 300, rapid: !!o.rapid, line: o.line || 1, tool: o.tool || 1 });

// LinuxCNC's orientation 2: the nose's centre at +X +Z of the controlled point; the outline beyond it
for (const t of T.filter((x) => x.q === 2 && x.i == null)) {
  const sh = L.toolShape(t), pts = [...sh.poly.lo, ...sh.poly.hi];
  ok(pts.every((p) => p[0] >= -1e-9 && p[1] >= -1e-9) && pts.some((p) => Math.abs(p[0]) < 1e-6) && pts.some((p) => Math.abs(p[1]) < 1e-6), `T${t.t} Q2: the controlled point the outline's -X -Z corner`);
}
ok(L.toolShape(T[0]).side === 1 && L.toolShape({ q: 3, d: 0.4 }).side === -1 && L.toolShape(T[3]).side === 0, "sides: Q2 outside, Q3 inside, Q7 on the axis");

// a turning pass: Ø32 -> Ø29 over 30 mm
{
  const R = L.run(L.begin(STOCK), [mv(16, 2, 14.5, 2, { rapid: true }), mv(14.5, 2, 14.5, -30), mv(14.5, -30, 17, -30)], T, 3);
  const S = R.S;
  ok(near(S.rout[at(S, -15)], 14.5, 1e-4), "turned to Ø29", S.rout[at(S, -15)]);
  ok(near(S.rout[at(S, -31)], 16, 1e-6), "behind the shoulder untouched");
  ok(S.rout[at(S, -29.9)] > 14.5 && S.rout[at(S, -29.9)] < 15, "the nose radius's fillet at the shoulder", S.rout[at(S, -29.9)]);
  const want = Math.PI * (16 * 16 - 14.5 * 14.5) * 30;
  ok(near(R.stat.vol, want, want * 0.01), "the volume taken", [R.stat.vol, want]);
  ok(R.find.length === 0, "no findings", R.find);
  ok(S.tout[at(S, -15)] === 1, "the surface T1's");
}
// a rapid into the stock; a rapid in the air
{
  const R = L.simulate(STOCK, [mv(20, 5, 15, 5, { rapid: true, line: 3 }), mv(15, 5, 15, -10, { rapid: true, line: 4 })], T);
  ok(R.find.length === 1 && R.find[0].kind === "rapid" && R.find[0].line === 4 && R.find[0].level === "bad", "G0 into the stock: line 4 only", R.find);
}
// facing: the face to Z-1, no part off
{
  const R = L.simulate(STOCK, [mv(17, -1, -0.4, -1, { line: 5 })], T);
  const S = R.S;
  ok(!L.solid(S, at(S, -0.5)) && L.solid(S, at(S, -1.5)), "faced to Z-1");
  ok(!R.find.some((f) => f.kind === "off"), "a face cut is not a part off", R.find);
}
// into the chuck
{
  const R = L.simulate(STOCK, [mv(14.5, 2, 14.5, -62, { line: 7 })], T);
  ok(R.find.some((f) => f.kind === "chuck" && f.line === 7), "turning to Z-62: into the chuck (its face at Z-60)", R.find);
}
// parting off at Z-40 with the 2 mm blade: what is beyond is gone
{
  const R = L.simulate(STOCK, [mv(17, -42, -0.5, -42, { tool: 4, line: 9 })], T);
  const S = R.S, f = R.find.find((x) => x.kind === "off");
  ok(f && f.line === 9 && /Z-40\.00: a part 40\.00 mm long/.test(f.text) && R.find.length === 1, "parted off at Z-40: a 40 mm part", R.find);
  ok(!L.solid(S, at(S, -20)) && L.solid(S, at(S, -45)), "the part gone, the rest in the chuck");
  const R2 = L.simulate(STOCK, [mv(17, -42, -0.5, -42, { tool: 5, line: 9 })], T);
  ok(R2.find.some((x) => x.kind === "holder"), "a 12 mm blade on Ø32: its holder in the part", R2.find);
}
// too deep: 8 mm with a 9 mm edge (a warning); 12 mm (beyond the insert: its holder in the part)
{
  let R = L.simulate(STOCK, [mv(8, 2, 8, -10, { line: 11 })], T);
  ok(R.find.length === 1 && R.find[0].kind === "depth" && R.find[0].level === "warn", "8 mm deep with a 9 mm edge: a warning", R.find);
  R = L.simulate(STOCK, [mv(4, 2, 4, -10, { line: 11 })], T);
  ok(R.find.some((f) => f.kind === "holder" && f.line === 11), "12 mm deep: the holder in the part", R.find);
}
// a drill on the axis: Ø6 to Z-20
{
  const R = L.simulate(STOCK, [mv(0, 3, 0, -20, { tool: 6, line: 12 })], T);
  const S = R.S;
  ok(near(S.rin[at(S, -10)], 3, 1e-6) && near(S.rin[at(S, -25)], 0, 1e-9), "drilled Ø6 to Z-20", [S.rin[at(S, -10)], S.rin[at(S, -25)]]);
  ok(S.rin[at(S, -19.5)] > 0 && S.rin[at(S, -19.5)] < 3, "its point's cone", S.rin[at(S, -19.5)]);
  ok(R.find.length === 0, "a drill on the axis: fine", R.find);
}
// an outside tool past the axis
{
  const R = L.simulate(STOCK, [mv(17, -1, -3, -1, { line: 13 })], T);
  ok(R.find.some((f) => f.kind === "centre"), "an OD tool to Ø-6: past the axis", R.find);
}
// a tube: the bore stays
{
  const R = L.simulate({ ...STOCK, bore: 20 }, [mv(14.5, 2, 14.5, -30)], T);
  ok(near(R.S.rin[at(R.S, -10)], 10, 1e-6) && near(R.S.rout[at(R.S, -10)], 14.5, 1e-4), "a tube: bore Ø20 kept");
}
// the table's front and back angles, as AXIS draws them: the edges along (sin a, cos a) from the nose
{
  const sh = L.toolShape({ ...T[5], cl: 9 }), pts = [...sh.poly.lo, ...sh.poly.hi], r = 0.4, d = (a) => [Math.sin(a * Math.PI / 180), Math.cos(a * Math.PI / 180)];
  const far = (a, c) => [r + r * Math.sin(c) + 9 * d(a)[0], r + r * Math.cos(c) + 9 * d(a)[1]];
  const has = (p) => pts.some((v) => Math.hypot(v[0] - p[0], v[1] - p[1]) < 1e-6);
  ok(has(far(95, -Math.PI / 2 + 95 * Math.PI / 180)) && has(far(155, -3 * Math.PI / 2 + 155 * Math.PI / 180)), "I95 J155: the edges' ends where AXIS draws them");
}
// a thread: M20x1.5 over 30, passes along Z with the 60° insert: the major Ø kept, its depth noted
{
  const th = { t: 3, q: 6, d: 0.2, ins: "60", cl: 8 }, segs = [];
  for (const dd of [0.3, 0.6, 0.92]) segs.push(mv(11, 3, 10 - dd, 3, { rapid: true, tool: 3 }), mv(10 - dd, 3, 10 - dd, -30, { tool: 3, line: 20 }), mv(10 - dd, -30, 11, -30, { rapid: true, tool: 3 }), mv(11, -30, 11, 3, { rapid: true, tool: 3 }));
  const R = L.simulate({ ...STOCK, d: 20 }, segs, [th]), S = R.S;
  ok(near(S.rout[at(S, -15)], 10, 1e-6) && near(S.thr[at(S, -15)], 0.92, 0.01) && S.thr[at(S, -35)] === 0, "the thread: Ø20 kept, 0.92 deep, none past its end", [S.rout[at(S, -15)], S.thr[at(S, -15)]]);
  ok(R.find.length === 0, "threading: no findings", R.find);
}
// a step at a time: the same as all at once
{
  const segs = [mv(14.5, 2, 14.5, -30), mv(14.5, -30, 16, -30), mv(16, 2, 13, 2, { rapid: true }), mv(13, 2, 13, -20)];
  const A = L.simulate(STOCK, segs, T), B = L.begin(STOCK);
  for (let k = 0; k <= segs.length; k++) for (const u of [0.3, 0.7]) L.run(B, segs, T, k, u);
  L.run(B, segs, T, segs.length);
  let same = true; for (let i = 0; i < A.S.n; i++) if (Math.abs(A.S.rout[i] - B.S.rout[i]) > 1e-5) { same = false; break; }
  ok(same, "a move taken in parts: the same stock");
}
console.log(`${n - fails}/${n} passed`);
process.exit(fails ? 1 : 0);
