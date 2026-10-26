"""The measurement bus's frames on the PC (shared/vibproto.h, the same
layouts): decode what the sensors send, encode what the PC sends."""
import struct

EVENT, STATE, FEAT, SUM, SPEC, RAW, CON_TO, CON_FROM = 0x010, 0x050, 0x100, 0x140, 0x300, 0x400, 0x600, 0x580
NODE_MAX = 15
EV_CRASH, EV_LEVEL, EV_TEMP = 1, 2, 3
ST_SPINDLE, ST_RUNNING, ST_TOOLCHANGE = 1, 2, 4
SPEC_BINS = 26
CON_TEXT = 62
FD_LENS = (8, 12, 16, 20, 24, 32, 48, 64)


def fd_pad(data):
    n = len(data)
    if n <= 8:
        return bytes(data)
    return bytes(data) + bytes(next(l for l in FD_LENS if l >= n) - n)


def kind(can_id):
    """(kind, node) of a sensor's frame ID, or (None, None)."""
    for base, name in ((EVENT, "event"), (FEAT, "features"), (SUM, "summary"), (SPEC, "spectrum"),
                       (RAW, "raw"), (CON_FROM, "console")):
        if base < can_id <= base + NODE_MAX:
            return name, can_id - base
    return None, None


def state(time_ms, rpm, feed, teeth, flags, tool):
    return STATE, struct.pack("<IffBBH", time_ms & 0xFFFFFFFF, rpm, feed, teeth, flags, tool)


def event(d):
    t, v, seq, x = struct.unpack_from("<BBHf", d)
    return {"type": {EV_CRASH: "crash", EV_LEVEL: "level", EV_TEMP: "temp"}.get(t, t), "value": v, "seq": seq, "x": x}


def features(d):
    t, seq, level, flags = struct.unpack_from("<IHBB", d)
    f = struct.unpack_from("<14f", d, 8)
    return {"time_ms": t, "seq": seq, "level": level, "crash": flags & 1, "temp_level": (flags >> 1) & 3,
            "rms": list(f[0:3]), "peak": list(f[3:6]), "vel": list(f[6:9]), "kurt": list(f[9:12]),
            "env": f[12], "temp": f[13]}


def summary(d):
    t, seq, n = struct.unpack_from("<IHH", d)
    x = struct.unpack_from("<10f", d, 8)
    return {"time_ms": t, "seq": seq, "fft_n": n, "fft_f": x[0], "fft_a": x[1], "ord": list(x[2:5]), "tpf": x[5],
            "chat_f": x[6], "chat_a": x[7], "env_f": x[8], "rpm": x[9]}


def spectrum(d):
    seq, first, total, axis = struct.unpack_from("<HHHB", d)
    scale = struct.unpack_from("<f", d, 8)[0]
    bins = struct.unpack_from("<%dH" % SPEC_BINS, d, 12)
    return {"seq": seq, "first": first, "total": total, "axis": axis,
            "bins": [b / 65535.0 * scale for b in bins]}


def raw(d):
    first, flags = struct.unpack_from("<IH", d)
    s = struct.unpack_from("<27h", d, 6)
    return {"first": first, "lost": flags & 1, "xyz": [s[i:i + 3] for i in range(0, 27, 3)]}


def console_frames(node, seq, text):
    """Console text to a sensor, as frames [(id, data)]."""
    out, b = [], text.encode()
    for i in range(0, len(b), CON_TEXT):
        chunk = b[i:i + CON_TEXT]
        out.append((CON_TO + node, fd_pad(bytes([seq & 0xFF, len(chunk)]) + chunk)))
        seq += 1
    return out


def console_text(d):
    n = min(d[1], len(d) - 2)
    return d[2:2 + n].decode("utf-8", "replace")
