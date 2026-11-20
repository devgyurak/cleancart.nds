# Buoyane 실기 QA

[English](HW-QA.md)

**실행 1(2026-11-20, 본체 시계 2026-01-18 07:42), DS Lite + R4 Plus(Wood):** QA 실행은 `RESULT PASS`에 닿고 모든 체크포인트 해시가 melonDS 1.1 소프트웨어와 같습니다
(`compare.py`: 차이 0). QA 블록 판정은 **L1 11/11, L2 18/22 OK, 판정 NG 4개**, 참고용 NG 1개입니다. NG 4개는 에뮬레이터 프로필이 아니라 우리 기대값에 대한 발견이며
검토 중이고 `expected.json`은 바꾸지 않았습니다(아래 "실행 1"). 에뮬레이터끼리 갈리던 `aa-opaque-dot-hidden`, `edge-aa-translucent`, `aa-marked-line-complete`,
`attr-latch-begin`은 콘솔이 GBATEK과 같았습니다.

순서(스킬 `nds-hw-qa`, DS Lite + R4 Plus): `buoyane-hw-support.nds`(게임 코드가 `####`인 릴리스 ROM. `tools/hwsupport.py`가 만들어 SD 카드를 받게 합니다)를 카드에 두고,
`cleancart/buoyane/` 폴더와 그 안의 빈 파일 `autoqa`를 만든 뒤 켜서 타이틀을 건드리지 않습니다. QA 실행이 접근 30개를 진행하고 `qa-*.log`를 쓰며,
판정 스위트가 장면 5개의 `res-*-check-<scene>.log`를 씁니다. `suite-<stamp>.log`에 `suite end`가 보일 때까지 기다린 뒤 끕니다. 파일을 거두어 `tools/qa/ingest.py`, `python3 games/buoyane/tools/judge.py --hardware`를 실행하고 이 표와 영어 쌍에 기록합니다.

| 항목 | 에뮬레이터 결과(소프트웨어 / GL Classic / GL Compute / DeSmuME) | 실기(실행 1) |
|---|---|---|
| `aa-opaque-dot-hidden` | 모두 NG(GBATEK 항목, 판정 대상) | **OK**(GBATEK이 맞고 에뮬레이터 4종이 틀림) |
| `fog-alpha-glitch` | 모두 NG | NG: 0, 이 장면에서는 글리치 없음(참고용) |
| `wire-texture-alpha` | 모두 NG(GBATEK 항목, 판정 대상) | **NG**: 0, 에뮬레이터와 같음(우리 기대값에 대한 발견) |
| `aa-marked-line-complete` | 소프트웨어·GL Compute OK, GL Classic·DeSmuME NG(GBATEK 항목, 판정 대상) | **OK**(GL Classic·DeSmuME이 틀림) |
| `edge-aa-translucent` | 소프트웨어 OK, GL Compute·GL Classic·DeSmuME NG | **OK**(GL Compute·GL Classic·DeSmuME이 틀림) |
| `attr-latch-begin` | 네 가지 모두 OK | OK |
| `zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-decal-whole` | melonDS 소프트웨어 OK(105, 152, 구멍 없음), 실행 전에는 묻지 않음 | **NG**: 109, 159, 구멍 960픽셀(발견, 실행 1 참고) |

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
| `zeq-slope-band-left`(`values[10]`, L2, GBATEK, 기대 101..107) | **109**(NG) | 105(OK) | 예: Equal 띠 경계가 GBATEK의 ±200h 모델과 melonDS보다 4픽셀 오른쪽 |
| `zeq-slope-band-right`(`values[11]`, 기대 149..155) | **159**(NG) | 152(OK) | 예: 7픽셀 오른쪽 |
| `zeq-decal-whole`(`values[9]` 비트 3, 기대 구멍 없음) | **구멍 있음**(NG), `values[18]` = 960 | 구멍 없음, `values[18]` = 0 | 예: 콘솔에서는 데칼 일부가 Equal 테스트에 실패 |
| `wire-texture-alpha`(`values[15]` 비트 2, 기대 블렌딩) | **0**(NG) | 0(NG, 프로필) | 아니오: 에뮬레이터 4종과 같음. 우리가 GBATEK을 읽은 방식에 대한 발견 |
| `fog-alpha-glitch`(`unconfirmed`, 기대 1) | 0(참고용 NG) | 0 | 아니오: 콘솔에서도 이 장면에는 글리치가 없음 |
| `aa-opaque-dot-hidden`(`values[1]` 비트 8, 9) | OK(`values[1]` = 0x7ff) | NG(0x6ff) | 예: 콘솔에서는 GBATEK이 맞음 |
| `edge-aa-translucent`, `aa-marked-line-complete`, `attr-latch-begin` | OK | 위 표 참고 | 콘솔은 GBATEK과 같음, 에뮬레이터는 프로필대로 |
| 판정 안 함: `values[13]`(`W_ZEQ_1`, 깊이가 한 단계 다른 평면) | **0** | `0x04000400`(1024 + 1024픽셀) | 예: melonDS는 평면을 그리고 콘솔은 그리지 않음 |
| 판정 안 함: `values[14]`(`W_AA_LINES`) | `0xc000be`(192와 190) | `0xc000c0`(192와 192) | 예: 두 번째 값이 2픽셀 적음 |
| 나머지 판정 항목 | melonDS 소프트웨어와 같음 | | 아니오 |

두 결과 블록(실기, melonDS 소프트웨어)에서 다른 단어: `values[1]`(`0x7ff`, `0x6ff`), `[9]`(`0x1f7`, `0x1ff`), `[10]`(109, 105), `[11]`(159, 152), `[13]`(0, `0x04000400`),
`[14]`(`0xc000be`, `0xc000c0`), `[18]`(960, 0), 그리고 프레임 단어와 랙 합계. 나머지는 상태 해시를 포함해 모두 같습니다.

실행은 한 번뿐입니다. Equal 값(109, 159, 960, 그리고 없는 `values[13]` 평면)은 GBATEK의 ±200h 허용 모델로 설명되지 않습니다. 이를 어떻게 할지(콘솔 값으로 기대값을 정할지,
레벨 4로 `probes/`에 옮길지)는 열려 있고, `wire-texture-alpha`(GBATEK이 와이어프레임이 텍스처 알파로 블렌딩된다고 실제로 쓰는지)도 열려 있습니다. 이 실행 기록은 `expected.json`을 바꾸지 않습니다.
