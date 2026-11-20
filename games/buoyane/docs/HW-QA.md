# Buoyane hardware QA

[한국어](HW-QA-KO.md)

**Run 1 (2026-11-20, console clock 2026-01-18 07:42) on a DS Lite + R4 Plus (Wood):** the QA run reaches `RESULT PASS` and every checkpoint hash equals melonDS 1.1 software
(`compare.py`: 0 differences). The QA block grades **L1 11/11, L2 18/22 OK, 4 gating NG** and 1 informational NG. The four NG items were findings about our expected values, not
emulator profiles; `expected.json` was changed afterwards (see "Expected values from the run" below). The console agrees with GBATEK on `aa-opaque-dot-hidden`, `edge-aa-translucent`,
`aa-marked-line-complete` and `attr-latch-begin`, where the emulators differ.

Steps (skill `nds-hw-qa`; DS Lite + R4 Plus): put `buoyane-hw-support.nds` (the release ROM with game code `####`, built by `tools/hwsupport.py`, so the
card gets the SD card) on the card with the folder `cleancart/buoyane/` and an empty file `autoqa` in it, start it, leave the title alone. The QA run plays
the 30 approaches, writes `qa-*.log`, and the judge suite writes `res-*-check-<scene>.log` for the five scenes. Wait for `suite end` in `suite-<stamp>.log` before powering off. Collect the files, then
`tools/qa/ingest.py`, `python3 games/buoyane/tools/judge.py --hardware`, and record the table here and in the Korean twin.

| Item | Emulator result (software / GL Classic / GL Compute / DeSmuME) | Hardware (run 1) |
|---|---|---|
| `aa-opaque-dot-hidden` | NG everywhere (GBATEK item, gates) | **OK** (GBATEK holds, all four emulators are wrong) |
| `fog-alpha-glitch` | NG everywhere | NG: 0, no glitch in this scene (informational; moved to `probes/`) |
| `wire-texture-alpha` | NG everywhere (GBATEK item, gates) | **NG**: 0, same as the emulators (GBATEK says otherwise; expected value now 0, `hw:dslite-ntr`) |
| `aa-marked-line-complete` | OK software and GL Compute; NG GL Classic, DeSmuME (GBATEK item, gates) | **OK** (GL Classic and DeSmuME are wrong) |
| `edge-aa-translucent` | OK software; NG GL Compute, GL Classic, DeSmuME | **OK** (GL Compute, GL Classic, DeSmuME are wrong) |
| `attr-latch-begin` | OK on all four | OK |
| `zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-decal-whole` | OK on melonDS software (105, 152, no hole); not asked before the run | **NG**: 109, 159, a hole of 960 pixels (the position and the holes are not judged any more; the width is, see below) |

## Run 1: QA run and judge suite, 2026-11-20

| Field | Value |
|---|---|
| Date, console, card | 2026-11-20, DS Lite (NTR), "R4 Plus" with the Wood R4 kernel. The console clock read 2026-01-18 07:42, so the files say `*-20260118-*` |
| ROM SHA-256 | release `buoyane-hw-support.nds` v0.1.0, `15587e8fa38195578bf6e1712172041ca3e0c5fdd6337a6e76c768a888f91ad3`, build `9aea0989642b`; melonDS 1.1 soft was run with the same file |
| `ingest.py` / `judge.py --hardware` | QA log PASS, 112 lines, 7 checkpoints, `start=marker`. QA block: L1 11/11 OK, L2 18/22 OK +1 informational, **4 gating NG** (`zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-decal-whole`, `wire-texture-alpha`) and 1 informational NG (`fog-alpha-glitch`) |
| QA log header | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1`; every `save write` has `sd_match=1` |
| First checkpoint that differs from melonDS | None: ch1-ch5, judge and credits hashes equal melonDS 1.1 soft (`compare.py`, 0 differences) |
| Lag frames | 16 / 19 / 18 / 16 / 17 / 3 / 0 (ch1-ch5, judge, credits), 89 in all; melonDS soft 0 everywhere |
| 3D flags (`gx=` `rdl=`) | `uf0,of0` everywhere; `rdl=46` at every checkpoint, as in melonDS (the game draws 3D) |
| Suite | `suite block block=qa saved=1`, the five demos `check-dot`, `-fog`, `-edge`, `-depth`, `-blend` all `saved=1`, `suite end demos=5 saved=5`, then 12 probe steps `ok=1` up to `wait60`. No `suite done` line (a known problem of every game) |

| Item | Hardware | melonDS 1.1 soft | Differs? |
|---|---|---|---|
| `zeq-slope-band-left` (`values[10]`, L2, GBATEK, want 101..107) | **109** (NG) | 105 (OK) | yes: the Equal band edge sits 4 pixels further right than our depth model and than melonDS |
| `zeq-slope-band-right` (`values[11]`, want 149..155) | **159** (NG) | 152 (OK) | yes: 7 pixels further right |
| `zeq-decal-whole` (`values[9]` bit 3, want no hole) | **hole** (NG); `values[18]` = 960 | no hole; `values[18]` = 0 | yes: part of the decal fails the Equal test on the console |
| `wire-texture-alpha` (`values[15]` bit 2, want blended) | **0** (NG) | 0 (NG, profile) | no: the console agrees with all four emulators; GBATEK's text (quoted below) is what differs |
| `fog-alpha-glitch` (`unconfirmed`, want 1) | 0 (informational NG) | 0 | no: no glitch in this scene on the console either |
| `aa-opaque-dot-hidden` (`values[1]` bits 8, 9) | OK (`values[1]` = 0x7ff) | NG (0x6ff) | yes: GBATEK holds on the console |
| `edge-aa-translucent`, `aa-marked-line-complete`, `attr-latch-begin` | OK | see the table above | the console agrees with GBATEK; emulators per profile |
| not judged: `values[13]` (`W_ZEQ_1`, plane one depth step off) | **0** | `0x04000400` (1024 + 1024 pixels) | yes: melonDS draws the plane, the console does not |
| not judged: `values[14]` (`W_AA_LINES`) | `0xc000be` (192 and 190) | `0xc000c0` (192 and 192) | yes: 2 pixels fewer on the second count |
| the other judged items | as melonDS soft | | no |

Words that differ between the two result blocks (hardware, melonDS soft): `values[1]` (`0x7ff`, `0x6ff`), `[9]` (`0x1f7`, `0x1ff`), `[10]` (109, 105), `[11]` (159, 152), `[13]` (0, `0x04000400`),
`[14]` (`0xc000be`, `0xc000c0`), `[18]` (960, 0), plus the frame word and the lag total. Everything else is equal, including the state hash.

One run only; this record changed no expected value. The next section says what was done with the four NG items afterwards.

## Expected values from the run

| Item | Change | Why |
|---|---|---|
| `wire-texture-alpha` | expected `0` (mask 4), source `hw:dslite-ntr`, `level_basis` `hw:games/buoyane/docs/HW-QA.md`; title "GBATEK says At<31 makes the wireframe translucent; a DS Lite draws it without blending"; leaves the `ng` of all four profiles | GBATEK ("DS 3D Texture Blending", Uni-Colored Textures): "Wire-frame polygons are always having Av=31, however, they can be made transparent by using Translucent Textures (ie. A5I3 or A3I5 formats) with At<31." The console draws no blended wire pixel (`values[15]` bit 2 = 0, `values[16..17]` = `001f7fff 001fffff`), and neither does any emulator. The bit cannot tell "drawn opaque" from "not drawn": the white wire pixels are not counted yet (planned ROM change), so the item only guards against an emulator that starts blending |
| `zeq-slope-band-left`, `zeq-slope-band-right` | replaced by `zeq-slope-band-width` (L2, GBATEK `DS 3D Polygon Attributes`): right x minus left x in 42..54; the two position items are gone | the width follows from GBATEK's +-200h window and fits the console (50) and melonDS software and Compute (47); the position (x 104 and 152) leaned on our own depth model, and the console sits about 5.5 pixels further right. GL Classic (95) and DeSmuME (23) fail the width and join `ng` |
| `zeq-decal-whole` | removed from the game | the holes (960 pixels) depend on rasteriser rounding, which nothing documents. `values[18]` stays in the block, not judged |
| `values[13]` (`W_ZEQ_1`) | still in the block, not judged | the probe `zeq-step-boundary` (`probes/`) measures the same one-step planes |
| `fog-alpha-glitch` | removed from the game | one negative result in one scene does not confirm "no glitch", and an `unconfirmed` item is L4, which games do not hold. The console value 0 is recorded here and in `docs/HW-FINDINGS.md` section 3 |
| Profiles | `ng`: software 1 (`aa-opaque-dot-hidden`), Compute 2 (that and `edge-aa-translucent`), GL Classic 15, DeSmuME 13 | the rehearsal block of each renderer was graded again: all four MATCH |

Follow-ups (not in this change): the probe items for the band position (109 and 159 on the console), the decal holes (960) and the fog alpha glitch (0) go to `probes/`, with these values as the hardware observation (source "Buoyane v0.1.0 hw run 1");
a ROM change adds the white wire pixel count and fixes the stale step wording of `zeq-tolerance-flat` and the band comment in `lightcheck.c`; then Buoyane v0.1.1 and hardware run 2.
