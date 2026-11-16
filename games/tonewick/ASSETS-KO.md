# Tonewick 에셋

[English](ASSETS.md)

Tonewick의 그래픽, 폰트, 효과음, 음악은 모두 **이 저장소의 코드가 빌드할 때 생성**합니다. 다른 곳에서 가져온 이미지,
폰트, 녹음, 악보는 없고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두 소스와 같은 저작권·라이선스의 오리지널 작업물입니다:
**Apache-2.0** (Copyright 2026 devgyurak, the cleancart.nds maintainer).

생성기는 `tools/mkassets.py`이고, 그리거나 합성하고 배치하는 코드는 `tools/cvtools/`에 있습니다(다른 게임과 공유). 같은
소스는 언제나 같은 바이트를 만듭니다.

`docs/images/`의 PNG 파일(있다면)은 빌드한 ROM에서 찍은 README 스크린샷입니다. 에셋이 아니라 문서이므로 빌드가
읽지 않고 ROM에도 들어가지 않습니다.

| 파일 (출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트 (ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS` (글자를 문자열로 그림) | 자체 제작, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리와 게이지 타일 | `images.ui_tiles()` | 자체 제작, Apache-2.0 |
| `cv_title_lz`, `cv_title_pal` | 타이틀 그림 (256x192, 8bpp, LZ77). **뼈대용 임시 그림**(`TODO(title)`): 폴더를 만든 출발점의 탑과 관 그림이 그대로이며, 릴리스 전에 황야와 신호탑 그림으로 바꿉니다 | `cvtools/twart.py` `title_art()` | 자체 제작, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플 (`cvtools/audio.py`의 `SAMPLES_USED`. `stamp`는 줄을 푼 도장 소리로, 노이즈 꼬리가 달린 나무 둔탁음) | `cvtools/audio.py` `make_samples()` | 자체 제작, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 곡 `minuet`: 코드로 작곡한 느린 도리안 미뉴에트 | `cvtools/audio.py` `SONGS`, `compose()` | 자체 제작, Apache-2.0 |
| `cv_adpcm_ramp` | 에코실이 사운드 실험실에서 반복하는 IMA-ADPCM 톱니파 (샘플 256개, 헤더 1워드와 데이터 32워드) | `tools/mkassets.py`(톱니파)와 `cvtools/codec.py` `ima_adpcm()` | 자체 제작, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 점검이 읽는 CRC 알려진 2 KB 파일 | `tools/mkassets.py` | 자체 제작, Apache-2.0 |
| 배너 아이콘 (`icon.bmp`) | 32x32 4bpp 아이콘, 놋쇠 잠금쇠가 달린 덮인 장부 (타이틀처럼 뼈대용 임시) | `cvtools/twart.py` `icon()` | 자체 제작, Apache-2.0 |
