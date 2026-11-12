<div align="center">

<h1 align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo/cleancart-lockup-dark.svg">
    <img src="assets/logo/cleancart-lockup.svg" alt="cleancart.nds" width="420">
  </picture>
</h1>

**라이선스가 깨끗한 닌텐도 DS 홈브루**

[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#플레이)
[![Built with BlocksDS](https://img.shields.io/badge/built%20with-BlocksDS%20v1.25.0-blue)](https://blocksds.skylyrac.net/)
[![Languages: C · Python](https://img.shields.io/badge/languages-C%20%C2%B7%20Python-informational)](AGENTS.md)
[![Build: Docker](https://img.shields.io/badge/build-Docker-2496ED?logo=docker&logoColor=white)](#빌드)
[![Last commit](https://img.shields.io/github/last-commit/devgyurak/cleancart.nds)](https://github.com/devgyurak/cleancart.nds/commits/main)

[English](README.md)

</div>

[BlocksDS](https://blocksds.skylyrac.net/)로 만든 닌텐도 DS 오리지널 홈브루 게임 모음입니다.
라이선스를 깨끗하게 유지합니다. 코드는 직접 작성하고, 에셋은 이 저장소의 코드로
생성하며, ROM에 들어가는 서드파티 런타임 라이브러리는 모두 고지합니다.

## 목적

에뮬레이터 개발자가 개발하고 검증할 때 쓰도록 만든 홈브루 게임 모음입니다.

- **스스로 판정하는 완성된 게임.** 각 ROM은 끝까지 플레이할 수 있는 게임이면서, 하드웨어가 어떻게
  동작했는지도 보고합니다(결과 블록, `expected.json`, `judge.py`). 실기와 다른 동작은 느낌이 아니라
  실패한 항목으로 드러납니다.
- **CI에서 그대로 사용.** `judge.py`가 종료 코드를 돌려주고, 렌더러별 known 프로필이 새로 생긴
  차이와 이미 알려진 차이를 구분해 줍니다. 판정하는 모든 항목에는 L1 기능부터 L4 연구까지 난이도 레벨이
  있습니다([docs/LEVELS-KO.md](docs/LEVELS-KO.md)).
- **라이선스가 깨끗함.** ROM 안의 모든 것이 직접 작성한 코드, 이 저장소의 코드로 생성한 에셋,
  또는 고지한 서드파티 런타임 라이브러리입니다. 테스트 스위트에서 뺄 것이 없습니다.

## 게임

| 게임 | 설명과 시험하는 것 | 판정 | 레벨 | 문서 |
|---|---|---|---|---|
| [Corvane](games/corvane/)<br><img src="games/corvane/docs/images/title.png" width="192" alt="Corvane 타이틀 화면"> | 다섯 전선을 돌파하는 3D 호버 슈팅. 위 화면은 3D 전장, 아래 화면은 레이더와 터치 락온.<br>시험: 2D 엔진, 3D 엔진, ARM7 사운드, DMA·타이머·IPC·인터럽트, 카트·세이브·RTC·터치·전원 입출력. | 작성 중 | <!-- cell:corvane -->판정기 작성 중<!-- /cell --> | [README](games/corvane/README-KO.md) · [QA 체크리스트](games/corvane/docs/QA-KO.md) · [에셋](games/corvane/ASSETS-KO.md) |
| [Hushwell](games/hushwell/)<br><img src="games/hushwell/docs/images/title.png" width="192" alt="Hushwell 타이틀 화면"> | 우물 위에 지은 요양원의 1인칭 서바이벌 호러. 위 화면은 손전등 시점, 아래 화면은 손으로 그린 지도와 녹음기, 그리고 소리로 사냥하는 그것.<br>시험: 2D 엔진, 3D 엔진(조명·안개·텍스처), ARM7 사운드, DMA·타이머·IPC·인터럽트, 카트·세이브·RTC·터치·전원 입출력. | 작성 중 | <!-- cell:hushwell -->판정기 작성 중<!-- /cell --> | [README](games/hushwell/README-KO.md) · [QA 체크리스트](games/hushwell/docs/QA-KO.md) · [에셋](games/hushwell/ASSETS-KO.md) |
| [Lightenette](games/lightenette/)<br><img src="games/lightenette/docs/images/title.png" width="192" alt="Lightenette 타이틀 화면"> | 5막으로 된 등불과 마리오네트 퍼즐 액션. ROM이 자기 화면의 픽셀 수를 세어 스스로 판정합니다.<br>시험: 디스플레이 캡처, 반투명 OBJ 블렌딩, 마스터 밝기, 3D 텍스처 좌표, 에지 마킹, 선 폴리곤. | 자동(`judge.py`, `expected.json`) | <!-- cell:lightenette -->L1 11 · L2 6 · L3 3 · L4 7<!-- /cell --> | [README](games/lightenette/README-KO.md) · [QA 체크리스트](games/lightenette/docs/QA-KO.md) · [에셋](games/lightenette/ASSETS-KO.md) |
| [Elevane](games/elevane/)<br><img src="games/elevane/docs/images/title.png" width="192" alt="Elevane 타이틀 화면"> | 다섯 장(주문서 40개)의 복셀 실루엣 퍼즐. 두 화면 모두 디스플레이 캡처로 3D이며, 리허설 ROM으로 스스로 판정합니다.<br>시험: 양쪽 화면의 3D 디스플레이 캡처, HBlank DMA 합성, 뒷면(rear plane), 지오메트리·폴리곤 한계. | 자동(`judge.py`, `expected.json`) | <!-- cell:elevane -->L1 3 · L2 2 · L3 0 · L4 9<!-- /cell --> | [README](games/elevane/README-KO.md) · [QA 점검표](games/elevane/docs/QA-KO.md) · [실기 QA](games/elevane/docs/HW-QA-KO.md) · [에셋](games/elevane/ASSETS-KO.md) |

게임마다 폴더가 따로 있고, 빌드·문서·라이선스 파일도 각 폴더 안에 있습니다.

## 레벨별 테스트

판정하는 모든 항목에는 L1 기능부터 L4 연구까지 레벨이 있습니다([docs/LEVELS-KO.md](docs/LEVELS-KO.md)).
이 목록은 `scripts/mklevels.py`가 각 게임의 `expected.json`에서 생성합니다. 항목 설명은 `expected.json`의 영어 그대로입니다.

<!-- levels:begin -->
<details><summary><b>L1 기능</b> (14 개)</summary>

#### Corvane

_판정기 작성 중_

#### Elevane

- `puzzles-delivered` · Every scripted puzzle (the first of each chapter that has puzzles) is solved and delivered · [expected.json](games/elevane/expected.json)
- `chapters-played` · The rehearsal played the first puzzle of all five chapters (mask 0x1f) · [expected.json](games/elevane/expected.json)
- `puzzles-count` · Five puzzles were delivered · [expected.json](games/elevane/expected.json)

#### Hushwell

_판정기 작성 중_

#### Lightenette

- `edge-marking` · Edge marking draws edge pixels (melonDS #2549) · [expected.json](games/lightenette/expected.json)
- `darken-works` · Darken lowers the 3D backdrop outside the lantern · [expected.json](games/lightenette/expected.json)
- `act1-cleared` · Act 1 is cleared by the embedded touch script · [expected.json](games/lightenette/expected.json)
- `act2-cleared` · Act 2 is cleared by the embedded key script · [expected.json](games/lightenette/expected.json)
- `act3-cleared` · Act 3 is cleared by the embedded key script (all three marks found) · [expected.json](games/lightenette/expected.json)
- `act3-darken-works` · Act 3 darken lowers the 3D backdrop outside the lantern · [expected.json](games/lightenette/expected.json)
- `act4-cleared` · Act 4 is cleared by the embedded key script (no strings cross) · [expected.json](games/lightenette/expected.json)
- `act5-cleared` · Act 5 is cleared by the embedded key script (all dolls lit and turned) · [expected.json](games/lightenette/expected.json)
- `act5-darken-works` · Act 5 darken lowers the 3D backdrop outside the lantern · [expected.json](games/lightenette/expected.json)
- `rehearsal-finished` · The rehearsal ran to the end · [expected.json](games/lightenette/expected.json)
- `qa-verdict` · The auto QA run ended with RESULT PASS (every act cleared, credits reached, judge run) · [expected.json](games/lightenette/expected.json)

</details>

<details><summary><b>L2 경계</b> (8 개)</summary>

#### Elevane

- `poly-passes` · Showcase: each pass under 2048 polygons while the whole plaza is over it (two passes are needed) · [expected.json](games/elevane/expected.json)
- `no-overflow` · Showcase: no polygon or vertex overflow flag in either preset · [expected.json](games/elevane/expected.json)

#### Lightenette

- `lantern-blend-equal` · Translucent lantern OBJ looks the same with BLDCNT none / alpha / darken (melonDS #2503) · [expected.json](games/lightenette/expected.json)
- `act1-score` · Act 1 silhouette match at the end, 85-100 · [expected.json](games/lightenette/expected.json)
- `act2-edge-marking` · Act 2 plinths get both edge-marking colours (melonDS #2549) · [expected.json](games/lightenette/expected.json)
- `act3-lantern-equal` · Act 3 translucent lantern OBJ over the darkened 3D stage looks the same with BLDCNT none / alpha / darken (melonDS #2503) · [expected.json](games/lightenette/expected.json)
- `act5-edge-3-colours` · Act 5 plinths get all three edge-marking colours (melonDS #2549) · [expected.json](games/lightenette/expected.json)
- `act5-lantern-equal` · Act 5 translucent lantern OBJ over the darkened 3D stage looks the same with BLDCNT none / alpha / darken (melonDS #2503) · [expected.json](games/lightenette/expected.json)

</details>

<details><summary><b>L3 타이밍</b> (3 개)</summary>

#### Lightenette

- `seam-uv` · Reversed-UV cut line: rows 0 and 1 show no cut line, row 2 (the uncompensated reverse) shows one (qualitative reading of the hardware report in melonDS #2008; the exact count is not a hardware value) · [expected.json](games/lightenette/expected.json)
- `act2-seam` · Act 2 doll cut lines: rows 0 and 1 show no cut line, row 2 (the uncompensated reverse) shows one (qualitative reading of the hardware report in melonDS #2008; the exact count is not a hardware value) · [expected.json](games/lightenette/expected.json)
- `act5-seam` · Act 5 doll cut lines: rows 0 and 1 show no cut line, row 2 (the uncompensated reverse) shows one (qualitative reading of the hardware report in melonDS #2008; the exact count is not a hardware value) · [expected.json](games/lightenette/expected.json)

</details>

<details><summary><b>L4 연구</b> (16 개)</summary>

#### Elevane

- `dual-front` · Top screen: the front view reached bank D (clear-colour pixels, and no plan clear colour in it) _(참고용)_ · [expected.json](games/elevane/expected.json)
- `dual-plan` · Bottom screen: the plan view reached bank C _(참고용)_ · [expected.json](games/elevane/expected.json)
- `dual-cross-zero` · The two captures never carry each other's clear colour (plan clear in D, front clear in C) _(참고용)_ · [expected.json](games/elevane/expected.json)
- `hdma-holes-zero` · Preset 1 (HBlank DMA, the melonDS #2618 path): no transparent or black hole in the sub BG after any frame _(참고용)_ · [expected.json](games/elevane/expected.json)
- `hdma-composite` · Preset 1: the composite has pixels on both sides of x = 128 _(참고용)_ · [expected.json](games/elevane/expected.json)
- `hdma-composite-full` · Preset 1: both halves of the sampled lines are fully opaque (3072 + 3072) _(참고용)_ · [expected.json](games/elevane/expected.json)
- `hdma-composite-right-full` · Preset 1: the right half is fully opaque (3072) _(참고용)_ · [expected.json](games/elevane/expected.json)
- `rear-plane` · Preset 2 (rear plane): the captured bank holds pixels _(참고용)_ · [expected.json](games/elevane/expected.json)
- `rear-plane-full` · Preset 2: the captured pass is fully opaque (4096 samples; half means one side is missing) _(참고용)_ · [expected.json](games/elevane/expected.json)

#### Lightenette

- `seam-uv-exact` · Reversed-UV cut line: exactly 0/0/18 (18 = the card height; a measurement of melonDS software/compute and DeSmuME, not a hardware value) _(참고용)_ · [expected.json](games/lightenette/expected.json)
- `line-polygon` · Line polygons draw pixels (melonDS #2549); the count differs per emulator (705/710/834/338) _(참고용)_ · [expected.json](games/lightenette/expected.json)
- `act2-seam-exact` · Act 2 doll cut lines: exactly 0/0/18 (18 = the card height; a measurement of melonDS software/compute and DeSmuME, not a hardware value) _(참고용)_ · [expected.json](games/lightenette/expected.json)
- `act4-hline` · Act 4 horizontal line-polygon thread draws pixels (melonDS #2549); the count differs per emulator _(참고용)_ · [expected.json](games/lightenette/expected.json)
- `act4-dline` · Act 4 diagonal line-polygon thread draws pixels (melonDS #2549); the count differs per emulator _(참고용)_ · [expected.json](games/lightenette/expected.json)
- `act5-seam-exact` · Act 5 doll cut lines: exactly 0/0/18 (18 = the card height; a measurement of melonDS software/compute and DeSmuME, not a hardware value) _(참고용)_ · [expected.json](games/lightenette/expected.json)
- `act5-line-polygon` · Act 5 strings (line polygons) draw pixels (melonDS #2549); the count differs per emulator _(참고용)_ · [expected.json](games/lightenette/expected.json)

</details>
<!-- levels:end -->

## 빌드

[Docker](https://www.docker.com/)만 있으면 됩니다. 각 게임은 다이제스트로 고정한
BlocksDS 이미지 안에서 빌드되므로, 로컬 툴체인 없이 누가 빌드해도 같은 ROM이 나옵니다.

```sh
cd games/corvane     # 또는: cd games/hushwell …
./build.sh            # → corvane.nds (hushwell.nds)
./build.sh demos      # 기능별 데모 ROM (corvane-<데모>.nds)
```

세이브 칩 변형, 데모 하나만 빌드하기, 조작법은 각 게임의 README를 보세요.

## 브랜치와 릴리스

`develop`과 `main`은 풀 리퀘스트로만 바뀝니다.

```
<접두사>/<요약>  ──PR──▶  develop  ──릴리스 PR──▶  main  ──태그 <게임>-v<x.y.z>──▶  Releases
```

작업은 `develop`에서 딴 브랜치에서 하고, 제목이 커밋 접두사(`ADD:`, `FIX:` …)로 시작하는
PR로 `develop`에 머지합니다. 새 게임이나 게임의 새 버전은 `develop` → `main` PR(머지 커밋)로
내보낸 뒤 main에 태그를 붙여 릴리스합니다. PR은 메인테이너만 올립니다
([CODEOWNERS](.github/CODEOWNERS)). 대신 보낼 수 있는 것은 [CONTRIBUTING-KO.md](CONTRIBUTING-KO.md)를 보세요.

`<게임>-v<버전>` 형식의 태그를 푸시하면 GitHub Actions
([`release.yml`](.github/workflows/release.yml))가 해당 게임을 빌드해
[Releases](https://github.com/devgyurak/cleancart.nds/releases) 페이지에 ROM을 올립니다.

```sh
git tag corvane-v0.1.0 origin/main
git push origin corvane-v0.1.0
```

| 릴리스 파일 | 내용 |
|---|---|
| `corvane-v0.1.0.zip` | `corvane.nds`, 세이브 칩 변형, `SHA256SUMS`, 라이선스 원문과 고지 |
| `corvane-v0.1.0-demos.zip` | 기능 데모 ROM 전부, `SHA256SUMS`, 라이선스 원문과 고지 |
| `SHA256SUMS` | zip 파일들의 체크섬 |

버전에 접미사가 붙으면(`corvane-v0.2.0-rc.1`) 프리릴리스가 됩니다. 같은 파일이
**Actions** 탭의 실행 페이지에도 워크플로 아티팩트로 남습니다(기본 90일). 재현 가능한
빌드라서 같은 커밋을 `./build.sh release`로 빌드하면 SHA-256이 같은 ROM이 나옵니다.

## 플레이

`.nds` 파일을 [melonDS](https://melonds.kuribo64.net/)로 열면 됩니다. BIOS 파일은 필요 없습니다.
macOS에서 키가 먹지 않으면 melonDS의 키보드 매핑이 비어 있을 수 있습니다.
[AGENTS.md](AGENTS.md#controls-and-key-mapping)의 키 표와 `tools/melonds/set_keys.py`를 참고하세요.

## 저장소 구성

```
games/corvane/    게임 하나: 소스, build.sh, Dockerfile, 문서, 라이선스
games/hushwell/   게임 하나(corvane 템플릿으로 시작)
games/lightenette/ 게임 하나(corvane 템플릿으로 시작)
games/elevane/    게임 하나(lightenette에서 시작)
tools/emu/        스모크 테스트용 헤드리스 DeSmuME 하네스 (Docker)
tools/melonds/    melonDS 키 매핑·입력 확인 도구 (macOS), 헤드리스 melonDS 실행기
scripts/          유지보수 스크립트 (에이전트 설정 동기화, 로고 생성)
assets/logo/      저장소 로고 (scripts/mklogo.py가 생성, 브랜딩용이며 ROM 에셋 아님)
docs/             저장소 공통 문서 (난이도 레벨)
.github/          워크플로(릴리스, PR 검사), ruleset, PR·이슈 템플릿
AGENTS.md         코딩 에이전트용 프로젝트 규칙 (Claude Code는 CLAUDE.md로 읽음)
LICENSE           games/ 밖의 모든 것에 적용되는 Apache-2.0
CONTRIBUTING-KO.md 받는 기여의 범위 (이슈만)
```

## 라이선스

| 범위 | 라이선스 |
|---|---|
| `games/` 밖의 모든 것(도구, 스크립트, 문서, 로고, `.github/`) | Apache License 2.0, [LICENSE](LICENSE) |
| [corvane](games/corvane/LICENSE), [hushwell](games/hushwell/LICENSE), [lightenette](games/lightenette/LICENSE), [elevane](games/elevane/LICENSE) | Apache License 2.0, 각 게임 폴더의 `LICENSE` |

ROM에 들어가는 서드파티 구성 요소는 각 게임의 `THIRD_PARTY_NOTICES`에 있습니다. ROM은 커밋하지
않으니 소스에서 빌드해 주세요.

---

Nintendo와 제휴하거나 Nintendo의 승인을 받은 프로젝트가 아닙니다. Nintendo DS는 Nintendo의 상표입니다.
