# Paneveil 에셋

[English](ASSETS.md)

Paneveil의 모든 그래픽, 폰트, 3D 모델, 효과음, 음악은 **빌드할 때 이 저장소의 코드가 만들어 냅니다.**
다른 곳의 이미지, 폰트, 녹음, 악보는 쓰지 않고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두
소스와 같은 저작권·라이선스의 오리지널 작업물입니다: **Apache-2.0**(Copyright 2026 devgyurak,
cleancart.nds 유지보수자).

생성기는 `tools/mkassets.py`이고, 그리거나 합성하고 배치하는 코드는 `tools/cvtools/`(다른 게임과 공유하고, `pvart.py`는 Paneveil 전용)에
있습니다. 같은 소스는 항상 같은 바이트를 만듭니다.

`docs/images/`의 PNG(있다면)는 빌드한 ROM에서 찍은 README 스크린샷입니다. 문서이지 에셋이 아니며,
빌드가 읽지 않고 ROM에도 들어가지 않습니다.

| 파일(출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트(ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS`(글자를 문자열로 그림) | 오리지널, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리와 게이지 타일 | `images.ui_tiles()` | 오리지널, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플(`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 오리지널, Apache-2.0 |
| `cv_title_tiles`, `cv_title_map`, `cv_title_pal` | 타이틀 레터링 "PANEVEIL": 유리로 자른 24 x 4 타일(4bpp). 글자 둘레와 여덟 색 마름모 유리 조각 사이에 어두운 납틀, 빈 타일과 단색 패널 타일 포함. 16색 팔레트 한 줄(BG 팔레트 8번 줄) | `cvtools/pvart.py` `title_lettering()` | 오리지널, Apache-2.0 |
| `pv_title_preset` | 타이틀 뒤 장미창의 창 설정 12개(맑은 유리, 꽃잎 8, 링 3. 1~5장의 어떤 주문서도 요구하지 않는 값, `tools/tests/test_title.py`) | `cvtools/pvart.py` `TITLE_PRESET` | 오리지널, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 곡 `clerestory`(느린 단조 패드 곡, 드럼 없음), 코드로 작곡. 타이틀과 크레딧에서 재생 | `cvtools/audio.py` `SONGS`, `compose()` | 오리지널, Apache-2.0 |
| `cv_tex_chevron`, `cv_tex_arc`, `cv_tex_bars` | 유리 문양 세 가지. 32x32, 텍셀당 8비트(인덱스 0-7, 인덱스 0은 오려 내는 색). 기하 도형(기운 V자, 사분원 띠, 엇갈린 막대)과 납 테두리를 코드로 그림 | `tools/glassgen.py` `motif()` | 오리지널, Apache-2.0 |
| `cv_tex_lbox` | 라이트박스 텍스처. 8x8, 텍셀당 8비트, 일부러 비대칭인 인덱스 표와 인덱스 0 구멍 네 개 | `tools/glassgen.py` `LBOX` | 오리지널, Apache-2.0 |
| `cv_pal_tex` | 텍스처 팔레트 블록: 8색 세트 여덟 개(0-3번은 HSV 램프로 만든 호박·루비·바다·보라 유리, 4-7번은 라이트박스 색) | `tools/glassgen.py` `palette_block()` | 오리지널, Apache-2.0 |
| `cv_windows` | 창 주문서(목표 설정값)와 창마다의 par. `tools/pvorder/pack.py`가 묶음 | `tools/window_check.py`가 `tools/orders/ch*.txt`(손으로 작성)를 읽음 | 오리지널, Apache-2.0 |
| `pv_field_*`, `pv_chapter_name` | 설정과 장 이름, 값 개수(글자 표) | `tools/pvorder/model.py`에서 `tools/mkassets.py`가 생성 | 오리지널, Apache-2.0 |
| `probe.bin`(NitroFS) | 카트 점검이 읽는 CRC가 알려진 2 KB 파일 | `tools/mkassets.py` | 오리지널, Apache-2.0 |
| 배너 아이콘(`icon.bmp`) | 32x32 4bpp 아이콘, 둥근 틀 안의 여섯 꽃잎 장미창 | `tools/mkassets.py` `paneveil_icon()` | 오리지널, Apache-2.0 |
