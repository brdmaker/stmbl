# Config patches

`NNNN.json`: how a drive config for firmware version N-1 becomes one for N
(`STMBL_FW_VERSION`, shared/version_number.h). Only versions that change
what a config must say have one; the others change nothing. Every operation
has an exact inverse, so a config also goes back down to an older firmware.
Format and tool: `tools/stmblconf.py`; the whole scheme:
docs/firmware_versions.md.

```json
{"version": 2, "description": "what changed and why",
 "ops": [
   {"op": "rename", "from": "comp0.old_pin", "to": "comp0.new_pin"},
   {"op": "set", "pin": "conf0.max_sat", "from": 0.2, "to": 0.3},
   {"op": "line", "from": "link old_template", "to": "link new_template"}
 ]}
```
