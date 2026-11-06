#!/usr/bin/env python3
"""encmon on the betaiS 4/4000 #2 (fanuc_tw): A correct offset, B offset +0.5 rad
(3 A), C motor #1's offset (~pi off, 2 A). stp0 moves over the USB console;
the drive is disabled at once on any fault. Prints a summary per phase."""
import re, sys, time, json, serial
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.03)
OFF = -1.5818  # this motor's commissioned offset
def q(cmds, wait=0.25):
    s.read(100000); s.write(('\n'.join(cmds) + '\n').encode())
    end = time.time() + wait; buf = b''
    while time.time() < end:
        buf += s.read(4096)
    txt = buf.replace(b'\xff', b'').replace(b'\x80', b'').decode(errors='replace')
    return {m.group(1): float(m.group(2)) for m in re.finditer(r'^(\S+) = ([-0-9.eE+]+)', txt, re.M)}, txt
P = ['fault0.fault', 'fault0.state', 'encmon0.fault', 'encmon0.max_ratio', 'encmon0.ratio', 'encmon0.active',
     'encmon0.max_timer', 'encmon0.bemf', 'pid0.pos_error', 'stp0.at_target', 'hv0.abs_cur']
def disable():
    q(['fault0.en = 0'], 0.3)
def phase(name, offset, cur, targets, vel=100, hold=0.0):
    disable(); time.sleep(0.3)
    q(['conf0.mot_fb_offset = %g' % offset, 'conf0.max_ac_cur = %g' % cur, 'stp0.max_vel = %g' % vel,
       'stp0.max_acc = 1500', 'stp0.min_pos = -30', 'stp0.max_pos = 30'], 0.4)
    v, _ = q(['stp0.pos', 'fanuc_tw0.pos'], 0.3)
    q(['stp0.target = %g' % v.get('stp0.pos', 0)], 0.2)  # stay where it is
    q(['fault0.en = 1'], 0.3)
    res = {'phase': name, 'offset': offset, 'cur': cur, 'max_ratio': 0, 'max_bemf': 0, 'max_err': 0, 'max_cur': 0,
           'fault': 0, 'encmon_fault': 0, 'active_seen': 0}
    t_end = time.time() + hold
    seq = list(targets)
    start = v.get('stp0.pos', 0)
    while True:
        v, txt = q(P, 0.12)
        if 'fault0.fault' not in v:
            continue
        res['max_ratio'] = max(res['max_ratio'], v.get('encmon0.max_ratio', 0))
        res['max_bemf'] = max(res['max_bemf'], abs(v.get('encmon0.bemf', 0)))
        res['max_err'] = max(res['max_err'], abs(v.get('pid0.pos_error', 0)))
        res['max_cur'] = max(res['max_cur'], abs(v.get('hv0.abs_cur', 0)))
        res['active_seen'] = max(res['active_seen'], v.get('encmon0.active', 0))
        if v['fault0.fault'] != 0 or v.get('encmon0.fault', 0) > 0:
            res['fault'] = v['fault0.fault']; res['encmon_fault'] = v.get('encmon0.fault', 0)
            disable(); break
        if time.time() < t_end:
            continue
        if v.get('stp0.at_target', 0) > 0.5:
            if not seq:
                break
            q(['stp0.target = %g' % (start + seq.pop(0))], 0.15)
            time.sleep(0.3)
    disable()
    v, _ = q(['encmon0.max_ratio', 'encmon0.max_timer', 'fault0.last_fault'], 0.3)
    res['max_ratio'] = max(res['max_ratio'], v.get('encmon0.max_ratio', 0)); res['max_timer'] = v.get('encmon0.max_timer')
    res['last_fault'] = v.get('fault0.last_fault')
    print(json.dumps(res)); sys.stdout.flush()
    return res
which = sys.argv[1] if len(sys.argv) > 1 else 'A'
try:
    if 'A' in which:
        phase('A correct', OFF, 12.7279, [6, 0, -6, 0, 6, 0, -6, 0])
    if 'B' in which:
        phase('B +0.5 rad', OFF + 0.5, 3, [3, 0, -3, 0])
    if 'E' in which:  # 180 deg electrical (pi/4 mechanical, 4 pole pairs)
        phase('C2 180 deg el', OFF + 0.785398, 2, [1, 0], hold=1.0)
    if 'D' in which:  # 30 deg electrical
        phase('D 30 deg el', OFF + 0.1309, 12.7279, [6, 0, -6, 0, 6, 0])
    if 'F' in which:  # 10 deg electrical
        phase('F 10 deg el', OFF + 0.04363, 12.7279, [6, 0, -6, 0, 6, 0])
    if 'C' in which:
        phase('C motor #1 offset', 1.56012, 2, [1, 0], hold=1.0)
finally:
    disable()
    q(['conf0.mot_fb_offset = %g' % OFF, 'conf0.max_ac_cur = 12.7279', 'stp0.max_vel = 100'], 0.4)
