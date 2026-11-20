# Buoyane hardware QA

[한국어](HW-QA-KO.md)

**Run 1 (2026-11-20, console clock 2026-01-18 07:42) on a DS Lite + R4 Plus (Wood):** the QA run reaches `RESULT PASS` and every checkpoint hash equals melonDS 1.1 software
(`compare.py`: 0 differences). The QA block grades **L1 11/11, L2 18/22 OK, 4 gating NG** and 1 informational NG. The four NG items are findings about our expected values, not
emulator profiles; they are under review and `expected.json` is unchanged (see "Run 1" below). The console agrees with GBATEK on `aa-opaque-dot-hidden`, `edge-aa-translucent`,
`aa-marked-line-complete` and `attr-latch-begin`, where the emulators differ.

Steps (skill `nds-hw-qa`; DS Lite + R4 Plus): put `buoyane-hw-support.nds` (the release ROM with game code `####`, built by `tools/hwsupport.py`, so the
card gets the SD card) on the card with the folder `cleancart/buoyane/` and an empty file `autoqa` in it, start it, leave the title alone. The QA run plays
the 30 approaches, writes `qa-*.log`, and the judge suite writes `res-*-check-<scene>.log` for the five scenes. Wait for `suite end` in `suite-<stamp>.log` before powering off. Collect the files, then
`tools/qa/ingest.py`, `python3 games/buoyane/tools/judge.py --hardware`, and record the table here and in the Korean twin.

| Item | Emulator result (software / GL Classic / GL Compute / DeSmuME) | Hardware (run 1) |
|---|---|---|
| `aa-opaque-dot-hidden` | NG everywhere (GBATEK item, gates) | **OK** (GBATEK holds, all four emulators are wrong) |
| `fog-alpha-glitch` | NG everywhere | NG: 0, no glitch in this scene (informational) |
| `wire-texture-alpha` | NG everywhere (GBATEK item, gates) | **NG**: 0, same as the emulators (finding about our expectation) |
| `aa-marked-line-complete` | OK software and GL Compute; NG GL Classic, DeSmuME (GBATEK item, gates) | **OK** (GL Classic and DeSmuME are wrong) |
| `edge-aa-translucent` | OK software; NG GL Compute, GL Classic, DeSmuME | **OK** (GL Compute, GL Classic, DeSmuME are wrong) |
| `attr-latch-begin` | OK on all four | OK |
| `zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-decal-whole` | OK on melonDS software (105, 152, no hole); not asked before the run | **NG**: 109, 159, a hole of 960 pixels (findings, see run 1) |

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
| `zeq-slope-band-left` (`values[10]`, L2, GBATEK, want 101..107) | **109** (NG) | 105 (OK) | yes: the Equal band edge sits 4 pixels further right than GBATEK's +-200h model and than melonDS |
| `zeq-slope-band-right` (`values[11]`, want 149..155) | **159** (NG) | 152 (OK) | yes: 7 pixels further right |
| `zeq-decal-whole` (`values[9]` bit 3, want no hole) | **hole** (NG); `values[18]` = 960 | no hole; `values[18]` = 0 | yes: part of the decal fails the Equal test on the console |
| `wire-texture-alpha` (`values[15]` bit 2, want blended) | **0** (NG) | 0 (NG, profile) | no: the console agrees with all four emulators, so the finding is about our reading of GBATEK |
| `fog-alpha-glitch` (`unconfirmed`, want 1) | 0 (informational NG) | 0 | no: no glitch in this scene on the console either |
| `aa-opaque-dot-hidden` (`values[1]` bits 8, 9) | OK (`values[1]` = 0x7ff) | NG (0x6ff) | yes: GBATEK holds on the console |
| `edge-aa-translucent`, `aa-marked-line-complete`, `attr-latch-begin` | OK | see the table above | the console agrees with GBATEK; emulators per profile |
| not judged: `values[13]` (`W_ZEQ_1`, plane one depth step off) | **0** | `0x04000400` (1024 + 1024 pixels) | yes: melonDS draws the plane, the console does not |
| not judged: `values[14]` (`W_AA_LINES`) | `0xc000be` (192 and 190) | `0xc000c0` (192 and 192) | yes: 2 pixels fewer on the second count |
| the other judged items | as melonDS soft | | no |

Words that differ between the two result blocks (hardware, melonDS soft): `values[1]` (`0x7ff`, `0x6ff`), `[9]` (`0x1f7`, `0x1ff`), `[10]` (109, 105), `[11]` (159, 152), `[13]` (0, `0x04000400`),
`[14]` (`0xc000be`, `0xc000c0`), `[18]` (960, 0), plus the frame word and the lag total. Everything else is equal, including the state hash.

One run only. The Equal values (109, 159, 960 and the missing `values[13]` plane) are not explained by GBATEK's +-200h tolerance model; what to do with them (set the expectation from the console, or
move the item to `probes/` at level 4) is open, and so is `wire-texture-alpha` (whether GBATEK says that a wireframe blends with the texture alpha). `expected.json` is not changed by this run's record.
