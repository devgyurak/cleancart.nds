# cleancart.nds

[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#play)
[![Built with BlocksDS](https://img.shields.io/badge/built%20with-BlocksDS%20v1.25.0-blue)](https://blocksds.skylyrac.net/)
[![Languages: C · Python](https://img.shields.io/badge/languages-C%20%C2%B7%20Python-informational)](AGENTS.md)
[![Build: Docker](https://img.shields.io/badge/build-Docker-2496ED?logo=docker&logoColor=white)](#build)
[![Last commit](https://img.shields.io/github/last-commit/devgyurak/cleancart.nds)](https://github.com/devgyurak/cleancart.nds/commits/main)

[한국어](README-KO.md)

Original Nintendo DS homebrew games built with [BlocksDS](https://blocksds.skylyrac.net/),
kept licence-clean: our own code, assets generated from code in this repository,
and clearly noticed third-party runtime libraries.

## Games

| Game | Description | Docs |
|---|---|---|
| [Corvane](corvane/)<br><img src="corvane/docs/images/title.png" width="192" alt="Corvane title screen"> | 3D hover shooter across five fronts: 3D battlefield on the top screen, radar and touch lock-on on the bottom screen. | [README](corvane/README.md) · [QA checklist](corvane/docs/QA.md) · [Assets](corvane/ASSETS.md) |

Each game lives in its own folder with its own build, docs and licence files.

## Build

Requires [Docker](https://www.docker.com/). Every game builds inside a BlocksDS
image pinned by digest, so builds are reproducible and need no local toolchain.

```sh
cd corvane
./build.sh            # → corvane.nds
./build.sh demos      # single-feature demo ROMs (corvane-<demo>.nds)
```

See the game's README for build options (save-chip variants, single demos) and controls.

## Releases

Pushing a tag named `<game>-v<version>` builds that game on GitHub Actions
([`release.yml`](.github/workflows/release.yml)) and publishes the ROMs on the
[Releases](https://github.com/devgyurak/cleancart.nds/releases) page:

```sh
git tag corvane-v0.1.0
git push origin corvane-v0.1.0
```

| Release file | Contents |
|---|---|
| `corvane-v0.1.0.zip` | `corvane.nds`, save-chip variants, `SHA256SUMS`, licence texts and notices |
| `corvane-v0.1.0-demos.zip` | every feature demo ROM, `SHA256SUMS`, licence texts and notices |
| `SHA256SUMS` | checksums of the zips |

A version with a suffix (`corvane-v0.2.0-rc.1`) becomes a pre-release. The same files are
also kept as workflow artifacts on the run's page under **Actions** (90 days by default).
The build is reproducible, so `./build.sh release` of the same commit gives ROMs with the
same SHA-256.

## Play

Open the `.nds` file in [melonDS](https://melonds.kuribo64.net/). No BIOS files are needed.
On macOS, if keys do nothing, the keyboard mapping in melonDS may be empty; see the
key table in [AGENTS.md](AGENTS.md#controls-and-key-mapping) and `tools/melonds/set_keys.py`.

## Repository layout

```
corvane/          a game: sources, build.sh, Dockerfile, docs, licences
tools/emu/        headless DeSmuME harness for smoke tests (Docker)
tools/melonds/    melonDS key-mapping and input-check helpers (macOS)
scripts/          maintenance scripts (agent config sync)
.github/          release workflow (tag <game>-v<version> → GitHub Release)
AGENTS.md         project conventions for coding agents (Claude Code reads it via CLAUDE.md)
```

## Licence

Each game states its licence in its folder. Corvane is licensed under the
[Apache License 2.0](corvane/LICENSE); third-party components in its ROM are listed
in [THIRD_PARTY_NOTICES](corvane/THIRD_PARTY_NOTICES). ROMs are not committed;
build them from source.
