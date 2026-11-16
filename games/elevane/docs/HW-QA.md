# Elevane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

Nothing in Elevane has been measured on a console yet. This is the checklist for the run, so the
first values can be recorded and the `unconfirmed` items in `expected.json` can be promoted.
The console is operated by a person; the steps below are what to do and what to write down.

**Automatic route (skill `nds-hw-qa`):** create the empty folder `cleancart/elevane/` on the card and the ROMs
write their result blocks there (`RESULT SAVED` on the demos); an empty file `cleancart/elevane/autoqa` starts the QA
run without a button hold. `python3 tools/qa/ingest.py /Volumes/<card> --game elevane` then grades everything with
`judge.py --hardware --partial`. The photo route below (steps 3 and 6) stays as the fallback when the card cannot be
written. Hardware NG items are findings, not profiles: see the template pages of the other kits for the record table.

## What the hardware has to answer

| Question | Where it shows |
|---|---|
| Does both-screens 3D (display capture, 30 fps each) look right, and is the routing latency of one call correct? | game, any puzzle: both screens show the matching front/plan views, no flicker between the two |
| Does showcase preset 1 (HBlank DMA, the melonDS #2618 path) draw the whole picture every frame? | rehearsal values 10, 11 (holes) = 0, 12/13 = 3072; game: X in a puzzle, look at the top screen |
| Does preset 2 (rear plane) draw both halves? | rehearsal value 15 = 4096; game: A in the showcase |
| Are the two passes under 2048 polygons / 6144 vertices each, no overflow flag? | rehearsal values 7, 8, 9 (polygon counters exist on hardware) |
| Is there a bright seam column at x = 128 anywhere? (not seen in the emulators on these paths) | look at the showcase and at the dual views |
| Does the whole game run to the credits (auto QA run), and does the save chip behave? | game ROM, hidden QA mode |

## Steps

1. Build: `cd elevane && ./build.sh -j8 release`. Note `shasum -a 256` of `build/release/rom/elevane.nds` and
   of `build/release/judge/elevane-rehearsal.nds`.
2. Copy both ROMs to the R4 card (leave room for `cleancart/elevane/`).
3. **Rehearsal**: start `elevane-rehearsal.nds`, touch nothing, wait about a minute. It solves the first puzzle of
   every chapter, runs both showcase presets and then prints `REHEARSAL RESULT` with values 0 to 17 in hex.
   Photograph the top screen (sharp, all lines readable). Also note, from watching it run, whether the showcase
   pictures flickered or showed a hole.
4. **Game**: start `elevane.nds`. Play a puzzle (`1-01`: a row of four, then A to deliver), press X for the showcase,
   A to switch preset, B back. Write down per preset: complete picture / half missing (which side) / flicker /
   bright seam column.
5. **Auto QA run** (skill `nds-hw-qa`): on the title press L and R, then X and Y, hold all four for two seconds (or leave an empty `cleancart/elevane/autoqa` on the card: no button needed).
   The game plays itself to the credits (about 5 minutes) and writes `cleancart/elevane/qa-*.log` to the SD card.
   Then `python3 tools/qa/ingest.py /Volumes/<card> --game elevane` and compare with an emulator run
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
6. Grade the rehearsal photo: `python3 games/elevane/tools/judge.py --values <hex> <hex> ...` (values 0 to 17 in order,
   exit 0 = no gating item failed). Add `--profile melonds-1.1-software` to see how it differs from an emulator.

## Recording

- `docs/QA.md` and `docs/QA-KO.md` (twins): ROM SHA-256s, console and card, rehearsal values, what each preset
  looked like, the QA log header (`dldi`, `nitro`, `save_detected`, `cart_ready`), the first checkpoint that differs
  from the emulator, and "chip save not verified on R4" if no chip was detected.
- `expected.json`: for every `unconfirmed` item whose hardware value matches the ROM's expectation change `source`
  to `hw:ds-lite` and set `model` to `"DS Lite"`. If the hardware disagrees the item is a finding: keep it
  informational, record the value, and decide whether the ROM or the expectation is wrong. Add a profile
  `hw-ds-lite` (the failing items) to `known`.
- If a seam column shows: implement the two fixes from the work order (overlap the viewports by one column, or change the
  blend weights), compare them on the console and set the one that hides it as the default.
- Never commit anything under `build/qa/`.
