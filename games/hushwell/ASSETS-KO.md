# Hushwell 에셋

[English](ASSETS.md)

Hushwell의 그래픽, 폰트, 3D 모델, 효과음, 목소리, 음악, 맵은 모두 **이 저장소의 코드가
빌드할 때 생성**합니다. 다른 곳에서 가져온 이미지, 폰트, 녹음, 악보는 없고 CC0 에셋도
쓰지 않습니다. 아래 에셋은 모두 소스와 같은 저작권·라이선스를 따르는 창작물입니다:
**Apache-2.0** (Copyright 2026 devgyurak, the cleancart.nds maintainer).

생성기는 `tools/mkassets.py`이고, 그리고·합성하고·배치하는 코드는 `tools/cvtools/`에
있습니다. 같은 소스는 언제나 같은 바이트를 만듭니다(난수는 고정 시드 xorshift, 파이썬
`random`은 쓰지 않음). 무선과 테이프 속 "목소리"는 포먼트 합성이며 누군가를 녹음한 것이
아닙니다.

`docs/images/`의 PNG는 빌드한 ROM을 헤드리스 DeSmuME로 찍은 README 스크린샷입니다.
문서일 뿐 에셋이 아닙니다. 빌드는 이 파일을 읽지 않고 ROM에도 들어가지 않습니다.

| 파일(출력) | 내용 | 생성기 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트(ASCII 0x20–0x5F), 4bpp | `cvtools/font.py` `GLYPHS`(문자열로 그린 글리프) | 창작, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리와 게이지 타일 | `images.ui_tiles()` | 창작, Apache-2.0 |
| `cv_sprite_tiles`, `cv_sprite_pal` | 지도 화살표, 메모 핀, 녹음기 릴, 미터 눈금과 바늘, 열쇠구멍 마스크, 먼지, 빗방울, 벌레, 열쇠/테이프/신호탄/배터리 아이콘, 조준점, 불똥, 와이퍼 날 | `images.sprite_sheet()` | 창작, Apache-2.0 |
| `cv_portrait_tiles`, `cv_portrait_extpal` | 8bpp 환자 초상화 + 확장 OBJ 팔레트 4개(원래, 바램, 뒤틀림, 반전) | `images.portrait_8bpp()`, `portrait_palettes()` | 창작, Apache-2.0 |
| `cv_photo_bitmap` | 64x64 16bpp 찢어진 사진(비트맵 OBJ) | `images.photo_bitmap()` | 창작, Apache-2.0 |
| `cv_caustic_tiles`, `cv_caustic_pal` | 2장 어파인 레이어용 8bpp 물빛(커스틱) | `images.caustic_tiles()`, `caustic_palette()` | 창작, Apache-2.0 |
| `cv_map_tiles`, `cv_map_pal` | 어파인 지도용 8bpp 연필 그림 타일 | `images.map_tiles()`, `map_palette()` | 창작, Apache-2.0 |
| `cv_logo_lz`, `cv_logo_pal` | HUSHWELL 로고(폰트를 키운 8bpp 비트맵, 흘러내리는 모서리) | `images.logo_bitmap()` | 창작, Apache-2.0 |
| `cv_tex_flame` | A3I5 텍스처(타는 신호탄) | `images.textures()` | 창작, Apache-2.0 |
| `cv_tex_steam`, `cv_tex_glow` | A5I3 텍스처(김, 비상등·신호탄의 부드러운 빛) | `images.textures()` | 창작, Apache-2.0 |
| `cv_tex_tiles` | 4색 텍스처(곰팡이 핀 욕실 타일) | `images.textures()` | 창작, Apache-2.0 |
| `cv_tex_flesh` | 16색 텍스처(그것의 피부) | `images.textures()` | 창작, Apache-2.0 |
| `cv_tex_wood` | 256색 텍스처(문, 신자석, 서가) | `images.textures()` | 창작, Apache-2.0 |
| `cv_tex_wall`, `cv_tex_floor` | 4x4 압축 텍스처(초록 징두리판 위 회벽, 얼룩진 리놀륨) | `images.textures()`, `gfx.tex_4x4()` | 창작, Apache-2.0 |
| `cv_tex_photo`, `cv_tex_branch` | 다이렉트 컬러 텍스처(액자 사진, 구멍 뚫린 잔가지와 난간) | `images.textures()` | 창작, Apache-2.0 |
| `cv_toon_table` | 스테인드글라스 툰 테이블 | `images.toon_table()` | 창작, Apache-2.0 |
| 모든 3D 모델 | 그것(몸통, 머리, 위팔, 아래팔, 다리), 열쇠, 테이프, 신호탄 상자, 배터리, 무선기, 침대, 자동차, 신자석, 제단, 영사기, 우물, 비상등, 정문, 정육면체 | `arm9/source/gfx/models.c`(C로 조립) | 창작, Apache-2.0 |
| `cv_smp_*` (26) | step, splash, door, growl, voice, sting, pickup, flare, cursor, select, bass, pad, kick, snare, hat, hum, scream, thunder, static, drip, tape, taperev, heart, drone, hit, locked | `audio.make_samples()`(사인·사각파·노이즈·포먼트 합성, IMA-ADPCM 인코더 `codec.ima_adpcm`) | 창작 합성, Apache-2.0 |
| `cv_song_*` (11) | title, radio, rain, ward, hydro, archive, chapel, well, chase, ending, saved | `audio.compose()`(고정 시드 작곡기) | 창작 작곡, Apache-2.0 |
| `nitrofs/ch0–5.map` | 챕터 맵(칸과 물건) | `maps.py`(ASCII 맵, 빌드할 때 도달 가능성 검사) | 창작, Apache-2.0 |
| `nitrofs/title.lz` | 256x192 16bpp 타이틀 그림(요양원, 달, 우물, 비) | `images.title_bitmap()` | 창작, Apache-2.0 |
| `nitrofs/rain.pk` | 빗줄기 BG(3장 필름 긁힘에도 사용) | `images.rain_layer()` | 창작, Apache-2.0 |
| `nitrofs/brief0–5.pk` | 무선 교신 그림(8bpp + 확장 팔레트) | `images.briefing_art()` | 창작, Apache-2.0 |
| `nitrofs/stream.pcm` | 스트리밍 빗소리와 저음 배경음(PCM8 11025 Hz, 약 16초) | `audio.make_stream()` | 창작 합성, Apache-2.0 |
| `nitrofs/demo0–5.rpl` | 데모 플레이 입력 로그 | `maps.demo_replays()` | 창작, Apache-2.0 |
| `nitrofs/probe.bin` | 카트 DMA 확인용 2 KB 패턴(기대 CRC 포함) | `mkassets.py` | 창작, Apache-2.0 |
| `nitrofs/licenses/*` | 라이선스 원문(아래) | `Makefile`이 복사 | 각 원문의 라이선스 |
| `icon.bmp` | 32x32 배너 아이콘 | `images.icon()` | 창작, Apache-2.0 |

실행 중에 C 코드가 그리는 것(데이터 파일 없음): 4장 밤하늘 후면 비트맵(`game/chfx.c`),
3장 영사기 필름(`game/chfx.c`), 부팅 로고(`game/scenes.c`).

## 라이선스 원문(에셋 아님)

ROM의 `nitrofs/licenses/`에는 고지 의무를 지키려고 아래 원문을 그대로 넣습니다. 각
프로젝트의 라이선스 문서이며 Hushwell 에셋이 아닙니다.

| 파일 | 대상 |
|---|---|
| `LICENSE` | Hushwell(Apache-2.0) |
| `THIRD_PARTY_NOTICES` | 서드파티 고지 요약 |
| `MPL-2.0.txt` | BlocksDS crts |
| `crts-COPYING.zlib` | BlocksDS crts 중 zlib 라이선스 부분 |
| `libnds-LICENSE.txt`, `libnds-LICENSE.FatFs.txt` | libnds |
| `picolibc-COPYING.picolibc` | picolibc |
| `GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt` | libgcc |
