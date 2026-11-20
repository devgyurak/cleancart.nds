# Pendulane 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `pendulane.nds` 대신 `build/release/rom/pendulane-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

**판정 스위트(QA 판정 스위트 PR 이후):** QA 실행(`autoqa`)이 이제 스위트로 끝납니다. `RESULT PASS` 뒤에 QA 블록(리허설 항목을 담음)를 `res-*-pendulane-*.log`로 쓰고 진행 파일 `suite-<stamp>.log`도 남깁니다. hw-support ROM 한 번 실행으로 충분하고, **전원은 스위트가 끝난 뒤에** 끕니다(`suite-<stamp>.log`에 `suite end`). 아래의 리허설·데모 ROM은 항목 하나를 다시 돌릴 때만 씁니다(Wood 커널에서는 SD 카드를 받지 못함). `tools/qa/ingest.py`가 파일마다 채점합니다.

**2026-11-17 DS Lite 실행 1·2(이 문서 끝에 기록):** QA 실행은 두 번 모두 PASS입니다. 판정 NG 항목 둘(`mem7-main-nonseq`, `mem7-code-main-seq`)은 우리 기대값에 대한 발견이었고, 이후 콘솔 값으로 기대값을 정했습니다("실행에서 정한 기대값" 참조).

실행 1 전에는 실기에서 잰 값이 없었습니다. 이 문서는 실행 점검표로, 첫 값을 기록하고 `expected.json`의 `unconfirmed` 항목을 승격할 수 있게 합니다.
콘솔은 사람이 조작합니다 (스킬 `nds-hw-qa`): 카드에 빈 폴더 `cleancart/pendulane/`를 만들면 리허설이 결과 블록을 거기에 씁니다.
`python3 tools/qa/ingest.py /Volumes/<card> --game pendulane`가 `judge.py --hardware --partial`로 채점합니다. 실기 NG는 프로파일이 아니라 발견입니다.

**첫 R4 실행 순서:** 먼저 카드에 빈 파일 `cleancart/pendulane/nocart`를 두고 SD 로그가 써지는지 확인한 뒤, 마커 없이 한 번 더 돌려 카드 커널이 카트 벤치를 받는지 봅니다. 리허설과 QA 로그는 따로 채점하세요: 둘을 함께 `judge.py --partial`에 넘기면 마지막 블록만 채점됩니다.

## 실기가 답해야 할 것

| 질문 | 어디에 나오나 |
|---|---|
| DIV 모드 0은 버스 클럭 18, 모드 1·2는 34, SQRT는 13이 걸리나 (GBATEK "DS Maths")? | 리허설 값 3~5 (반사이클 10, 32, 0; 창 ±2) |
| 펌웨어 SPI 바이트가 512 kHz에서 4 MHz보다 8 x (64 - 8) = 448 사이클 긴가 (/8, /64 분주는 공칭 속도에서 추정)? | 리허설 값 6 (448, 창 ±8); 원시 엣지 28~31 |
| ARM7 메인 RAM 비순차 32비트 접근이 10사이클(GBATEK 본문)인가 5("reportedly")인가? 두 에뮬레이터 모두 10과 다름. | 리허설 값 16 (10이면 576, 5이면 256) |
| 나머지 ARM7 메모리 타이밍(메인 RAM S32 = 2, 메인 RAM의 코드, VRAM S32 = 2)이 맞나? | 값 15, 17, 18 |
| 원샷 채널의 Busy가 PCM16 16샘플이면 시작 18샘플 뒤, ADPCM 32샘플이면 42샘플 뒤에 꺼지나 (GBATEK: 마지막 샘플의 시작에서)? 두 에뮬레이터 모두 1샘플 늦게 꺼짐. | 리허설 값 10, 11 |
| PSG, PCM16, ADPCM이 시작 쓰기 1, 3, 11샘플 뒤에 시작하고, Hold 재시작이 옛 값을 한 샘플, 이어서 0 샘플 둘을 내나? | 값 12, 13, 14 (2, 8, 258) |
| Busy가 일정한 속도로 꺼지나 (32샘플을 더 냈을 때의 기울기가 16 이내로 0 사이클)? | 값 20 (참고용; 두 에뮬레이터 모두 부팅 경로마다 값이 다름) |
| (카드 커널이 카트 벤치의 슬롯 1 명령을 받지 못하면 카드에 빈 파일 `cleancart/pendulane/nocart`를 두세요: 벤치를 건너뛰고 카트 항목 둘은 측정되지 않은 채로 남습니다. 이 항목은 `env: "cart"`라 콘솔에서는 어차피 n/a입니다.) gap1이 +0x100 클럭 펄스면 슬롯 1 B8 명령이 CLK/5에서 버스 1280사이클, CLK/8에서 2048사이클 길어지나? 항목은 `env: "cart"`라 플래시 카드 커널에는 물을 수 없지만 카드가 읽은 값은 기록합니다. B8이 카드의 KEY2 상태를 그대로 두나(이후 NitroFS 읽기)? | 값 8, 9; 원시 엣지 19, 22 |
| ARM9 쓰기 버퍼가 엣지를 옮기나 (콘솔에서는 고정 경로가 같지 않을 수 있음)? | 세 차이가 창 안에 남는지; 값 24~27의 원시 엣지는 참고용 |

## 기록

| 날짜 | 콘솔 | 카드 | 결과 |
|---|---|---|---|
| 2026-11-17 | DS Lite(NTR) | R4 Plus(Wood) | QA 실행 PASS. 판정 NG 2, 아래 실행 1 참조 |
| 2026-11-17 (본체 시계 2026-01-15 12:06) | DS Lite(NTR) | R4 Plus(Wood) | QA 실행 PASS. 같은 판정 NG 2, 아래 실행 2 참조 |

## 실행 1: QA 실행과 판정 스위트, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | `pendulane-hw-support.nds` v0.2.1 `3ab56cf90dcd375c0fb8e990b20b2f5a4a3ef1f9a392b1263d062db4bc7e6a70`(439,660바이트, 릴리스와 같음). melonDS 1.1 soft도 같은 파일로 실행 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 53줄, 체크포인트 7개, build `dc770b923005`, `start=marker`로 시작. 스위트: QA 블록만(`suite block block=qa saved=1`. 스위트 데모가 없어 `suite end` 줄은 원래 없음). 판정: L1 7/7, L3 11/13(**판정 NG 2: `mem7-main-nonseq`, `mem7-code-main-seq`**) +참고용 1. n/a 3(env: `cart-gap1-clk5`, `cart-gap1-clk8`, `pre-cart-busy`) |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=0 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | 없음. ch1-ch5, judge, credits 해시가 melonDS 1.1 soft와 같음(`compare.py`, 차이 0) |
| 랙 프레임 | 5 / 6 / 6 / 6 / 5 / 2 / 0 (ch1-ch5, judge, credits). melonDS soft 모두 0 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모두 `uf0,of0`. 콘솔은 `rdl=63`(3D 없음), melonDS는 46 |

| 항목 | 실기 | melonDS 1.1 soft | 차이? |
|---|---|---|---|
| `mem7-main-nonseq` (L3, GBATEK, 기대 574..578) | **383** (NG) | 384 (NG, 프로필) | 콘솔이 GBATEK의 9 x 64가 아니라 melonDS와 같음. 기대값이 틀렸음: 이제 376..392, source `hw:dslite-ntr`(아래 참조) |
| `mem7-code-main-seq` (L3, GBATEK, 기대 126..130) | **142** (NG) | 128 | 있음. 콘솔에서는 메인 RAM의 ARM7 순차 코드 읽기가 더 느림. 기대값이 틀렸음: 이제 139..145, source `hw:dslite-ntr`(아래 참조) |
| `snd-hold-restart` / `snd-pcm-busy` / `snd-adpcm-busy` | 258 / 18 / 42 (OK) | 7 / 19 / 43 (NG, 프로필) | melonDS 프로필 NG가 실제 에뮬레이터 차이임이 확인됨 |
| `snd-rate-slope` (unconfirmed) | 13 | 830 | 콘솔에서 -16..16 안 |
| `math-div32-vs-sqrt`, `spi-512k-minus-4m` | 10, 448 | 9, 449 | 둘 다 OK |
| 타이밍 원시값 values[24..31], [36..39] (판정 안 함) | melonDS보다 2-3 큼 | | 있음, 판정 안 함 |
| 나머지 판정 항목 | melonDS soft와 같음 | | 없음 |

NG 항목은 실행 1을 기록한 PR에서는 고치지 않았고, 아래 "실행에서 정한 기대값"에서 다룹니다.

## 실행 2: QA 실행과 판정 스위트, 2026-11-17 (본체 시계 2026-01-15 12:06)

실행 1과 같은 콘솔·카드·ROM(`pendulane-hw-support.nds`, build `dc770b923005`)으로 다시 돌렸습니다. 본체 시계가 2026-01-15 12:06을 가리켜 파일 이름이 `res-20260115-*`입니다(실행 1: `res-20020115-*`).

| 항목 | 값 |
|---|---|
| `judge.py --hardware` | QA 블록: 실행 1과 같은 판정(기대값을 바꾸기 전: L3 11/13, 같은 판정 NG 2, 383과 142 그대로). n/a 3(env: `cart-gap1-clk5`, `cart-gap1-clk8`, `pre-cart-busy`) |
| 실행 1과의 차이 | `snd-rate-slope`(`values[20]`, 원시 `[46]`) 13 -> -10, 랙 프레임 합 30 -> 32, 블록 프레임 4292 -> 4294. 나머지 워드는 모두 같음 |

## 실행에서 정한 기대값

| 항목 | 변경 | 이유 |
|---|---|---|
| `mem7-main-nonseq` | 범위 `376..392`, source `hw:dslite-ntr`. 제목: 메인 RAM N32는 실효 7, WRAM은 1: 6x64 = 384. GBATEK의 표 10은 명령 비용이 아님 | 두 실행 모두 383. melonDS는 384로 이제 통과하고, DeSmuME는 64로 NG 그대로 |
| `mem7-code-main-seq` | 범위 `139..145`, source `hw:dslite-ntr`. 제목: 64워드에서 GBATEK의 워드당 2보다 약 14사이클 더 듦. 원인은 문서화되지 않음 | 두 실행 모두 142(GBATEK 128). melonDS와 DeSmuME는 128이라 이제 NG |
| `snd-rate-slope` | 그대로: `unconfirmed`, -16..16, 참고용 | 콘솔 값이 실행마다 흔들려(13, 그다음 -10) 기대할 값이 아님 |
| `snd-pcm-busy`, `snd-adpcm-busy`, `snd-onset-pcm-psg`, `snd-onset-adpcm-pcm`, `snd-hold-restart`, `spi-512k-minus-4m`, `mem7-main-seq`, `mem7-vram-seq`, `math-*` 셋, `env`가 아닌 `pre-*` 다섯 | `level_basis`에 `hw:games/pendulane/docs/HW-QA.md` 추가 | 콘솔이 GBATEK과 같음(두 실행 모두). source는 `GBATEK` 그대로 |
| 게임 안 장부 | v0.2.2부터 장부 화면(`game/ledger.c`)도 같은 창을 씁니다: MAIN NONSEQ 384 ±8, CODE IN MAIN 142 ±3. 4장의 글자색과 인장 표시만 여기에 달려 있고, 장치 퍼즐의 정거장 비용(`pdorder/model.py`)은 그대로입니다. melonDS는 MAIN NONSEQ 384(창 안)와 CODE IN MAIN 128(창 밖)을 읽어 4장이 계속 NO SEAL로 보입니다 |
| 프로필 | `mem7-main-nonseq`를 melonDS 세 프로필의 `ng`에서 뺌(DeSmuME는 유지). `mem7-code-main-seq`를 네 프로필 모두의 `ng`에 추가 | |
