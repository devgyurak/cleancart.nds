# Ledgerine 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `ledgerine.nds` 대신 `build/release/rom/ledgerine-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

**판정 스위트(QA 판정 스위트 PR 이후):** QA 실행(`autoqa`)이 이제 스위트로 끝납니다. `RESULT PASS` 뒤에 QA 블록과 `press-min`를 `res-*-ledgerine-*.log`로 쓰고 진행 파일 `suite-<stamp>.log`도 남깁니다. hw-support ROM 한 번 실행으로 충분하고, **전원은 스위트가 끝난 뒤에** 끕니다(`suite-<stamp>.log`에 `suite end`). 아래의 리허설·데모 ROM은 항목 하나를 다시 돌릴 때만 씁니다(Wood 커널에서는 SD 카드를 받지 못함). `tools/qa/ingest.py`가 파일마다 채점합니다.

**2026-11-17 DS Lite 실행 1·2(이 문서 끝에 기록):** QA 실행은 두 번 모두 PASS입니다. 판정 NG 항목 하나(`ipc-empty-cleared`)는 우리 기대값에 대한 발견이었고, 이후 콘솔 값으로 기대값을 정했습니다("실행에서 정한 기대값" 참조).

실행 1 전에는 실기에서 잰 값이 없었습니다. 이 문서는 첫 측정을 기록하고, 에뮬레이터나 GBATEK 해석에만 근거를 둔 항목을
확정하기 위한 점검표입니다. 콘솔은 사람이 다루며, 아래는 할 일과 적어 둘 것입니다.

**자동 경로(스킬 `nds-hw-qa`):** 카드에 빈 폴더 `cleancart/ledgerine/`를 만들어 두면 리허설과 `press-min` ROM이 결과 블록을 그곳에 씁니다
(`res-*-ledgerine-rehearsal.log`, `res-*-ledgerine-press-min.log`). 빈 파일 `cleancart/ledgerine/autoqa`는 버튼을 누르지 않고도 QA 실행을
시작합니다. `python3 tools/qa/ingest.py /Volumes/<카드> --game ledgerine`이 `judge.py --hardware --partial`로 모두 채점합니다. 실기에서 NG인
항목은 프로필이 아니라 발견입니다. 카드의 예전 `res-*` 로그는 먼저 지웁니다.

## 실기가 답해야 하는 것

| 질문 | 항목 / 보이는 곳 |
|---|---|
| 빈 FIFO 읽기가 마지막 수신 워드(GBATEK)를 돌려주는가, 다음 칸(melonDS)인가, 0(DeSmuME)인가? | `ipc-empty-last`, 리허설 `values[41]`(읽은 워드) |
| 꺼진 FIFO가 쓰기를 에러 없이 무시하고, 읽으면 가장 오래된 워드를 보여 주는가? (DeSmuME는 아니라고 함) | `ipc-disabled`, `values[2]` 비트 5·6 |
| 긴 DMA 중에도 CPU가 ITCM 코드를 계속 실행하는가? (melonDS는 멈춤. 두 타이머 값의 비율이 직렬이면 1.5, 계속 돌면 약 1.0) | `dma-itcm-runs`, `values[30]`·`values[31]`(DMA만, DMA와 루프, 타이머 틱) |
| ARM7 CpuSet이 BIOS 영역의 소스를 거부하는가? (melonDS의 대체 BIOS는 복사함) | `bios-arm7-guard`, `values[6]` 비트 4 |
| 32비트 나눗셈기가 0으로 나눌 때 상위 결과 워드에 무엇을 돌려주고, DIV0 플래그가 64비트 분모 전체를 따르는가? | `math-div0`, `math-div0-flag-upper`, `values[8..11]` |
| 타이머가 도는 중에 쓴 reload가 오버플로까지 기다리는가? 전송 뒤 SAD·DAD·CNT_L이 초깃값을 유지하는가? (DeSmuME는 둘 다 아니라고 함) | `timer-reload-deferred`, `dma-regs-unchanged` |
| 모드 2의 `-2^63 / -1`이 `8000000000000000h`이고 DIV0가 없는가? (`ledgerine-press-min.nds`를 실행. DeSmuME는 여기서 멈춤) | `math-overflow-min64` |
| ARM9의 바이트 쓰기가 LCDC VRAM에서 정말 무시되는가? 게임이 크레딧까지 끝까지 도는가(자동 QA 실행)? | `vram-byte-write`, 게임 ROM, 숨은 QA 모드 |

## 순서

1. 빌드: `cd games/ledgerine && ./build.sh -j8 release`. `build/release/rom/ledgerine.nds`와 `build/release/judge/ledgerine-rehearsal.nds`,
   `ledgerine-press-min.nds`의 `shasum -a 256`을 적어 둡니다.
2. 세 ROM을 R4 카드에 복사하고 `cleancart/ledgerine/`를 만듭니다.
3. **리허설**: `ledgerine-rehearsal.nds`를 실행하고, 손대지 않은 채 여섯 방이 DONE으로 바뀔 때까지 약 5초 기다립니다.
4. **press-min**: `ledgerine-press-min.nds`를 실행합니다(화면에 그리는 것은 없습니다. 2초 기다림).
5. **자동 QA 실행**: 카드에 빈 `cleancart/ledgerine/autoqa`를 두거나(또는 타이틀에서 L+R+X+Y를 2초 누름) 게임이 크레딧까지 스스로 진행합니다(약 30초).
   `cleancart/ledgerine/qa-*.log`가 생깁니다. 2번 방은 레버를 당길 때마다 약 1초 동안 입력과 소리가 멈추며, 그 밖에 이상한 것은 없어야 합니다.
   이어서 `python3 tools/qa/ingest.py /Volumes/<카드> --game ledgerine`을 실행하고 에뮬레이터 실행과 비교합니다
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).

## 기록

- `docs/QA.md`와 `docs/QA-KO.md`(쌍): ROM SHA-256, 콘솔과 카드, 실패한 항목, QA 로그 머리글.
- `expected.json`: 실기 값이 ROM의 기대와 같은 항목은 `source`를 `hw:dslite-ntr`로 바꿉니다. 실기가 다르면 그 항목은 발견입니다. 값을 적고 ROM과
  기대값 중 어느 쪽이 틀렸는지 정합니다(BIOS 항목은 BIOS에 따라서만 다를 수 있으니 콘솔의 BIOS를 적어 둡니다). 실패한 항목으로 프로필
  `hw-ds-lite`를 `known`에 추가합니다.
- `build/qa/` 아래 것은 커밋하지 않습니다.

## 실행 1: QA 실행과 판정 스위트, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | `ledgerine-hw-support.nds` v0.2.1 `8e2fbdfad675ebfa14246c44d99eab59aa9eacf88c2d52d6fb9b2ca25c7de2dc`(455,020바이트, 릴리스와 같음). melonDS 1.1 soft도 같은 파일로 실행 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 27줄, 체크포인트 6개, build `8821063cd048`, `start=marker`로 시작. 스위트: QA 블록과 `press-min` 데모 `saved=1`. `suite end demos=1 saved=1`, probe 12개 모두 `ok=1`, `suite done` 줄 없음(참고 참조). 판정: QA 블록 L1 11/11, L2 28/29(**판정 NG 1: `ipc-empty-cleared`**). `press-min` L2 1/1. 41개 중 40개 OK, n/a 없음 |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | room2(room1은 같음). room2-room6 해시가 melonDS 1.1 soft와 다름 |
| 랙 프레임 | 3 / 11 / 54 / 63 / 13 / 2 (room1-room6). melonDS soft 0 / 10 / 53 / 62 / 11 / 1 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모두 `uf0,of0`. 콘솔은 `rdl=63`(이 게임은 3D를 쓰지 않음), melonDS는 46 |

| 항목 | 실기 | melonDS 1.1 soft | 차이? |
|---|---|---|---|
| `ipc-empty-cleared` (L2, GBATEK, mask 0x8) | **0x0** (NG) | 0x8 | 있음. 기대값이 틀렸음: 이제 mask 0xC / expect 0x4, source `hw:dslite-ntr`(아래 참조) |
| `ipc-empty-last` | 0x3 (OK) | 0x2 (NG, 프로필) | melonDS 프로필 NG가 실제 에뮬레이터 차이임이 확인됨 |
| `bios-arm7-guard` | 0x10 (OK) | 0x0 (NG, 프로필) | 위와 같음 |
| `dma-itcm-runs` | 0x20 (OK) | 0x0 (NG, 프로필) | 위와 같음 |
| QA 블록 values[2], [4], [6], [30], [31] (판정 안 함) | 247, 63, 31, 590170, 590170 | 254, 31, 15, 589864, 884598 | 있음, 판정 안 함 |
| 나머지 판정 항목 | melonDS soft와 같음 | | 없음 |

참고: 스위트는 이제 마지막 리셋 전에 `suite end`를 쓰고(#186) 그 줄은 카드에 있습니다. 리셋 뒤의 probe 12줄도 `wait60`까지 모두 있습니다. 실행의 마지막 쓰기인 `suite done` 줄만 빠졌습니다(melonDS는 씀). 따라서 줄을 잃게 만드는 것은 리셋이 아니라, 카드에 마지막으로 쓴 줄이 사라지는 것입니다. 아직 열려 있습니다.

NG 항목은 실행 1을 기록한 PR에서는 고치지 않았고, 아래 "실행에서 정한 기대값"에서 다룹니다.

## 실행 2: QA 실행과 판정 스위트, 2026-11-17 (본체 시계 2026-01-15 12:04)

실행 1과 같은 콘솔·카드·ROM(`ledgerine-hw-support.nds`, build `8821063cd048`)으로 다시 돌렸습니다. 이번에는 본체 시계가 2026-01-15 12:04를 가리켜 파일 이름이 `res-20260115-*`입니다(실행 1: `res-20020115-*`).

| 항목 | 값 |
|---|---|
| `judge.py --hardware` | QA 블록과 `press-min`: 실행 1과 같은 판정(기대값을 바꾸기 전: 판정 NG 1, `ipc-empty-cleared`, `values[2]` = 0xF7 그대로. 바꾼 뒤: 두 실행의 로그 4개 모두 QA 블록 40개 중 40개, `press-min` 1개 중 1개 OK, NG 0) |
| 실행 1과의 차이 | 판정 값은 같음. 판정하지 않는 값: `values[19]`, `[20]` 58437 -> 58438, `[21]` 16397 -> 16398, 랙 프레임 합 146 -> 145. `press-min` 블록 프레임 1382 -> 1383 |

## 실행에서 정한 기대값

| 항목 | 변경 | 이유 |
|---|---|---|
| `ipc-empty-cleared` | `mask 0xC`, `expect 0x4`, source `hw:dslite-ntr`. 제목: 상대가 보내기 FIFO를 비운 뒤의 빈 읽기는 0을 돌려주지 않음 | GBATEK은 0이라고 하지만 두 실행 모두 `values[2]` = 0xF7(비트 3 "0을 돌려줌"은 꺼짐, 비트 2 오류 확인은 켜짐). 이 실행들에서는 그 읽기의 원시 워드를 기록하지 않았고 0이 아니라는 것만 압니다. v0.2.2부터는 `values[44]`(그 직후의 IPCFIFOCNT는 `values[45]`)에 기록하며, 다음 실행에서 읽습니다. melonDS 1.1 soft와 DeSmuME는 둘 다 `values[44]` = 0, `values[45]` = 0xC101(수신 FIFO 비어 있음, 오류 비트 켜짐)을 읽습니다. 마스크가 비트 2도 검사하므로 확인 동작이 깨져도 항목이 실패합니다 |
| `ipc-empty-last`, `dma-itcm-runs`, `bios-arm7-guard` | `level_basis`에 `hw:games/ledgerine/docs/HW-QA.md` 추가 | 콘솔은 GBATEK과 같고, melonDS(와 DeSmuME)는 다름 |
| 프로필 | `ipc-empty-cleared`를 네 프로필(melonDS soft / GL classic / GL compute, DeSmuME) 모두 `ng`에 추가 | 에뮬레이터는 모두 0을 돌려주고 콘솔은 아님 |
