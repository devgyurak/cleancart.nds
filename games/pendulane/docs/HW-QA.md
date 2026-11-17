# Pendulane hardware QA (DS Lite + R4)

[한국어](HW-QA-KO.md)

**Wood-style R4 kernels:** put `build/release/rom/pendulane-hw-support.nds` on the card instead of `pendulane.nds`. It is the same ROM with the homebrew game code `####`; these kernels DLDI-patch only such ROMs, and with the game's own code the ROM sees no SD card (no QA log, no result files). The demo and rehearsal ROMs keep their own code.

Nothing in Pendulane has been measured on a console yet. This is the checklist for the run, so the first values can be recorded and the
`unconfirmed` items in `expected.json` can be promoted. The console is operated by a person (skill `nds-hw-qa`): create the empty folder
`cleancart/pendulane/` on the card and the rehearsal writes its result block there; `python3 tools/qa/ingest.py /Volumes/<card> --game
pendulane` grades it with `judge.py --hardware --partial`. Hardware NG items are findings, not profiles.

**Order for the first R4 run:** put the empty file `cleancart/pendulane/nocart` on the card first and check that the SD log is written; then run again without the marker to see whether the card kernel takes the cartridge bench. When grading rehearsal and QA logs together, grade each log on its own: `judge.py --partial` with both blocks grades only the last one.

## What the hardware has to answer

| Question | Where it shows |
|---|---|
| Does DIV mode 0 take 18 bus clocks, modes 1 and 2 34, SQRT 13 (GBATEK "DS Maths")? | rehearsal values 3 to 5 (10, 32, 0 half cycles; window ±2) |
| Is the firmware SPI byte 8 x (64 - 8) = 448 cycles longer at 512 kHz than at 4 MHz (dividers /8 and /64 are inferred from the nominal rates)? | rehearsal value 6 (448, window ±8); raw edges 28 to 31 |
| Is the ARM7 main RAM non-sequential 32-bit access 10 cycles (GBATEK's text) or 5 ("reportedly")? Both emulators disagree with 10. | rehearsal value 16 (576 for 10; 256 for 5) |
| Do the other ARM7 memory timings (main RAM S32 = 2, code from main RAM, VRAM S32 = 2) match? | values 15, 17, 18 |
| Does a one-shot channel's Busy clear 18 samples after the start for 16 PCM16 samples and 42 for 32 ADPCM samples (GBATEK: Busy drops at the start of the last sample)? Both emulators clear one sample later. | rehearsal values 10 and 11 |
| Do PSG, PCM16 and ADPCM start 1, 3 and 11 samples after the start write, and does a Hold restart play the old value for one sample and then two zero samples? | values 12, 13, 14 (2, 8, 258) |
| Does Busy clear at a constant rate (the slope of 32 extra samples is 0 cycles within 16)? | value 20 (informational; the value differs between boot paths on both emulators) |
| (If a card kernel does not take the cartridge bench's slot-1 command, put an empty file `cleancart/pendulane/nocart` on the card: the bench is skipped and the two cart items stay unmeasured. The item is `env: "cart"` and n/a on a console anyway.) Does +0x100 gap1 clock pulses lengthen a slot-1 B8 command by 1280 bus cycles at CLK/5 and 2048 at CLK/8? The items are `env: "cart"`: a flash card kernel cannot be asked, but record what the card reads. Does B8 leave the KEY2 state of the card alone (NitroFS reads afterwards)? | values 8, 9; raw edges 19 and 22 |
| Does the ARM9 write buffer move the edge (the fixed path is not the same on the console)? | the three differences stay in their windows; the raw edges in values 24 to 27 are information |

## Record

| Date | Console | Card | Result |
|---|---|---|---|
| — | — | — | not run yet |
