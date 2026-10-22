"""The Debug page's scope: HAL values sampled at a steady rate in a process of
its own (millweb's event loop and GIL stay out of its timing).

    python3 sampler.py     with {"names": [...], "rate": Hz} on stdin

writes frames to stdout: a 4-byte little-endian length, then a JSON header
line {"n": samples, "rate": measured Hz, "missed": late samples, "t0": time
of the frame's first sample} and the samples as float32, interleaved by
channel. It samples in userspace: steady to a fraction of a millisecond, not
locked to the servo thread as halscope's real-time part is.
"""
import json
import signal
import struct
import sys
import time

MAX_RATE, MAX_CH = 2000.0, 8


def sample_loop(get, names, rate, write, frame_s=0.05, stop=lambda: False, duration=None):
    """Sample get(name) for each name at rate Hz, write(header_dict, bytes)
    every frame_s seconds. A sample that comes late is taken late (and
    counted), not skipped: the record stays continuous."""
    period = 1.0 / rate
    t0 = time.perf_counter()
    nxt, buf, n, first, missed, frame_end, count0, tcount = t0, bytearray(), 0, None, 0, t0 + frame_s, 0, t0
    pack = struct.Struct("<%df" % len(names)).pack
    while not stop() and (duration is None or time.perf_counter() - t0 < duration):
        now = time.perf_counter()
        if now < nxt:
            time.sleep(min(nxt - now, 0.0005) if nxt - now > 0.0007 else 0)  # sleep most of the wait, spin the rest
            continue
        if now - nxt > period:
            missed += 1
        vals = []
        for nm in names:
            try:
                v = get(nm)
                vals.append(float(v) if not isinstance(v, bool) else (1.0 if v else 0.0))
            except Exception:
                vals.append(float("nan"))
        buf += pack(*vals)
        if first is None:
            first = time.time()
        n += 1
        nxt += period
        if now - nxt > 0.5:  # far behind (the machine was busy): start again from now
            nxt = now + period
        if now >= frame_end and n:
            el = now - tcount
            write({"n": n, "rate": round((count0 + n) / el, 1) if el > 0.2 else rate, "missed": missed, "t0": first}, bytes(buf))
            if el > 2.0:  # the rate over the last couple of seconds
                tcount, count0 = now, 0
            else:
                count0 += n
            buf, n, first, missed = bytearray(), 0, None, 0
            frame_end = now + frame_s
    if n:
        write({"n": n, "rate": rate, "missed": missed, "t0": first}, bytes(buf))


def main():
    # stopped with SIGTERM, never killed: hal.get_value holds HAL's mutex, and
    # a process killed while it does leaves it locked, every HAL user (millweb,
    # halcmd, LinuxCNC's shutdown) waiting forever. The handler only sets a
    # flag, so the get in progress finishes first.
    stopping = []
    signal.signal(signal.SIGTERM, lambda *a: stopping.append(1))
    signal.signal(signal.SIGINT, lambda *a: stopping.append(1))
    req = json.loads(sys.stdin.readline())
    names = [str(x) for x in req.get("names", [])][:MAX_CH]
    rate = max(10.0, min(MAX_RATE, float(req.get("rate", 1000))))
    out = sys.stdout.buffer

    def write(head, data):
        h = (json.dumps(head) + "\n").encode()
        out.write(struct.pack("<I", len(h) + len(data)) + h + data)
        out.flush()
    try:
        import hal
        comp = hal.component("millweb-scope-%d" % int(time.time() * 1000 % 1e9))  # get_value needs a component
        comp.ready()
        get = hal.get_value
    except Exception as e:
        write({"error": "the scope needs LinuxCNC's hal module: %s" % e, "n": 0}, b"")
        return
    try:
        sample_loop(get, names, rate, write, stop=lambda: bool(stopping))
    except BrokenPipeError:  # millweb went away
        pass
    finally:
        comp.exit()


if __name__ == "__main__":
    main()
