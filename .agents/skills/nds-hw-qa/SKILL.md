---
name: nds-hw-qa
description: Run the judged demos, the rehearsal and the auto QA mode on a real DS Lite + R4 (slot-1 flashcart, SD card), collect the logs, grade them with judge.py --hardware, compare with the emulators and record the result in docs/HW-QA(-KO).md. Use when the user has the console and a card ready; tell the user the steps in Korean.
---

# Hardware QA run (DS Lite + R4)

The Mac side (build, card preparation, ingest, judging, comparing, recording) is done by you, the
agent, step by step; it is not a script on purpose. The user only handles the card and the console.
Tell the user each step in **Korean**, one at a time, short.

What the ROMs do by themselves (`core/sdcard.c`, `core/qa.c`):
- **Result files:** a demo, rehearsal or probe ROM writes its result block to
  `cleancart/<kit>/res-<date>-<time>-<rom>.log` as a `RESULT w0 w1 ...` line **only if the folder
  `cleancart/<kit>/` already exists**. Demos and probes then print `RESULT SAVED` under the
  checkpoint (the rehearsals print nothing). No folder: nothing is written.
- **QA start:** an empty file `cleancart/<game>/autoqa` starts the QA run on the first title screen
  (renamed to `autoqa.used-*` first, so it runs once). No button hold. The key combo
  (L+R+X+Y for 120 frames) still works.

## 1. Prepare the card (you, on the Mac)

1. Build the release ROMs: `games/<game>/build.sh -j8 release` (probes: `probes/build.sh -j8 release`).
   Note `shasum -a 256` of every ROM that goes on the card.
2. Copy to the R4 card: the game ROM, `build/release/demos/*.nds` and the rehearsal/judge ROM of the
   game (`build/release/judge/`); for probes `build/release/demos/*.nds`.
3. Create the **empty folder** `cleancart/<kit>/` for every kit that will run (this is what turns on
   the result files). For the games you want QA-run, create the empty file `cleancart/<game>/autoqa`.
4. Tell the user: put the card in, power on.

## 2. Run (the user, on the console)

- **Game QA:** start the game ROM and leave it alone. It starts QA at the title and plays itself to the
  credits (Corvane about 4 minutes, the others longer). Nothing is drawn for it. Then power off.
- **Demos, probes:** start each ROM from the R4 menu one after another. When `RESULT SAVED` shows,
  that ROM is done; reset to the menu and start the next one. A ROM that never shows it either did not
  reach its checkpoint or has no folder: say which ROM.
- **Rehearsal (Lightenette, Elevane):** start it, touch nothing; the result screen appears after about
  a minute. The file is written silently when that screen shows.
- Items that need the test harness' input (touch, keys) have `"env"` and are n/a on hardware; if a demo
  needs a human to press something, the game's `docs/HW-QA.md` says so.

Unknown until the first run: whether the R4 kernel auto-starts the last ROM or can chain ROMs. If the
user has to pick every ROM by hand, say how long the whole card takes.

## 3. Collect and grade (you)

1. User powers off, puts the card in the Mac. `python3 tools/qa/ingest.py /Volumes/<card>` (one kit:
   `--game <kit>`; probes are `--game probes`). It copies `qa-*.log`, `res-*.log` and `save-*.bin` to
   `games/<kit>/build/qa/` (probes: `probes/build/qa/`), parses the QA logs and runs the kit's
   `tools/judge.py --hardware --partial` on the result files. Hardware values are never compared with an
   emulator profile.
2. Compare with the emulators: the same ROMs in headless melonDS (`tools/melonds/mds.sh demos <folder>`,
   `RENDERER=soft|gl|compute`) judged with `--profile`, and for QA
   `python3 tools/qa/compare.py <emu.log> <hw.log> --info`. Make a table per item: hardware value,
   each profile's value, which differ.
3. First-run checks (card): QA log header `sd=1 fat_fail=0`, `dldi`, `save_detected` (R4 usually -1: write
   "chip save not verified on R4"), `cart_ready`, `nitro`. Four games: does `RESULT PASS` come, and where
   does the first checkpoint differ from melonDS? Values that matter most: Elevane `hdma-holes-zero` and
   `acc-split-hdma-holes` (melonDS#2618, L3), Lightenette `snd-wave-pan-right-exact` (GBATEK rounding 2
   vs melonDS 3), every probe (FIFO capture `sys-dma-disp-pixels`, `2d-display-pixels`, `rear-plane*`,
   `2d-obj-line-budget`, alpha-15 bitmap OBJ).

## 4. Record and follow up (you)

1. Write `games/<kit>/docs/HW-QA.md` and `HW-QA-KO.md` (twins; the templates are in place): date,
   console, card, ROM SHA-256s, the judge summary, a per-item table (hardware / emulators), QA header values,
   lag, the first differing checkpoint.
2. **Games:** a hardware NG is not an "emulator profile". It is a finding: our expected value or level
   has to be re-examined. Do not edit `expected.json` in the recording change. Report the table to the user,
   ask the `level-judge` agent for the level proposal (`hw:games/<game>/docs/HW-QA.md` as the basis), and
   change expected values or levels in a separate PR after the user agrees.
3. **Probes:** `python3 probes/tools/judge.py --hardware --partial --observed <res logs>` prints the
   observed values; the proposal is to store them as `known.hardware-dslite-r4.observed` in
   `probes/expected.json`. Do not write it automatically: show the user, get a yes, then make the PR.
   `level-judge` lists the items that can move down to L2/L3 with the `hw:` evidence.
4. Never commit anything under `build/qa/`.

Things that can go wrong: no `cleancart/` folder (FAT did not mount: header `sd=0 fat_fail=1` or `errno=`),
the kit folder missing (no `res-*.log`, no `RESULT SAVED`), `nitro=0` (NitroFS failed on the cart: not
fatal), a log that stops mid-run (power cut: the last flush is what you get), ring overflow (`dropped=` in
the RESULT line), `autoqa-error.log` (the marker could not be renamed: nothing started).
