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
./build.sh                    # every probe ROM: probes-l4-<name>.nds (14 of them)
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
| `arm9/source/demo/demo.c` | the 14 probes (`demos[]`), `main.c` runs `demo_main(CV_DEMO)` |
| `tools/mkassets.py`, `tools/cvtools/` | the asset generators Corvane's demos use (trimmed `mkassets.py`) |
| `tools/judge.py`, `tools/tests/` | recorder/comparer and its unit tests |

## Contracts

- **Result block** at `0x0237F000`: `u32 'CVER', u32 FNV-1a(name), u32 frame, u32 values[13]` at frame 120,
  exactly as Corvane's demos. The ROM demo names carry the `l4-` prefix, so the ids differ from Corvane's.
- The probe code must stay equal to Corvane's `demo/demo.c` bodies, otherwise the recorded values stop being
  comparable. When Corvane changes one of these demos, re-copy it and re-measure all profiles.
- `expected.json` `known.*.observed` is **measured, never reasoned**: after any change of a probe or an
  emulator run `judge.py --observed --partial <log>` for each of the four profiles and paste the output.

## Adding a probe

Copy a `d_xxx()` from Corvane's `demo.c` (and its helpers), add it to `demos[]` under an `l4-` name, to `DEMOS`
in the `Makefile`, add the item(s) to `expected.json` (`level` 4, `role` `target`, `source` `unconfirmed`),
a row to `docs/QA.md` and `docs/QA-KO.md`, re-measure the four profiles, and run
`python3 -m unittest discover -s tools/tests` and `python3 ../scripts/mklevels.py`.
