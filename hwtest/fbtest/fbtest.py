#!/usr/bin/env python3
"""Feedback-port hardware test with the fbtest Arduino Uno (README.md).

    fbtest.py [--uno /dev/ttyUSB0] [--dev /dev/ttyACM0] [--port fb1]

The Uno's A0..A5 are on RJ45 pins 1..6, GND on pin 8, the Uno powered
from USB. The drive must be disabled; the test changes no drive pin.

Tests, each against the v4.0 schematic (hw/kicad/v4.0, sheet fb.sch):

  supply      pin 7 as the drive reads it (io0.fb1)
  idle        every line with the Uno hi-Z and the transceivers disabled
              (1k pull-downs on their DE/RE): the input bias. Pins 1,2,3,6:
              SN65176B receiver || 25k to the 1.83 V op-amp reference;
              pins 4,5: receiver only
  shunt       one line driven high, the voltage across the 120 R
              termination gives the partner line's resistance to its bias
  pairs       each pair driven differentially both ways: the termination
              (the share of Vcc across it), the receiver (a/b/z), and the
              sin/cos front end (it clips at about +1.08 / -1.22 V)

Results go to runs/<date>-<time>/ (summary.md, results.json, fbtest.log).
"""
import argparse
import datetime
import json
import os
import subprocess
import sys
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
CLI = os.path.join(HERE, '..', '..', 'servoterm-cli', 'servoterm-cli')

R_TERM = 120.0
# (pins, receiver pin, analog pin or None, the other analog pin)
PAIRS = [((1, 2), 'a', 'sin', 'cos'),
         ((3, 6), 'b', 'cos', 'sin'),
         ((5, 4), 'z', None, None)]  # pin 5 is Z+, pin 4 Z-

# limits
SUPPLY = (4.75, 5.5)                     # V, pin 7
IDLE = {1: (0.9, 1.25), 2: (0.9, 1.25), 3: (0.9, 1.25), 6: (0.9, 1.25),
        4: (0.05, 0.4), 5: (0.05, 0.4)}  # V, expected ~1.05 and ~0.2
IDLE_BALANCE = 0.03                      # V between the lines of a pair
SHUNT = {1: (9e3, 17e3), 2: (9e3, 17e3), 3: (9e3, 17e3), 6: (9e3, 17e3),
         4: (18e3, 40e3), 5: (18e3, 40e3)}  # ohm, expected ~13k and ~27k
TERM_SHARE = (0.62, 0.80)  # V across the pair / Vcc: 120 R with ~25 R each side
CLIP_POS = (0.95, 1.2)     # V, sin/cos with the pair driven
CLIP_NEG = (-1.35, -1.0)
CROSS = 0.05               # V on the other analog channel


class Uno:
    def __init__(self, port, log):
        self.s = serial.Serial(port, 115200, timeout=2)
        self.log = log
        time.sleep(2.0)  # opening the port resets the Uno
        self.s.reset_input_buffer()
        ident = self.cmd('i')
        if not ident.startswith('fbtest '):
            raise IOError('no fbtest firmware on %s: %r' % (port, ident))
        self.ident = ident

    def cmd(self, c):
        self.s.write((c + '\n').encode())
        r = self.s.readline().decode(errors='replace').strip()
        self.log('uno %s -> %s' % (c, r))
        return r

    def set(self, ddr, port):
        if self.cmd('o %x %x' % (ddr, port)) != 'ok':
            raise IOError('uno: o failed')

    def pins(self, n=64):
        """{rj45 pin: volts}, vcc volts"""
        r = self.cmd('a %d' % n).split()
        if r[0] != 'a' or len(r) != 8:
            raise IOError('uno: bad analog answer %r' % r)
        mv = [int(x) for x in r[1:7]]
        return {i + 1: mv[i] / 1000.0 for i in range(6)}, int(r[7][4:]) / 1000.0

    def release(self):
        self.set(0, 0)


def bit(pin):
    return 1 << (pin - 1)  # A0 = pin 1


class Drive:
    def __init__(self, dev, port, log):
        self.dev, self.port, self.log = dev, port, log

    def get(self, *names):
        args = [CLI, '-d', self.dev, '-t', '0.3']
        for n in names:
            args += ['-c', n]
        out = subprocess.run(args, capture_output=True, text=True,
                             timeout=30).stdout
        self.log('drive %s -> %s' % (' '.join(names), out.strip().replace('\n', ' | ')))
        vals = {}
        for line in out.splitlines():
            if '=' in line:
                k, v = line.split('=', 1)
                k = k.split('<')[0].strip()
                try:
                    vals[k] = float(v.strip())
                except ValueError:
                    pass
        return {n: vals[n] for n in names if n in vals}


class Run:
    def __init__(self, out):
        self.out = out
        self.results = []
        self.logf = open(os.path.join(out, 'fbtest.log'), 'w')

    def log(self, msg):
        self.logf.write('%.3f %s\n' % (time.time(), msg))
        self.logf.flush()

    def check(self, test, name, value, lo, hi, unit=''):
        ok = lo <= value <= hi
        self.results.append({'test': test, 'check': name, 'value': value,
                             'lo': lo, 'hi': hi, 'unit': unit, 'pass': ok})
        print('  %-4s %-28s %10.4g %-3s [%g .. %g]'
              % ('ok' if ok else 'FAIL', name, value, unit, lo, hi))
        return ok

    def equal(self, test, name, value, want):
        ok = value == want
        self.results.append({'test': test, 'check': name, 'value': value,
                             'want': want, 'pass': ok})
        print('  %-4s %-28s %10s     want %s' % ('ok' if ok else 'FAIL', name, value, want))
        return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--uno', default='/dev/ttyUSB0')
    ap.add_argument('--dev', default='/dev/ttyACM0', help='the drive console')
    ap.add_argument('--port', default='fb1', choices=['fb1'],
                    help='the drive port the fixture is on')
    a = ap.parse_args()

    out = os.path.join(HERE, 'runs', datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    os.makedirs(out)
    run = Run(out)
    uno = Uno(a.uno, run.log)
    drv = Drive(a.dev, a.port, run.log)
    p = a.port
    rx = {'a': 'io0.%sa' % p, 'b': 'io0.%sb' % p, 'z': 'io0.%sz' % p}
    an = {'sin': 'adc0.sin1', 'cos': 'adc0.cos1'}
    info = drv.get('io0.fb1', 'fault0.en_out')
    if info.get('fault0.en_out', 0) > 0:
        sys.exit('fbtest: the drive is enabled; disable it first')
    print('fbtest: %s, drive on %s, fixture on %s; results in %s'
          % (uno.ident, a.dev, p, out))
    try:
        uno.release()

        print('supply')
        run.check('supply', 'pin 7 (io0.%s)' % p, info['io0.fb1'], *SUPPLY, unit='V')

        print('idle (Uno hi-Z, transceivers disabled)')
        time.sleep(0.2)
        idle, vcc = uno.pins()
        for pin in range(1, 7):
            run.check('idle', 'pin %d' % pin, idle[pin], *IDLE[pin], unit='V')
        for (pp, pn), _, _, _ in PAIRS:
            run.check('idle', 'pins %d-%d balance' % (pp, pn),
                      abs(idle[pp] - idle[pn]), 0, IDLE_BALANCE, unit='V')

        print('shunt (one line high, partner through %g R)' % R_TERM)
        for (pp, pn), _, _, _ in PAIRS:
            for drv_pin, other in ((pp, pn), (pn, pp)):
                uno.set(bit(drv_pin), bit(drv_pin))
                v, vcc = uno.pins()
                uno.release()
                drop = v[drv_pin] - v[other]
                r = R_TERM * (v[other] - idle[other]) / drop if drop > 0.002 else 1e9
                run.check('shunt', 'pin %d to bias' % other, r, *SHUNT[other], unit='ohm')

        print('pairs (driven differentially)')
        for (pp, pn), r, ach, other in PAIRS:
            for sign, hi, lo in ((+1, pp, pn), (-1, pn, pp)):
                tag = 'pin %d%s %d%s' % (pp, '+' if sign > 0 else '-',
                                         pn, '-' if sign > 0 else '+')
                uno.set(bit(hi) | bit(lo), bit(hi))
                v, vcc = uno.pins(16)
                names = [rx[r]] + ([an[ach], an[other]] if ach else [])
                d = drv.get(*names)
                uno.release()
                run.check('pairs', tag + ' termination', (v[hi] - v[lo]) / vcc,
                          *TERM_SHARE)
                run.equal('pairs', tag + ' ' + rx[r], int(d[rx[r]]), 1 if sign > 0 else 0)
                if ach:
                    clip = CLIP_POS if sign > 0 else CLIP_NEG
                    run.check('pairs', tag + ' ' + an[ach], d[an[ach]], *clip, unit='V')
                    run.check('pairs', tag + ' ' + an[other], d[an[other]], -CROSS, CROSS, unit='V')
                time.sleep(0.2)
    finally:
        uno.release()

    bad = [x for x in run.results if not x['pass']]
    verdict = 'PASS' if not bad else 'FAIL'
    with open(os.path.join(out, 'results.json'), 'w') as f:
        json.dump({'uno': uno.ident, 'drive': a.dev, 'port': p, 'verdict': verdict,
                   'results': run.results}, f, indent=1)
    with open(os.path.join(out, 'summary.md'), 'w') as f:
        f.write('# fbtest %s: %s\n\n%d checks, %d failed\n\n' % (p, verdict, len(run.results), len(bad)))
        f.write('| Test | Check | Value | Limits | |\n|---|---|---|---|---|\n')
        for x in run.results:
            lim = ('%g .. %g %s' % (x['lo'], x['hi'], x['unit'])) if 'lo' in x else 'want %s' % x['want']
            val = ('%.4g' % x['value']) if isinstance(x['value'], float) else x['value']
            f.write('| %s | %s | %s | %s | %s |\n' % (x['test'], x['check'], val, lim,
                                                      'ok' if x['pass'] else '**FAIL**'))
    print('fbtest: %s (%d checks, %d failed)' % (verdict, len(run.results), len(bad)))
    sys.exit(0 if not bad else 1)


if __name__ == '__main__':
    main()
