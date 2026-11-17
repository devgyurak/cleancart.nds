# Pendulane 에셋

[English](ASSETS.md)

Pendulane의 모든 그래픽, 폰트, 효과음, 음악은 **빌드할 때 이 저장소의 코드로 생성**합니다. 다른 곳의 이미지, 폰트, 녹음, 악보를
가져오지 않았고 CC0 에셋도 쓰지 않습니다. 아래 에셋은 모두 소스와 같은 저작권·라이선스인 **Apache-2.0**
(Copyright 2026 devgyurak, cleancart.nds maintainer)의 오리지널 작업물입니다.

생성기는 `tools/mkassets.py`이고, 그리는·합성하는·배치하는 코드는 `tools/cvtools/`에 있습니다 (다른 게임과 공유). 같은 소스는 항상 같은
바이트를 만듭니다.

`docs/images/`의 PNG는 빌드한 ROM에서 찍은 README 스크린샷입니다. 에셋이 아니라 문서이며 빌드가 읽지 않고 ROM에도 없습니다.

| 파일 (출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트 (ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS` (글리프를 문자열로 그림) | 오리지널, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 틀과 게이지 타일 | `images.ui_tiles()` | 오리지널, Apache-2.0 |
| `cv_smp_*`, `cv_sound_bank`, `cv_sample_names` | 엔진이 링크하는 샘플 (`cvtools/audio.py`의 `SAMPLES_USED`) | `cvtools/audio.py` `make_samples()` | 오리지널, Apache-2.0 |
| `cv_song_*`, `cv_song_names` | 곡 `pendulum` (차분한 도리안 곡, 드럼 없음), 코드로 작곡; 타이틀과 크레딧에서 재생 | `cvtools/audio.py` `SONGS`, `compose()` | 오리지널, Apache-2.0 |
| `pd_snd_adpcm` | 사운드 벤치의 ADPCM 블록: 헤더 워드(예측값 0, 스텝 인덱스 50)와 니블 7 32샘플이라 첫 샘플부터 출력이 0이 아님 | `tools/mkassets.py` | 오리지널, Apache-2.0 |
| `cv_title_tiles`, `cv_title_map`, `cv_title_pal` | 타이틀 글자 "PENDULANE": 28 x 4 타일(4bpp), 5x7 폰트를 4배로 키우고 획을 굵게 한 뒤 어두운 외곽선, 황동 띠 네 줄, 위쪽 밝은 선, 흩어진 리벳을 넣음. 빈 타일과 패널 타일 포함; 16색 팔레트 한 줄(BG 팔레트 8번 줄) | `cvtools/pdart.py` `title_lettering()` | 오리지널, Apache-2.0 |
| `pd_bob_tiles`, `pd_dot_tiles`, `pd_obj_pal` | 타이틀 진자의 스프라이트: 황동 추(32x32, 테두리, 세 띠, 광택, 고리)와 막대 점(8x8), OBJ 팔레트 한 줄 | `cvtools/pdart.py` `sprites()` | 오리지널, Apache-2.0 |
| `pd_stations`, `pd_devices`, `pd_chapter_name` | 다섯 장의 역(이름, 틱 비용 = 하드웨어 단계의 책 값), 장치 20개(이름, 목표, 슬롯, par), 장 이름 | `tools/mkassets.py`가 `tools/pdorder/model.py`(역)와 `tools/contraptions/ch*.txt`(손으로 씀, `tools/pdorder/check.py`가 풀리는지 증명)에서 만듦 | 오리지널, Apache-2.0 |
| `probe.bin` (NitroFS) | 카트 점검이 읽는 CRC가 알려진 2 KB 파일 | `tools/mkassets.py` | 오리지널, Apache-2.0 |
| 배너 아이콘 (`icon.bmp`) | 32x32 4bpp 아이콘, 톱니바퀴 앞에서 흔들리는 진자 추 | `tools/mkassets.py` `pendulane_icon()` | 오리지널, Apache-2.0 |
