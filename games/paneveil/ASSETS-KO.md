# Paneveil 에셋

[English](ASSETS.md)

Paneveil의 모든 그래픽, 폰트, 3D 모델, 효과음, 음악은 **빌드할 때 이 저장소의 코드가 만들어 냅니다.**
다른 곳의 이미지, 폰트, 녹음, 악보는 쓰지 않고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두
소스와 같은 저작권·라이선스의 오리지널 작업물입니다: **Apache-2.0**(Copyright 2026 devgyurak,
cleancart.nds 유지보수자).

생성기는 `tools/mkassets.py`이고, 그리거나 합성하고 배치하는 코드는 `tools/cvtools/`(다른 게임과 공유)에
있습니다. 같은 소스는 항상 같은 바이트를 만듭니다.

`docs/images/`의 PNG(있다면)는 빌드한 ROM에서 찍은 README 스크린샷입니다. 문서이지 에셋이 아니며,
빌드가 읽지 않고 ROM에도 들어가지 않습니다.

| 파일(출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트(ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS`(글자를 문자열로 그림) | 오리지널, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리와 게이지 타일 | `images.ui_tiles()` | 오리지널, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플(`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 오리지널, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 코드로 작곡한, 다른 게임과 공유하는 곡. 재생하는 것은 `lantern`뿐(크레딧) | `cvtools/audio.py` `SONGS`, `compose()` | 오리지널, Apache-2.0 |
| `cv_tex_paper` | 32x32 다이렉트 컬러 종이 텍스처(유리 문양이 들어올 때까지의 자리표시) | `images.textures()` `paper` | 오리지널, Apache-2.0 |
| `cv_windows` | 창 주문서(목표 설정값)와 창마다의 par. `tools/pvorder/pack.py`가 묶음 | `tools/window_check.py`가 `tools/orders/ch*.txt`(손으로 작성)를 읽음 | 오리지널, Apache-2.0 |
| `pv_field_*`, `pv_chapter_name` | 설정과 장 이름, 값 개수(글자 표) | `tools/pvorder/model.py`에서 `tools/mkassets.py`가 생성 | 오리지널, Apache-2.0 |
| `probe.bin`(NitroFS) | 카트 점검이 읽는 CRC가 알려진 2 KB 파일 | `tools/mkassets.py` | 오리지널, Apache-2.0 |
| 배너 아이콘(`icon.bmp`) | 32x32 4bpp 아이콘, 둥근 틀 안의 여섯 꽃잎 장미창 | `tools/mkassets.py` `paneveil_icon()` | 오리지널, Apache-2.0 |
