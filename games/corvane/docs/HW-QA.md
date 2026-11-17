# Corvane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/corvane-hw-support.nds` on the card instead of `corvane.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Corvane (L1, graphics and system) is judged by its demos and the auto QA run. The auto QA run passed on a DS Lite on 2026-11-17 (run 1 below); the demos have not been run on a console yet. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
our expected value or level has to be re-examined (the proposal comes from `level-judge`, the change goes in a separate PR).

## What to put on the card

- ROMs: `build/release/rom/corvane.nds`, `build/release/demos/*.nds`
- The empty folder `cleancart/corvane/` (without it the ROMs write nothing) and the empty file `cleancart/corvane/autoqa` for the QA run (a folder of that name works too: the ROM only checks the name and renames it, which helps when the card is prepared on a phone)

## Run

1. Game QA: start `corvane.nds`, leave it alone until the credits (the marker file starts QA on the title).
2. Demos: start each ROM one by one; wait for `RESULT SAVED`, then reset.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game corvane`.

## Record (one block per run)

### Run 1: auto QA, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock was not set, so the log says 2002-01-15 |
| ROM SHA-256s | `corvane-hw-support.nds` v0.3.2 `b8fbbca47b49d445d076cfc2f139cbbcd43e478868ac6487ca7f8a4ccbfa587e` (the card's copy matches the release); `corvane.nds` v0.3.2 `26da3829e12bff4e633d9995b3f673070dbe8cf92171555b195f9c015317846c` was the melonDS comparison |
| `ingest.py` / `judge.py --hardware` summary | Only the QA run; no demos, so nothing to grade. `parse_log.py`: PASS, 45 lines, 5 checkpoints, build `6f32321674c1`, sd=1. Last line `RESULT PASS reason=credits lines=45 dropped=0`, started with `start=marker` |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None. `compare.py` against melonDS 1.1 soft, with the same ROM and with `corvane.nds`: 5 checkpoints, 0 differences |
| Lag frames | 60 / 182 / 213 / 181 / 200 (st1–st5); melonDS 58 / 118 / 134 / 119 / 129 |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| Checkpoint hashes st1–st5 | `5ef6d723 c2bafc29 e8c0a1c2 57d27b7b 37db96a8` | same | not run | not run | not run | no |
| Game frames per stage | 2429 / 2585 / 2309 / 2760 / 1216 | same | | | | no |
| VBlanks per stage (stage enter to result) | 2533 / 2693 / 2437 / 2871 / 1301 | 2471 / 2627 / 2369 / 2801 / 1271 | | | | yes (information) |
| Chip save writes | 6 writes, all `chip_ok=0`; SD copies `sd_match=1` | no chip (`save_detected=-1`) | | | | expected on R4 |
| `cart_ready` | 0 | 1 | | | | expected on a flash card |

Findings (information, not judged items):
- **Wood R4 gives SD only to `####` ROMs.** With the game's own code `CRVN` the kernel emulated a save and did not DLDI-patch the ROM: no marker, no log; `corvane-io-qalog` showed `NO SD CARD (RAM ONLY)`. The same ROM with `####` showed `SD LOG: OPEN`. The release ROMs now come with `<game>-hw-support.nds` (#151).
- **The console is about 2.5 % slower than melonDS.** The game logic runs the same frames, but on the console 60–70 more frames per stage take two VBlanks. melonDS runs the frame work faster than the console. Which part (CPU, 3D) is not known yet.
- **Chip save on R4:** the card answers as an EEPROM but no write verifies (`chip_ok=0`); the SD mirror holds every save. Chip save is not verified on this card.

## Follow-up

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/corvane/docs/HW-QA.md`); expected values and levels change in a separate PR.
