---
name: nds-hw-qa
description: Run the auto QA mode on a real DS Lite + R4 (slot-1 flashcart, SD log), collect the log, ingest and compare it with the emulator run, and record the result in docs/QA(-KO).md. Use when the user has the console and a card ready; tell the user the steps in Korean.
---

# Hardware QA run (DS Lite + R4)

Tell the user each step in **Korean**, one at a time, short.

1. **Build** the release ROM with `<game>/build.sh` (and for corvane the SAVE variant the
   user wants). Note the SHA-256 (`shasum -a 256`). The ROM that runs on the console is
   the release ROM.
2. **Copy**: ROM to the R4 SD card as usual. Make sure the card has the R4 kernel's DLDI
   patch applied by the kernel (nothing to do for R4). Leave room for `cleancart/<game>/`.
3. **Run**: power on, start the game, wait for the title screen, hold **L+R+X+Y for about
   two seconds** (lightenette: press L and R first, then X and Y: with L or R held the title
   ignores X, which otherwise opens the credits). Nothing is drawn. The game now plays itself to the credits; the user only
   watches (corvane ~4 min, longer for others). The run ends on the records screen (corvane),
   or as the game's QA notes say.
4. **Retrieve**: power off, put the SD card in the Mac, mount it.
5. **Ingest**: `python3 tools/qa/ingest.py /Volumes/<card> --game <game>` (copies the log and
   save mirrors to `<game>/build/qa/`, parses them). Exit 0 = `RESULT PASS`.
6. **Compare** with a headless melonDS run of the same ROM (`SDDIR=…`):
   `python3 tools/qa/compare.py <emu.log> <hw.log> --info`. Differences are informational,
   not a gate, but they are findings: say which checkpoint diverges first.
7. **Record** in `<game>/docs/QA.md` and `QA-KO.md` (twins): ROM SHA-256, console/card,
   result, header values (`dldi`, `nitro`, `save_detected`, `cart_ready`), checkpoints, lag,
   what differed from the emulators. Saves: report the chip detection as logged; with no
   chip say "chip save not verified on R4". For games with a judge, the real-hardware values
   are the reference for promoting informational items to gating ones.

Things that can go wrong: no `cleancart/` folder (FAT did not mount: header `sd=0 fat_fail=1`
or `errno=`), `nitro=0` (NitroFS failed on the cart: not fatal), log stops mid-run (power
cut: the last flush is what you get), ring overflow (`dropped=` in the RESULT line).
Never commit anything under `build/qa/`.
