# Quayholt 에셋

[English](ASSETS.md)

Quayholt의 모든 그래픽, 글꼴, 효과음, 음악은 **빌드할 때 이 저장소의 코드가 생성**합니다. 다른 곳에서 가져온
이미지, 글꼴, 녹음, 악보는 없고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두 소스와 같은 저작권과 라이선스인
**Apache-2.0**의 자체 제작물입니다 (Copyright 2026 devgyurak, cleancart.nds 관리자).

생성기는 `tools/mkassets.py`이고, 그리거나 합성하는 코드는 `tools/cvtools/`에 있습니다(다른 게임과 공유). 같은
소스는 항상 같은 바이트를 만듭니다.

`docs/images/`의 PNG(있다면)는 빌드된 ROM에서 찍은 README 스크린샷입니다. 문서이지 에셋이 아니며, 빌드가 읽지 않고
ROM에도 들어가지 않습니다.

| 파일 (출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 글꼴 (ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS` (글리프를 문자열로 그림) | 자체 제작, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리와 게이지 타일 | `images.ui_tiles()` | 자체 제작, Apache-2.0 |
| `cv_title_lz`, `cv_title_pal` | 타이틀 그림: 해질녘 항구, 부두의 크레인 두 대(격자 지브와 가는 박스 붐), 쌓인 화물 상자, 물에 비친 해, 스텐실 글자로 쓴 이름 (256x192, 8bpp, LZ77) | `cvtools/qyart.py` `title_art()` | 자체 제작, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플 (`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 자체 제작, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 노래 `harbour`, 코드로 작곡한 느린 도리안 선율 | `cvtools/audio.py` `SONGS`, `compose()` | 자체 제작, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 검사가 읽는 CRC가 알려진 2 KB 파일 | `tools/mkassets.py` | 자체 제작, Apache-2.0 |
| 배너 아이콘 (`icon.bmp`) | 32x32 4bpp 아이콘, 크레인 갈고리 아래의 화물 상자 | `cvtools/qyart.py` `icon()` | 자체 제작, Apache-2.0 |
