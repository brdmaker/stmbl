/* Portal Mill Console: a lathe's stock and what a program takes off it. No DOM, no THREE: the browser loads it
   as a classic script (lathe.js draws with it), node's tests require it (test/stock_test.js).

   The stock: slices along Z (dz apart, from the chuck's face to the stock's face), each an annulus rin..rout
   (radii; a solid bar's rin 0, a tube's its bore). A tool: its insert's outline in the XZ section (X a
   radius, signed: past the axis is negative), from LinuxCNC's tool data as AXIS draws it (orientation Q: the
   nose's centre at r·(dx, dz) from the controlled point, glcanon.py lathe_shapes; front and back angles I, J,
   else the insert's shape from the tool database: 80°, 55°, 35° rhombus, 60° thread, groove, round, drill),
   and its holder. A move sweeps the outline from its start to its end: the hull of the two (the outlines are
   convex), cut slice by slice from the stock. What a program would do wrong comes out as findings: a rapid
   into the stock, the holder or the insert's back in the material, the tool into the chuck, a cut deeper than
   the insert's edge, an OD tool past the axis; a part off when a slice is cut through. A thread (a 60° insert's
   passes along Z) can't be cut into a turned section: its slices keep the major diameter and note its depth
   (thr), half the groove's volume taken. */
"use strict";

const LSTOCK = (() => {
  // LinuxCNC's orientations: where the nose's centre is from the controlled point, [dx, dz] (glcanon.py)
  const Q_SHAPE = [null, [1, -1], [1, 1], [-1, 1], [-1, -1], [0, -1], [1, 0], [0, 1], [-1, 0], [0, 0]];
  const EDGE = { 80: 9, 55: 7, 35: 7, 60: 8, groove: 10, round: 6, drill: 30 };  // a cutting edge's length when the table has none
  const D2R = Math.PI / 180;
  const dir = (a) => [Math.sin(a), Math.cos(a)];  // an angle (from +Z toward +X) as [x, z], as glcanon.py's

  // ------------------------------------------------------------ the tool
  // {poly: [[x, z], ...] the insert's outline from the controlled point (convex), holder: [[x, z], ...] or null,
  //  side: 1 an outside tool, -1 an inside one, 0 on the axis; edge: its cutting edge's length; ins: its shape}
  function toolShape(t) {
    t = t || {};
    const q = t.q >= 1 && t.q <= 9 ? t.q : 2, [dx, dz] = Q_SHAPE[q], r = Math.max(0, (+t.d || 0) / 2);
    let ins = t.ins || (q === 9 ? "round" : (dx === 0 && q !== 9 && (+t.d || 0) >= 1) ? "drill" : "80");
    const edge = +t.cl > 0 ? +t.cl : EDGE[ins] || 9;
    const side = dx > 0 ? 1 : dx < 0 ? -1 : 0;
    const poly = [];
    if (ins === "groove") {  // a blade: w wide along Z (across X for a face groove), edge long toward the holder
      const w = +t.w > 0 ? +t.w : 2;
      let x0, x1, z0, z1;
      if (dx !== 0) { x0 = dx > 0 ? 0 : -edge; x1 = dx > 0 ? edge : 0; z0 = dz > 0 ? 0 : dz < 0 ? -w : -w / 2; z1 = z0 + w; }
      else { z0 = dz > 0 ? 0 : -edge; z1 = dz > 0 ? edge : 0; x0 = -w / 2; x1 = w / 2; }
      poly.push([x0, z0], [x1, z0], [x1, z1], [x0, z1]);
    } else if (ins === "drill") {  // on the axis: its point (118°) at the controlled point, its body toward dz
      const R = Math.max(r, 0.1), s = dz >= 0 ? 1 : -1, tipL = R / Math.tan(59 * D2R);
      poly.push([0, 0], [R, s * tipL], [R, s * edge], [-R, s * edge], [-R, s * tipL]);
    } else if (ins === "round") {
      const cx = r * dx, cz = r * dz, R = Math.max(r, 0.1);
      for (let k = 0; k < 24; k++) { const a = k * Math.PI / 12; poly.push([cx + R * Math.sin(a), cz + R * Math.cos(a)]); }
    } else {
      // the edges' angles: the table's front and back angles, else the insert's point angle around the bisector
      // toward its body; then the outline as glcanon.py draws it (the nose's arc between the two edges)
      let a0, a1;
      if (t.i != null && t.j != null && (t.i || t.j)) { a0 = Math.min(t.i, t.j) * D2R; a1 = Math.max(t.i, t.j) * D2R; }
      else {
        const A = (+ins || 80) * D2R, ab = Math.atan2(dx || 0, dz || (dx ? 0 : 1));
        a0 = ab - A / 2; a1 = ab + A / 2;
      }
      const cx = r * dx, cz = r * dz, c0 = -Math.PI / 2 + a0, c1 = -3 * Math.PI / 2 + a1, e0 = dir(a0), e1 = dir(a1);
      poly.push([cx + r * Math.sin(c0) + edge * e0[0], cz + r * Math.cos(c0) + edge * e0[1]]);
      // the arc, with its points on the axes' directions exactly (they are the controlled point's X and Z)
      const us = []; for (let k = 0; k <= 18; k++) us.push(c0 + k * (c1 - c0) / 18);
      for (let m = Math.ceil(Math.min(c0, c1) / (Math.PI / 2)); m * Math.PI / 2 <= Math.max(c0, c1); m++) us.push(m * Math.PI / 2);
      for (const u of us) poly.push([cx + r * Math.sin(u), cz + r * Math.cos(u)]);
      poly.push([cx + r * Math.sin(c1) + edge * e1[0], cz + r * Math.cos(c1) + edge * e1[1]]);
      ins = String(ins);
    }
    // the holder: a 12 mm shank away from the axis behind an outside tool; a Ø10 bar along +Z under an inside
    // one; none for a drill (its body is its outline)
    let holder = null;
    const back = ins === "groove" ? edge : 0.8 * edge;
    if (ins !== "drill") {
      if (side > 0) { const z0 = dz > 0 ? 1 : dz < 0 ? -13 : -6; holder = [[back, z0], [back + 60, z0], [back + 60, z0 + 12], [back, z0 + 12]]; }  // clear of the tip: it turns to a shoulder
      else if (side < 0) { const z0 = back; holder = [[-11, z0], [-1, z0], [-1, z0 + 100], [-11, z0 + 100]]; }
    }
    return { poly: hull(poly), holder: holder && hull(holder), side, edge, ins, r, q };
  }

  // ------------------------------------------------------------ convex hulls in the (z, x) plane
  // points [x, z] -> the hull's lower chain (x's least at each z) and upper, each from its least z to its most
  function hull(pts) {
    const p = pts.map((v) => [v[0], v[1]]).sort((a, b) => a[1] - b[1] || a[0] - b[0]);
    const cross = (o, a, b) => (a[1] - o[1]) * (b[0] - o[0]) - (a[0] - o[0]) * (b[1] - o[1]);
    const lo = [], hi = [];
    for (const v of p) { while (lo.length >= 2 && cross(lo[lo.length - 2], lo[lo.length - 1], v) <= 0) lo.pop(); lo.push(v); }
    for (const v of p) { while (hi.length >= 2 && cross(hi[hi.length - 2], hi[hi.length - 1], v) >= 0) hi.pop(); hi.push(v); }
    return { lo, hi, z0: p[0][1], z1: p[p.length - 1][1] };
  }
  // a hull moved by (x, z)
  const moved = (h, x, z) => h && { lo: h.lo.map((v) => [v[0] + x, v[1] + z]), hi: h.hi.map((v) => [v[0] + x, v[1] + z]), z0: h.z0 + z, z1: h.z1 + z };
  // a hull swept from (x0, z0) to (x1, z1): the hull of the two
  function swept(h, x0, z0, x1, z1) {
    const a = moved(h, x0, z0), b = moved(h, x1, z1);
    return hull([...a.lo, ...a.hi, ...b.lo, ...b.hi]);
  }
  // a chain's x at z (z inside it)
  function chainAt(ch, z, k) {
    while (k < ch.length - 2 && ch[k + 1][1] < z) k++;
    const a = ch[k], b = ch[k + 1] || a, u = b[1] > a[1] ? (z - a[1]) / (b[1] - a[1]) : 0;
    return [a[0] + (b[0] - a[0]) * Math.min(1, Math.max(0, u)), k];
  }
  // each slice i (centre z(i)) inside the hull: f(i, xlo, xhi)
  function slices(S, h, f) {
    const i0 = Math.max(0, Math.ceil((h.z0 - S.z0) / S.dz - 0.5)), i1 = Math.min(S.n - 1, Math.floor((h.z1 - S.z0) / S.dz - 0.5));
    let kl = 0, kh = 0, a, b;
    for (let i = i0; i <= i1; i++) {
      const z = S.z0 + (i + 0.5) * S.dz;
      [a, kl] = chainAt(h.lo, z, kl); [b, kh] = chainAt(h.hi, z, kh);
      f(i, a, b);
    }
  }
  // a signed X interval as radii: [0, …] when it spans the axis
  const radial = (a, b) => (a >= 0 ? [a, b] : b <= 0 ? [-b, -a] : [0, Math.max(-a, b)]);

  // ------------------------------------------------------------ the stock
  // o: {d, bore, stick (out of the chuck), face (the stock's face at work Z), chuckD, dz}
  function stock(o) {
    const d = +o.d > 0 ? +o.d : 32, stick = +o.stick > 0 ? +o.stick : 60, dz = +o.dz > 0 ? +o.dz : 0.05;
    const n = Math.max(1, Math.round(stick / dz)), face = +o.face || 0;
    const S = { d, bore: Math.max(0, +o.bore || 0), stick, face, dz, n, z0: face - n * dz, chuckR: (+o.chuckD > 0 ? +o.chuckD : 100) / 2,
      rin: new Float32Array(n), rout: new Float32Array(n), tin: new Uint8Array(n), tout: new Uint8Array(n), thr: new Float32Array(n), vol0: 0, off: -1, parts: [] };
    S.rin.fill(S.bore / 2); S.rout.fill(d / 2);
    S.vol0 = Math.PI * (d * d - S.bore * S.bore) / 4 * n * S.dz;
    return S;
  }
  const solid = (S, i) => S.rout[i] > S.rin[i] + 1e-6;
  function volume(S) { let v = 0; for (let i = 0; i < S.n; i++) if (solid(S, i)) v += Math.PI * (S.rout[i] ** 2 - S.rin[i] ** 2) * S.dz; return v; }
  const copy = (S) => ({ ...S, rin: S.rin.slice(), rout: S.rout.slice(), tin: S.tin.slice(), tout: S.tout.slice(), thr: S.thr.slice(), parts: S.parts.slice() });

  // one move of a tool (its shape sh, number tool) from (x0, z0) to (x1, z1): the stock cut; what it found
  // {vol: removed [mm³], depth: the deepest cut across the move [mm], buried, holder (its overlap [mm]), chuck,
  //  off: the slice a part came off at (-1)}
  function cut(S, sh, tool, x0, z0, x1, z1) {
    const out = { vol: 0, depth: 0, holder: 0, chuck: false, off: -1, top: -1, partLen: 0 };
    const h = swept(sh.poly, x0, z0, x1, z1), along = Math.abs(z1 - z0) >= Math.abs(x1 - x0);
    const thread = sh.ins === "60" && sh.side > 0 && along && Math.abs(z1 - z0) > 1e-6;
    const outOf = sh.ins === "60" && sh.side > 0 && x1 > x0 + 1e-9;  // out of a thread's groove: nothing in its way
    let zlo = Infinity, zhi = -Infinity, emptied = -1;
    const left = new Map();
    slices(S, h, (i, xa, xb) => {
      if (!solid(S, i)) return;
      const [a, b] = radial(xa, xb), ri = S.rin[i], ro = S.rout[i];
      if (b <= ri + 1e-9 || a >= ro - 1e-9) return;
      if (outOf && S.thr[i] > 0) return;
      if (thread) {  // a thread's pass: its depth here
        const dd = ro - Math.max(a, ri);
        if (dd > S.thr[i]) { out.vol += Math.PI * ((ro - S.thr[i]) ** 2 - (ro - dd) ** 2) * S.dz / 2; S.thr[i] = dd; S.tout[i] = tool; }
        out.depth = Math.max(out.depth, dd); zlo = Math.min(zlo, i); zhi = Math.max(zhi, i);
        return;
      }
      const v0 = ro * ro - ri * ri;
      // an outside tool cuts from the outside in, an inside one (or a drill) from the inside out: what its insert
      // doesn't reach stays (left: for the holder's check)
      if (a <= ri && b >= ro) { S.rin[i] = 0; S.rout[i] = 0; if (emptied < 0 || i < emptied) emptied = i; }
      else if (sh.side > 0) {
        if (b >= ro) { S.rout[i] = a; S.tout[i] = tool; }
        else { left.set(i, [b, ro]); if (a <= ri) { S.rin[i] = b; S.tin[i] = tool; } else { S.rout[i] = a; S.tout[i] = tool; } }
      } else {
        if (a <= ri) { S.rin[i] = b; S.tin[i] = tool; }
        else { left.set(i, [ri, a]); if (b >= ro) { S.rout[i] = a; S.tout[i] = tool; } else { S.rin[i] = b; S.tin[i] = tool; } }
      }
      const v1 = solid(S, i) ? S.rout[i] ** 2 - S.rin[i] ** 2 : 0;
      out.vol += Math.PI * (v0 - v1) * S.dz;
      if (along) {  // the thickness taken off this slice
        const l = left.get(i), kept = (solid(S, i) ? S.rout[i] - S.rin[i] : 0) + (l ? l[1] - l[0] : 0) - (l && solid(S, i) ? Math.max(0, Math.min(l[1], S.rout[i]) - Math.max(l[0], S.rin[i])) : 0);
        out.depth = Math.max(out.depth, ro - ri - kept);
      }
      zlo = Math.min(zlo, i); zhi = Math.max(zhi, i);
    });
    if (!along && zhi >= zlo) out.depth = (zhi - zlo + 1) * S.dz;
    // a slice cut through with material beyond it (toward the face; not a face cut): that is off the chuck, gone
    if (emptied >= 0) {
      let top = emptied; while (top + 1 < S.n && !solid(S, top + 1) && top + 1 <= zhi) top++;  // the kerf's far side
      for (let i = top + 1; i < S.n; i++) if (solid(S, i)) { out.off = emptied; out.top = top + 1; break; }
    }
    // the holder in what is left
    if (sh.holder) {
      slices(S, swept(sh.holder, x0, z0, x1, z1), (i, xa, xb) => {
        const [a, b] = radial(xa, xb), l = left.get(i);
        if (solid(S, i)) out.holder = Math.max(out.holder, Math.min(b, S.rout[i]) - Math.max(a, S.rin[i]));
        if (l) out.holder = Math.max(out.holder, Math.min(b, l[1]) - Math.max(a, l[0]));
      });
    }
    if (out.off >= 0) {
      let last = out.top; for (let i = out.top; i < S.n; i++) if (solid(S, i)) last = i;
      out.partLen = (last - out.top + 1) * S.dz;
      // the part as it came off (the view shows it where it was)
      S.parts.push({ i0: out.top, rin: S.rin.slice(out.top, last + 1), rout: S.rout.slice(out.top, last + 1), tin: S.tin.slice(out.top, last + 1), tout: S.tout.slice(out.top, last + 1), thr: S.thr.slice(out.top, last + 1) });
      for (let i = out.off + 1; i < S.n; i++) if (solid(S, i)) { out.vol += Math.PI * (S.rout[i] ** 2 - S.rin[i] ** 2) * S.dz; S.rin[i] = S.rout[i] = 0; }
      S.off = S.off < 0 ? out.off : Math.min(S.off, out.off);
    }
    // into the chuck: the insert or the holder behind the chuck's face, inside its radius
    for (const hh of [h, sh.holder && swept(sh.holder, x0, z0, x1, z1)]) {
      if (!hh || hh.z0 >= S.z0) continue;
      const zc = S.z0, pts = [...hh.lo, ...hh.hi].filter((v) => v[1] < zc);
      for (const ch of [hh.lo, hh.hi]) for (let k = 0; k + 1 < ch.length; k++) {
        const [p, q] = [ch[k], ch[k + 1]];
        if ((p[1] - zc) * (q[1] - zc) < 0) pts.push([p[0] + (q[0] - p[0]) * (zc - p[1]) / (q[1] - p[1]), zc]);
      }
      if (pts.length) {
        const xs = pts.map((v) => v[0]), lo = Math.min(...xs), hi = Math.max(...xs);
        const near = lo <= 0 && hi >= 0 ? 0 : Math.min(Math.abs(lo), Math.abs(hi));
        if (near < S.chuckR) out.chuck = true;
      }
    }
    return out;
  }

  // ------------------------------------------------------------ a program
  // the run's state: the stock, the segment next, the tool on; run(st, segs, tools, upto, u): the segments
  // before upto, and upto's first u (0..1); every move's findings into st.find (kept to one each kind a line)
  function begin(o) { return { S: stock(o), o, i: 0, tool: 0, shapes: new Map(), find: [], seen: new Set(), stat: { vol: 0, air: 0, cutT: 0, byTool: {} } }; }
  function shapeOf(R, tools, t) {
    if (!R.shapes.has(t)) R.shapes.set(t, toolShape((tools || []).find((x) => x.t === t) || { q: 2, d: 0.8 }));
    return R.shapes.get(t);
  }
  function note(R, s, kind, level, text, x, z) {
    const key = kind + ":" + s.line;
    if (R.seen.has(key)) return;
    R.seen.add(key);
    R.find.push({ kind, level, line: s.line, tool: s.tool, x, z, text });
  }
  function move(R, tools, s, u) {
    const tool = s.tool || R.tool;
    if (!tool) return null;
    const sh = shapeOf(R, tools, tool), x1 = s.x0 + (s.x - s.x0) * u, z1 = s.z0 + (s.z - s.z0) * u;
    return cut(R.S, sh, tool, s.x0, s.z0, x1, z1);
  }
  function run(R, segs, tools, upto, u = 0, checks = true) {
    const fmt = (v) => (+v).toFixed(2);
    for (; R.i < Math.min(upto, segs.length); R.i++) {
      const s = segs[R.i];
      if (s.k === "tc") { R.tool = s.tool; continue; }
      if (s.k !== "mv" || s.probe) continue;
      const tool = s.tool || R.tool, r = move(R, tools, s, 1);
      if (!r) continue;
      const len = Math.hypot(s.x - s.x0, s.z - s.z0), t = s.t || (s.f > 0 ? len / s.f * 60 : 0);
      R.stat.vol += r.vol;
      const bt = R.stat.byTool[tool] || (R.stat.byTool[tool] = { vol: 0, t: 0, mrr: 0, depth: 0 });
      bt.vol += r.vol; bt.depth = Math.max(bt.depth, r.depth);
      if (!s.rapid) { if (r.vol > 1e-3) { R.stat.cutT += t; bt.t += t; if (t > 0) bt.mrr = Math.max(bt.mrr, r.vol / t * 60 / 1000); } else R.stat.air += t; }
      if (!checks) continue;
      const sh = shapeOf(R, tools, tool);
      if (s.rapid && r.vol > 0.05) note(R, s, "rapid", "bad", `G0 into the stock: T${tool} takes ${fmt(r.vol)} mm³ at rapid`, s.x, s.z);
      if (r.chuck) note(R, s, "chuck", "bad", `T${tool} into the chuck (its face at Z${fmt(R.S.z0)}, Ø${fmt(R.S.chuckR * 2)})`, s.x, s.z);
      if (r.holder > 0.05) note(R, s, "holder", "bad", `T${tool}'s holder in the part, ${fmt(r.holder)} mm`, s.x, s.z);
      if (!s.rapid && r.depth > 0.7 * sh.edge && sh.ins !== "groove" && sh.ins !== "drill" && sh.ins !== "60") note(R, s, "depth", "warn", `T${tool} cuts ${fmt(r.depth)} mm deep, its edge ${fmt(sh.edge)} mm long`, s.x, s.z);
      if (sh.side > 0 && r.vol > 1e-3 && Math.min(s.x, s.x0) < -(sh.r + 0.5)) note(R, s, "centre", "warn", `T${tool}, an outside tool, cuts past the axis (X${fmt(Math.min(s.x, s.x0) * 2)} as a diameter)`, s.x, s.z);
      if (r.off >= 0) { const zt = R.S.z0 + r.top * R.S.dz; note(R, s, "off", "info", `Parted off at Z${fmt(zt)}: a part ${fmt(r.partLen)} mm long`, s.x, s.z); }
    }
    if (u > 0 && R.i < segs.length && segs[R.i].k === "mv") move(R, tools, segs[R.i], u);  // the move on: as far as it is
    return R;
  }
  // a whole program: the finished stock, its findings, its numbers
  function simulate(o, segs, tools) {
    const R = run(begin(o), segs, tools, segs.length);
    const order = { bad: 0, warn: 1, info: 2 };
    R.find.sort((a, b) => order[a.level] - order[b.level] || a.line - b.line);
    return R;
  }

  return { Q_SHAPE, toolShape, hull, swept, stock, cut, volume, copy, begin, run, simulate, solid };
})();
if (typeof module !== "undefined") module.exports = LSTOCK;
