#!/usr/bin/env python3
"""encmon0.min_bemf on the betaiS 4/4000 #2 (fanuc_tw): small moves (±0.5,
±1 rad) for 45 s at min_bemf 5 and 10 V; then a 30 deg el. offset error
with min_bemf 10 V and ±6 rad moves at 100 rad/s: it must still trip."""
import re, sys, time, json, serial
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.03)
OFF = -1.5818
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
def run(name, secs, seq, setup):
    q(['fault0.en = 0'], 0.3)
    v = q(['stp0.pos'], 0.3); start = v.get('stp0.pos', 0)
    q(setup + ['stp0.target = %g' % start, 'fault0.en = 1'], 0.6)
    t0 = time.time(); bad = None; seq = list(seq) * 200
    while time.time() - t0 < secs:
        v = q(['fault0.fault', 'stp0.at_target'], 0.12)
        f = v.get('fault0.fault', 0)
        if f >= 1 and f == int(f):
            bad = f; break
        if v.get('stp0.at_target', 0) > 0.5:
            q(['stp0.target = %g' % (start + seq.pop(0))], 0.1); time.sleep(0.15)
    v = q(['encmon0.max_ratio', 'encmon0.max_timer', 'fault0.last_fault', 'hv0.hv_temp'], 0.3)
    q(['fault0.en = 0'], 0.3)
    r = dict(name=name, max_ratio=v.get('encmon0.max_ratio'), max_timer=v.get('encmon0.max_timer'), fault=bad, last=v.get('fault0.last_fault'), hv_temp=v.get('hv0.hv_temp'))
    print(json.dumps(r)); sys.stdout.flush()
base = ['conf0.mot_fb_offset = %g' % OFF, 'stp0.max_vel = 150', 'stp0.max_acc = 2000', 'stp0.min_pos = -30', 'stp0.max_pos = 30']
try:
    run('small moves, min_bemf 5', 45, [0.5, 0, -0.5, 0, 1, 0, -1, 0], base + ['encmon0.min_bemf = 5'])
    run('small moves, min_bemf 10', 45, [0.5, 0, -0.5, 0, 1, 0, -1, 0], base + ['encmon0.min_bemf = 10'])
    run('30 deg el. offset, min_bemf 10, 100 rad/s', 20, [6, 0, -6, 0],
        ['conf0.mot_fb_offset = %g' % (OFF + 0.1309), 'stp0.max_vel = 100', 'stp0.max_acc = 1500', 'encmon0.min_bemf = 10'])
finally:
    q(['fault0.en = 0', 'conf0.mot_fb_offset = %g' % OFF, 'encmon0.min_bemf = 5'], 0.4)
