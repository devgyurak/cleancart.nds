# Elevane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/elevane-hw-support.nds` on the card instead of `elevane.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

**Judge suite (since the QA judge suite PRs):** the QA run (`autoqa`) now ends with a suite: after `RESULT PASS` it writes the QA block, the rehearsal and the nine `l3-*` demos as `res-*-elevane-*.log` and a progress file `suite-<stamp>.log`. One run of the hw-support ROM is enough; **wait for the end of the suite** (`suite end` in `suite-<stamp>.log`) before powering off. The separate rehearsal / demo ROMs below are only for re-running one item (a Wood kernel gives them no SD card). `tools/qa/ingest.py` grades every file on its own.

**Run 1 on a DS Lite (2026-11-17), recorded at the end of this page:** QA run PASS, every judged item OK.

Before run 1 nothing had been measured on a console. This is the checklist for the run, so the
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

## Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256 | `elevane-hw-support.nds` v0.2.1 `15c360c5bf71fd2eeb2a4f1935b93a09ac90cf558730c62fa0fe3f375e9aeea9` (465,260 bytes, same as the release); melonDS 1.1 soft was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 32 lines, 6 checkpoints, build `538d47ce7408`, started with `start=marker`. Suite: QA block, then the rehearsal and 9 `l3-*` demos, 10 of 10 `saved=1`, no timeout; `suite end demos=10 saved=10`, all 12 probes `ok=1`, no `suite done` line (see the note). Judge: **20 of 20 judged items OK** (L1 5/5, L3 3/3 gating; the 12 informational L2 items also OK), no n/a |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None: ch1-ch5 and credits hashes equal melonDS 1.1 soft (`compare.py`, 0 differences) |
| Lag frames | 22 / 22 / 22 / 22 / 22 / 23 (ch1-ch5, credits); melonDS soft 20 / 20 / 20 / 20 / 20 / 21 |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0` everywhere; `rdl` 46 / 43 / 37 / 46 / 42 / 46 (melonDS 46 everywhere): the console's render buffer ran lower in ch2, ch3 and ch5, no underflow |

| Item | Hardware | melonDS 1.1 soft | Differs? |
|---|---|---|---|
| `hdma-holes-zero`, `acc-split-hdma-holes` (L3, melonDS#2618) | OK (no holes) | OK | no |
| `l3-acc-dual` values[4] (one pass: polygons, overflow flag; not judged) | 2310, 1 | 1536, 1 | yes: RAM_COUNT is not clamped at 2048 on the console (as in Lightenette); capture hashes values[7..9] differ |
| rehearsal values[14] (a picture hash, not judged) | differs | | yes, not judged |
| the other judged items | as melonDS soft | | no |

Note: the suite now writes `suite end` before its final reset (#186) and it is on the card, as are the 12 probe lines after the reset up to `wait60`. Only the closing `suite done` line, the last write of the run, is missing (melonDS writes it). So the reset is not what loses the line: the last line written to the card is lost. This is still open.
