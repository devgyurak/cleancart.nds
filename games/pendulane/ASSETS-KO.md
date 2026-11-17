# Pendulane 에셋

[English](ASSETS.md)

Pendulane의 모든 그래픽, 폰트, 효과음, 음악은 **빌드할 때 이 저장소의 코드로 생성**합니다. 다른 곳의 이미지, 폰트, 녹음, 악보를
가져오지 않았고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두 소스와 같은 저작권·라이선스인 **Apache-2.0**
(Copyright 2026 devgyurak, cleancart.nds maintainer)의 오리지널 작업물입니다.

생성기는 `tools/mkassets.py`이고, 그리는·합성하는·배치하는 코드는 `tools/cvtools/`에 있습니다 (다른 게임과 공유). 같은 소스는 항상 같은
바이트를 만듭니다.

`docs/images/`의 PNG(있다면)는 빌드한 ROM에서 찍은 README 스크린샷입니다. 에셋이 아니라 문서이며 빌드가 읽지 않고 ROM에도 없습니다.

| 파일 (출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트 (ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS` (글리프를 문자열로 그림) | 오리지널, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 틀과 게이지 타일 | `images.ui_tiles()` | 오리지널, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플 (`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 오리지널, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 곡 `pendulum` (차분한 도리안 곡, 드럼 없음), 코드로 작곡; 타이틀과 크레딧에서 재생 | `cvtools/audio.py` `SONGS`, `compose()` | 오리지널, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 점검이 읽는 CRC가 알려진 2 KB 파일 | `tools/mkassets.py` | 오리지널, Apache-2.0 |
| 배너 아이콘 (`icon.bmp`) | 32x32 4bpp 아이콘, 톱니바퀴 앞에서 흔들리는 진자 추 | `tools/mkassets.py` `pendulane_icon()` | 오리지널, Apache-2.0 |
