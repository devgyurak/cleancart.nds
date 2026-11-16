# Probes

The L4 research collection of cleancart.nds: demo ROMs that report what an emulator does for
behaviour nobody has confirmed on hardware (level L4, `../docs/LEVELS.md`). **It is not a game.**
Human docs (English, each with a Korean twin `*-KO.md`): `README.md`, `docs/QA.md` (the measured
table), `ASSETS.md`; plus `THIRD_PARTY_NOTICES`. Repo-wide rules are in `../AGENTS.md`.

## Exceptions to the repo-wide game rules

The probes are a collection of demo ROMs, so these rules of `../AGENTS.md` do not apply here:

- No auto QA mode, no play-through, no title, no credits screen, no `qa*`/`qalog*` code.
  Definition-of-done step 4 (QA mode) does not apply. The third-party credit that licensing rule 4
  asks for lives in `THIRD_PARTY_NOTICES` and the NitroFS `/licenses/` copy instead of an in-game
  credits screen.
- Items are all `level` 4, `role: target`, `source: unconfirmed`; they never fail a run.
  `expected.json` `known.<profile>.observed` records what each emulator gave, and
  `tools/judge.py --profile` exits 1 when a recorded value changes (a change is news, not a
  failure of the ROM). The judge enforces the role rules: every item L4, no `relocate`.
- Everything else applies: warning-free builds, reproducible ROMs, SPDX headers, every asset
  generated and listed in `ASSETS.md`, notices for linked libraries, the own ARM7 core, Korean twins.

## Build

```sh
./build.sh                    # every probe ROM: probes-l4-<name>.nds (15 of them)
./build.sh DEMO=l4-3d-fifo    # one probe ROM
./build.sh -j8 release        # build/release/{demos,judge} (tag probes-vX.Y.Z runs this in CI)
./build.sh clean
```

Header: `GAME_CODE := PRBS 00 PROBES`, unit code 0 (`../.claude/rules/build-and-verify.md`).

## Code map

The tree is a trimmed copy of `games/corvane` (the identifiers keep the `cv_`/`Cv` prefix so the
shared layers stay diffable against Corvane's).

| Path | Contents |
|---|---|
| `common/cv_ipc.h`, `arm7/source/` | own ARM7 core, as in Corvane |
| `arm9/source/core/` | sys/IPC, irq, timer, dma, cvmath, rng, input, snd, save (state only), nitro, cart; no `qa*` |
| `arm9/source/gfx/` | vram, oam, text, fx, gl3d, models, terrain |
| `arm9/source/game/` | `boss.c`, `hud.c` (the boss radar), `stagedata.c` (stage loader and actor helpers cut out of Corvane's `stage.c`), `game.h` (state the boss touches) |
| `arm9/source/demo/demo.c` | the 15 probes (`demos[]`), `main.c` runs `demo_main(CV_DEMO)` |
| `tools/mkassets.py`, `tools/cvtools/` | Corvane's asset generators (as copied; the sound bank has 20 slots, 14 with data, and no probe plays any of them) |
| `tools/judge.py`, `tools/tests/` | recorder/comparer and its unit tests |

## Contracts

- **Result block** at `0x0237F000`: `u32 'CVER', u32 FNV-1a(name), u32 frame, u32 values[13]` at frame 120,
  exactly as Corvane's demos. The ROM demo names carry the `l4-` prefix, so the ids differ from Corvane's.
- The probe code must stay equal to its source, otherwise the recorded values stop being comparable. The sources:
  `acc-dual`, `acc-split-rear` and `acc-split-hdma` are the Elevane demos `l3-acc-dual` / `l3-acc-split-*`
  (`games/elevane/arm9/source/demo/demo.c`, which hold the boot-time SwapBuffers fix); `sys-dma-*`, `2d-*`, `3d-*` and
  `acc-stage` are Corvane's demos (`games/corvane/arm9/source/demo/demo.c`, until Corvane drops them; then this
  collection holds the only copy). `l4-acc-rear-plane` has no other source: it is the `split_demo` rear-plane build
  plus a pixel count of the captured pass (variant `SPLIT_REAR_PLANE`). When a source demo changes, re-copy it and
  re-measure all profiles.
- Anything that measures a frame latency or offset must first wait for the geometry engine to be idle and call
  `sys_vsync()` once: the boot leaves a SwapBuffers pending (`glInit`), which otherwise shifts every output by a frame.
- `expected.json` `known.*.observed` is **measured, never reasoned**: after any change of a probe or an
  emulator run `judge.py --observed --partial <log>` for each of the seven profiles (melonDS 1.1 and master `4ce2188`,
  each software / GL classic / GL compute, and DeSmuME; master: `MELONDS_COMMIT=4ce2188`) and paste the output.
  melonDS#2618 is recorded only here and in Elevane's gating hole items.

## Adding a probe

Copy a `d_xxx()` from the source demo (see Contracts; helpers too), add it to `demos[]` under an `l4-` name, to `DEMOS`
in the `Makefile`, add the item(s) to `expected.json` (`level` 4, `role` `target`, `source` `unconfirmed`),
a row to `docs/QA.md` and `docs/QA-KO.md`, re-measure the seven profiles, and run
`python3 -m unittest discover -s tools/tests` and `python3 ../scripts/mklevels.py`.

## Result block on the SD card

`core/sdcard.c/.h` (identical in all five kits, with `qalog.c` using its FAT mount): when the
folder `<drive>cleancart/probes/` already exists on the card, the probe `finish()` writes its result
block there as `res-<date>-<time>-probes-<name>.log` (a `# ...` header and a `<rom> RESULT w0 w1 ...`
line, the format `tools/judge.py` reads; never overwrites an earlier run). The ROM never creates the
folder: whoever prepares the card does. Without the folder (every emulator CI run, an ordinary card)
nothing is written and nothing on screen changes. The probe then shows `RESULT SAVED` under the checkpoint. `tools/qa/ingest.py SD` copies the logs to
`build/qa/` and runs `tools/judge.py --hardware --partial` (hardware values are never compared with an
emulator profile). `sd_*` symbols stay out of `gfx/`, `judge/`, `capture.c`, `save.c` (`tools/qa/lint.py`).
