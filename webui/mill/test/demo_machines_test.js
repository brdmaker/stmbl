// node test/demo_machines_test.js: the simulation's example machines are the repo's machine.json files
"use strict";
const fs = require("fs");
const path = require("path");
const D = require("../js/demo_machines.js");
let fails = 0;
for (const [name, dir] of [["router", "excitech"]]) {
  const file = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "machines", dir, "machine.json"), "utf8"));
  const same = JSON.stringify(file) === JSON.stringify(D[name]);
  console.log((same ? "ok   " : "FAIL ") + `${name}: js/demo_machines.js is machines/${dir}/machine.json`);
  if (!same) fails++;
}
process.exit(fails ? 1 : 0);
