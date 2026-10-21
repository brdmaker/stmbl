// A protocol client for the live GUI suite: puts the machine in a known
// state between cases and reads what the server says, beside the page.
class Client {
  static open(port) {
    return new Promise((resolve, reject) => {
      const c = new Client();
      c.ws = new WebSocket(`ws://127.0.0.1:${port}/ws`);
      c.ws.onmessage = (e) => c.on(JSON.parse(e.data));
      c.ws.onerror = (e) => reject(e);
      c.ws.onclose = () => { c.closed = true; };
      c.ready = resolve.bind(null, c);
    });
  }
  constructor() { this.n = 0; this.st = {}; this.wait = new Map(); this.last = {}; this.closed = false; }
  on(m) {
    if (m.t === "hello") { this.hello = m; this.ready(); }
    if (m.t === "st") Object.assign(this.st, m);
    if (m.t === "reply" && this.wait.has(m.id)) { this.wait.get(m.id)(m); this.wait.delete(m.id); }
    this.last[m.t] = m;
  }
  call(op, args = {}, ms = 130000) {
    const id = ++this.n;
    return new Promise((resolve) => {
      this.wait.set(id, resolve);
      this.ws.send(JSON.stringify({ op, id, ...args }));
      setTimeout(() => { if (this.wait.has(id)) { this.wait.delete(id); resolve({ ok: false, err: "no reply" }); } }, ms);
    });
  }
  async until(fn, ms = 15000) {
    const end = Date.now() + ms;
    while (Date.now() < end) { try { if (fn(this.st, this)) return true; } catch (e) { /* not yet */ } await new Promise((r) => setTimeout(r, 40)); }
    return false;
  }
  async idle() {
    await new Promise((r) => setTimeout(r, 150));
    return this.until((s) => s.interp === "idle" && !s.queue && Math.abs(s.vel) < 1e-6, 60000);
  }
  async mdi(cmd) { const r = await this.call("mdi", { cmd }); if (!r.ok) throw new Error(`MDI ${cmd}: ${r.err}`); await this.idle(); }
  // on, homed, idle, G54 at the test's values, nothing running; nobody in control
  async baseline() {
    const must = async (op, a) => { const r = await this.call(op, a); if (!r.ok) throw new Error(`${op}: ${r.err}`); return r; };
    await must("control", { take: true, force: true });
    await this.call("jog_stop");
    await this.call("cycle", { action: "stop" });
    if (this.st.estop) await must("estop", { on: false });
    if (!this.st.on) { await must("power", { on: true }); await this.until((s) => s.on, 5000); }
    if (!(this.st.homed || []).every(Boolean)) { await must("home", { axis: "all" }); await this.until((s) => s.homed.every(Boolean), 30000); }
    await this.idle();
    for (const k of ["feed", "rapid", "spin", "maxv"]) await this.call("override", { k, v: 1 });
    await this.call("spindle", { dir: 0 });
    await this.call("coolant", { k: "flood", on: false });
    await this.call("coolant", { k: "mist", on: false });
    await this.call("opt", { m1: false, bd: false });
    await this.mdi("G90 G92.1");
    await must("offset", { index: 0, x: 440, y: 460, z: -195 });
    await must("offset", { index: 1, x: 640, y: 460, z: -195 });
    await must("wcs", { index: 0 });
    await this.mdi("G53 G0 Z0");
    await this.mdi("G0 X10 Y10");
    await must("control", { take: false });
  }
  close() { try { this.ws.close(); } catch (e) { /* gone */ } }
}
module.exports = { Client };
