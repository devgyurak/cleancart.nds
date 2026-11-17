# Heddlemere hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/heddlemere-hw-support.nds` on the card instead of `heddlemere.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

**Judge suite (since the QA judge suite PRs):** the QA run (`autoqa`) now ends with a suite: after `RESULT PASS` it writes the QA block (it carries the rehearsal items) as `res-*-heddlemere-*.log` and a progress file `suite-<stamp>.log`. One run of the hw-support ROM is enough; **wait for the end of the suite** (`suite end` in `suite-<stamp>.log`) before powering off. The separate rehearsal / demo ROMs below are only for re-running one item (a Wood kernel gives them no SD card). `tools/qa/ingest.py` grades every file on its own.

Nothing in Heddlemere has been measured on a console yet. This is the checklist for the run, so the first values can be
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
