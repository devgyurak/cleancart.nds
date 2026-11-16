# Tonewick hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

Nothing in Tonewick has been measured on a console yet. This is the checklist for the run, so the first values can be
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
| Do the two capture bugs GBATEK documents exist? Add overflow: 7000h + 7000h captures as -2000h; both negative: -8000h. Neither is confirmed by anyone else; if the console disagrees the two items move to `probes/` | `cap-add-wrap` (`values[21]`), `cap-both-negative` (`values[22]`) |
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
3. **Rehearsal**: start `tonewick-rehearsal.nds`, touch nothing, wait about ten seconds until the four districts read DONE.
   Use headphones or leave the volume low: the speaker amplifier is switched off during the experiments but the console is
   being asked for sound.
4. **Auto QA run**: leave an empty `cleancart/tonewick/autoqa` on the card (or hold L+R+X+Y for two seconds on the title). The
   game plays itself to the credits (about 30 seconds) and writes `cleancart/tonewick/qa-*.log`.
   Then `python3 tools/qa/ingest.py /Volumes/<card> --game tonewick` and compare with an emulator run
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
5. Optional, by hand: open the test menu (Select on the title) and the morning log; write down what it shows.

## Recording

- `docs/QA.md` and `docs/QA-KO.md` (twins): ROM SHA-256s, console and card, the failing items, the QA log header.
- `expected.json`: for every item whose hardware value matches the ROM's expectation change `source` to `hw:ds-lite` and add
  `hw:` to `level_basis`. If the hardware disagrees the item is a finding: record the value and decide whether the ROM or the
  expectation is wrong. Add a profile `hw-ds-lite` (the failing items) to `known`.
- Never commit anything under `build/qa/`.
