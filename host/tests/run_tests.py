#!/usr/bin/env python3
"""Run host simulator tests (*.test) and check their expectations (TAP output).

A .test file is a stmbl_host script plus directives:

  @args ARGS...            extra stmbl_host arguments, paths relative to host/
                           (e.g. --drive ../hwtest/burnin/ECMA-C30604FS/stmbl.conf)
  @include FILE            insert FILE (relative to host/) into the script
  @sample PIN DT T         run T seconds, printing PIN every DT seconds
  @timeout SEC             wall-clock limit (default 120)
  @reject REGEX            fail if any output line matches REGEX
  @match REGEX             fail unless some output line matches REGEX
  @fail MESSAGE            always fail with MESSAGE (a generator's way to
                           report that it could not build the test)
  @todo REASON             known failure: failing checks print as
                           "not ok ... # TODO REASON" (TAP) and do not fail
                           the run; passing ones print "# TODO", so a fixed
                           issue shows up
  @expect CHECK            check printed pin values after the run

CHECK forms (PIN[k] is the k-th printed value of PIN, 1-based, -1 = last;
plain PIN means the last one):

  PIN OP VALUE             OP is one of == != < <= > >=
  PIN ~ VALUE TOL          |got - VALUE| <= TOL; TOL is absolute or "N%"
  max|PIN| OP VALUE        over every printed value of PIN
  max PIN OP VALUE / min PIN OP VALUE
  span PIN OP VALUE        max - min (a ripple)
  max|PIN[a:b]| ...        aggregates over printed values a..b (1-based,
                           inclusive; an end left out: to the first / last)

Values are read from lines "pin = v" and "OK sink <= source = v".
Everything else is passed to stmbl_host unchanged.

usage: run_tests.py [-v] FILE.test...   (run from host/, or any directory)
"""

import collections
import math
import operator
import os
import re
import shlex
import subprocess
import sys
import tempfile

HOST_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VALUE_RE = re.compile(r"^(?:OK )?([A-Za-z_]\w*\.\w+)(?: <= \S+)? = (\S+)")
OPS = {"==": operator.eq, "!=": operator.ne, "<=": operator.le,
       ">=": operator.ge, "<": operator.lt, ">": operator.gt}
CHECK_RE = re.compile(
    r"^(?:(?P<agg>max|min|span)\s*(?P<abs>\|)?\s*)?"
    r"(?P<pin>[A-Za-z_]\w*\.\w+)(?:\[(?:(?P<idx>-?\d+)|(?P<a>\d*):(?P<b>\d*))\])?\s*(?P=abs)?\s*"
    r"(?P<op>==|!=|<=|>=|<|>|~)\s*(?P<val>\S+)(?:\s+(?P<tol>\S+))?$")


class TestError(Exception):
    pass


Script = collections.namedtuple("Script", "text args expects rejects timeout fails todo matches")


def build_script(path):
    """Expand a .test file into a Script."""
    args, expects, rejects, out, fails, matches = [], [], [], [], [], []
    timeout = 120.0
    todo = None

    def walk(p, depth):
        nonlocal timeout, todo
        if depth > 4:
            raise TestError("@include nested too deep")
        with open(p) as f:
            for n, raw in enumerate(f, 1):
                line = raw.rstrip("\n")
                s = line.strip()
                if not s.startswith("@"):
                    out.append(line)
                    continue
                word, _, rest = s.partition(" ")
                rest = rest.strip()
                where = "%s:%d" % (os.path.basename(p), n)
                if word == "@args":
                    args.extend(shlex.split(rest))
                elif word == "@include":
                    walk(os.path.join(HOST_DIR, rest), depth + 1)
                elif word == "@sample":
                    pin, dt, t = rest.split()
                    for _ in range(int(round(float(t) / float(dt)))):
                        out.append("run %s" % dt)
                        out.append(pin)
                elif word == "@timeout":
                    timeout = float(rest)
                elif word == "@fail":
                    fails.append((rest, where))
                elif word == "@todo":
                    todo = rest
                elif word == "@reject":
                    rejects.append((re.compile(rest), where))
                elif word == "@match":
                    matches.append((re.compile(rest), where))
                elif word == "@expect":
                    if not CHECK_RE.match(rest):
                        raise TestError("%s: bad @expect: %s" % (where, rest))
                    expects.append((rest, where))
                else:
                    raise TestError("%s: unknown directive %s" % (where, word))

    walk(path, 0)
    return Script("\n".join(out) + "\n", args, expects, rejects, timeout, fails, todo, matches)


def parse_values(output):
    values = {}
    for line in output.splitlines():
        m = VALUE_RE.match(line)
        if m:
            try:
                v = float(m.group(2))
            except ValueError:
                continue
            values.setdefault(m.group(1), []).append(v)
    return values


def check(expr, values):
    """Return (passed, detail)."""
    m = CHECK_RE.match(expr)
    pin, op, val, tol = m.group("pin"), m.group("op"), m.group("val"), m.group("tol")
    seq = values.get(pin)
    if not seq:
        return False, "%s was never printed" % pin
    if m.group("agg"):
        if m.group("idx") is not None:
            return False, "index not allowed with max/min/span (use [a:b])"
        if m.group("a") is not None:
            a = int(m.group("a") or 1)
            b = int(m.group("b") or len(seq))
            if a < 1 or b > len(seq) or a > b:
                return False, "%s printed %d times, wanted [%d:%d]" % (pin, len(seq), a, b)
            seq = seq[a - 1:b]
        data = [abs(v) for v in seq] if m.group("abs") else seq
        if any(math.isnan(v) for v in data):
            return False, "%s has nan values" % pin
        if m.group("agg") == "span":
            got = max(data) - min(data)
        else:
            got = max(data) if m.group("agg") == "max" else min(data)
    else:
        if m.group("a") is not None:
            return False, "a range needs max/min/span"
        idx = int(m.group("idx") or -1)
        try:
            got = seq[idx - 1] if idx > 0 else seq[idx]
        except IndexError:
            return False, "%s printed %d times, wanted [%d]" % (pin, len(seq), idx)
    if math.isnan(got):
        return False, "got nan"
    want = float(val)
    if op == "~":
        if tol is None:
            return False, "~ needs a tolerance"
        t = abs(want) * float(tol[:-1]) / 100.0 if tol.endswith("%") else float(tol)
        return abs(got - want) <= t, "got %g" % got
    return OPS[op](got, want), "got %g" % got


def run_test(path, verbose):
    """Yield (passed, description, detail_lines, todo_reason_or_None)."""
    name = os.path.splitext(os.path.basename(path))[0]
    try:
        sc = build_script(path)
    except (OSError, ValueError, TestError) as e:
        yield False, "%s: parse" % name, [str(e)], None
        return
    for r in _run_checks(name, sc, verbose):
        yield r + (sc.todo,)


def _run_checks(name, sc, verbose):
    args, expects, rejects, timeout = sc.args, sc.expects, sc.rejects, sc.timeout
    for msg, where in sc.fails:
        yield False, "%s: %s" % (name, msg), [where]
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write(sc.text)
        tmp = f.name
    try:
        proc = subprocess.run(["./stmbl_host"] + args + [tmp], cwd=HOST_DIR,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=timeout, universal_newlines=True)
        output, rc = proc.stdout, proc.returncode
    except subprocess.TimeoutExpired as e:
        out = e.stdout or ""
        output = out.decode(errors="replace") if isinstance(out, bytes) else out
        rc = "timeout after %gs" % timeout
    finally:
        os.unlink(tmp)

    tail = output.splitlines()[-15:]
    yield rc == 0, "%s: stmbl_host exits cleanly" % name, (
        ["exit status: %s" % rc] + tail if rc != 0 else [])
    for rx, where in rejects:
        hits = [l for l in output.splitlines() if rx.search(l)]
        yield not hits, "%s: no output matching /%s/" % (name, rx.pattern), hits[:5]
    for rx, where in sc.matches:
        hit = any(rx.search(l) for l in output.splitlines())
        yield hit, "%s: output matches /%s/" % (name, rx.pattern), ([] if hit else tail)
    values = parse_values(output)
    for expr, where in expects:
        ok, detail = check(expr, values)
        lines = [detail] if (not ok or verbose) else []
        yield ok, "%s: %s" % (name, expr), lines


def main(argv):
    verbose = "-v" in argv
    files = [a for a in argv if a != "-v"]
    if not files:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    if not os.access(os.path.join(HOST_DIR, "stmbl_host"), os.X_OK):
        print("Bail out! %s/stmbl_host not built" % HOST_DIR)
        return 1
    n = failed = todo_failed = 0
    for path in files:
        print("# %s" % os.path.relpath(path))
        for ok, desc, detail, todo in run_test(os.path.abspath(path), verbose):
            n += 1
            if not ok:
                if todo:
                    todo_failed += 1
                else:
                    failed += 1
            print("%s %d - %s%s" % ("ok" if ok else "not ok", n, desc,
                                    " # TODO %s" % todo if todo else ""))
            for d in detail:
                print("#   %s" % d)
        sys.stdout.flush()
    print("1..%d" % n)
    print("# %d/%d passed%s" % (n - failed - todo_failed, n,
                                ", %d known (TODO) failures" % todo_failed if todo_failed else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
