# Elevane 에셋

[English](ASSETS.md)

Elevane의 모든 그래픽, 글꼴, 3D 모델, 효과음, 음악은 **빌드 시점에 이 저장소의 코드가
생성**합니다. 다른 곳의 이미지, 글꼴, 녹음, 악보는 쓰지 않으며 CC0 에셋도 쓰지 않습니다.
아래 에셋은 모두 소스와 같은 저작권과 라이선스의 독자 제작물입니다:
**Apache-2.0** (Copyright 2026 Corvane contributors).

생성기는 `tools/mkassets.py`이고, 그리거나 합성하는 코드는 `tools/cvtools/`에 있습니다
(corvane, lightenette와 공유). 같은 소스는 항상 같은 바이트를 만듭니다.

`docs/images/`의 PNG(있다면)는 빌드한 ROM에서 찍은 README 스크린샷입니다. 에셋이 아니라
문서이며, 빌드가 읽지 않고 ROM에도 들어가지 않습니다.

퍼즐 40개(1~5장)가 모두 `tools/puzzles/ch*.txt`에 들어 있습니다.

| 파일 (출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 글꼴 (ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS` (글리프를 문자열로 그림) | 자체 제작, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리와 게이지 타일 | `images.ui_tiles()` | 자체 제작, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플 (`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 자체 제작, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 곡 두 개 `lantern`, `puppets`, 코드로 작곡해 lightenette와 공유. `lantern`만 재생(크레딧)하고 `puppets`는 링크만 되고 쓰이지 않음 | `cvtools/audio.py` `SONGS`, `compose()` | 자체 제작, Apache-2.0 |
| `cv_tex_paper` | 32x32 다이렉트 컬러 종이 질감, lightenette와 공유. ROM에 링크되는 유일한 텍스처이며 Elevane 장면에서는 쓰이지 않음(도면은 단색 폴리곤) | `images.textures()` `paper` | 자체 제작, Apache-2.0 |
| `cv_puzzles` | 퍼즐 주문서(정면도·평면도), 기준 블록 수, 퍼즐마다 최소 해 하나. `tools/evpuzzle/pack.py`가 묶음 | `tools/puzzles/ch*.txt`(제작자 구조물을 높이맵이나 층으로 손으로 씀)를 `tools/puzzle_check.py`가 읽음 | 자체 제작, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 검사가 읽는 CRC 고정 2 KB 파일 | `tools/mkassets.py` | 자체 제작, Apache-2.0 |
| 배너 아이콘 (`icon.bmp`) | 32x32 4bpp 아이콘, 세 계단짜리 블록 | `tools/mkassets.py` `elevane_icon()` | 자체 제작, Apache-2.0 |
