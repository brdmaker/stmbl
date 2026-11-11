#!/usr/bin/env python3
"""tools/comp_docs.py's parser on made-up component sources. Prints TAP."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import comp_docs as cd  # noqa: E402

n = fails = 0


def ok(c, name):
    global n, fails
    n += 1
    fails += not c
    print("%s %d - comp_docs: %s" % ("ok" if c else "not ok", n, name))


src = """#include "x_comp.h"
/*
* This program is free software: you can redistribute it ...
* Copyright (C) 2013 someone
*/
#include "hal.h"

// The x component: does a thing.
// Second line.
//
//   x0.a   indented: kept as written

HAL_COMP(x);

HAL_PIN(a);  // the input [V]
HAL_PIN(b);
HAL_PINA(c, 3);  // three of them
// HAL_PIN(commented_out);
"""
name, desc, pins = cd.parse(src)
ok(name == "x", "the component's name from HAL_COMP")
ok(desc.startswith("The x component: does a thing.\nSecond line.") and "Copyright" not in desc,
   "the description: the first comment that isn't the licence")
ok(pins == [("a", 1, "the input [V]"), ("b", 1, ""), ("c", 3, "three of them")], "pins, arrays and their comments")
ok("```\n  x0.a   indented: kept as written\n```" in cd.as_markdown(desc), "indented lines become a code block")
p = cd.page(name, "shared/comps/x.c", "test", desc, pins)
ok("| `c0`..`c2` | three of them |" in p and "## Pins (3, 2 described)" in p, "the page's pin table")
ok(cd.parse("int main(void) { return 0; }\n") is None, "no HAL_COMP: not a component")
doc = cd.parse('/** Doc block style.\n * More. */\nHAL_COMP(y);\nHAL_PIN(z); // zed\n')
ok(doc[1] == "Doc block style.\nMore.", "a /** */ doc block")
print("1..%d" % n)
sys.exit(1 if fails else 0)
