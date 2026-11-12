# Corvane 에셋 목록

[English](ASSETS.md)

Corvane의 그래픽·폰트·3D 모델·효과음·음악·스테이지 데이터는 **전부 이 저장소의
코드가 빌드할 때 생성**합니다. 외부에서 가져온 이미지·폰트·샘플·악보는 없습니다.
따라서 아래 모든 에셋은 직접 제작물이며, 저작권과 라이선스는 소스와 같은
**Apache-2.0** (Copyright 2026 devgyurak, the cleancart.nds maintainer)입니다. CC0 에셋도 쓰지 않았습니다.

생성기는 `tools/mkassets.py`이고, 실제 그리는/합성하는 코드는 `tools/cvtools/`에
있습니다. 같은 소스에서는 언제나 같은 바이트가 나옵니다(난수는 고정 시드의
xorshift, 파이썬 `random` 미사용).

`docs/images/`의 PNG/GIF는 빌드한 ROM을 헤드리스 DeSmuME에서 캡처한 README용
스크린샷입니다. 에셋이 아니라 문서이며, 빌드에서 읽지 않고 ROM에도 들어가지 않습니다.

| 파일(출력) | 내용 | 생성 코드 | 출처 / 라이선스 |
|---|---|---|---|
| `cv_font_tiles` | 5x7 픽셀 폰트(ASCII 0x20–0x5F) 4bpp | `cvtools/font.py` `GLYPHS` (글리프를 문자열로 직접 그림) | 직접 제작, Apache-2.0 |
| `cv_ui_tiles`, `cv_ui_pal` | 창 테두리·게이지 타일 | `images.ui_tiles()` | 직접 제작, Apache-2.0 |
| `cv_sprite_tiles`, `cv_sprite_pal` | 탄·폭발·레이더 점·락온·조준선·레이더 마스크·잔기 스프라이트 | `images.sprite_sheet()` | 직접 제작, Apache-2.0 |
| `cv_meteor_tiles`, `cv_meteor_extpal` | 8bpp 운석 + 확장 OBJ 팔레트 4종 | `images.meteor_8bpp()`, `meteor_palettes()` | 직접 제작, Apache-2.0 |
| `cv_cutin_bitmap` | 64x64 16bpp 조종사 컷인(비트맵 OBJ) | `images.cutin_bitmap()` | 직접 제작, Apache-2.0 |
| `cv_radar_grid`, `cv_radar_ring` | 레이더 격자·링 | `images.radar_grid()`, `hud_radar_ring()` | 직접 제작, Apache-2.0 |
| `cv_logo_lz`, `cv_logo_pal` | 타이틀 워드마크(폰트를 확대한 8bpp 비트맵) | `images.logo_bitmap()` | 직접 제작, Apache-2.0 |
| `cv_tex_glow` | A3I5 텍스처(빛나는 탄) | `images.textures()` | 직접 제작, Apache-2.0 |
| `cv_tex_smoke` | A5I3 텍스처(연기) | `images.textures()` | 직접 제작, Apache-2.0 |
| `cv_tex_flare` | A5I3 텍스처(큰 섬광용 부드러운 플레어) | `images.textures()` | 직접 제작, Apache-2.0 |
| `cv_tex_stripes` | 4색 텍스처(경고 줄무늬) | `images.textures()` | 직접 제작, Apache-2.0 |
| `cv_tex_hull` | 16색 텍스처(장갑판) | `images.textures()` | 직접 제작, Apache-2.0 |
| `cv_tex_armor` | 256색 텍스처(보스 장갑) | `images.textures()` | 직접 제작, Apache-2.0 |
| `cv_tex_terrain` | 4x4 압축 텍스처(지형) | `images.textures()`, `gfx.tex_4x4()` | 직접 제작, Apache-2.0 |
| `cv_tex_panel`, `cv_tex_mesh` | 다이렉트 컬러 텍스처(패널, 구멍 뚫린 철망) | `images.textures()` | 직접 제작, Apache-2.0 |
| `cv_toon_table` | 툰 셰이딩 테이블 | `images.toon_table()` | 직접 제작, Apache-2.0 |
| 3D 모델 전부 | 기체·포대·드론·비행선·전투기·정거장·벽포·보스 파츠 | `arm9/source/gfx/models.c` (C 코드로 조립) | 직접 제작, Apache-2.0 |
| `cv_smp_*` (20종) | 효과음·악기: shot, explode, bigboom, warning, jingle, lockon, missile, hit, cursor, select, bass, pad, kick, snare, hat, hum, charge, bomb, crash, drone | `audio.make_samples()` (사인·사각파·노이즈 합성, IMA-ADPCM 인코더 `codec.ima_adpcm`) | 직접 합성, Apache-2.0 |
| `cv_song_*` (9곡) | title, briefing, stage1–4, boss, ending, result | `audio.compose()` (시드 고정 작곡기: 진행·모티프·아르페지오·드럼을 코드로 생성) | 직접 작곡, Apache-2.0 |
| `nitrofs/stage1–5.lz` | 지형 높이맵·적 출현 스크립트 | `stages.py` (또는 `tools/stages/stageN.json`) | 직접 제작, Apache-2.0 |
| `nitrofs/title.lz` | 256x192 16bpp 타이틀 일러스트 | `images.title_bitmap()` | 직접 제작, Apache-2.0 |
| `nitrofs/nebula.pk` | 1024x384 8bpp 성운(대형 비트맵) | `images.nebula()` | 직접 제작, Apache-2.0 |
| `nitrofs/sky.pk` | 회전 하늘 어파인 BG | `images.sky_affine()` | 직접 제작, Apache-2.0 |
| `nitrofs/clouds.pk` | 구름 BG | `images.cloud_layer()` | 직접 제작, Apache-2.0 |
| `nitrofs/sea.pk` | 하늘·바다 BG | `images.sea_layer()` | 직접 제작, Apache-2.0 |
| `nitrofs/brief1–5.pk` | 브리핑 일러스트(8bpp + 확장 팔레트) | `images.briefing_art()` | 직접 제작, Apache-2.0 |
| `nitrofs/stream.pcm` | 스트리밍 BGM(앰비언트 패드, PCM8 11025 Hz, 약 16초) | `audio.make_stream()` | 직접 합성, Apache-2.0 |
| `nitrofs/demo1–5.rpl` | 데모 플레이용 입력 로그 | `stages.demo_replays()` | 직접 제작, Apache-2.0 |
| `nitrofs/probe.bin` | 카트 DMA 검증용 2 KB 패턴(CRC 기대값 포함) | `mkassets.py` | 직접 제작, Apache-2.0 |
| (실행 중 생성) `acc-stage` 카드 텍스처와 등불 스프라이트 | 재단선이 있는 64x16 다이렉트 컬러 종이 카드, 64x64 4bpp 원형 빛 | `arm9/source/demo/demo.c`(`acc_make_card`, `acc_make_lamp`)가 실행 중에 그림, 데모 빌드 전용 | 직접 제작, Apache-2.0 |
| `nitrofs/licenses/*` | 라이선스 원문(아래 참고) | `Makefile`이 복사 | 각 원문의 라이선스 |
| `icon.bmp` | 32x32 배너 아이콘 | `images.icon()` | 직접 제작, Apache-2.0 |

## 라이선스 원문(에셋 아님)

ROM 안의 `nitrofs/licenses/`에는 고지 의무를 위해 다음 원문이 그대로 들어갑니다.
이 파일들은 Corvane의 에셋이 아니라 각 프로젝트의 라이선스 문서입니다.

| 파일 | 대상 |
|---|---|
| `LICENSE` | Corvane (Apache-2.0) |
| `THIRD_PARTY_NOTICES` | 서드파티 고지 요약 |
| `MPL-2.0.txt` | BlocksDS crts |
| `crts-COPYING.zlib` | BlocksDS crts의 zlib 부분 |
| `libnds-LICENSE.txt`, `libnds-LICENSE.FatFs.txt` | libnds |
| `picolibc-COPYING.picolibc` | picolibc |
| `GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt` | libgcc |
