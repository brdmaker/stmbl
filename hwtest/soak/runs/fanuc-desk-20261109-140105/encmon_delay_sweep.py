#!/usr/bin/env python3
"""encmon0.delay against the soak's fast moves (betaiS 4/4000 #2, fanuc_tw):
for each delay, 30 s of moves (±25, ±6 rad; 150 rad/s, 2000 rad/s^2), then
encmon0.max_ratio / max_timer. Disables at once on a fault."""
import re, sys, time, json, serial
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.03)
def unscope(b):
    out = bytearray(); skip = 0
    for x in b:
        if skip: skip -= 1
        elif x == 0xFF: skip = 8
        else: out.append(x)
    return out.decode(errors='replace')
def q(cmds, wait=0.2):
    s.read(100000); s.write(('\n'.join(cmds) + '\n').encode()); raw = b''; end = time.time() + wait
    while time.time() < end:
        raw += s.read(4096)
    return {m.group(1): float(m.group(2)) for m in re.finditer(r'^(\S+) = ([-0-9.eE+]+)', unscope(raw), re.M)}
delays = [float(x) for x in sys.argv[1:]] or [0.00012, 0.00032, 0.00052, -0.00008]
q(['fault0.en = 0', 'stp0.max_vel = 150', 'stp0.max_acc = 2000', 'stp0.min_pos = -30', 'stp0.max_pos = 30'], 0.4)
for dl in delays:
    v = q(['stp0.pos'], 0.3); start = v.get('stp0.pos', 0)
    q(['encmon0.delay = %g' % dl, 'stp0.target = %g' % start, 'fault0.en = 1'], 0.5)
    seq = [25, -25, 6, -6] * 20; t0 = time.time(); bad = None
    while time.time() - t0 < float(__import__("os").environ.get("SWEEP_S", "30")):
        v = q(['fault0.fault', 'stp0.at_target', 'encmon0.max_ratio'], 0.12)
        if v.get('fault0.fault', 0):
            bad = v['fault0.fault']; break
        if v.get('stp0.at_target', 0) > 0.5 and seq:
            q(['stp0.target = %g' % (start + seq.pop(0))], 0.1); time.sleep(0.2)
    v = q(['encmon0.max_ratio', 'encmon0.max_timer', 'fault0.last_fault'], 0.3)
    q(['stp0.target = %g' % start], 0.2); time.sleep(1.0); q(['fault0.en = 0'], 0.3)
    print(json.dumps({'delay': dl, 'max_ratio': v.get('encmon0.max_ratio'), 'max_timer': v.get('encmon0.max_timer'), 'fault': bad})); sys.stdout.flush()
q(['fault0.en = 0', 'encmon0.delay = 0.00012'], 0.3)
