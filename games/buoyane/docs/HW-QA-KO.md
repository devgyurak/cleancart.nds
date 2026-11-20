# Buoyane 실기 QA

[English](HW-QA.md)

**실행 2(2026-11-20, v0.1.1, 본체 시계 2026-01-18 08:46):** 판정 항목 모두 OK(QA 블록 L1 11/11, L2 20/20, check 블록 다섯 개 OK)이고, 체크포인트는 실행 1 및 melonDS 1.1 소프트웨어와 같으며, 실행 1의 값이 그대로 반복됩니다. 새 `values[21]`은 흰 와이어 픽셀 170, 블렌딩 픽셀 0: 콘솔은 A5I3 와이어프레임을 불투명하게 그립니다. 이 문서 끝의 "실행 2" 참고.

**실행 1(2026-11-20, 본체 시계 2026-01-18 07:42), DS Lite + R4 Plus(Wood):** QA 실행은 `RESULT PASS`에 닿고 모든 체크포인트 해시가 melonDS 1.1 소프트웨어와 같습니다
(`compare.py`: 차이 0). QA 블록 판정은 **L1 11/11, L2 18/22 OK, 판정 NG 4개**, 참고용 NG 1개입니다. NG 4개는 에뮬레이터 프로필이 아니라 우리 기대값에 대한 발견이며
나중에 `expected.json`을 바꿨습니다(아래 "실행에서 나온 기대값"). 에뮬레이터끼리 갈리던 `aa-opaque-dot-hidden`, `edge-aa-translucent`, `aa-marked-line-complete`,
`attr-latch-begin`은 콘솔이 GBATEK과 같았습니다.

순서(스킬 `nds-hw-qa`, DS Lite + R4 Plus): `buoyane-hw-support.nds`(게임 코드가 `####`인 릴리스 ROM. `tools/hwsupport.py`가 만들어 SD 카드를 받게 합니다)를 카드에 두고,
`cleancart/buoyane/` 폴더와 그 안의 빈 파일 `autoqa`를 만든 뒤 켜서 타이틀을 건드리지 않습니다. QA 실행이 접근 30개를 진행하고 `qa-*.log`를 쓰며,
판정 스위트가 장면 5개의 `res-*-check-<scene>.log`를 씁니다. `suite-<stamp>.log`에 `suite end`가 보일 때까지 기다린 뒤 끕니다. 파일을 거두어 `tools/qa/ingest.py`, `python3 games/buoyane/tools/judge.py --hardware`를 실행하고 이 표와 영어 쌍에 기록합니다.

| 항목 | 에뮬레이터 결과(소프트웨어 / GL Classic / GL Compute / DeSmuME) | 실기(실행 1) |
|---|---|---|
| `aa-opaque-dot-hidden` | 모두 NG(GBATEK 항목, 판정 대상) | **OK**(GBATEK이 맞고 에뮬레이터 4종이 틀림) |
| `fog-alpha-glitch` | 모두 NG | NG: 0, 이 장면에서는 글리치 없음(참고용, `probes/`로 옮김) |
| `wire-texture-alpha` | 모두 NG(GBATEK 항목, 판정 대상) | **NG**: 0, 에뮬레이터와 같음(GBATEK과 다름. 기대값을 0, `hw:dslite-ntr`로 바꿈) |
| `aa-marked-line-complete` | 소프트웨어·GL Compute OK, GL Classic·DeSmuME NG(GBATEK 항목, 판정 대상) | **OK**(GL Classic·DeSmuME이 틀림) |
| `edge-aa-translucent` | 소프트웨어 OK, GL Compute·GL Classic·DeSmuME NG | **OK**(GL Compute·GL Classic·DeSmuME이 틀림) |
| `attr-latch-begin` | 네 가지 모두 OK | OK |
| `zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-decal-whole` | melonDS 소프트웨어 OK(105, 152, 구멍 없음), 실행 전에는 묻지 않음 | **NG**: 109, 159, 구멍 960픽셀(위치와 구멍은 더 판정하지 않고 폭을 판정함, 아래 참고) |

## 실행 1: QA 실행과 판정 스위트, 2026-11-20

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드 | 2026-11-20, DS Lite(NTR), Wood R4 커널의 "R4 Plus". 본체 시계가 2026-01-18 07:42여서 파일 이름이 `*-20260118-*` |
| ROM SHA-256 | 릴리스 `buoyane-hw-support.nds` v0.1.0, `15587e8fa38195578bf6e1712172041ca3e0c5fdd6337a6e76c768a888f91ad3`, 빌드 `9aea0989642b`. melonDS 1.1 소프트웨어도 같은 파일로 돌림 |
| `ingest.py` / `judge.py --hardware` | QA 로그 PASS, 112줄, 체크포인트 7개, `start=marker`. QA 블록: L1 11/11 OK, L2 18/22 OK +참고용 1, **판정 NG 4**(`zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-decal-whole`, `wire-texture-alpha`), 참고용 NG 1(`fog-alpha-glitch`) |
| QA 로그 머리 | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1`; 모든 `save write`가 `sd_match=1` |
| melonDS와 처음 달라지는 체크포인트 | 없음: ch1~ch5, judge, credits 해시가 melonDS 1.1 소프트웨어와 같음(`compare.py`, 차이 0) |
| 랙 프레임 | 16 / 19 / 18 / 16 / 17 / 3 / 0(ch1~ch5, judge, credits), 합계 89. melonDS 소프트웨어는 모두 0 |
| 3D 플래그(`gx=` `rdl=`) | 모두 `uf0,of0`. `rdl=46`이 체크포인트마다 같음(melonDS와 같음, 게임이 3D를 그림) |
| 스위트 | `suite block block=qa saved=1`, 데모 5개(`check-dot`, `-fog`, `-edge`, `-depth`, `-blend`) 모두 `saved=1`, `suite end demos=5 saved=5`, 이어서 probe 12단계가 `wait60`까지 `ok=1`. `suite done` 줄은 없음(모든 게임 공통의 알려진 문제) |

| 항목 | 실기 | melonDS 1.1 소프트웨어 | 다른가? |
|---|---|---|---|
| `zeq-slope-band-left`(`values[10]`, L2, GBATEK, 기대 101..107) | **109**(NG) | 105(OK) | 예: Equal 띠 경계가 우리 깊이 모델과 melonDS보다 4픽셀 오른쪽 |
| `zeq-slope-band-right`(`values[11]`, 기대 149..155) | **159**(NG) | 152(OK) | 예: 7픽셀 오른쪽 |
| `zeq-decal-whole`(`values[9]` 비트 3, 기대 구멍 없음) | **구멍 있음**(NG), `values[18]` = 960 | 구멍 없음, `values[18]` = 0 | 예: 콘솔에서는 데칼 일부가 Equal 테스트에 실패 |
| `wire-texture-alpha`(`values[15]` 비트 2, 기대 블렌딩) | **0**(NG) | 0(NG, 프로필) | 아니오: 에뮬레이터 4종과 같음. 다른 것은 GBATEK의 문장(아래 인용) |
| `fog-alpha-glitch`(`unconfirmed`, 기대 1) | 0(참고용 NG) | 0 | 아니오: 콘솔에서도 이 장면에는 글리치가 없음 |
| `aa-opaque-dot-hidden`(`values[1]` 비트 8, 9) | OK(`values[1]` = 0x7ff) | NG(0x6ff) | 예: 콘솔에서는 GBATEK이 맞음 |
| `edge-aa-translucent`, `aa-marked-line-complete`, `attr-latch-begin` | OK | 위 표 참고 | 콘솔은 GBATEK과 같음, 에뮬레이터는 프로필대로 |
| 판정 안 함: `values[13]`(`W_ZEQ_1`, 깊이가 한 단계 다른 평면) | **0** | `0x04000400`(1024 + 1024픽셀) | 예: melonDS는 평면을 그리고 콘솔은 그리지 않음 |
| 판정 안 함: `values[14]`(`W_AA_LINES`) | `0xc000be`(192와 190) | `0xc000c0`(192와 192) | 예: 두 번째 값이 2픽셀 적음 |
| 나머지 판정 항목 | melonDS 소프트웨어와 같음 | | 아니오 |

두 결과 블록(실기, melonDS 소프트웨어)에서 다른 단어: `values[1]`(`0x7ff`, `0x6ff`), `[9]`(`0x1f7`, `0x1ff`), `[10]`(109, 105), `[11]`(159, 152), `[13]`(0, `0x04000400`),
`[14]`(`0xc000be`, `0xc000c0`), `[18]`(960, 0), 그리고 프레임 단어와 랙 합계. 나머지는 상태 해시를 포함해 모두 같습니다.

실행은 한 번뿐이고, 이 기록은 기대값을 바꾸지 않았습니다. NG 4개를 그 뒤 어떻게 했는지는 다음 절에 있습니다.

## 실행에서 나온 기대값

| 항목 | 변경 | 이유 |
|---|---|---|
| `wire-texture-alpha` | 기대 `0`(마스크 4), source `hw:dslite-ntr`, `level_basis` `gbatek:DS 3D Texture Blending; hw:games/buoyane/docs/HW-QA.md`. 제목 "GBATEK says At<31 makes the wireframe translucent; a DS Lite draws it without blending". 네 프로필의 `ng`에서 뺌 | GBATEK("DS 3D Texture Blending"의 Uni-Colored Textures): "Wire-frame polygons are always having Av=31, however, they can be made transparent by using Translucent Textures (ie. A5I3 or A3I5 formats) with At<31." 콘솔은 블렌딩된 와이어 픽셀을 그리지 않고(`values[15]` 비트 2 = 0, `values[16..17]` = `001f7fff 001fffff`) 에뮬레이터도 마찬가지입니다. 이 비트로는 "불투명하게 그림"과 "안 그림"을 구분할 수 없습니다(흰 와이어 픽셀은 아직 세지 않음, ROM 변경 예정). 그래서 이 항목은 에뮬레이터가 블렌딩을 시작하는 것만 막아 줍니다 |
| `zeq-slope-band-left`, `zeq-slope-band-right` | `zeq-slope-band-width`(L2, GBATEK `DS 3D Polygon Attributes`)로 대체: 오른쪽 x − 왼쪽 x가 42..54. 위치 항목 둘은 없앰 | 폭은 GBATEK의 ±200h 창에서 나오고 콘솔(50), melonDS 소프트웨어·Compute(47)와 맞습니다. 위치(x 104, 152)는 우리 깊이 모델에 기댄 값이었고 콘솔은 약 5.5픽셀 오른쪽입니다. GL Classic(95)과 DeSmuME(23)은 폭에서 틀려 `ng`에 들어감 |
| `zeq-decal-whole` | 게임에서 뺌 | 구멍(960픽셀)은 문서에 없는 래스터라이저 반올림에 달려 있습니다. `values[18]`은 블록에 남기고 판정하지 않음 |
| `values[13]`(`W_ZEQ_1`) | 블록에 남기고 판정하지 않음 | `probes/`의 `zeq-step-boundary`가 같은 한 단계 평면을 잼 |
| `fog-alpha-glitch` | 게임에서 뺌 | 한 장면의 음성 결과 하나로 "글리치 없음"을 확정할 수 없고, `unconfirmed` 항목은 L4인데 게임에는 L4를 둘 수 없습니다. 콘솔 값 0은 여기와 `docs/HW-FINDINGS-KO.md` 3절에 기록 |
| 프로필 | `ng`: 소프트웨어 1(`aa-opaque-dot-hidden`), Compute 2(그것과 `edge-aa-translucent`), GL Classic 15, DeSmuME 13 | 렌더러마다 리허설 블록을 다시 채점: 네 프로필 모두 MATCH |

후속 작업: ROM 변경(진단용, 판정 비트와 상태 해시는 그대로)이 `values[21]`(와이어 사각형의 흰 | 블렌딩 픽셀 수)을 더하고, `zeq-tolerance-flat`의 낡은 단계 표기(단계 -2와 +2)와 `lightcheck.c`의 띠 주석을 고칩니다. 그다음 Buoyane v0.1.1과 실기 2차입니다.
실기 2차에서는 `values[21]`(흰 픽셀 0 = "안 그림", 0보다 크고 `values[15]` 비트 2가 0 = "불투명하게 그림". melonDS 소프트웨어·Compute는 흰 픽셀 170, GL Classic·DeSmuME는 0)을 읽고, 109 / 159(`values[10]`, `[11]`), 데칼 구멍 960(`values[18]`), ±1단계 평면 0(`values[13]`)이 반복되는지 봅니다.
그 뒤에야 띠 위치, 데칼 구멍, fog alpha 글리치의 probe 항목을 `probes/`에 추가하고, 반복된 값을 실기 관측으로 남깁니다(출처 "Buoyane hw run").

## 실행 2: v0.1.1로 한 QA 실행과 판정 스위트, 2026-11-20 (본체 시계 2026-01-18 08:46)

| 항목 | 값 |
|---|---|
| ROM | 릴리스 `buoyane-hw-support.nds` v0.1.1, SHA-256 `6f77608124b8e964718f189ac6a775ca7109534d4a0ac5d618d1215133e369c3`(카드의 복사본도 같음), build `3dd5ac721c8a` |
| `ingest.py` / `judge.py --hardware`(#217의 기대값) | QA 로그 PASS, 112줄, 체크포인트 7개, `start=marker`. QA 블록 L1 11/11, L2 20/20 OK. `check-dot`, `-fog`, `-edge`, `-depth`, `-blend` 모두 OK. **NG 0** |
| 체크포인트 | ch1-ch5, judge, credits가 실행 1 및 melonDS 1.1 소프트웨어와 같음(`compare.py`, 차이 0) |
| 스위트 | 실행 1과 같음: QA 블록과 데모 다섯 개 `saved=1`, `suite end demos=5 saved=5`, probe 12단계가 `wait60`까지 `ok=1`, `suite done` 줄 없음 |
| 실행 1과의 차이(모든 블록, 워드 단위) | 새 `values[21]`(`0xaa`)과 랙 합계뿐. `values[10]`, `[11]` = 109, 159(폭 50), `[13]` = 0, `[18]` = 960, `[14]` = `0xc000be` 그대로 |

`values[21]` = 흰 와이어 픽셀 170 | 블렌딩 픽셀 0 << 16: A5I3 텍스처(알파 15) 와이어프레임은 **그려지고, 불투명**합니다. melonDS 소프트웨어·Compute(170 | 0)와 같습니다. GL Classic과 DeSmuME는 0으로, 아예 그리지 않습니다.
`wire-texture-alpha`는 이제 `values[15]` 비트 2 대신 이 워드를 판정합니다(범위 1..65535: 흰 픽셀이 하나 이상, 블렌딩 픽셀 없음). 비트 2로는 "불투명하게 그림"과 "안 그림"을 구분하지 못했습니다. GL Classic과 DeSmuME는 이 항목에서 NG가 되어 `ng`에 들어갑니다.
실행 1은 `values[21]`이 없는 v0.1.0이라, 지금 `expected.json`으로 채점하면 이 항목이 NG(0)입니다.
Equal 띠 위치, 데칼 구멍, 한 단계 평면이 반복됐으므로 이 값들을 실기 관측으로 삼아 `probes/`로 옮길 수 있습니다.
