/* Portal Mill Console: a mill's material removal (mstock.js) in the console. The stock is the setup's workpiece
   (the built-in block, or a model's triangles), the fixtures' boxes what the tools must miss. The view draws the
   stock as the program cuts it (the simulation's and LinuxCNC's run), coloured by the tool that cut each cell;
   the File page's Preflight checks each program shown (Material removal), the loaded one with a warning when it
   finds a crash risk. Classic script after setup3d.js and mstock.js; a lathe's is lathe.js's. */
"use strict";

// budget: the drawn surface's points, halved while frames take over 50 ms with it shown (a software renderer)
const MREM = { R: null, prog: null, key: "", ver: 0, built: -1, at: 0, mesh: null, checked: null, final: false, through: -1, budget: 40000, last: 0, slow: 0 };
let millRemVer = 0;  // the setup's objects changed (their nodes rebuilt): the stock again

// the setup in the work's coordinates: the workpieces as boxes or triangles, the fixtures as boxes
function millRemSetup() {
  const o = off(), g = st.g92, boxes = [], tris = [], fixtures = { boxes: [], tris: [] };
  if (!R) return { boxes, tris, fixtures };
  R.objGroup.updateMatrixWorld(true);
  const inv = new THREE.Matrix4().copy(R.objGroup.matrixWorld).invert(), w = new THREE.Vector3(o.x + g.x, o.y + g.y, o.z + g.z + ZTOP);
  for (const ob of setup.objects) {
    const n = setup.nodes.get(ob.id); if (!n) continue;
    const m = new THREE.Matrix4(), builtin = ob.src.startsWith("builtin:"), name = ob.name || ob.src;
    n.node.traverse((c) => {  // a built-in's boxes one by one (the vise's base and jaws); a model's triangles
      if (!c.isMesh) return;
      if (!c.geometry.boundingBox) c.geometry.computeBoundingBox();
      m.multiplyMatrices(inv, c.matrixWorld);
      if (builtin) {
        const box = c.geometry.boundingBox.clone().applyMatrix4(m);
        (ob.role === "fixture" ? fixtures.boxes : boxes).push({ name, x0: box.min.x - w.x, y0: box.min.y - w.y, z0: box.min.z - w.z, x1: box.max.x - w.x, y1: box.max.y - w.y, z1: box.max.z - w.z });
      } else {
        const p = c.geometry.attributes.position, v = new THREE.Vector3(), out = new Float32Array(p.count * 3);
        for (let i = 0; i < p.count; i++) { v.fromBufferAttribute(p, i).applyMatrix4(m).sub(w); out.set([v.x, v.y, v.z], i * 3); }
        (ob.role === "fixture" ? fixtures.tris : tris).push(out);
      }
    });
  }
  return { boxes, tris, fixtures };
}
const millRemKey = () => JSON.stringify([setup.objects.map((o) => [o.id, o.src, o.role, o.pos, o.rot, o.scale, o.frame]), off(), st.g92, typeof latheVer !== "undefined" ? latheVer : 0, millRemVer, setup.nodes.size]);
const millRemOpts = () => ({ ...millRemSetup(), cell: 0.25, maxCells: 250000 });

// every frame: the stock as far as the program is; all of it when done or the finished part is asked for
function millRemFollow(now) {
  if (!R || R.type === "lathe" || typeof MSTOCK === "undefined") return;
  let to = 0, u = 0;
  if (st.motion === prog && busy()) {
    to = st.seg;
    const s = prog.segs[to];
    if (!LIVE && s && s.t > 0) u = clamp((st.t - s.t0) / s.t, 0, 1);
  } else if (st.run === "done" || MREM.final) to = prog.segs.length;
  const key = millRemKey();
  if (!MREM.R || MREM.prog !== prog || MREM.key !== key || to < MREM.R.i) {
    if (MREM.prog !== prog || MREM.key !== key) millCheckLoaded();
    MREM.R = MSTOCK.begin(millRemOpts()); MREM.prog = prog; MREM.key = key; MREM.ver++; MREM.through = -1;
  }
  if (to > MREM.R.i || u > 0) {
    const lim = Math.min(to, MREM.R.i + 3000);
    MSTOCK.run(MREM.R, prog.segs, st.tools, lim, lim === to ? u : 0, false);
    MREM.ver++;
  }
  const shown = to > 0 || u > 0;
  if (shown && MREM.mesh && MREM.last) {
    MREM.slow = now - MREM.last > 50 ? MREM.slow + 1 : Math.max(0, MREM.slow - 1);
    if (MREM.slow > 20 && MREM.budget > 2500) { MREM.budget /= 2; MREM.slow = 0; MREM.built = -1; }
  }
  MREM.last = now;
  if (MREM.mesh) MREM.mesh.visible = shown && st.show.work !== false;
  for (const ob of setup.objects) if (ob.role === "workpiece") { const n = setup.nodes.get(ob.id); if (n) n.node.visible = !shown && st.show.work !== false && ob.look !== "hidden"; }
  if (shown && MREM.ver !== MREM.built && (now - MREM.at > 150 || !busy())) { millRemMesh(); MREM.built = MREM.ver; MREM.at = now; }
}
// the height map as a surface, and walls down to its bottom around it. Drawn coarser than it is cut (at most
// ~40 000 points: a tablet's GPU, a software renderer): a point the lowest of the cells under it, so a pocket or a
// hole still shows. After the first time only the rows a cut changed (normals from the heights around them).
function millRemMesh() {
  const S = MREM.R.S, sd = Math.max(1, Math.ceil(Math.sqrt(S.nx * S.ny / MREM.budget))), nx = Math.ceil(S.nx / sd), ny = Math.ceil(S.ny / sd), N = nx * ny;
  const o = off(), g = st.g92, c = S.cell * sd;
  let full = false;
  if (!MREM.mesh || MREM.mesh.userData.S !== S || MREM.mesh.userData.sd !== sd) {
    if (MREM.mesh) { MREM.mesh.parent.remove(MREM.mesh); MREM.mesh.geometry.dispose(); }
    const geo = new THREE.BufferGeometry(), ring = [];
    for (let i = 0; i < nx; i++) ring.push(i);
    for (let j = 1; j < ny; j++) ring.push(j * nx + nx - 1);
    for (let i = nx - 2; i >= 0; i--) ring.push((ny - 1) * nx + i);
    for (let j = ny - 2; j > 0; j--) ring.push(j * nx);
    const M2 = N + 2 * ring.length, pos = new Float32Array(M2 * 3), col = new Float32Array(M2 * 3), nor = new Float32Array(M2 * 3);
    for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
      const k = j * nx + i;
      pos[k * 3] = S.x0 + Math.min(S.nx * S.cell, (i + 0.5) * c); pos[k * 3 + 1] = S.y0 + Math.min(S.ny * S.cell, (j + 0.5) * c);
    }
    ring.forEach((k, r) => {  // the walls: outward, by the side they are on
      const i = k % nx, j = (k / nx) | 0, n = j === 0 ? [0, -1, 0] : i === nx - 1 ? [1, 0, 0] : j === ny - 1 ? [0, 1, 0] : [-1, 0, 0];
      nor.set(n, (N + r) * 3); nor.set(n, (N + ring.length + r) * 3);
    });
    for (const [n, a] of [["position", pos], ["color", col], ["normal", nor]]) geo.setAttribute(n, new THREE.BufferAttribute(a, 3));
    geo.userData = { ring, N, h: new Float32Array(N), bot: new Float32Array(N), sol: new Uint8Array(N), tool: new Uint8Array(N) };
    const mat = new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0.4, roughness: 0.5 });
    MREM.mesh = new THREE.Mesh(geo, mat); MREM.mesh.userData.S = S; MREM.mesh.userData.sd = sd; MREM.through = -1;
    R.workG.add(MREM.mesh);
    full = true;
  }
  MREM.mesh.position.set(o.x + g.x, o.y + g.y, o.z + g.z + ZTOP);
  MREM.mesh.userData.objId = (setup.objects.find((x) => x.role === "workpiece") || {}).id;  // a click on it: the workpiece (setup3d.js)
  const geo = MREM.mesh.geometry, pos = geo.attributes.position.array, col = geo.attributes.color.array, nor = geo.attributes.normal.array, U = geo.userData, ring = U.ring;
  const j0 = full ? 0 : Math.max(0, Math.floor(S.dirty[0] / sd) - 1), j1 = full ? ny - 1 : Math.min(ny - 1, Math.floor(S.dirty[1] / sd) + 1);
  S.dirty = [Infinity, -Infinity];
  if (j1 < j0) return;
  // each point from its block of cells: the lowest solid top (a hole: none solid)
  let holes = full;
  for (let j = j0; j <= j1; j++) for (let i = 0; i < nx; i++) {
    let lo = Infinity, bot = Infinity, t = 0, any = false;
    for (let jj = j * sd; jj < Math.min(S.ny, j * sd + sd); jj++) for (let ii = i * sd; ii < Math.min(S.nx, i * sd + sd); ii++) {
      const k = jj * S.nx + ii;
      if (S.tool[k] === 255) continue;
      bot = Math.min(bot, S.bot[k]);
      if (!MSTOCK.solid(S, k)) continue;
      any = true;
      if (S.top[k] < lo) { lo = S.top[k]; t = S.top[k] < S.top0[k] - 1e-6 ? S.tool[k] : 0; }
    }
    const k = j * nx + i, sol = any ? 1 : 0;
    if (sol !== U.sol[k]) holes = true;
    U.sol[k] = sol; U.bot[k] = Number.isFinite(bot) ? bot : 0; U.h[k] = any ? lo : U.bot[k]; U.tool[k] = t;
  }
  const base = [0.78, 0.81, 0.85], H = (i, j) => U.h[Math.min(ny - 1, Math.max(0, j)) * nx + Math.min(nx - 1, Math.max(0, i))];
  for (let j = Math.max(0, j0 - 1); j <= Math.min(ny - 1, j1 + 1); j++) for (let i = 0; i < nx; i++) {
    const k = j * nx + i, t = U.tool[k];
    pos[k * 3 + 2] = U.h[k];
    const gx = (H(i + 1, j) - H(i - 1, j)) / (2 * c), gy = (H(i, j + 1) - H(i, j - 1)) / (2 * c), l = Math.hypot(gx, gy, 1);
    nor[k * 3] = -gx / l; nor[k * 3 + 1] = -gy / l; nor[k * 3 + 2] = 1 / l;
    let cc = base;
    if (t) { const tc = toolColors[t] || millToolCol(t); cc = [tc[0] * 0.75 + base[0] * 0.25, tc[1] * 0.75 + base[1] * 0.25, tc[2] * 0.75 + base[2] * 0.25]; }
    col.set(cc, k * 3);
  }
  ring.forEach((k, r) => {
    const a = (N + r) * 3, b = (N + ring.length + r) * 3;
    pos[a] = pos[b] = pos[k * 3]; pos[a + 1] = pos[b + 1] = pos[k * 3 + 1];
    pos[a + 2] = U.h[k]; pos[b + 2] = U.bot[k];
    col.set(base, a); col.set(base, b);
  });
  // the triangles: a square where all four corners are stock (again when a hole opened)
  if (holes) {
    const idx = [];
    for (let j = 0; j + 1 < ny; j++) for (let i = 0; i + 1 < nx; i++) {
      const a = j * nx + i, b = a + 1, cc = a + nx, d = cc + 1;
      if (U.sol[a] && U.sol[b] && U.sol[cc] && U.sol[d]) idx.push(a, b, d, a, d, cc);
    }
    for (let r = 0; r < ring.length; r++) {
      const a = N + r, b = N + (r + 1) % ring.length, cc = N + ring.length + r, d = N + ring.length + (r + 1) % ring.length;
      if (U.sol[ring[r]] || U.sol[ring[(r + 1) % ring.length]]) idx.push(a, cc, d, a, d, b);
    }
    geo.setIndex(idx);
  }
  const r0 = full ? 0 : Math.max(0, j0 - 1) * nx * 3;
  for (const at of [geo.attributes.position, geo.attributes.color, geo.attributes.normal]) {
    at.updateRange.offset = r0; at.updateRange.count = full ? -1 : (N + 2 * ring.length) * 3 - r0;
    at.needsUpdate = true;
  }
  if (full) geo.computeBoundingSphere();
}
function millToolCol(t) { const c = new THREE.Color().setHSL((t * 0.137) % 1, 0.75, 0.6); return [c.r, c.g, c.b]; }

// a program's whole run: its findings and numbers, kept on it until the setup or the tools change
async function millCheck(p) {
  const key = millRemKey();
  if (p.mremoval && p.mremoval.key === key) return p.mremoval;
  const R2 = MSTOCK.begin(millRemOpts());
  for (let i = 0; i < p.segs.length;) {
    i = Math.min(p.segs.length, i + 15000);
    MSTOCK.run(R2, p.segs, st.tools, i);
    if (i < p.segs.length) await nextFrame();
  }
  const order = { bad: 0, warn: 1, info: 2 };
  R2.find.sort((a, b) => order[a.level] - order[b.level] || a.line - b.line);
  R2.key = key; p.mremoval = R2;
  return R2;
}
function millCheckLoaded() {
  const p = prog;
  if (!p || !p.segs.length || !setup.nodes.size) { millMarks([]); return; }
  millCheck(p).then((r) => {
    if (p !== prog) return;
    millMarks(r.find);
    const bad = r.find.filter((f) => f.level === "bad");
    if (bad.length && MREM.checked !== p) msg(`${p.name}: ${bad.length} crash risk${bad.length > 1 ? "s" : ""} in the simulation. ${nline(bad[0].line)}: ${bad[0].text}. File page, Preflight.`, "warn");
    MREM.checked = p;
    if (st.page === "file" && $("fl-rem")) renderPreflight(p);
  });
}
function millMarks(find) {
  if (!R) return;
  if (!R.mfindG) { R.mfindG = new THREE.Group(); R.workG.add(R.mfindG); }
  R.mfindG.clear();
  const o = off(), g = st.g92, col = { bad: 0xff4d5e, warn: 0xffb547, info: 0x4cc9ff };
  for (const f of find.slice(0, 200)) {
    const m = new THREE.Mesh(R.markGeo || (R.markGeo = new THREE.SphereGeometry(1.4, 12, 8)), new THREE.MeshBasicMaterial({ color: col[f.level], depthTest: false }));
    m.renderOrder = 12; m.position.set(f.x + o.x + g.x, f.y + o.y + g.y, f.z + o.z + g.z + ZTOP); R.mfindG.add(m);
  }
}
// findings for a card: one item a kind and tool (its first line; how many lines more), the worst first
function remFindList(find) {
  const groups = new Map();
  for (const f of find) { const k = f.kind + ":" + f.tool, g = groups.get(k); if (g) g.n++; else groups.set(k, { f, n: 1 }); }
  const items = [...groups.values()];
  return `<ul class="rem-find">${items.slice(0, 14).map(({ f, n }) => `<li class="${f.level}" data-line="${f.line}"><b>${nline(f.line)}</b> ${esc(f.text)}${n > 1 ? ` <span class="muted">· on ${n} lines</span>` : ""}</li>`).join("")}${items.length > 14 ? `<li class="muted">… ${items.length - 14} more</li>` : ""}</ul>`;
}
// the Preflight card
function millRemCard(p) {
  const el = document.createElement("div"); el.className = "card"; el.id = "fl-rem";
  const head = (pill, cls) => `<h3>Material removal <span class="pill ${cls}">${pill}</span></h3>`;
  if (!setup.objects.some((o) => o.role === "workpiece")) { el.innerHTML = head("NO STOCK", "info") + `<p class="note">No workpiece in the 3D setup: add one (the stock) to check the program against it.</p>`; return el; }
  const r = p.mremoval && p.mremoval.key === millRemKey() ? p.mremoval : null;
  if (!r) { el.innerHTML = head("CHECKING…", "info"); millCheck(p).then(() => { if ($("fl-rem") === el && st.page === "file") el.replaceWith(millRemCard(p)); }); return el; }
  const nb = r.find.filter((f) => f.level === "bad").length, nw = r.find.filter((f) => f.level === "warn").length, S = r.S;
  const cm3 = (v) => (v / 1000).toFixed(1) + " cm³";
  const tools = Object.entries(r.stat.byTool).filter(([, b]) => b.vol > 0.5).map(([t, b]) => `<dt>T${t}</dt><dd>${cm3(b.vol)} · ${b.mrr.toFixed(1)}/min · ap ${b.ap.toFixed(2)}${b.fz > 0 ? ` · fz ${b.fz.toFixed(3)}` : ""}</dd>`).join("");
  let zl = Infinity; for (let k = 0; k < S.top.length; k++) if (MSTOCK.solid(S, k)) zl = Math.min(zl, S.top[k]);
  el.innerHTML = head(nb ? `${nb} CRASH RISK${nb > 1 ? "S" : ""}` : nw ? `${nw} WARNING${nw > 1 ? "S" : ""}` : "CLEAN", nb ? "bad" : nw ? "warn" : "ok") +
    `<dl class="kv"><dt>Stock</dt><dd>${(S.nx * S.cell).toFixed(0)} × ${(S.ny * S.cell).toFixed(0)} · ${S.cell.toFixed(2)} mm cells</dd><dt>Removed</dt><dd>${cm3(r.stat.vol)} of ${cm3(S.vol0)}</dd><dt>Cut · air</dt><dd>${mmss(r.stat.cutT)} · ${mmss(r.stat.air)}</dd>${tools}
      <dt>Deepest floor</dt><dd>${Number.isFinite(zl) ? "Z" + zl.toFixed(2) : "—"}</dd></dl>` +
    (r.find.length ? remFindList(r.find)
      : `<p class="note">No rapid into the stock; every cut within the flutes; holders and tools clear of the stock and the fixtures.</p>`) +
    `<div class="row"><button class="btn sm" id="ms-final" aria-pressed="${MREM.final}">${MREM.final ? "Showing the finished part" : "Show the finished part"}</button></div>`;
  return el;
}
document.addEventListener("click", (e) => {
  if (e.target.id !== "ms-final") return;
  MREM.final = !MREM.final; e.target.setAttribute("aria-pressed", String(MREM.final)); e.target.textContent = MREM.final ? "Showing the finished part" : "Show the finished part";
});
