# Tonewick hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/tonewick-hw-support.nds` on the card instead of `tonewick.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

**Judge suite (since the QA judge suite PRs):** the QA run (`autoqa`) now ends with a suite: after `RESULT PASS` it writes the QA block (it carries the rehearsal items) as `res-*-tonewick-*.log` and a progress file `suite-<stamp>.log`. One run of the hw-support ROM is enough; **wait for the end of the suite** (`suite end` in `suite-<stamp>.log`) before powering off. The separate rehearsal / demo ROMs below are only for re-running one item (a Wood kernel gives them no SD card). `tools/qa/ingest.py` grades every file on its own.

**Runs 1 and 2 on a DS Lite (2026-11-17), recorded at the end of this page:** QA run PASS both times. The one gating NG item (`snd-short-silent`) was a finding about our expected value; the expectation was set from the console afterwards (see "Expected values from the runs").

Before run 1 nothing had been measured on a console. This is the checklist for the run, so the first values can be
recorded and the items that only an emulator or a reading of GBATEK stands behind can be settled. The console is operated
by a person; the steps below are what to do and what to write down.

**Automatic route (skill `nds-hw-qa`):** create the empty folder `cleancart/tonewick/` on the card and the rehearsal writes its
result block there (`res-*-tonewick-rehearsal.log`); an empty file `cleancart/tonewick/autoqa` starts the QA run without a
button hold. `python3 tools/qa/ingest.py /Volumes/<card> --game tonewick` then grades everything with `judge.py --hardware
--partial`. Hardware NG items are findings, not profiles. Old `res-*` logs on the card are cleared first. The three save items
(`env: cart`) are n/a on an R4 (no save chip behind the loader).

## What the hardware has to answer

| Question | Item / where it shows |
|---|---|
| Do the two capture bugs GBATEK documents exist? Add overflow: 7000h + 7000h captures as -2000h; both negative: -8000h. Neither is confirmed by anyone else; what a disagreement leads to is in "Judging the hardware result" below | `cap-add-wrap` (`values[21]`), `cap-both-negative` (`values[22]`) |
| Does channel 0 as the capture source (bit 1) give the level of channel 0 alone? | `cap-src-channel`, `values[20]` |
| Is a capture length of 0 one word? | `cap-len-zero`, `values[18]` |
| Does a PSG wave with duty 7 stay LOW? (DeSmuME used to say HIGH) | `snd-duty7-low`, `values[2]` |
| Does the hold bit keep the last sample of a one-shot? (melonDS and DeSmuME: no) | `snd-hold-level`, `values[11]` (raw `values[13]`, `values[14]`) |
| Does a tape of 3 words keep the busy bit and stay silent? | `snd-short-busy`, `snd-short-silent`, `values[9]`, `values[10]` |
| Does the noise channel follow X = 7FFFh, XOR 6000h exactly? Is volume register 127 exactly twice register 64, and divider 3 exactly 1/16? | `snd-noise-lfsr`, `snd-vol127-full`, `snd-voldiv16` |
| Does the ADPCM loop reload index and value? | `snd-adpcm-loop-reload`, `values[15]` |
| Which console is it (firmware byte 1Dh) and do registers 4, 5, 0Ch mirror as documented? | `pm-reg-mirror`, `values[27..31]` |
| Is the AM/PM flag set for hours 12-23 in 24 hour mode? | `rtc-ampm-24h`, `values[25]`, `values[26]` |
| Is the newest firmware user-settings copy the one the loader leaves at 027FFC80h? (An R4 kernel may leave an older one or none: note the loader) | `fw-user-newest`, `values[35]`, `values[36]` |
| WEL after WREN and after a write, RDSR bits 4-6 (needs a console with a cartridge save chip; not on an R4) | `save-wel-*`, `save-status-fixed`, `values[32..34]` |

## Steps

1. Build: `cd games/tonewick && ./build.sh -j8 release`. Note `shasum -a 256` of `build/release/rom/tonewick.nds` and of
   `build/release/judge/tonewick-rehearsal.nds`.
2. Copy both ROMs to the R4 card and create `cleancart/tonewick/`.
3. **Rehearsal**: do this once in the morning and once at 12:00 or later: before noon the AM/PM flag is 0 either way and `rtc-ampm-24h` is n/a. Start `tonewick-rehearsal.nds`, touch nothing, wait about ten seconds until the four districts read DONE.
   Use headphones or leave the volume low: the speaker amplifier is switched off during the experiments but the console is
   being asked for sound.
4. **Auto QA run**: leave an empty `cleancart/tonewick/autoqa` on the card (or hold L+R+X+Y for two seconds on the title). The
   game plays itself to the credits (about 30 seconds) and writes `cleancart/tonewick/qa-*.log`.
   Then `python3 tools/qa/ingest.py /Volumes/<card> --game tonewick` and compare with an emulator run
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
5. Optional, by hand: open the test menu (Select on the title) and the morning log; write down what it shows.

## Judging the hardware result

Applies to every item, and in particular to the two capture bugs that only GBATEK documents (`cap-add-wrap`, `cap-both-negative`).
Before judging, rule out coupling: both bugs rest on volume register 127 counting as 128 (unity gain on the channel path), so check `snd-vol127-full`, `snd-voldiv16` and `cap-src-channel` first; a failure there explains the others.
Run the rehearsal at least three times (and the QA run once) and on every console you have (DS Lite and the original DS,
firmware byte 1Dh in `values[27]`).

| Outcome | What it means | What changes (file, in the PR that records the run) |
|---|---|---|
| The value is in the range every time | GBATEK and the ROM are right | `expected.json`: `source` becomes `hw:dslite-ntr` (the model) and `level_basis` gets `; hw:games/tonewick/docs/HW-QA.md` (the file that records the run); the item stays level 2 |
| The value differs from the range, but is the same every run and has an explanation (the bug as another source describes it, a register mirror, the loader) | the expectation was wrong, the item is documented | `expected.json`: range or `expect` changed to the measured value, `source` `hw:dslite-ntr`, `level_basis` `hw:games/tonewick/docs/HW-QA.md`, the explanation in `title`; the item stays level 2; the lines' model in the district file follows (`junction.c` lines 10 and 11 use the GBATEK bug as their model) |
| The value differs between runs or between consoles, or nobody can explain it | not a documented behaviour: level 4 research | **One PR does all of it** (a `role: relocate` item fails `mklevels.py --check`, so it is never committed): the item leaves `expected.json`, a demo ROM with the experiment goes into `probes/` (its `known` profiles per emulator, see `probes/AGENTS.md`), the district audit may keep the number as a raw value, lines 10 and 11 keep working (their model is the GBATEK bug and the ear's answer is shown beside it), the profiles drop the item from `ng`, and `mklevels.py` regenerates the LEVELS and README cells |

Whoever records the run (the maintainer, with a console and the QA checklist above) opens the PR; the profile `hw-ds-lite`
lists the items that fail on the console.

## Recording

- `docs/QA.md` and `docs/QA-KO.md` (twins): ROM SHA-256s, console and card, the failing items, the QA log header.
- `expected.json`: for every item whose hardware value matches the ROM's expectation change `source` to `hw:dslite-ntr` and add
  `hw:` to `level_basis`. If the hardware disagrees the item is a finding: record the value and decide whether the ROM or the
  expectation is wrong. Add a profile `hw-ds-lite` (the failing items) to `known`.
- Never commit anything under `build/qa/`.

## Run 1: QA run and judge suite, 2026-11-17

| Field | Value |
|---|---|
| Date, console, card, R4 kernel | 2026-11-17, DS Lite (NTR), "R4 Plus" dual core card, Wood R4 kernel (`__rpg/`, `_DS_MENU.DAT`). The console clock is not set, so the files say 2002-01-15 |
| ROM SHA-256 | `tonewick-hw-support.nds` v0.2.1 `a2320df3274725c46c47fb125708b207c086d3f6a7034a1284e2f822d5a16c3d` (453,996 bytes, same as the release); melonDS 1.1 soft was run with the same file |
| `ingest.py` / `judge.py --hardware` summary | QA log PASS, 21 lines, 4 checkpoints, build `91e6e87fe1c6`, started with `start=marker`. Suite: QA block only (`suite block block=qa saved=1`; no suite demos, so no `suite end` line by design). Judge: L1 9/9, L2 13/14 (**1 gating NG: `snd-short-silent`**); n/a 4: `rtc-ampm-24h` (morning, the console clock is unset) and `save-wel-set/-reset`, `save-status-fixed` (env: no chip access on the R4) |
| QA log header (`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| First checkpoint that differs from melonDS | None: dist1-dist4 hashes equal melonDS 1.1 soft (`compare.py`, 0 differences) |
| Lag frames | 39 / 78 / 73 / 54 (dist1-dist4); melonDS soft 34 / 67 / 62 / 45 |
| 3D flags (`gx=` `rdl=`) per checkpoint | `uf0,of0` everywhere; `rdl=63` on the console (no 3D), melonDS 46 |

| Item | Hardware | melonDS 1.1 soft | Differs? |
|---|---|---|---|
| `snd-short-silent` (L2, GBATEK, want 0..2) | **16384** (NG) | 0 | yes: the looped short sample is not silent on the console; the expectation was wrong, now 16320..16448, source `hw:dslite-ntr` (see below) |
| `snd-hold-level` | 100 (OK) | 0 (NG, profile) | the melonDS profile NG is a real emulator difference |
| `cap-add-wrap` / `cap-both-negative` / `cap-src-channel` | 24576 / 0 / 40960 (OK) | 65535 / 16384 / 49152 (NG, profile) | as above |
| `pm-reg-mirror` | 1 (OK) | 0 (NG, profile) | as above |
| `snd-duty3-edges` | 128 | 127 | both OK |
| `rtc-ampm-24h` | n/a (values[19] = 0) | OK 8/8 | needs an afternoon clock; OK 8/8 in run 2 |
| the other judged items | as melonDS soft | | no |

The NG item was left alone in the PR that recorded run 1; it is dealt with in "Expected values from the runs" below.

## Run 2: QA run and judge suite, 2026-11-17 (console clock 2026-01-15 12:07)

The same console, card and ROM as run 1 (`tonewick-hw-support.nds`, build `91e6e87fe1c6`), run again after noon by the console clock (2026-01-15, 12:07; the files say `res-20260115-*`, run 1 `res-20020115-*`). This is the afternoon run the checklist asks for.

| Field | Value |
|---|---|
| `judge.py --hardware` | QA block: the same verdicts as run 1, plus `rtc-ampm-24h` graded OK (`values[19]` = 8, all 8 raw reads consistent); `snd-short-silent` 16384 again (NG before the expected values were changed). n/a 3 (env: `save-wel-set`, `save-wel-reset`, `save-status-fixed`; no chip access on the R4) |
| Difference from run 1 | only `values[19]` (0 -> 8). Every other word of the block is equal, including the 16384 |

## Expected values from the runs

| Item | Change | Why |
|---|---|---|
| `snd-short-silent` | range `16320..16448`, source `hw:dslite-ntr`; title: a looped 3-word sample keeps the tape level on the console (GBATEK's "no sound" is not a zero level) | both runs read 16384 (the captured peak) where GBATEK's "no sound" suggested at most 2; The value is the same in both runs. melonDS reads 0, DeSmuME 16384 |
| `cap-add-wrap`, `cap-both-negative`, `cap-src-channel`, `snd-hold-level`, `pm-reg-mirror`, `rtc-ampm-24h` | `level_basis` gets `hw:games/tonewick/docs/HW-QA.md` | the console agrees with GBATEK (the two capture bugs exist); `rtc-ampm-24h` is confirmed by run 2 (afternoon) |
| Profiles | `snd-short-silent` is in `ng` of the three melonDS profiles and `-save-chip`; DeSmuME drops it (it reads 16384 now) | melonDS keeps the looped 3-word sample silent |
