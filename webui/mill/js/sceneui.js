/* Portal Mill Console: importing a CAM setup's scene dump (js/scene.js reads it) into the 3D setup, and
   the File page's "CAM scene" panel. On a machine millweb stores the models under scenes/<name>/ in the
   program folder and every browser gets the new setup; in the simulation it stays in the page.
   Classic script after setup3d.js: shares its top-level names (setup, parseModel, setupLoad ...). */
"use strict";

let sceneLast = null;  // this page's last import: {folder, scene, warnings}

async function sceneImportFile(f) {
  if (!/\.zip$/i.test(f.name)) { msg(`${f.name}: a CAM scene is a .zip (scene.json and its STL files)`, "warn"); return; }
  if (f.size > SCENE.MAX_ZIP) { msg(`${f.name}: ${(f.size / 1048576).toFixed(1)} MB; scenes up to ${SCENE.MAX_ZIP >> 20} MB`, "warn"); return; }
  busyShow(`Reading ${f.name}…`, null, "scene");
  let buf, r;
  try { buf = await f.arrayBuffer(); r = await SCENE.read(buf); } catch (e) { busyHide("scene"); msg(`${f.name}: ${e.message}`, "warn"); return; }
  busyHide("scene");
  const folder = SCENE.folderName(f.name);
  const there = LIVE ? LIVE.modelFiles().some((n) => n.startsWith(folder + "/")) : [...setup.geoms.keys()].some((n) => n.startsWith(folder + "/"));
  const keep = await sceneAsk(f.name, r, folder, there);
  if (keep === null) return;
  // the models' geometry now, so the view needn't fetch back what it just sent
  for (const [n, b] of Object.entries(r.files)) {
    try { setup.geoms.set(`${folder}/${n}`, parseModel(n, b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength))); } catch (e) { msg(`${n}: ${e.message}`, "warn"); return; }
  }
  let warnings = r.warnings;
  if (LIVE) {
    const label = `Importing ${folder}`;
    const res = await LIVE.sceneImport(folder.slice(7), buf, keep, there, (fr) => busyShow(fr < 1 ? label : `${folder}: the machine stores it…`, fr < 1 ? fr : null, "scene"));
    busyHide("scene");
    if (!res.ok) return;  // act() said why
    warnings = res.warnings || warnings;
  } else {
    setupLoad([...(keep ? setup.objects.filter((o) => o.role === "fixture") : []), ...SCENE.objects(r.scene, folder)], null);
  }
  sceneLast = { folder, scene: r.scene, warnings };
  msg(`Imported ${folder}: stock ${r.scene.stock.size.join(" x ")} ${r.scene.units}, ${SCENE.copies(r.scene)} part${SCENE.copies(r.scene) === 1 ? "" : "s"}${warnings.length ? ` (${warnings.length} note${warnings.length === 1 ? "" : "s"})` : ""}`, warnings.length ? "warn" : "ok");
  sceneView();
  sceneRender();
}

// the camera onto the scene's stock (the "part" view frames the built-in block, not a 500 mm plate)
function sceneView() {
  const n = R && setup.nodes.get("scene-stock"); if (!n) return;
  const b = new THREE.Box3().setFromObject(n.node), c = b.getCenter(new THREE.Vector3()), sz = b.getSize(new THREE.Vector3()), d = Math.max(sz.x, sz.y, 100) * 1.1;
  $$("[data-cam]").forEach((x) => x.setAttribute("aria-pressed", "false"));
  R.follow = false;
  R.camAnim = { from: [R.cam.position.toArray(), R.controls ? R.controls.target.toArray() : [c.x, c.y, c.z]], to: [[c.x - 0.45 * d, c.y - 0.95 * d, c.z + 0.85 * d], [c.x, c.y, c.z]], t: 0 };
}

// the import's dialog: what the scene holds; resolves to keep-the-fixtures (true / false), or null (cancelled)
function sceneAsk(file, r, folder, there) {
  const s = r.scene, st = s.stock, u = s.units;
  const rows = s.parts.map((p) => `<tr><td>${esc(p.name)}</td><td class="num">${p.copies.length}</td><td class="muted">${p.up === "-z" ? "upside down" : ""}</td></tr>`).join("");
  $("sc-dlg-b").innerHTML = `
    <p><b>${esc(file)}</b> → <span class="mono">${esc(folder)}/</span>${there ? ` <span style="color:var(--warn)">(replaces what is there)</span>` : ""}</p>
    <p>Stock <b>${st.size.join(" x ")} ${u}</b>${st.material ? ` · ${esc(st.material)}` : ""} · work zero <b>${esc((s.work_zero || "not given").replace(/_/g, " "))}</b></p>
    <table class="sc-parts"><tr><th>Part</th><th>Copies</th><th></th></tr>${rows || `<tr><td colspan="3" class="muted">no parts</td></tr>`}</table>
    ${r.warnings.length ? `<ul class="sc-warn">${r.warnings.map((w) => `<li>${esc(w)}</li>`).join("")}</ul>` : ""}
    <p class="note">The 3D setup becomes the scene: the stock as the workpiece, each copy a see-through finished part, all placed in the work's coordinates as the CAM placed them.</p>`;
  $("sc-keep").checked = false;
  $("sc-dlg").hidden = false;
  $("sc-ok").focus();
  return new Promise((res) => {
    const done = (v) => { $("sc-dlg").hidden = true; $("sc-ok").onclick = $("sc-cancel").onclick = null; res(v); };
    $("sc-ok").onclick = () => done($("sc-keep").checked);
    $("sc-cancel").onclick = () => done(null);
  });
}

// the File page's panel: the scene in the setup (its stock and parts), the last import's notes
function sceneRender() {
  const el = $("sc-info"); if (!el) return;
  const stock = setup.objects.find((o) => o.id === "scene-stock");
  if (!stock) { $("sc-name").textContent = "—"; el.innerHTML = `<p class="muted">No CAM scene in the 3D setup.</p>`; return; }
  const folder = stock.src.slice(6, stock.src.lastIndexOf("/")), parts = setup.objects.filter((o) => o.id.startsWith("scene-p"));
  const zero = (stock.name.match(/, zero (.+)$/) || [])[1];
  $("sc-name").textContent = folder;
  const last = sceneLast && sceneLast.folder === folder ? sceneLast : null;
  el.innerHTML = `
    <p><b>${esc(stock.name.replace(/, zero .+$/, ""))}</b>${zero ? ` · work zero <b>${esc(zero)}</b>` : ""}</p>
    <div class="sc-list">${parts.map((o) => `<button class="btn sm" data-scobj="${esc(o.id)}" aria-pressed="${setup.sel === o.id}">${esc(o.name)}</button>`).join("")}</div>
    <p class="note">The scene's models are in the work's coordinates: set the work offset at the scene's zero on the real stock${zero ? ` (its ${esc(zero)})` : ""} and the stock and the parts in the view sit where the program cuts them. The material removal cuts the stock and compares it with the parts.</p>
    ${last && last.warnings.length ? `<ul class="sc-warn">${last.warnings.map((w) => `<li>${esc(w)}</li>`).join("")}</ul>` : ""}`;
}

$("sc-import").addEventListener("click", () => $("sc-input").click());
$("sc-input").addEventListener("change", (e) => { const f = e.target.files[0]; e.target.value = ""; if (f) sceneImportFile(f); });
$("sc-info").addEventListener("click", (e) => { const b = e.target.closest("[data-scobj]"); if (b) { selectObject(b.dataset.scobj); sceneRender(); } });
sceneRender();
