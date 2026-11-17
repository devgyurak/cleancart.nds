# Buoyane 에셋

[English](ASSETS.md)

Buoyane의 모든 그래픽, 폰트, 효과음, 음악은 **빌드할 때 이 저장소의 코드가 만들어 냅니다.**
다른 곳의 이미지, 폰트, 녹음, 악보는 쓰지 않고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두 소스와 같은 저작권·라이선스의
오리지널 작업물입니다: **Apache-2.0**(Copyright 2026 devgyurak, cleancart.nds 유지보수자).

생성기는 `tools/mkassets.py`이고, 그리거나 합성하고 배치하는 코드는 `tools/cvtools/`(다른 게임과 공유)에 있습니다.
같은 소스는 항상 같은 바이트를 만듭니다. 확인 장면의 텍스처와 팔레트는 `arm9/source/judge/lightcheck.c`가 코드 속 상수(8x8 텍셀)로
실행 때 쓰므로 파일이 없습니다.

| 파일(출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트(ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS` | 자체 제작, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 프레임·게이지 타일 | `images.ui_tiles()` | 자체 제작, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플(`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 자체 제작, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 곡 `slackwater`(느린 도리안 너울, 패드와 낮은 베이스, 드럼 없음), 코드가 작곡하며 크레딧에서 재생 | `cvtools/audio.py` `SONGS`, `compose()` | 자체 제작, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 점검이 읽는 CRC가 알려진 2 KB 파일 | `tools/mkassets.py` | 자체 제작, Apache-2.0 |
| 배너 아이콘(`icon.bmp`) | 32x32 4bpp 아이콘, 어두운 물 위의 램프 달린 빨강·흰색 캔 부표 | `tools/mkassets.py` `buoyane_icon()` | 자체 제작, Apache-2.0 |
