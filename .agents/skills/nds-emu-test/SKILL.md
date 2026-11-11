---
name: nds-emu-test
description: Smoke-test a DS ROM headless in DeSmuME (Docker), drive it with scripted keys/touch, take screenshots, read the demo result block and triage hangs or crashes from the CPU PC. Use after building to verify a scene, a stage or the demo suite; not a substitute for melonDS QA.
---

# Headless smoke test (DeSmuME)

The harness is `tools/emu/` (py-desmume in an x86_64 container). Run from the
repository root; outputs go under `out/` (not committed).

```sh
tools/emu/emu.sh run corvane/corvane.nds 900 out/boot "60:A,200:A,400:START" 150,450,899
tools/emu/emu.sh montage out/m_boot.png out/boot_00150.png out/boot_00450.png out/boot_00899.png
tools/emu/emu.sh demos corvane        # every demo ROM → "<name> RESULT 52455643 …"
```

Then **look at the montage image** (Read the PNG). Screens are 256x384 (top over bottom).

## Event script

`FRAME:KEY[:HOLD]` comma list; keys `A B X Y L R START SELECT UP DOWN LEFT RIGHT`,
touch as `FRAME:Tx/y[:HOLD]`. Do not press anything before frame ~30 (DeSmuME
freezes if a key is held at power-on). Typical corvane navigation: boot logo
skips with A (~60), intro with A (~200), title menu at ~400 (Select there
cycles the start stage), briefing needs A/START per line.

## Useful environment

- `SAVETYPE=3|4|5` EEPROM / FRAM / FLASH (the demos command sets it per ROM).
- `POKE=ADDR:VALUE` writes a u32 every 30 frames, e.g. keep lives up:
  `POKE=<cv_game+8>:9` (address from `nm`, see nds-build).

## Reading results

- `RESULT 52455643 …` = demo checkpoint reached (`'CVER'`); values follow
  (see `<game>/docs/QA.md` for what each demo reports).
- All zeros + identical screenshots = hang. Resolve the PC:
  `arm-none-eabi-addr2line -i -f -e build/<variant>/arm9.elf <PC9>` (via the build image).
  A PC inside `swiIntrWait` is just a normal VBlank wait.
- To detect a frozen game loop, watch a counter the loop increments
  (e.g. `cv_game.stage_frames`), not `cv_frame` (IRQ-driven, keeps counting).
- For deeper inspection write a small script that imports `tools/emu/run.py`
  logic and reads `e.memory.unsigned.read_long(addr)`.

## Known DeSmuME limits

Found in the corvane melonDS QA; DeSmuME passes all of these where hardware and
melonDS don't:

- no MPU: ARM9 accesses outside every MPU region (shared WRAM at 0x03000000
  in DS mode) don't abort;
- polygon RAM usage/overflow read 0 (`3d-limits`);
- write-only registers (BGxPA-PD, MOSAIC) read back what was written;
- line-start (display sync) DMA does nothing; the save chip comes from
  `SAVETYPE`, not from the header game code.

Re-check such code in headless melonDS (`tools/melonds/mds.sh`, skill
`melonds-mac`). Always say which emulator a result came from.
