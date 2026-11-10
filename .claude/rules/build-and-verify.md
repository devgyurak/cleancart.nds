---
paths:
  - "**/Makefile"
  - "**/Dockerfile"
  - "**/build.sh"
  - "tools/emu/**"
---

# Build and verification

- The BlocksDS image is pinned by digest in `<game>/Dockerfile`. Changing it
  is a deliberate toolchain upgrade: rebuild everything, re-run the demo suite,
  update the version/digest in README and notes.
- Builds must stay reproducible: `./build.sh clean && ./build.sh` twice →
  identical `shasum -a 256 <game>.nds`.
- Compiler warnings are errors in practice: fix them, don't silence them.
- Demo names in the Makefile `DEMOS` list must match `demos[]` in `demo/demo.c`
  and a row in `docs/QA.md` and `docs/QA-KO.md`.
- The headless emulator image is x86_64 (`--platform linux/amd64`, slow on Apple
  Silicon). Run independent emulator jobs in parallel; run long ones in background.
- DeSmuME quirks: pressing a key at frame 0 freezes boot (start input after ~30
  frames); polygon RAM usage/overflow registers read 0; `set_savetype` before `open`.
