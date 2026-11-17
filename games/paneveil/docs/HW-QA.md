# Paneveil hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/paneveil-hw-support.nds` on the card instead of `paneveil.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Paneveil (L1, 3D geometry and texturing) is judged by the rehearsal and the auto QA run. The QA run passed on a DS Lite on 2026-11-17 (run 1 below): **31 of 31 judged items OK**, every checkpoint equal to melonDS. The steps are in the skill `nds-hw-qa`; this page is
where the result is recorded. Hardware values are never an "emulator profile": a hardware NG means
our expected value or level has to be re-examined (the proposal comes from `level-judge`, the change goes in a separate PR).

## What to put on the card

- ROM: the hw-support build of `paneveil.nds` only. The QA run judges everything: after `RESULT PASS` it writes the QA block (it carries the rehearsal's items: kiln check and light box) as `res-*-paneveil-qa.log`. Paneveil has no judged demos, so the progress file `suite-<stamp>.log` holds only the `suite block` line (no `suite end` line, by design). The rehearsal ROM is only for re-running the items without the game (a Wood kernel gives it no SD card).
- The empty folder `cleancart/paneveil/` (without it the ROMs write nothing) and the empty file `cleancart/paneveil/autoqa` for the QA run
- Check the copy: the file on the card must have the release size and SHA-256 (run 1's copy was cut short, see below).

## Run

1. Game QA: start `paneveil-hw-support.nds` and leave it alone (the marker file starts QA on the title). It plays all 30 windows, the credits, then the kiln check and the light box once (about a minute).
2. The run is over when `suite-<stamp>.log` shows `suite block block=qa saved=1`; then power off.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<card> --game paneveil`.

## What the hardware answered (run 1)

| Question | Where it shows | Run 1 |
|---|---|---|
| Do the matrix stack levels (GXSTAT 8-12, 13), pop with N = 2, store and restore, and the projection stack behave as GBATEK says? | values 3 to 11 (expected 3 0 1 0 2 0 1 0 0) | yes, `3 0 1 0 2 0 1 0 0` |
| Do CLIPMTX_RESULT and VECMTX_RESULT equal the model after load and multiply, through the packed GXFIFO and the direct ports? | values 12 to 15 (all 0) | yes, all 0 |
| Are the six vertex formats, culling, the back/front render bits, alpha, wireframe, the depth test and texture repeat, flip, clamp, colour 0, PLTT_BASE and coordinate mode 1 right in the picture? | values 16 to 30 and the flags (capture bits 8-12 set) | yes, every item OK, flags `0x1f00` |
| Is the wireframe (alpha 0) drawn at all on the hardware? (it is not in DeSmuME and melonDS GL Classic) | value 21 (`poly-wire-edges`) | yes, 4 (as melonDS soft and Compute) |
| Does the whole game run to the credits (auto QA run) and does the save chip behave? | game ROM, hidden QA mode | runs to the credits, `RESULT PASS`; chip save not verified on the R4 (`chip_ok=0`, the save is mirrored to the SD card) |

## Record (one block per run)

### Run 1: QA run, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256s | release `paneveil-hw-support.nds` v0.2.0 `1905cc7932ea4ef754fd7cc55eeccf71775a2fd25396190cf6f1d573e9e83b84` (450,924 bytes; melonDS was run with this file). The copy on the card was **cut at 393,216 bytes** (`0d9a7957a3f35ca63a15deb7575d1b5d66a06bdd9b2044b9e64c924d418ab487`); the bytes it has equal the release. The cut falls inside the last NitroFS file, `licenses/picolibc-COPYING.picolibc`: code, `probe.bin` and the other licence texts are whole, so the judged run is not affected |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 33 lines, 7 checkpoints, build `5778bba6ff09`, started with `start=marker`. Suite: `block=qa saved=1`. Judge over the QA block: **L1 31/31 OK** (target 30/30, prerequisite 1/1), no n/a |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None: ch1-ch5, judge and credits hashes equal melonDS 1.1 soft, GL Classic and GL Compute (`compare.py`, 0 differences) |
| Lag frames | 3 / 2 / 2 / 2 / 2 / 1 / 0 (ch1-ch5, judge, credits); melonDS 0 at every checkpoint |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0 rdl=46` at every 3D checkpoint, as melonDS |

| Item | Hardware | melonDS 1.1 soft | GL | Compute | DeSmuME | Differs? |
|---|---|---|---|---|---|---|
| `poly-wire-edges` | 4 | 4 | 0 (NG, profile) | 4 | NG (profile, not run) | no: the console draws the wireframe as GBATEK says |
| `poly-alpha-half` | 16 | 16 | 16 | 16 | not run | no |
| the other judged items | as melonDS soft | | | | | no |

Notes:
- The result block equals melonDS soft word for word except the QA lag total (values[53]: 12 vs 0).

## Follow-up

Report every hardware NG as a table to the user; do not edit `expected.json` here. Ask `level-judge` for the level proposal (basis `hw:games/paneveil/docs/HW-QA.md`); expected values and levels change in a separate PR.
