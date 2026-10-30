/* Portal Mill Console: a mill's stock and what a program takes off it. No DOM, no THREE: the browser loads it as
   a classic script (millrem.js draws with it), node's tests require it (test/mstock_test.js).

   The stock: a height map over the workpiece's footprint, a cell `cell` mm square, each its top and bottom (work
   Z): a box, or what a workpiece model's triangles give (the space between their highest and lowest point). A
   tool: a body of revolution from the tool database (type: flat, ball, bull (corner radius cr), drill (point
   angle ang, 118°), vee (a chamfer or engraving cutter, ang), probe; flute length cl, stick-out so (tip to the
   holder), holder Ø hd), its bottom at a distance from its axis the profile below. A move lowers each cell's top
   to the tool's lowest point over it: exact for a flat end mill on any straight move (the lowest point at an end
   of the stretch the cell is under the tool), sampled along the stretch for the others.

   What a program would do wrong comes out as findings: a rapid into the stock, a cut deeper than the flutes (the
   shank in the material), the holder in the stock, the tool or holder into a fixture (its box), a probe into the
   stock off a probing move, cutting with the spindle stopped; through the stock's bottom as a note. */
"use strict";

const MSTOCK = (() => {
  const D2R = Math.PI / 180;

  // ------------------------------------------------------------ the tool
  // {R, ty, cr, ang, cl, so, rh, probe}: its radius, type, corner radius, point angle, flute length, stick-out,
  // holder radius. The type from the database, else guessed from the comment (a drill, a ball, a chamfer mill)
  function toolShape(t) {
    t = t || {};
    const d = Math.max(0.1, +t.d || 6), R = d / 2, c = String(t.c || "").toLowerCase();
    let ty = t.ty || (t.t === 99 || /probe/.test(c) ? "probe" : /drill/.test(c) ? "drill" : /ball/.test(c) ? "ball" : /chamfer|engrav|v-bit|\bvee\b/.test(c) ? "vee" : "flat");
    const ang = +t.ang > 0 ? +t.ang : ty === "drill" ? 118 : 90;
    const cr = ty === "ball" ? R : ty === "bull" ? Math.min(R, Math.max(0, +t.cr || 0)) : 0;
    const cl = +t.cl > 0 ? +t.cl : ty === "probe" ? 0 : Math.max(2 * d, Math.min(4 * d, 40));
    const so = +t.so > 0 ? +t.so : Math.max(cl + Math.max(5, d), 15);
    const rh = (+t.hd > 0 ? +t.hd : d + 20) / 2;
    return { R, d, ty, cr, ang, cl, so, rh, probe: ty === "probe" };
  }
  // the tool's bottom above its tip at rho from its axis (rho <= R)
  function prof(sh, rho) {
    if (sh.ty === "drill" || sh.ty === "vee") return rho / Math.tan(sh.ang / 2 * D2R);
    if (sh.cr > 0) { const r0 = sh.R - sh.cr; return rho <= r0 ? 0 : sh.cr - Math.sqrt(Math.max(0, sh.cr * sh.cr - (rho - r0) * (rho - r0))); }
    return 0;
  }

  // ------------------------------------------------------------ the stock
  // o: {boxes: [{x0, y0, z0, x1, y1, z1}], tris: [Float32Array of triangles' x y z, …] (work coordinates),
  //     cell (mm), maxCells}
  function stock(o) {
    const boxes = o.boxes || [], tris = o.tris || [];
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const b of boxes) { x0 = Math.min(x0, b.x0); y0 = Math.min(y0, b.y0); x1 = Math.max(x1, b.x1); y1 = Math.max(y1, b.y1); }
    for (const T of tris) for (let i = 0; i < T.length; i += 3) { x0 = Math.min(x0, T[i]); x1 = Math.max(x1, T[i]); y0 = Math.min(y0, T[i + 1]); y1 = Math.max(y1, T[i + 1]); }
    if (!(x1 > x0 && y1 > y0)) { x0 = 0; y0 = 0; x1 = 1; y1 = 1; }
    const area = (x1 - x0) * (y1 - y0), cell = Math.max(+o.cell || 0.25, Math.sqrt(area / (o.maxCells || 250000)));
    const nx = Math.max(1, Math.ceil((x1 - x0) / cell)), ny = Math.max(1, Math.ceil((y1 - y0) / cell));
    const S = { x0, y0, cell, nx, ny, top: new Float32Array(nx * ny).fill(-Infinity), bot: new Float32Array(nx * ny).fill(Infinity),
      tool: new Uint8Array(nx * ny), vol0: 0, zmax: -Infinity, zmin: Infinity };
    const cx = (i) => x0 + (i + 0.5) * cell, cy = (j) => y0 + (j + 0.5) * cell;
    for (const b of boxes) {
      const i0 = Math.max(0, Math.round((b.x0 - x0) / cell)), i1 = Math.min(nx, Math.round((b.x1 - x0) / cell));
      const j0 = Math.max(0, Math.round((b.y0 - y0) / cell)), j1 = Math.min(ny, Math.round((b.y1 - y0) / cell));
      for (let j = j0; j < j1; j++) for (let i = i0; i < i1; i++) {
        const k = j * nx + i;
        if (b.z1 > S.top[k]) { S.top[k] = b.z1; if (b.name) (S.names || (S.names = []))[k] = b.name; }
        S.bot[k] = Math.min(S.bot[k], b.z0);
      }
    }
    // a model: each triangle over the cells under it, its height there into their top and bottom
    for (const T of tris) for (let t = 0; t + 8 < T.length; t += 9) {
      const ax = T[t], ay = T[t + 1], az = T[t + 2], bx = T[t + 3], by = T[t + 4], bz = T[t + 5], qx = T[t + 6], qy = T[t + 7], qz = T[t + 8];
      const den = (by - qy) * (ax - qx) + (qx - bx) * (ay - qy);
      if (Math.abs(den) < 1e-12) continue;  // edge-on from above
      const i0 = Math.max(0, Math.floor((Math.min(ax, bx, qx) - x0) / cell - 0.5)), i1 = Math.min(nx - 1, Math.ceil((Math.max(ax, bx, qx) - x0) / cell - 0.5));
      const j0 = Math.max(0, Math.floor((Math.min(ay, by, qy) - y0) / cell - 0.5)), j1 = Math.min(ny - 1, Math.ceil((Math.max(ay, by, qy) - y0) / cell - 0.5));
      for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
        const px = cx(i), py = cy(j);
        const u = ((by - qy) * (px - qx) + (qx - bx) * (py - qy)) / den, v = ((qy - ay) * (px - qx) + (ax - qx) * (py - qy)) / den, w = 1 - u - v;
        if (u < -1e-9 || v < -1e-9 || w < -1e-9) continue;
        const z = u * az + v * bz + w * qz, k = j * nx + i;
        if (z > S.top[k]) S.top[k] = z;
        if (z < S.bot[k]) S.bot[k] = z;
      }
    }
    for (let k = 0; k < nx * ny; k++) {
      if (!(S.top[k] > S.bot[k])) { S.top[k] = S.bot[k] = 0; S.tool[k] = 255; continue; }  // no stock here
      S.vol0 += (S.top[k] - S.bot[k]) * cell * cell;
      S.zmax = Math.max(S.zmax, S.top[k]); S.zmin = Math.min(S.zmin, S.bot[k]);
    }
    S.top0 = S.top.slice();
    S.dirty = [Infinity, -Infinity];
    return S;
  }
  const solid = (S, k) => S.tool[k] !== 255 && S.top[k] > S.bot[k] + 1e-6;
  const copy = (S) => ({ ...S, top: S.top.slice(), tool: S.tool.slice() });

  // one move of a tool from p0 to p1 ([x, y, z], its tip): the stock cut (unless dry: a probe) and what it found
  // {vol: removed [mm³], ap: the deepest cut, flutes: deeper than the flutes by, holder: the holder's overlap,
  //  through: below the stock's bottom by}
  function cut(S, sh, tool, p0, p1, dry) {
    const out = { vol: 0, ap: 0, flutes: 0, holder: 0, through: 0, touched: 0 };
    const lo = Math.min(p0[2], p1[2]);
    // the holder's wider ring only when its bottom can be below the stock's top
    const dx = p1[0] - p0[0], dy = p1[1] - p0[1], dz = p1[2] - p0[2], L2 = dx * dx + dy * dy, R = sh.R, RH = lo + sh.so < S.zmax ? Math.max(sh.rh, R) : R;
    if (lo >= S.zmax + 1e-9 || sh.R <= 0) return out;
    const c = S.cell, flat = sh.ty === "flat" || (sh.cr === 0 && sh.ty !== "drill" && sh.ty !== "vee");
    const bx0 = Math.min(p0[0], p1[0]) - RH, bx1 = Math.max(p0[0], p1[0]) + RH, by0 = Math.min(p0[1], p1[1]) - RH, by1 = Math.max(p0[1], p1[1]) + RH;
    const i0 = Math.max(0, Math.floor((bx0 - S.x0) / c)), i1 = Math.min(S.nx - 1, Math.floor((bx1 - S.x0) / c));
    const j0 = Math.max(0, Math.floor((by0 - S.y0) / c)), j1 = Math.min(S.ny - 1, Math.floor((by1 - S.y0) / c));
    const zAt = (t) => p0[2] + dz * t;
    for (let j = j0; j <= j1; j++) {
      const py = S.y0 + (j + 0.5) * c;
      for (let i = i0; i <= i1; i++) {
        const k = j * S.nx + i;
        if (!solid(S, k)) continue;
        const px = S.x0 + (i + 0.5) * c, ex = px - p0[0], ey = py - p0[1];
        const ts = L2 > 1e-18 ? Math.min(1, Math.max(0, (ex * dx + ey * dy) / L2)) : 0;
        const qx = ex - dx * ts, qy = ey - dy * ts, q2 = qx * qx + qy * qy;
        // the holder over this cell (at the nearest point): the stock above its bottom
        if (q2 <= RH * RH && q2 > R * R) {
          const hb = zAt(ts) + sh.so;
          if (S.top[k] > hb) out.holder = Math.max(out.holder, S.top[k] - hb);
          continue;
        }
        if (q2 > R * R) continue;
        // the stretch [t1, t2] the cell is under the tool
        let t1 = 0, t2 = 1;
        if (L2 > 1e-18) {
          const b = ex * dx + ey * dy, e2 = ex * ex + ey * ey, disc = b * b - L2 * (e2 - R * R);
          const r = Math.sqrt(Math.max(0, disc));
          t1 = Math.max(0, (b - r) / L2); t2 = Math.min(1, (b + r) / L2);
        }
        let zb;
        if (flat) zb = Math.min(zAt(t1), zAt(t2));
        else {
          const at = (t) => { const ux = ex - dx * t, uy = ey - dy * t; return zAt(t) + prof(sh, Math.min(R, Math.sqrt(ux * ux + uy * uy))); };
          zb = Math.min(at(ts), at(t1), at(t2));
          if (Math.abs(dz) > 1e-9) for (let s = 1; s < 8; s++) zb = Math.min(zb, at(t1 + (t2 - t1) * s / 8));
        }
        const top = S.top[k];
        if (zb >= top - 1e-9) continue;
        const depth = top - Math.max(zb, S.bot[k]);
        out.touched++;
        out.ap = Math.max(out.ap, depth);
        if (top - zb > sh.cl + 1e-6) out.flutes = Math.max(out.flutes, top - zb - sh.cl);
        if (zb < S.bot[k]) out.through = Math.max(out.through, S.bot[k] - zb);
        out.vol += depth * c * c;
        if (dry) continue;
        S.top[k] = Math.max(zb, S.bot[k]); S.tool[k] = tool;
        const D = S.dirty;  // the rows changed since the view last drew
        if (j < D[0]) D[0] = j;
        if (j > D[1]) D[1] = j;
      }
    }
    return out;
  }

  // the fixtures: a height map of their own (the vise's base and jaws as they are, a model from its triangles):
  // a move's tool (its radius from the tip up) or holder (from the stick-out up) over a fixture cell below it
  function fixtures(o) { return o && ((o.boxes && o.boxes.length) || (o.tris && o.tris.length)) ? stock({ ...o, cell: o.cell || 0.5, maxCells: 400000 }) : null; }
  function fixHit(F, sh, p0, p1) {
    const lo = Math.min(p0[2], p1[2]);
    if (!F || lo >= F.zmax) return null;
    const dx = p1[0] - p0[0], dy = p1[1] - p0[1], dz = p1[2] - p0[2], L2 = dx * dx + dy * dy, R = sh.R, RH = Math.max(sh.rh, R), c = F.cell;
    const i0 = Math.max(0, Math.floor((Math.min(p0[0], p1[0]) - RH - F.x0) / c)), i1 = Math.min(F.nx - 1, Math.floor((Math.max(p0[0], p1[0]) + RH - F.x0) / c));
    const j0 = Math.max(0, Math.floor((Math.min(p0[1], p1[1]) - RH - F.y0) / c)), j1 = Math.min(F.ny - 1, Math.floor((Math.max(p0[1], p1[1]) + RH - F.y0) / c));
    let hit = null;
    for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
      const k = j * F.nx + i;
      if (!solid(F, k)) continue;
      const ex = F.x0 + (i + 0.5) * c - p0[0], ey = F.y0 + (j + 0.5) * c - p0[1];
      const ts = L2 > 1e-18 ? Math.min(1, Math.max(0, (ex * dx + ey * dy) / L2)) : 0, qx = ex - dx * ts, qy = ey - dy * ts, q2 = qx * qx + qy * qy;
      if (q2 > RH * RH) continue;
      // the lowest the tool is over this cell: an end of the stretch it is over it (its radius; the holder's)
      const low = (r) => {
        if (L2 <= 1e-18) return lo;
        const b = ex * dx + ey * dy, e2 = ex * ex + ey * ey, rr = Math.sqrt(Math.max(0, b * b - L2 * (e2 - r * r)));
        return Math.min(p0[2] + dz * Math.max(0, (b - rr) / L2), p0[2] + dz * Math.min(1, (b + rr) / L2));
      };
      const z = q2 <= R * R ? low(R) : low(RH);
      if (q2 <= R * R && F.top[k] > z + 1e-6) return { what: "tool", name: F.names ? F.names[k] : "" };
      if (F.top[k] > z + sh.so + 1e-6) hit = hit || { what: "holder", name: F.names ? F.names[k] : "" };
    }
    return hit;
  }

  // ------------------------------------------------------------ a program
  // the run's state: R.i the segment next; run(R, segs, tools, upto, u, checks): as LSTOCK.run
  function begin(o) {
    return { S: stock(o), F: fixtures(o.fixtures), o, i: 0, tool: 0, spin: undefined, rpm: 0, shapes: new Map(), find: [], seen: new Set(),
      stat: { vol: 0, air: 0, cutT: 0, byTool: {} } };
  }
  function shapeOf(R, tools, t) {
    if (!R.shapes.has(t)) R.shapes.set(t, toolShape((tools || []).find((x) => x.t === t) || { t, d: 6 }));
    return R.shapes.get(t);
  }
  function note(R, s, kind, level, text) {
    const key = kind + ":" + s.line;
    if (R.seen.has(key)) return;
    R.seen.add(key);
    R.find.push({ kind, level, line: s.line, tool: s.tool, x: s.x, y: s.y, z: s.z, text });
  }
  function run(R, segs, tools, upto, u = 0, checks = true) {
    const fmt = (v) => (+v).toFixed(2);
    for (; R.i < Math.min(upto, segs.length); R.i++) {
      const s = segs[R.i];
      if (s.k === "tc") { R.tool = s.tool; continue; }
      if (s.k === "m3") { R.spin = true; R.rpm = s.rpm || R.rpm; continue; }
      if (s.k === "m5") { R.spin = false; continue; }
      if (s.k !== "mv") continue;
      const tool = s.tool || R.tool;
      if (!tool) continue;
      const sh = shapeOf(R, tools, tool), p0 = [s.x0, s.y0, s.z0], p1 = [s.x, s.y, s.z];
      const r = cut(R.S, sh, tool, p0, p1, sh.probe);
      const len = Math.hypot(s.x - s.x0, s.y - s.y0, s.z - s.z0), t = s.t || (s.f > 0 ? len / s.f * 60 : 0);
      if (!sh.probe) {
        R.stat.vol += r.vol;
        const bt = R.stat.byTool[tool] || (R.stat.byTool[tool] = { vol: 0, t: 0, mrr: 0, ap: 0, fz: 0 });
        bt.vol += r.vol; bt.ap = Math.max(bt.ap, r.ap);
        if (!s.rapid) {
          if (r.vol > 1e-3) {
            R.stat.cutT += t; bt.t += t;
            if (t > 0) bt.mrr = Math.max(bt.mrr, r.vol / t * 60 / 1000);
            const fl = ((tools || []).find((x) => x.t === tool) || {}).fl;
            if (R.rpm > 0 && fl > 0 && s.f > 0) bt.fz = Math.max(bt.fz, s.f / (R.rpm * fl));
          } else R.stat.air += t;
        }
      }
      if (!checks) continue;
      if (sh.probe) { if (!s.probe && r.vol > 0.01) note(R, s, "probe", "bad", `T${tool}, a probe, into the stock off a probing move`); }
      else {
        if (s.rapid && r.vol > 0.05) note(R, s, "rapid", "bad", `G0 into the stock: T${tool} takes ${fmt(r.vol)} mm³ at rapid`);
        if (r.flutes > 0.05) note(R, s, "flutes", "bad", `T${tool} cuts ${fmt(r.ap)} mm deep, its flutes ${fmt(sh.cl)} long: the shank in the material`);
        if (r.holder > 0.05) note(R, s, "holder", "bad", `T${tool}'s holder in the stock by ${fmt(r.holder)} mm (stick-out ${fmt(sh.so)})`);
        if (!s.rapid && r.vol > 0.01 && R.spin === false) note(R, s, "spindle", "bad", `T${tool} cuts with the spindle stopped`);
        if (r.through > 0.01) note(R, s, "through", "info", `T${tool} through the stock's bottom by ${fmt(r.through)} mm`);
      }
      const h = fixHit(R.F, sh, p0, p1);
      if (h) note(R, s, "fixture", "bad", `T${tool}'s ${h.what === "tool" ? "cutter" : "holder"} into ${h.name || "a fixture"}`);
    }
    if (u > 0 && R.i < segs.length && segs[R.i].k === "mv") {  // the move on: as far as it is
      const s = segs[R.i], tool = s.tool || R.tool, sh = tool && shapeOf(R, tools, tool);
      if (sh && !sh.probe) cut(R.S, sh, tool, [s.x0, s.y0, s.z0], [s.x0 + (s.x - s.x0) * u, s.y0 + (s.y - s.y0) * u, s.z0 + (s.z - s.z0) * u]);
    }
    return R;
  }
  function simulate(o, segs, tools) {
    const R = run(begin(o), segs, tools, segs.length);
    const order = { bad: 0, warn: 1, info: 2 };
    R.find.sort((a, b) => order[a.level] - order[b.level] || a.line - b.line);
    return R;
  }

  return { toolShape, prof, stock, fixtures, fixHit, cut, copy, begin, run, simulate, solid };
})();
if (typeof module !== "undefined") module.exports = MSTOCK;
