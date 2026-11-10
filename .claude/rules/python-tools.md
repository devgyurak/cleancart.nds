---
paths:
  - "**/tools/**/*.py"
  - "scripts/**/*.py"
---

# Python build-time tools

- Python 3, **standard library only** (the build image adds nothing else). The
  emulator harness in `tools/emu/` is the exception: it runs in its own image
  with py-desmume and Pillow.
- Output must be a pure function of the source tree: no `random`, no time, no
  dict/set iteration that affects output order without sorting. Use
  `cvtools.gfx.Rng` (xorshift, fixed seed) for noise.
- Asset generators live in `<game>/tools/cvtools/`, are wired up in
  `mkassets.py`, and each new output gets an `ASSETS.md` row.
- Binary formats shared with C (stage files, replay logs, CVPK containers, LZ77)
  are documented in the module docstring; change C and Python together.
- LZ77 output must keep displacement ≥ 2 (VRAM-safe BIOS decompression); the
  encoder self-checks by decoding.
- Keep the generator fast (whole asset build ≈ 2 s); downsample-then-upscale
  for big noise images instead of per-pixel fBm.
