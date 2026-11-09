#!/usr/bin/env python3
"""Motion soak on the drive's USB console, no LinuxCNC: stp0 (link jog_cmd)
moves the motor between targets; samples faults, following error, encoder
counters, temperatures. Disables the drive at the end, on a fault, or when
the console stops answering."""
import re, sys, time, serial, json
DUR = float(sys.argv[1]) if len(sys.argv) > 1 else 600
out = sys.argv[2] if len(sys.argv) > 2 else 'soak'
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.05)
log = open(out + '.log', 'w')
def unscope(b):
    """drop the scope stream: 0xFF and the 8 channel bytes after it"""
    out = bytearray(); skip = 0
    for x in b:
        if skip: skip -= 1
        elif x == 0xFF: skip = 8
        else: out.append(x)
    return out.decode(errors='replace')
def q(cmds, wait=0.25):
    s.read(100000)
    s.write(('\n'.join(cmds) + '\n').encode())
    end = time.time() + wait; raw = b''
    while time.time() < end:
        raw += s.read(4096)
        buf = unscope(raw)
        if all(re.search('^' + re.escape(c.split(' =')[0]) + r' = ', buf, re.M) for c in cmds if '=' not in c): break
    buf = unscope(raw)
    vals = {}
    for m in re.finditer(r'^(\S+) = ([-0-9.eE+naif]+)', buf, re.M):
        try: vals[m.group(1)] = float(m.group(2))
        except ValueError: pass
    return vals, buf
P = ['fault0.fault', 'fault0.state', 'pid0.pos_error', 'stp0.at_target', 'stp0.pos', 'fanuc_tw0.pos',
     'fanuc_tw0.state', 'fanuc_tw0.crc_er', 'fanuc_tw0.sync_er', 'fanuc_tw0.noreply', 'fanuc_tw0.ok',
     'iit0.temp', 'hv0.hv_temp', 'hv0.dc_volt', 'hv0.abs_cur', 'pid0.vel_error', 'encmon0.max_ratio',
     'encmon0.fault', 'fanuc_tw0.bat_fail', 'fanuc_tw0.nref']
setup = ['stp0.max_vel = 150', 'stp0.max_acc = 2000', 'stp0.min_pos = -30', 'stp0.max_pos = 30', 'stp0.target = 0']
q(setup, 0.5)
v0, _ = q(P, 0.6)
log.write('start ' + json.dumps(v0) + '\n')
q(['fault0.en = 1'], 0.5)
time.sleep(1.0)
targets = [25, -25, 25, -25, 6, -6, 6, -6, 0.5, -0.5, 0]
st = {'max_err_move': 0.0, 'max_err_rest': 0.0, 'max_vel_err': 0.0, 'max_cur': 0.0, 'samples': 0,
      'moves': 0, 'max_iit': 0.0, 'max_hv_temp': 0.0, 'fault': 0, 'silent': 0}
t0 = time.time(); ti = 0; reason = 'time'
cur_target = 0; settle_until = 0
while time.time() - t0 < DUR:
    v, raw = q(P, 0.3)
    if 'fault0.fault' not in v:
        st['silent'] += 1
        if st['silent'] >= 20:
            reason = 'console silent'; break
        continue
    st['silent'] = 0
    st['samples'] += 1
    e = abs(v.get('pid0.pos_error', 0)); moving = v.get('stp0.at_target', 1) < 0.5
    if moving: st['max_err_move'] = max(st['max_err_move'], e)
    elif time.time() > settle_until: st['max_err_rest'] = max(st['max_err_rest'], e)
    st['max_vel_err'] = max(st['max_vel_err'], abs(v.get('pid0.vel_error', 0)))
    st['max_cur'] = max(st['max_cur'], abs(v.get('hv0.abs_cur', 0)))
    st['max_iit'] = max(st['max_iit'], v.get('iit0.temp', 0))
    st['max_hv_temp'] = max(st['max_hv_temp'], v.get('hv0.hv_temp', 0))
    st['encmon_max_ratio'] = max(st.get('encmon_max_ratio', 0), v.get('encmon0.max_ratio', 0))
    st['bat_fail_seen'] = max(st.get('bat_fail_seen', 0), v.get('fanuc_tw0.bat_fail', 0))
    st['nref_seen'] = max(st.get('nref_seen', 0), v.get('fanuc_tw0.nref', 0))
    log.write('%.1f %s\n' % (time.time() - t0, json.dumps(v)))
    f = v['fault0.fault']
    if (f >= 1 and f == int(f)) or v.get('fault0.state', 1) not in (1.0,):
        # a garbled reply (fault0.fault = 0.1 once) isn't a fault: ask again
        v2, _ = q(['fault0.fault', 'fault0.state'], 0.4)
        if v2.get('fault0.fault', 0) < 1 and v2.get('fault0.state', 1) == 1.0:
            continue
        st['fault'] = v['fault0.fault']; reason = 'fault %g state %g' % (v['fault0.fault'], v.get('fault0.state', -1)); break
    if not moving and time.time() > settle_until:
        cur_target = targets[ti % len(targets)]; ti += 1
        q(['stp0.target = %g' % cur_target], 0.15)
        st['moves'] += 1
        settle_until = time.time() + 0.6
    log.flush()
# park and disable
q(['stp0.target = 0'], 0.2)
for _ in range(40):
    v, _ = q(['stp0.at_target'], 0.3)
    if v.get('stp0.at_target', 0) > 0.5: break
time.sleep(0.5)
q(['fault0.en = 0'], 0.5)
v1, _ = q(P, 0.6)
st['reason'] = reason; st['duration_s'] = round(time.time() - t0, 1)
st['enc_ok'] = v1.get('fanuc_tw0.ok', 0) - v0.get('fanuc_tw0.ok', 0)
for k in ('crc_er', 'sync_er', 'noreply'):
    st['enc_' + k] = v1.get('fanuc_tw0.' + k, 0) - v0.get('fanuc_tw0.' + k, 0)
st['end_fault'] = v1.get('fault0.fault'); st['end_state'] = v1.get('fault0.state')
log.write('end ' + json.dumps(v1) + '\n')
json.dump(st, open(out + '.json', 'w'), indent=1)
print(json.dumps(st, indent=1))
