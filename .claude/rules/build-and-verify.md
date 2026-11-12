---
paths:
  - "**/Makefile"
  - "**/Dockerfile"
  - "**/build.sh"
  - "tools/emu/**"
  - "tools/melonds/**"
---

# Build and verification

- The BlocksDS image is pinned by digest in `games/<game>/Dockerfile`. Changing it
  is a deliberate toolchain upgrade: rebuild everything, re-run the demo suite,
  update the version/digest in README and notes.
- Builds must stay reproducible: `./build.sh clean && ./build.sh` twice →
  identical `shasum -a 256 <game>.nds`.
- Compiler warnings are errors in practice: fix them, don't silence them.
- ROM header: every game sets `GAME_CODE := <CODE> 00 <TITLE>` and `UNIT_CODE := 0`
  in its Makefile and passes `-g $(GAME_CODE) -uc $(UNIT_CODE)` to ndstool.
  `<TITLE>` is at most 12 uppercase characters with no spaces (`GAME_CODE` is
  split on spaces into ndstool's `-g code maker title`).
  - Never the default code `####`: melonDS treats it as homebrew and attaches no
    save chip, so nothing saves (and boot loses frames to save SPI timeouts).
  - The 4-char code must be unique in this repo and absent from melonDS's
    `src/ROMList.cpp` (entries are `{0x<code as little-endian u32>, …}`), so
    melonDS falls back to its 8 KB EEPROM instead of a listed game's chip.
  - Unit code 0 (DS only): with unit code 2 and no DSi region, melonDS blocks every
    card read of a non-homebrew ROM and NitroFS fails to mount.
- Demo names in the Makefile `DEMOS` list must match `demos[]` in `demo/demo.c`
  and a row in `docs/QA.md` and `docs/QA-KO.md`.
- The headless emulator image is x86_64 (`--platform linux/amd64`, slow on Apple
  Silicon). Run independent emulator jobs in parallel; run long ones in background.
- DeSmuME quirks: pressing a key at frame 0 freezes boot (start input after ~30
  frames); `set_savetype` before `open`. Blind spots: no MPU (no data aborts),
  write-only registers read back as written, display-sync (line-start) DMA
  skipped, polygon RAM usage/overflow registers read 0, save chip taken from
  `SAVETYPE` instead of the header.
- A DeSmuME pass is not enough: corvane passed it and melonDS QA (issue #3) still
  found 8 bugs. Changes to MPU/WRAM, DMA/IRQ, cart/NitroFS, save, header or
  write-only-register code must also run in melonDS: headless with
  `tools/melonds/mds.sh run|demos` or in the melonDS app. Runner
  output: `CPU9 cpsr` with mode (low 5 bits) 0x17 = data abort;
  `SAVE <type> <len>` with type 0 = no chip attached.
- melonDS 1.1 notes: direct boot leaves power-management register 0 at 0 (firmware
  boot sets 0x0D: sound amp + both backlights), so the ARM7 sets the bits it needs
  itself; `PM0=0d` in the headless runner mimics firmware boot. Audio output is
  48 kHz.
