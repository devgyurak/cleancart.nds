# Paneveil hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

Nothing in Paneveil has been measured on a console yet. This is the checklist for the run, so the first values can
be recorded and the `unconfirmed` items in `expected.json` can be promoted. The console is operated by a person; the
steps below are what to do and what to write down.

**Automatic route (skill `nds-hw-qa`):** create the empty folder `cleancart/paneveil/` on the card and the rehearsal
writes its result block there (`res-*-paneveil-rehearsal.log`); an empty file `cleancart/paneveil/autoqa` starts the
QA run without a button hold. `python3 tools/qa/ingest.py /Volumes/<card> --game paneveil` then grades everything with
`judge.py --hardware --partial`. Hardware NG items are findings, not profiles: see the template pages of the other kits for
the record table. Old `res-*` logs on the card are cleared first.

## What the hardware has to answer

| Question | Where it shows |
|---|---|
| Do the matrix stack levels (GXSTAT 8-12, 13), pop with N = 2, store and restore, and the projection stack behave as GBATEK says? | rehearsal values 3 to 11 (expected 3 0 1 0 2 0 1 0 0) |
| Do CLIPMTX_RESULT and VECMTX_RESULT equal the model after load and multiply, through the packed GXFIFO and the direct ports? | values 12 to 15 (all 0) |
| Are the six vertex formats, culling, the back/front render bits, alpha, wireframe, the depth test and texture repeat, flip, clamp, colour 0, PLTT_BASE and coordinate mode 1 right in the picture? | values 16 to 30 and the flags (capture bits 8-12 set) |
| Is the wireframe (alpha 0) drawn at all on the hardware? (it is not in DeSmuME and melonDS GL Classic) | value 21 (`poly-wire-edges`) |
| Does the whole game run to the credits (auto QA run) and does the save chip behave? | game ROM, hidden QA mode |

## Steps

1. Build: `cd games/paneveil && ./build.sh -j8 release`. Note `shasum -a 256` of `build/release/rom/paneveil.nds` and of
   `build/release/judge/paneveil-rehearsal.nds`.
2. Copy both ROMs to the R4 card and create `cleancart/paneveil/`.
3. **Rehearsal**: start `paneveil-rehearsal.nds`, touch nothing, wait about 20 seconds. It plays the first window of every
   chapter, runs the light box and prints `REHEARSAL RESULT` with values 3 to 30 in hex. With the folder on the card the block
   is also written to the SD card.
4. **Game**: start `paneveil.nds`. Play 1-01 (Down to GLASS SIDE, Right once, A), Select for the 2D schematic, B back.
5. **Auto QA run**: leave an empty `cleancart/paneveil/autoqa` on the card (or hold L+R+X+Y for two seconds on the title). The game
   plays itself to the credits (about 70 seconds in an emulator) and writes `cleancart/paneveil/qa-*.log`.
   Then `python3 tools/qa/ingest.py /Volumes/<card> --game paneveil` and compare with an emulator run
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).

## Recording

- `docs/QA.md` and `docs/QA-KO.md` (twins): ROM SHA-256s, console and card, rehearsal values, the QA log header
  (`dldi`, `nitro`, `save_detected`, `cart_ready`), the first checkpoint that differs from the emulator.
- `expected.json`: for every `unconfirmed` item whose hardware value matches the ROM's expectation change `source` to
  `hw:ds-lite`. If the hardware disagrees the item is a finding: record the value and decide whether the ROM or the
  expectation is wrong. Add a profile `hw-ds-lite` (the failing items) to `known`.
- Never commit anything under `build/qa/`.
