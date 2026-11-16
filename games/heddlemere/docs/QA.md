# Heddlemere QA

[한국어](QA-KO.md)

State: scaffold. The checks below were run on this change; the measurement items are added by the next changes.

| Check | How | Result |
|---|---|---|
| Build | `./build.sh -j8`, `./build.sh DEMO=rehearsal` | zero warnings |
| Orders | `python3 tools/order_check.py --plan-hash` | 40 of 40 ok, plan hash `0xfc5a15bb` |
| Rehearsal block | `tools/melonds/mds.sh run games/heddlemere/heddlemere-rehearsal.nds 1500 out/hm/reh "" 1499` | id `0x48524D48` (`HMRH`), values[0] bit 5 (ARM7 answered), values[21] = 5, values[22] = `0x1f`, values[23] = `0xfc5a15bb` (equals the checker's hash) |
| Auto QA run | hold L+R+X+Y 120 frames on the title, or the SD marker `cleancart/heddlemere/autoqa`. Headless: `SDDIR=out/hm/x tools/melonds/mds.sh run games/heddlemere/heddlemere.nds 24000 out/hm/x/o "300:L:140,300:R:140,300:X:140,300:Y:140"` (create `out/hm/x/cleancart/heddlemere/` first) | melonDS 1.1 software: `RESULT PASS`, 40 of 40 orders, chapters mask `0x1f`, 7 checkpoints; `tools/qa/parse_log.py` and `tools/qa/lint.py` ok |

Hand play (melonDS): D-pad up/down chooses a cell, left/right turns it, A delivers, B returns to the list.
