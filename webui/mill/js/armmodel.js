/* Portal Mill Console: an arm's model, as linuxcnc/vis3d reads it, for the browser. No DOM, no THREE: arm.js draws
   with it, node's tests require it (test/armmodel_test.js).

   The files (the server's arm_model: {files: {name: text | {b64}}, model, cell, tools}): the .vis model and what it
   includes (arm_sim's .arm files: their dh, limits and body lines), armguard's cell (obstacles; the tool table with
   the tools' tcp, dock and bodies). Includes resolve from the including file's folder, as vis3d's (the server writes
   them as @<name>: the file's name in the bundle). Lengths in the
   file's unit (`units m`, the default, or `units mm`), angles in degrees; a tool's tcp and dock are mm.

   The arm: Craig (modified) DH, RotX(alpha) TransX(a) RotZ(theta) TransZ(d), as genserkins / arm_sim; `joints
   theta`: the joint values are the DH theta (LinuxCNC's), else offsets from theta0. A pose's A B C: R = Rz(C) Ry(B)
   Rx(A), as genserkins. Matrices: 16 numbers, column-major (THREE.Matrix4.fromArray), metres. */
"use strict";

const ARMMODEL = (() => {
  const D2R = Math.PI / 180;
  // ------------------------------------------------------------ 4x4 (column-major: m[col * 4 + row])
  const I = () => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
  function mul(a, b) {
    const r = new Array(16);
    for (let c = 0; c < 4; c++) for (let rw = 0; rw < 4; rw++) {
      let s = 0;
      for (let k = 0; k < 4; k++) s += a[k * 4 + rw] * b[c * 4 + k];
      r[c * 4 + rw] = s;
    }
    return r;
  }
  const T = (x, y, z) => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1];
  const RX = (a) => { const c = Math.cos(a), s = Math.sin(a); return [1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1]; };
  const RY = (a) => { const c = Math.cos(a), s = Math.sin(a); return [c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1]; };
  const RZ = (a) => { const c = Math.cos(a), s = Math.sin(a); return [c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]; };
  const S = (k) => [k, 0, 0, 0, 0, k, 0, 0, 0, 0, k, 0, 0, 0, 0, 1];
  const rpy = (a, b, c) => mul(RZ(c), mul(RY(b), RX(a)));                     // radians
  const dh = (a, al, d, th) => mul(mul(RX(al), T(a, 0, 0)), mul(RZ(th), T(0, 0, d)));
  const point = (m, p) => [m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13], m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]];
  // R's A B C [deg] (shared/robot/kin.c rb_rpy_from_R)
  function rpyOf(m) {
    const R = (r, c) => m[c * 4 + r], cb = Math.hypot(R(0, 0), R(1, 0));
    const b = Math.atan2(-R(2, 0), cb);
    const a = cb > 1e-9 ? Math.atan2(R(2, 1), R(2, 2)) : 0, c = cb > 1e-9 ? Math.atan2(R(1, 0), R(0, 0)) : Math.atan2(-R(0, 1), R(1, 1));
    return [a / D2R, b / D2R, c / D2R];
  }
  // a segment's frame: its z from p to q, its middle at the origin (a capsule's)
  function segFrame(p, q) {
    const d = [q[0] - p[0], q[1] - p[1], q[2] - p[2]], len = Math.hypot(...d);
    const z = len > 1e-12 ? d.map((v) => v / len) : [0, 0, 1];
    const t = Math.abs(z[2]) < 0.9 ? [0, 0, 1] : [1, 0, 0];
    let x = [t[1] * z[2] - t[2] * z[1], t[2] * z[0] - t[0] * z[2], t[0] * z[1] - t[1] * z[0]];
    const lx = Math.hypot(...x); x = x.map((v) => v / lx);
    const y = [z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]];
    return { M: [...x, 0, ...y, 0, ...z, 0, (p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2, 1], len };
  }

  // ------------------------------------------------------------ the files
  const dirOf = (n) => (n.includes("/") ? n.slice(0, n.lastIndexOf("/") + 1) : "");
  // a file an include or a mesh names: the server's @<name>, else a path from the including file's folder
  const ref = (r, dir) => (r.startsWith("@") ? r.slice(1) : norm(r.startsWith("/") ? r : dir + r));
  function norm(p) {
    const out = [];
    for (const s of p.split("/")) {
      if (s === "" || s === ".") continue;
      if (s === ".." && out.length && out[out.length - 1] !== "..") out.pop(); else out.push(s);
    }
    return out.join("/");
  }
  // bundle: {files, model, cell, tools} -> the model {n, dh, q_theta, lim, tcp, mats, parts, tools, obs, camera, errors}
  function load(bundle) {
    const files = bundle.files || {};
    const v = { prevent: 0, n: 0, dh: [], q_theta: false, lim: [], tcp: [0, 0, 0], mats: {}, parts: [], tools: {}, obs: [], camera: null, errors: [], unit: 1 };
    const mat = (n) => v.mats[n] || (v.mats[n] = { color: [0.7, 0.72, 0.75], metal: 0.2, rough: 0.5, emissive: 0, alpha: 1, accent: false });
    mat("body"); mat("tool"); mat("obstacle").alpha = 0.35; mat("stand").color = [0.25, 0.28, 0.31];
    let tool = null;
    const toolOf = (id) => v.tools[id] || (v.tools[id] = { id, name: "T" + id, tcp: null, dock: null, parts: [], bodies: [] });
    function file(name, cell, depth) {
      const text = files[name];
      if (typeof text !== "string") { v.errors.push(`${name}: not in the model's files`); return; }
      if (depth > 8) { v.errors.push(`${name}: included too deep`); return; }
      const dir = dirOf(name);
      text.split("\n").forEach((raw, ln) => {
        const tok = raw.split("#")[0].trim().split(/\s+/).filter(Boolean);
        if (!tok.length) return;
        try { line(tok, cell, dir, depth); } catch (e) { v.errors.push(`${name}:${ln + 1}: ${e.message}`); }
      });
    }
    const num = (t, i) => { const x = parseFloat(t[i]); if (!Number.isFinite(x)) throw new Error(`a number: ${t[i]}`); return x; };
    function opts(t, i0) {  // after a part's shape: at, rpy, axis, material, scale, tool
      const o = { at: [0, 0, 0], rpy: [0, 0, 0], axis: "z", mat: "body", scale: 1, tool: 0 };
      for (let i = i0; i < t.length; i++) {
        if (t[i] === "at") { o.at = [num(t, i + 1), num(t, i + 2), num(t, i + 3)].map((x) => x * v.unit); i += 3; }
        else if (t[i] === "rpy") { o.rpy = [num(t, i + 1), num(t, i + 2), num(t, i + 3)].map((x) => x * D2R); i += 3; }
        else if (t[i] === "axis") o.axis = t[++i];
        else if (t[i] === "material") o.mat = t[++i];
        else if (t[i] === "scale") o.scale = num(t, ++i);
        else if (t[i] === "tool") o.tool = parseInt(t[++i], 10);
        else throw new Error(`unexpected "${t[i]}"`);
      }
      return o;
    }
    const place = (o) => {
      let M = mul(T(...o.at), rpy(...o.rpy));
      if (o.axis === "x") M = mul(M, RY(Math.PI / 2)); else if (o.axis === "y") M = mul(M, RX(-Math.PI / 2));
      return M;
    };
    function line(t, cell, dir, depth) {
      const k = t[0], u = v.unit;
      if (k === "prevent") { v.prevent = num(t, 1); return; }  // armguard's clearance limit [mm]
      if (cell && !["obstacle", "include", "tool", "tcp", "dock", "body"].includes(k)) return;  // a cell: its obstacles and tools
      if (k === "units") v.unit = t[1] === "mm" ? 0.001 : 1;
      else if (k === "dh") {
        const j = parseInt(t[1], 10) - 1;
        v.dh[j] = { a: num(t, 2) * u, alpha: num(t, 3) * D2R, d: num(t, 4) * u, theta0: num(t, 5) * D2R, prismatic: (v.dh[j] || {}).prismatic || false };
        v.n = Math.max(v.n, j + 1);
      } else if (k === "joints") v.q_theta = t[1] === "theta";
      else if (k === "prismatic") { const j = parseInt(t[1], 10) - 1; v.dh[j] = { ...(v.dh[j] || { a: 0, alpha: 0, d: 0, theta0: 0 }), prismatic: true }; }
      else if (k === "limits") v.lim[parseInt(t[1], 10) - 1] = [num(t, 2), num(t, 3)];
      else if (k === "tool") { tool = toolOf(parseInt(t[1], 10)); if (t[2]) tool.name = t[2]; }
      else if (k === "tcp" && tool) tool.tcp = [num(t, 1), num(t, 2), num(t, 3)].map((x) => x * 0.001);  // a tool's: mm
      else if (k === "tcp") { if (!cell) v.tcp = [num(t, 1), num(t, 2), num(t, 3)].map((x) => x * u); }
      else if (k === "dock") {
        if (!tool) throw new Error("dock before a tool line");
        tool.dock = mul(T(num(t, 1) * 0.001, num(t, 2) * 0.001, num(t, 3) * 0.001), rpy(num(t, 4) * D2R, num(t, 5) * D2R, num(t, 6) * D2R));
      } else if (k === "material") {
        const m = mat(t[1]);
        for (let i = 2; i < t.length; i++) {
          if (t[i] === "color") { m.color = [num(t, i + 1), num(t, i + 2), num(t, i + 3)]; i += 3; }
          else if (t[i] === "metal") m.metal = num(t, ++i);
          else if (t[i] === "rough") m.rough = num(t, ++i);
          else if (t[i] === "emissive") m.emissive = num(t, ++i);
          else if (t[i] === "alpha") m.alpha = num(t, ++i);
          else if (t[i] === "accent") m.accent = true;
        }
      } else if (k === "part" || k === "body") {
        const link = parseInt(t[1], 10), shape = t[2], isBody = k === "body";
        const nargs = { box: 3, cylinder: 2, ring: 3, sphere: isBody ? 4 : 1, capsule: 7, mesh: 1 }[shape];
        if (nargs === undefined) throw new Error(`unknown shape ${shape}`);
        const o = opts(t, 3 + nargs);
        if (cell && !o.tool) return;  // the arm's bodies: the model has them
        const a = t.slice(3, 3 + nargs), n = (i) => num(a, i) * u;
        const p = { link, mat: isBody ? (o.tool ? "tool" : "body") : o.mat, tool: o.tool, body: isBody };
        if (shape === "box") Object.assign(p, { shape, M: place(o), size: [n(0), n(1), n(2)] });
        else if (shape === "cylinder") Object.assign(p, { shape: "tube", M: place(o), r: n(0), ri: 0, h: n(1) });
        else if (shape === "ring") Object.assign(p, { shape: "tube", M: place(o), r: n(0), ri: n(0) - n(1), h: n(2) });
        else if (shape === "sphere") {
          if (isBody) o.at = [n(0), n(1), n(2)];
          Object.assign(p, { shape: "capsule", M: place(o), r: isBody ? n(3) : n(0), len: 0 });
        } else if (shape === "capsule") { const f = segFrame([n(0), n(1), n(2)], [n(3), n(4), n(5)]); Object.assign(p, { shape, M: f.M, r: n(6), len: f.len }); }
        else Object.assign(p, { shape: "mesh", M: mul(place(o), S(o.scale * u)), file: ref(a[0], dir) });
        if (o.tool) { const tt = toolOf(o.tool); (isBody ? tt.bodies : tt.parts).push(p); }
        else v.parts.push(p);
      } else if (k === "obstacle") {
        const a = t.slice(3), s = t[2];
        if (s === "plane") return;  // the floor: the grid draws it
        if (s === "box") {
          const r = a.length >= 9 ? [num(a, 6), num(a, 7), num(a, 8)].map((x) => x * D2R) : [0, 0, 0];
          v.obs.push({ name: t[1], shape: "box", M: mul(T(num(a, 0) * u, num(a, 1) * u, num(a, 2) * u), rpy(...r)), size: [2 * num(a, 3) * u, 2 * num(a, 4) * u, 2 * num(a, 5) * u] });
        } else if (s === "capsule") { const f = segFrame([0, 1, 2].map((i) => num(a, i) * u), [3, 4, 5].map((i) => num(a, i) * u)); v.obs.push({ name: t[1], shape: "capsule", M: f.M, r: num(a, 6) * u, len: f.len }); }
        else if (s === "sphere") v.obs.push({ name: t[1], shape: "capsule", M: T(num(a, 0) * u, num(a, 1) * u, num(a, 2) * u), r: num(a, 3) * u, len: 0 });
      } else if (k === "camera") v.camera = { yaw: num(t, 1), pitch: num(t, 2), dist: num(t, 3) * u };
      else if (k === "include") {
        const keep = v.unit, keepTool = tool;
        v.unit = 1; tool = null;  // an included file starts in metres, as arm_sim's
        file(ref(t[1], dir), cell, depth + 1);
        v.unit = keep; tool = keepTool;
      }
    }
    if (bundle.model) file(bundle.model, false, 0);
    for (const extra of [bundle.cell, bundle.tools]) if (extra && files[extra] !== undefined) { v.unit = 1; tool = null; file(extra, true, 0); }
    return v;
  }

  // ------------------------------------------------------------ the arm's frames
  // q [deg; a prismatic joint's: the model's unit as m] -> each link's frame (frames[0] the base, frames[j] after joint j)
  function frames(v, q) {
    const out = [I()];
    let M = I();
    for (let j = 0; j < v.n; j++) {
      const L = v.dh[j] || { a: 0, alpha: 0, d: 0, theta0: 0 };
      const qj = (q[j] || 0) * (L.prismatic ? 1 : D2R);
      const th = L.prismatic ? L.theta0 : (v.q_theta ? qj : qj + L.theta0), d = L.prismatic ? L.d + qj : L.d;
      M = mul(M, dh(L.a, L.alpha, d, th));
      out.push(M);
    }
    return out;
  }
  // the flange's pose (or a tool point's, tcp [m] in the flange frame): {xyz [mm], abc [deg], M}
  function pose(v, q, tcp) {
    const F = frames(v, q), M = tcp ? mul(F[v.n], T(...tcp)) : F[v.n];
    return { xyz: [M[12] * 1000, M[13] * 1000, M[14] * 1000], abc: rpyOf(M), M };
  }
  // the joints that put the flange (tcp) at {xyz [mm], abc [deg]}, from q0 (damped least squares on a numeric
  // Jacobian): {q, ok, err [mm]}. The simulation's: LinuxCNC has its own kinematics
  function ik(v, target, q0, tcp, iters = 60) {
    const n = v.n, q = q0.slice(0, n), Rt = rpy(...target.abc.map((x) => x * D2R)), pt = target.xyz.map((x) => x / 1000);
    const errOf = (qq) => {
      const M = pose(v, qq, tcp).M;
      // the orientation error: the rotation vector of Rt * R^T (small angles: its skew part)
      const R = (m, r, c) => m[c * 4 + r];
      const E = (r, c) => R(Rt, r, 0) * R(M, c, 0) + R(Rt, r, 1) * R(M, c, 1) + R(Rt, r, 2) * R(M, c, 2);
      return [pt[0] - M[12], pt[1] - M[13], pt[2] - M[14], 0.5 * (E(2, 1) - E(1, 2)) * 0.5, 0.5 * (E(0, 2) - E(2, 0)) * 0.5, 0.5 * (E(1, 0) - E(0, 1)) * 0.5];
    };
    let e = errOf(q);
    for (let it = 0; it < iters; it++) {
      const ne = Math.hypot(e[0], e[1], e[2]) * 1000 + Math.hypot(e[3], e[4], e[5]) * 1000;
      if (ne < 1e-4) return { q, ok: true, err: ne };
      const J = [];
      for (let j = 0; j < n; j++) {
        const h = 1e-6 / D2R, qq = q.slice(); qq[j] += h;
        const ej = errOf(qq);
        J.push(e.map((x, i) => (x - ej[i]) / h));  // d(pose)/dq: -d(err)/dq
      }
      // (J J^T + l^2 I) y = e; dq = J^T y
      const A = [...Array(6)].map((_, r) => [...Array(6)].map((__, c) => J.reduce((s, col) => s + col[r] * col[c], 0) + (r === c ? 1e-8 : 0)));
      const y = solve(A, e.slice());
      if (!y) break;
      for (let j = 0; j < n; j++) q[j] += J[j].reduce((s, x, i) => s + x * y[i], 0);
      e = errOf(q);
    }
    const ne = Math.hypot(e[0], e[1], e[2]) * 1000;
    return { q, ok: ne < 0.01 && Math.hypot(e[3], e[4], e[5]) < 1e-5, err: ne };
  }
  function solve(A, b) {  // Gauss, partial pivoting
    const n = b.length;
    for (let c = 0; c < n; c++) {
      let p = c; for (let r = c + 1; r < n; r++) if (Math.abs(A[r][c]) > Math.abs(A[p][c])) p = r;
      if (Math.abs(A[p][c]) < 1e-18) return null;
      [A[c], A[p]] = [A[p], A[c]]; [b[c], b[p]] = [b[p], b[c]];
      for (let r = c + 1; r < n; r++) { const f = A[r][c] / A[c][c]; for (let k = c; k < n; k++) A[r][k] -= f * A[c][k]; b[r] -= f * b[c]; }
    }
    const x = new Array(n);
    for (let r = n - 1; r >= 0; r--) { let s = b[r]; for (let k = r + 1; k < n; k++) s -= A[r][k] * x[k]; x[r] = s / A[r][r]; }
    return x;
  }

  return { load, frames, pose, ik, mul, T, rpy, rpyOf, point, norm, I };
})();
if (typeof module !== "undefined") module.exports = ARMMODEL;
