# Lightenette 에셋

[English](ASSETS.md)

Lightenette의 모든 그래픽, 폰트, 3D 모델, 효과음, 음악은 **빌드 시 이 저장소의 코드가
생성**합니다. 외부 이미지·폰트·녹음·악보는 쓰지 않고 CC0 에셋도 쓰지 않습니다. 아래
에셋은 모두 소스와 같은 저작권·라이선스(**Apache-2.0**, Copyright 2026 Corvane
contributors)의 자체 제작물입니다.

생성기는 `tools/mkassets.py`이고, 그리거나 합성하는 코드는 `tools/cvtools/`(corvane과
공유)에 있습니다. 같은 소스는 항상 같은 바이트를 만듭니다.

`docs/images/`의 PNG(있다면)는 빌드한 ROM에서 찍은 README 스크린샷입니다. 문서이지
에셋이 아니므로 빌드가 읽지 않고 ROM에도 들어가지 않습니다.

| 파일(출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트(ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS`(글리프를 문자열로 그림) | 자체 제작, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리·게이지 타일 | `images.ui_tiles()` | 자체 제작, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 게임이 쓰는 샘플 7개: `bass`, `pad`, `kick`, `snare`, `hat`(곡의 악기, 공용 엔진 샘플은 corvane에서 옴)과 Lightenette 고유의 `chime`, `click`. 나머지 corvane 샘플은 같은 묶음으로 생성하지만 ROM에는 링크하지 않습니다(`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 자체 제작, Apache-2.0 |
| `cv_song_lantern_*`, `cv_song_puppets_*`, `cv_song_names` | 곡 두 개: `lantern`(타이틀), `puppets`(크레딧). 코드로 작곡 | `cvtools/audio.py` `SONGS`, `compose()` | 자체 제작, Apache-2.0 |
| `cv_title_lz`, `cv_title_pal` | 256x192 8bpp 타이틀 그림(등불 빛무리, 조종대에 매달린 마리오네트, 게임 폰트로 만든 워드마크), LZ77, 팔레트 96–255번 | `cvtools/ltart.py` `title_art()` | 자체 제작, Apache-2.0 |
| (런타임) 3막 | 새 에셋 없음: 무대와 표식은 텍스처 없는 3D 폴리곤이고, 등불은 판정의 64x64 글로우를 `judge_lamp_load()`로 불러 씀 | `arm9/source/game/act3.c` | 자체 제작, Apache-2.0 |
| (런타임) 4·5막 | 2막의 인형 카드를 5막에서 런타임에 다시 그리는 것(`make_card()`, 같은 픽셀) 외에는 새것 없음. 4막은 텍스처 없는 라인 폴리곤과 사각형, 5막은 `judge_lamp_load()`로 판정의 등불 글로우를 재사용 | `arm9/source/game/act4.c`, `act5.c` | 자체 제작, Apache-2.0 |
| (런타임) 2막 인형 카드 텍스처 | 재단선이 있는 64x16 다이렉트 컬러 종이 인형 카드(판정 카드와 같은 구성) | 실행 시 `arm9/source/game/act2.c`(`make_card`)가 그림 | 자체 제작, Apache-2.0 |
| (런타임) 판정 장면의 카드 텍스처와 등불 스프라이트 | 재단선이 있는 64x16 다이렉트 컬러 종이 카드, 64x64 4bpp 방사형 빛 | 실행 시 `arm9/source/judge/judge.c`(`acc_make_card`, `acc_make_lamp`)가 그림 | 자체 제작, Apache-2.0 |
| `cv_tex_paper` | 마리오네트 종이 인형용 32x32 다이렉트 컬러 종이(크림색, 섬유, 어두운 테두리). ROM에 들어가는 유일한 텍스처입니다(corvane의 다른 텍스처, 스프라이트 시트, 툰 테이블은 링크하지 않음) | `images.textures()`의 `paper` | 자체 제작, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 점검이 읽는 CRC를 아는 2 KB 파일 | `tools/mkassets.py` | 자체 제작, Apache-2.0 |
| 배너 아이콘(`icon.bmp`) | 32x32 4bpp 아이콘 | `images.icon()` | 자체 제작, Apache-2.0 |
