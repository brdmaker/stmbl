"""A small S-expression reader/writer for KiCad files (symbols, schematics)."""
import re

TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))')


class Sym(str):
    """An unquoted atom."""


def parse(text):
    stack, cur = [], []
    pos = 0
    while True:
        m = TOKEN.match(text, pos)
        if not m or m.end() == pos:
            break
        pos = m.end()
        if m.group(1):
            stack.append(cur)
            cur = []
        elif m.group(2):
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif m.group(3) is not None:
            cur.append(re.sub(r"\\(.)", lambda e: "\n" if e.group(1) == "n" else e.group(1), m.group(3)))
        else:
            cur.append(Sym(m.group(4)))
    return cur[0] if cur else None


def dump(x, indent=0):
    if isinstance(x, list):
        if not x:
            return "()"
        inner = [dump(e, indent + 1) for e in x]
        one = "(" + " ".join(inner) + ")"
        if len(one) < 100 and "\n" not in one:
            return one
        return "(" + inner[0] + "".join("\n" + "  " * (indent + 1) + i for i in inner[1:]) + ")"
    if isinstance(x, Sym):
        return str(x)
    if isinstance(x, bool):
        return "yes" if x else "no"
    if isinstance(x, (int, float)):
        return ("%.4f" % x).rstrip("0").rstrip(".") if isinstance(x, float) else str(x)
    return '"' + str(x).replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def find(x, key):
    """The first child list whose head is key."""
    for e in x:
        if isinstance(e, list) and e and e[0] == key:
            return e
    return None


def findall(x, key):
    return [e for e in x if isinstance(e, list) and e and e[0] == key]
