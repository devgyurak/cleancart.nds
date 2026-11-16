# Heddlemere 에셋

[English](ASSETS.md)

Heddlemere의 모든 그래픽, 폰트, 3D 모델, 효과음, 음악은 **빌드할 때 이 저장소의 코드가 만들어 냅니다.**
다른 곳의 이미지, 폰트, 녹음, 악보는 쓰지 않고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두
소스와 같은 저작권·라이선스의 오리지널 작업물입니다: **Apache-2.0**(Copyright 2026 devgyurak,
cleancart.nds 유지보수자).

생성기는 `tools/mkassets.py`이고, 그리거나 합성하고 배치하는 코드는 `tools/cvtools/`(다른 게임과 공유하고, `hmart.py`는 Heddlemere 전용)에
있습니다. 같은 소스는 항상 같은 바이트를 만듭니다.

`docs/images/`의 PNG(있다면)는 빌드한 ROM에서 찍은 README 스크린샷입니다. 문서이지 에셋이 아니며,
빌드가 읽지 않고 ROM에도 들어가지 않습니다.

| 파일(출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트(ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS`(글자를 문자열로 그림) | 오리지널, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리와 게이지 타일 | `images.ui_tiles()` | 오리지널, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플(`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 오리지널, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 곡 `clerestory`(느린 단조 패드 곡, 드럼 없음), 코드로 작곡. 타이틀과 크레딧에서 재생 | `cvtools/audio.py` `SONGS`, `compose()` | 오리지널, Apache-2.0 |
| `cv_title_tiles`, `cv_title_map`, `cv_title_pal` | 타이틀 레터링 "HEDDLEMERE": 30 x 4 타일(4bpp). 글자 둘레와 여덟 실 색 마름모 조각 사이에 어두운 날실 선, 빈 타일과 단색 패널 타일 포함. 16색 팔레트 한 줄(BG 팔레트 8번 줄). 뼈대용 임시(`TODO(title)`), 베틀 그림으로 교체 예정 | `cvtools/hmart.py` `title_lettering()` | 오리지널, Apache-2.0 |
| `cv_orders` | 베틀 주문 40개(줄마다 목표 색·짜임·염색, 예산, 패스 수, 여유)와 par. `tools/hmorder/pack.py`가 묶음 | `tools/order_check.py`가 `tools/orders/ch*.txt`(직접 쓴 텍스트, 탐색으로 검사)를 읽음 | 오리지널, Apache-2.0 |
| `hm_col_*`, `hm_chapter_name`, `hm_active_cols`, `hm_program_ticks`, `hm_dye_line` | 열·값·장 이름, 장마다 돌리는 열, 잉아 프로그램 네 가지의 GX 사이클 비용(GBATEK 수치), 염색 라인 | `tools/hmorder/model.py`를 읽는 `tools/mkassets.py` | 오리지널, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 점검이 읽는 CRC 고정 2 KB 파일 | `tools/mkassets.py` | 오리지널, Apache-2.0 |
| 배너 아이콘 (`icon.bmp`) | 32x32 4bpp 아이콘, 테두리 안의 직조 천: 어두운 날실 위의 씨실 네 색 | `tools/mkassets.py` `heddlemere_icon()` | 오리지널, Apache-2.0 |
