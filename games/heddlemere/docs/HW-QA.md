# Heddlemere hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/heddlemere-hw-support.nds` on the card instead of `heddlemere.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

**Judge suite (since the QA judge suite PRs):** the QA run (`autoqa`) now ends with a suite: after `RESULT PASS` it writes the QA block (it carries the rehearsal items) as `res-*-heddlemere-*.log` and a progress file `suite-<stamp>.log`. One run of the hw-support ROM is enough; **wait for the end of the suite** (`suite end` in `suite-<stamp>.log`) before powering off. The separate rehearsal / demo ROMs below are only for re-running one item (a Wood kernel gives them no SD card). `tools/qa/ingest.py` grades every file on its own.

**Run 1 on a DS Lite (2026-11-17), recorded at the end of this page:** QA run PASS, every judged item OK.

Before run 1 nothing had been measured on a console. This is the checklist for the run, so the first values can be
recorded, the four `unconfirmed` items in `expected.json` can be promoted and the riskiest gating figure (the ARM9
HBlank-low span, GBATEK 1606) can be settled. The console is operated by a person; the steps below are what to do and
what to write down. The target is one DS Lite; DSi and 3DS values do not exist.

**Automatic route (skill `nds-hw-qa`):** create the empty folder `cleancart/heddlemere/` on the card and the rehearsal
writes its result block there (`res-*-heddlemere-rehearsal.log`); an empty file `cleancart/heddlemere/autoqa` starts the
QA run without a button hold. `python3 tools/qa/ingest.py /Volumes/<card> --game heddlemere` then grades everything with
`judge.py --hardware --partial`. Hardware NG items are findings, not profiles. Old `res-*` logs on the card are cleared first.

## What the hardware has to answer

| Question | Where it shows | GBATEK | melonDS 1.1 |
|---|---|---|---|
| How long is the HBlank flag low on the ARM9? (informational until measured; promote to gating when 1606 +-2 repeats) | values[3] (x16) | 1606 cycles (25696) | 1584 |
| ... and on the ARM7? (GBATEK 1613 "for whatever reason"; polled from our own ARM7 core, so the poll loop may be part of it) | values[4] | 1613 | 1584 |
| Line and frame length | values[1], [2] | 2130 (34080) / 560190 | same |
| Does a VCOUNT write at line 212 repeat 10 or 11 lines? (the cycle residual, values[8], is recorded but not judged: probes/ material) | values[7], [8] | 10 or 11 / 0-4 | 11 / 1 |
| MTX_IDENTITY, PUSH+POP, TRANS in mode 2 (+30), SCALE in mode 2 (+0) | values[9], [10], [12], [13] | 19 / 53 / 30 / 0 | same |
| MTX_TRANS and BOX_TEST (the two where GBATEK and melonDS disagree) | values[11], [14] | 22 / 103 | 35 / 257 |
| Full-FIFO stall per write, half-full IRQ per entry | values[15], [16] | 19 / 19 | 18.9 / 19.0 |
| VBlank to idle after a bare SWAP_BUFFERS | values[17] | 392 | 328 |
| Polygon RAM count at the HBlank of lines 191 and 192 | values[18], [19] | 64 / 0 | 64 / 0 |
| Does a texture palette written in lines 192..213 show in the render? | values[20] (0 lines wrong) | 0 | 0 |

## Steps

1. Build: `cd games/heddlemere && ./build.sh -j8 release`. Note `shasum -a 256` of `build/release/rom/heddlemere.nds` and of
   `build/release/judge/heddlemere-rehearsal.nds`.
2. Copy both ROMs to the R4 card and create `cleancart/heddlemere/`.
3. **Rehearsal, three times** (the result must repeat): start `heddlemere-rehearsal.nds`, touch nothing, wait about 60 seconds.
   It measures (the screen stays as the last order for a while), plays the first order of every chapter and prints
   `REHEARSAL RESULT` with the values in hex; with the folder on the card the block is also written to the SD card.
4. **Game**: start `heddlemere.nds`. Play 1-01 (A, A on the list, set the cells, A), Select for the blueprint view, B back.
   Watch the motor lamp after a delivery: on the hardware it should read OK if the GBATEK costs hold.
5. **Auto QA run**: leave an empty `cleancart/heddlemere/autoqa` on the card (or hold L+R+X+Y for two seconds on the title). The game
   plays itself to the credits and writes `cleancart/heddlemere/qa-*.log`. Then
   `python3 tools/qa/ingest.py /Volumes/<card> --game heddlemere` and compare with an emulator run
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).

## Recording

- `docs/QA.md` and `docs/QA-KO.md` (twins), and this page's table: ROM SHA-256s, console and card, the three rehearsal blocks.
- `expected.json`: for every `unconfirmed` item whose hardware value is reproducible set `source` to `hw:docs/HW-QA.md` and the
  range to the measured value; if the hardware value differs from the GBATEK figure of a gating item, change the expectation to the
  measured value, set the basis to `hw:` and say why in the PR. A value that does not repeat between the three runs belongs to
  `probes/` (L4), not here. Add a profile `hw-ds-lite` to `known`.
- Never commit anything under `build/qa/`.

## Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256 | `heddlemere-hw-support.nds` v0.2.1 `034f3b0816abb48ce1d9645231994d4c52d8a5371d260832f32d073536b34b89` (442,732 bytes, same as the release); melonDS 1.1 soft was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 33 lines, 7 checkpoints, build `b3cd2aab29e4`, started with `start=marker`. Suite: QA block only (`suite block block=qa saved=1`; no suite demos, so no `suite end` line by design). Judge: **21 of 21 gating items OK** (L1 8/8, L2 3/3, L3 10/10) +5 informational (3 of them outside their unconfirmed ranges, below), no n/a |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None: ch1-ch5, judge and credits hashes equal melonDS 1.1 soft (`compare.py`, 0 differences) |
| Lag frames | 2 / 2 / 2 / 13 / 11 / 2 / 0 (ch1-ch5, judge, credits); melonDS soft 0 / 0 / 0 / 11 / 9 / 0 / 0 |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0 rdl=46` at every 3D checkpoint, as melonDS |

| Item | Hardware | melonDS 1.1 soft | Differs? |
|---|---|---|---|
| `hblank-low-arm9` (unconfirmed, 25664..25728) | 25710 | 25344 | inside the range on the console (GBATEK 1606 x16 holds) |
| `hblank-low-arm7` (unconfirmed, 25776..25840) | 25712 | 25342 | yes: the ARM7 span equals the ARM9 one on the console, not GBATEK's 1613 |
| `gx-trans-cycles` (unconfirmed, 348..356) | 560 (35 x16) | | the console charges 35 cycles for MTX_TRANS, as melonDS does; GBATEK's 22 does not hold |
| `gx-box-cycles` (unconfirmed, 1640..1656) | 2020 (about 126 x16) | 4112 (257 x16) | neither GBATEK's 103 nor melonDS's 257 |
| `swap-tail` (unconfirmed, 6208..6336) | 6256 | 5224 | inside the range on the console |
| gating slopes (`gx-identity-slope`, `gx-pushpop-slope`, `gx-halfirq-slope`, `gx-trans-mode2-extra`, `gx-scale-mode2-extra`) | 305 / 848 / 303 / 480 / -2 | 304 / 847 / 305 / 479 / 0 | within their tolerances on both |
| the other judged items | as melonDS soft | | no |
