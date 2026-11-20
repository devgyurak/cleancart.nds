# Tonewick 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `tonewick.nds` 대신 `build/release/rom/tonewick-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

**판정 스위트(QA 판정 스위트 PR 이후):** QA 실행(`autoqa`)이 이제 스위트로 끝납니다. `RESULT PASS` 뒤에 QA 블록(리허설 항목을 담음)를 `res-*-tonewick-*.log`로 쓰고 진행 파일 `suite-<stamp>.log`도 남깁니다. hw-support ROM 한 번 실행으로 충분하고, **전원은 스위트가 끝난 뒤에** 끕니다(`suite-<stamp>.log`에 `suite end`). 아래의 리허설·데모 ROM은 항목 하나를 다시 돌릴 때만 씁니다(Wood 커널에서는 SD 카드를 받지 못함). `tools/qa/ingest.py`가 파일마다 채점합니다.

**2026-11-17 DS Lite 실행 1·2(이 문서 끝에 기록):** QA 실행은 두 번 모두 PASS입니다. 판정 NG 항목 하나(`snd-short-silent`)는 우리 기대값에 대한 발견이었고, 이후 콘솔 값으로 기대값을 정했습니다("실행에서 정한 기대값" 참조).

실행 1 전에는 실기에서 잰 값이 없었습니다. 이 문서는 그 실행의 점검표입니다. 첫 값을 기록하고, 에뮬레이터나 GBATEK 읽기만
뒷받침하는 항목을 확정하기 위한 것입니다. 콘솔은 사람이 조작하며, 아래 단계는 할 일과 적어 둘 것입니다.

**자동 경로(스킬 `nds-hw-qa`):** 카드에 빈 폴더 `cleancart/tonewick/`를 만들면 리허설이 결과 블록을 그곳에 씁니다
(`res-*-tonewick-rehearsal.log`). 빈 파일 `cleancart/tonewick/autoqa`는 버튼을 누르지 않고도 QA 실행을 시작합니다.
`python3 tools/qa/ingest.py /Volumes/<카드> --game tonewick`가 `judge.py --hardware --partial`로 모두 채점합니다. 실기의 NG 항목은
프로필이 아니라 발견입니다. 카드의 예전 `res-*` 로그는 먼저 지웁니다. 세이브 항목 셋(`env: cart`)은 R4에서 n/a입니다
(로더 뒤에 세이브 칩이 없음).

## 실기가 답해야 하는 것

| 질문 | 항목 / 보이는 곳 |
|---|---|
| GBATEK이 적은 캡처 버그 둘이 실제로 있는가? add 오버플로: 7000h + 7000h가 -2000h로 캡처됨. 둘 다 음수: -8000h. 둘 다 다른 사람이 확인한 적이 없습니다. 다를 때 어떻게 하는지는 아래 "실기 결과 판정" 절에 있습니다 | `cap-add-wrap`(`values[21]`), `cap-both-negative`(`values[22]`) |
| 채널 0을 캡처 소스(비트 1)로 하면 채널 0 혼자의 값이 나오는가? | `cap-src-channel`, `values[20]` |
| 캡처 길이 0은 1워드인가? | `cap-len-zero`, `values[18]` |
| duty 7 PSG 파형은 LOW에 머무는가? (DeSmuME는 예전에 HIGH라고 함) | `snd-duty7-low`, `values[2]` |
| hold 비트가 one-shot의 마지막 샘플을 유지하는가? (melonDS와 DeSmuME는 아님) | `snd-hold-level`, `values[11]` (원값 `values[13]`, `values[14]`) |
| 3워드 테이프는 busy를 유지하면서 조용한가? | `snd-short-busy`, `snd-short-silent`, `values[9]`, `values[10]` |
| 노이즈 채널이 X = 7FFFh, XOR 6000h를 정확히 따르는가? 볼륨 레지스터 127은 64의 정확히 두 배이고 분주 3은 정확히 1/16인가? | `snd-noise-lfsr`, `snd-vol127-full`, `snd-voldiv16` |
| ADPCM 루프가 index와 값을 재적재하는가? | `snd-adpcm-loop-reload`, `values[15]` |
| 어떤 기종인가(펌웨어 바이트 1Dh), 레지스터 4, 5, 0Ch가 문서대로 미러되는가? | `pm-reg-mirror`, `values[27..31]` |
| 24시간 모드에서 12~23시에 AM/PM 플래그가 서 있는가? | `rtc-ampm-24h`, `values[25]`, `values[26]` |
| 가장 최신 펌웨어 사용자 설정 사본이 로더가 027FFC80h에 남긴 사본인가? (R4 커널은 오래된 사본이나 아무것도 안 남길 수 있음: 로더를 적어 두세요) | `fw-user-newest`, `values[35]`, `values[36]` |
| WREN 뒤와 쓰기 뒤의 WEL, RDSR 비트 4~6 (카트리지 세이브 칩이 있는 콘솔이 필요하며 R4에서는 안 됨) | `save-wel-*`, `save-status-fixed`, `values[32..34]` |

## 단계

1. 빌드: `cd games/tonewick && ./build.sh -j8 release`. `build/release/rom/tonewick.nds`와
   `build/release/judge/tonewick-rehearsal.nds`의 `shasum -a 256`을 적어 둡니다.
2. 두 ROM을 R4 카드에 복사하고 `cleancart/tonewick/`를 만듭니다.
3. **리허설**: 오전에 한 번, 12시 이후에 한 번 돌립니다. 정오 전에는 AM/PM 플래그가 어느 쪽이든 0이라 `rtc-ampm-24h`가 n/a입니다. `tonewick-rehearsal.nds`를 시작하고 아무것도 만지지 말고 지역 네 곳이 DONE이 될 때까지 10초쯤 기다립니다.
   이어폰을 쓰거나 볼륨을 낮추세요. 실험하는 동안 스피커 앰프는 꺼지지만 콘솔에 소리를 내라고 시키는 실험입니다.
4. **자동 QA 실행**: 카드에 빈 `cleancart/tonewick/autoqa`를 둡니다(또는 타이틀에서 L+R+X+Y를 2초 누릅니다). 게임이 스스로
   크레딧까지(약 30초) 진행하고 `cleancart/tonewick/qa-*.log`를 씁니다.
   그 뒤 `python3 tools/qa/ingest.py /Volumes/<카드> --game tonewick`를 실행하고 에뮬레이터 실행과 견줍니다
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
5. 선택, 손으로: 테스트 메뉴(타이틀에서 Select)와 아침 점검 일지를 열고 보이는 것을 적어 둡니다.

## 실기 결과 판정

모든 항목에 해당하며, 특히 GBATEK에만 적힌 캡처 버그 둘(`cap-add-wrap`, `cap-both-negative`)에 중요합니다. 판정하기 전에 결합을
배제하세요. 두 버그는 볼륨 레지스터 127이 128으로 세어진다는 것(채널 경로의 단위 이득)에 기대므로 `snd-vol127-full`,
`snd-voldiv16`, `cap-src-channel`을 먼저 보세요. 거기서 실패하면 나머지는 그 때문입니다. 리허설은 세 번 이상, QA 실행은 한 번,
가진 콘솔마다(DS Lite와 구형 DS, 펌웨어 바이트 1Dh는 `values[27]`) 돌립니다.

| 결과 | 뜻 | 바뀌는 것 (파일, 실행을 기록하는 PR에서) |
|---|---|---|
| 값이 매번 범위 안 | GBATEK과 ROM이 맞음 | `expected.json`: `source`는 `hw:dslite-ntr`(기종), `level_basis`에 `; hw:games/tonewick/docs/HW-QA.md`(실행을 기록한 파일) 추가. 항목은 레벨 2 유지 |
| 범위를 벗어나지만 매번 같고 설명이 있음(다른 자료가 말하는 버그, 레지스터 미러, 로더) | 기대값이 틀렸고 항목은 문서화됨 | `expected.json`: 범위나 `expect`를 측정값으로 바꾸고 `source`는 `hw:dslite-ntr`, `level_basis`는 `hw:games/tonewick/docs/HW-QA.md`, 설명은 `title`에. 항목은 레벨 2 유지. 줄의 모델도 따라감(`junction.c`의 10·11번 줄은 GBATEK 버그를 모델로 씁니다) |
| 값이 실행마다 또는 콘솔마다 다르거나 아무도 설명하지 못함 | 문서화된 동작이 아님: 레벨 4 연구 | **한 PR에서 모두 합니다**(`role: relocate` 항목은 `mklevels.py --check`가 실패시키므로 커밋하지 않습니다). 항목은 `expected.json`에서 빠지고, 그 실험을 담은 데모 ROM이 `probes/`에 들어가며(에뮬레이터별 `known` 프로필, `probes/AGENTS.md` 참고), 지역 감사는 숫자를 원값으로 남겨도 되고, 10·11번 줄은 그대로 동작하며(모델은 GBATEK 버그, 귀의 답은 옆에 표시), 프로필의 `ng`에서 항목을 빼고, `mklevels.py`가 LEVELS와 README 칸을 다시 만듭니다 |

실행을 기록하는 사람(콘솔이 있는 관리자)이 PR을 엽니다. 프로필 `hw-ds-lite`는 콘솔에서 실패하는 항목을 나열합니다.

## 기록

- `docs/QA.md`와 `docs/QA-KO.md`(쌍둥이): ROM SHA-256, 콘솔과 카드, 실패한 항목, QA 로그 머리글.
- `expected.json`: 실기 값이 ROM의 기대와 맞는 항목은 `source`를 `hw:dslite-ntr`로 바꾸고 `level_basis`에 `hw:`를 더합니다.
  실기가 다르면 그 항목은 발견입니다. 값을 기록하고 ROM과 기대 중 어느 쪽이 틀렸는지 정합니다. `known`에 프로필 `hw-ds-lite`
  (실패한 항목)를 더합니다.
- `build/qa/` 아래는 절대 커밋하지 않습니다.

## 실행 1: QA 실행과 판정 스위트, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | `tonewick-hw-support.nds` v0.2.1 `a2320df3274725c46c47fb125708b207c086d3f6a7034a1284e2f822d5a16c3d`(453,996바이트, 릴리스와 같음). melonDS 1.1 soft도 같은 파일로 실행 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 21줄, 체크포인트 4개, build `91e6e87fe1c6`, `start=marker`로 시작. 스위트: QA 블록만(`suite block block=qa saved=1`. 스위트 데모가 없어 `suite end` 줄은 원래 없음). 판정: L1 9/9, L2 13/14(**판정 NG 1: `snd-short-silent`**). n/a 4: `rtc-ampm-24h`(오전, 본체 시계 미설정), `save-wel-set/-reset`, `save-status-fixed`(env: R4에서는 칩 접근 불가) |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | 없음. dist1-dist4 해시가 melonDS 1.1 soft와 같음(`compare.py`, 차이 0) |
| 랙 프레임 | 39 / 78 / 73 / 54 (dist1-dist4). melonDS soft 34 / 67 / 62 / 45 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모두 `uf0,of0`. 콘솔은 `rdl=63`(3D 없음), melonDS는 46 |

| 항목 | 실기 | melonDS 1.1 soft | 차이? |
|---|---|---|---|
| `snd-short-silent` (L2, GBATEK, 기대 0..2) | **16384** (NG) | 0 | 있음. 콘솔에서는 반복하는 짧은 샘플이 무음이 아님. 기대값이 틀렸음: 이제 16320..16448, source `hw:dslite-ntr`(아래 참조) |
| `snd-hold-level` | 100 (OK) | 0 (NG, 프로필) | melonDS 프로필 NG가 실제 에뮬레이터 차이임이 확인됨 |
| `cap-add-wrap` / `cap-both-negative` / `cap-src-channel` | 24576 / 0 / 40960 (OK) | 65535 / 16384 / 49152 (NG, 프로필) | 위와 같음 |
| `pm-reg-mirror` | 1 (OK) | 0 (NG, 프로필) | 위와 같음 |
| `snd-duty3-edges` | 128 | 127 | 둘 다 OK |
| `rtc-ampm-24h` | n/a (values[19] = 0) | OK 8/8 | 오후 시계가 필요. 실행 2에서 OK 8/8 |
| 나머지 판정 항목 | melonDS soft와 같음 | | 없음 |

NG 항목은 실행 1을 기록한 PR에서는 고치지 않았고, 아래 "실행에서 정한 기대값"에서 다룹니다.

## 실행 2: QA 실행과 판정 스위트, 2026-11-17 (본체 시계 2026-01-15 12:07)

실행 1과 같은 콘솔·카드·ROM(`tonewick-hw-support.nds`, build `91e6e87fe1c6`)으로, 이번에는 본체 시계가 정오를 지난 뒤(2026-01-15 12:07, 파일 이름 `res-20260115-*`, 실행 1은 `res-20020115-*`) 다시 돌렸습니다. 점검표가 요구한 오후 실행입니다.

| 항목 | 값 |
|---|---|
| `judge.py --hardware` | QA 블록: 실행 1과 같은 판정에 `rtc-ampm-24h`가 OK로 채점됨(`values[19]` = 8, 원시 읽기 8번 모두 일치). `snd-short-silent`는 다시 16384(기대값을 바꾸기 전에는 NG). n/a 3(env: `save-wel-set`, `save-wel-reset`, `save-status-fixed`. R4에서는 칩에 접근 못함) |
| 실행 1과의 차이 | `values[19]`만 다름(0 -> 8). 16384를 포함해 블록의 나머지 워드는 모두 같음 |

## 실행에서 정한 기대값

| 항목 | 변경 | 이유 |
|---|---|---|
| `snd-short-silent` | 범위 `16320..16448`, source `hw:dslite-ntr`. 제목: 반복하는 3워드 샘플은 콘솔에서 테이프 레벨을 유지함(GBATEK의 "소리 없음"은 0 레벨이 아님) | 두 실행 모두 16384(캡처한 피크)를 읽었고 GBATEK의 "소리 없음"에서 짐작한 2 이하가 아님. 두 실행에서 값이 같습니다. melonDS는 0, DeSmuME는 16384를 읽음 |
| `cap-add-wrap`, `cap-both-negative`, `cap-src-channel`, `snd-hold-level`, `pm-reg-mirror`, `rtc-ampm-24h` | `level_basis`에 `hw:games/tonewick/docs/HW-QA.md` 추가 | 콘솔이 GBATEK과 같음(캡처 버그 둘이 실제로 있음). `rtc-ampm-24h`는 실행 2(오후)로 확인 |
| 프로필 | `snd-short-silent`를 melonDS 세 프로필과 `-save-chip`의 `ng`에 추가. DeSmuME는 뺌(이제 16384를 읽음) | melonDS는 반복하는 3워드 샘플을 무음으로 둠 |
