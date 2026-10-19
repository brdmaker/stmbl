#!/usr/bin/env python3
"""tools/create_hal_tbl.py must refuse a comp name the console cannot
address: it parses "<comp><instance>.<pin>" with %[a-zA-Z_]%li, so a
digit in the name becomes the instance (fb1_out0.en_a -> comp "fb",
instance 1, "not found"). Every comp in the tree must pass. Prints TAP."""
import glob
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
GEN = os.path.join(ROOT, 'tools', 'create_hal_tbl.py')
results = []


def gen(*files):
    with tempfile.TemporaryDirectory() as d:
        r = subprocess.run([sys.executable, GEN, os.path.join(d, 'tbl.c')] + list(files),
                           capture_output=True, text=True)
        return r.returncode, r.stderr


with tempfile.TemporaryDirectory() as d:
    for name, want in (('fb1_out', False), ('hx711', False), ('fbtx', True), ('o_fb', True)):
        f = os.path.join(d, name + '.c')
        open(f, 'w').write('HAL_COMP(%s);\nHAL_PIN(a);\n' % name)
        rc, err = gen(f)
        results.append(((rc == 0) == want, 'comp name %s %s' % (name, 'accepted' if want else 'refused'),
                        err.strip()))

comps = [f for f in glob.glob(os.path.join(ROOT, 'src', 'comps', '**', '*.c'), recursive=True) +
         glob.glob(os.path.join(ROOT, 'shared', 'comps', '*.c')) +
         glob.glob(os.path.join(ROOT, 'stm32f303', 'src', 'comps', '*.c'))
         if 'HAL_COMP(' in open(f, errors='replace').read()]
rc, err = gen(*comps)
results.append((rc == 0 and len(comps) > 50, 'all %d comps in the tree have addressable names' % len(comps),
                err.strip()))

print('1..%d' % len(results))
for i, (good, name, detail) in enumerate(results, 1):
    print('%sok %d - %s' % ('' if good else 'not ', i, name))
    if not good and detail:
        print('# ' + detail.replace('\n', '\n# '))
sys.exit(0 if all(r[0] for r in results) else 1)
