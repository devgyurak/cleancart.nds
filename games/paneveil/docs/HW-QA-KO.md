# Paneveil 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `paneveil.nds` 대신 `build/release/rom/paneveil-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Paneveil(L1, 3D 지오메트리와 텍스처)은 리허설과 자동 QA 실행으로 판정합니다. 2026-11-17 DS Lite에서 QA 실행이 통과했습니다(아래 실행 1). **판정 항목 31개 중 31개 OK**이고 모든 체크포인트가 melonDS와 같습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `paneveil.nds`의 hw-support 빌드 하나만. QA 실행이 모든 것을 판정합니다. `RESULT PASS` 뒤에 QA 블록(리허설 항목인 가마 점검과 라이트박스를 담음)을 `res-*-paneveil-qa.log`로 씁니다. Paneveil에는 판정 데모가 없어서 진행 파일 `suite-<stamp>.log`에는 `suite block` 줄만 있습니다(`suite end` 줄은 원래 없음). 리허설 ROM은 게임 없이 항목만 다시 돌릴 때 씁니다(Wood 커널에서는 SD 카드를 받지 못함).
- 빈 폴더 `cleancart/paneveil/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/paneveil/autoqa`
- 복사본 확인: 카드의 파일이 릴리스와 크기·SHA-256이 같아야 합니다(실행 1의 복사본은 잘려 있었음, 아래 참조).

## 실행

1. 게임 QA: `paneveil-hw-support.nds`를 켜고 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작). 30개 창을 모두 플레이하고 크레딧을 거쳐 가마 점검과 라이트박스를 한 번 돌립니다(약 1분).
2. `suite-<stamp>.log`에 `suite block block=qa saved=1`이 나오면 끝입니다. 그 뒤에 전원을 끕니다.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game paneveil`.

## 하드웨어의 답 (실행 1)

| 질문 | 어디에 나오는가 | 실행 1 |
|---|---|---|
| 행렬 스택 레벨(GXSTAT 8-12, 13), N = 2 pop, store와 restore, 투영 스택이 GBATEK대로인가? | 값 3~11 (기대값 3 0 1 0 2 0 1 0 0) | 예, `3 0 1 0 2 0 1 0 0` |
| load와 multiply 뒤에 CLIPMTX_RESULT와 VECMTX_RESULT가 모델과 같은가(packed GXFIFO와 직접 포트 모두)? | 값 12~15 (모두 0) | 예, 모두 0 |
| 버텍스 포맷 6종, 컬링, 앞/뒷면 렌더 비트, alpha, 와이어프레임, 깊이 시험, 텍스처 repeat·flip·clamp·색 0·PLTT_BASE·좌표 모드 1이 그림에서 맞는가? | 값 16~30과 flags(캡처 비트 8-12가 켜짐) | 예, 모든 항목 OK, flags `0x1f00` |
| 와이어프레임(alpha 0)이 실기에서 그려지는가? (DeSmuME와 melonDS GL Classic은 그리지 않음) | 값 21(`poly-wire-edges`) | 예, 4(melonDS soft·Compute와 같음) |
| 게임이 크레딧까지 끝까지 가고(자동 QA 실행) 세이브 칩이 제대로 동작하는가? | 게임 ROM, 숨은 QA 모드 | 크레딧까지 진행, `RESULT PASS`. 칩 세이브는 R4에서 검증 안 됨(`chip_ok=0`, 세이브는 SD 카드로 미러링) |

## 기록 (실행마다 한 블록)

### 실행 1: QA 실행, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | 릴리스 `paneveil-hw-support.nds` v0.2.0 `1905cc7932ea4ef754fd7cc55eeccf71775a2fd25396190cf6f1d573e9e83b84`(450,924바이트, melonDS도 이 파일로 실행). 카드의 복사본은 **393,216바이트에서 잘려** 있었음(`0d9a7957a3f35ca63a15deb7575d1b5d66a06bdd9b2044b9e64c924d418ab487`). 남은 바이트는 릴리스와 같음. 잘린 곳은 마지막 NitroFS 파일 `licenses/picolibc-COPYING.picolibc` 안이고 코드, `probe.bin`, 다른 라이선스 텍스트는 온전하므로 판정 실행에는 영향 없음 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 33줄, 체크포인트 7개, build `5778bba6ff09`, `start=marker`로 시작. 스위트: `block=qa saved=1`. QA 블록 판정: **L1 31/31 OK**(target 30/30, prerequisite 1/1), n/a 없음 |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | 없음. ch1-ch5, judge, credits 해시가 melonDS 1.1 soft, GL Classic, GL Compute와 같음(`compare.py`, 차이 0) |
| 랙 프레임 | 3 / 2 / 2 / 2 / 2 / 1 / 0 (ch1-ch5, judge, credits). melonDS는 모든 체크포인트에서 0 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 3D 체크포인트마다 `uf0,of0 rdl=46`, melonDS와 같음 |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| `poly-wire-edges` | 4 | 4 | 0(NG, 프로필) | 4 | NG(프로필, 실행 안 함) | 없음. 콘솔은 GBATEK대로 와이어프레임을 그림 |
| `poly-alpha-half` | 16 | 16 | 16 | 16 | 실행 안 함 | 없음 |
| 나머지 판정 항목 | melonDS soft와 같음 | | | | | 없음 |

참고:
- 결과 블록은 QA 랙 합계(values[53]: 12 대 0)를 빼면 melonDS soft와 단어 단위로 같습니다.

## 후속 조치

실기 NG는 모두 표로 사용자에게 보고하고, 여기서 `expected.json`을 고치지 않습니다. `level-judge`에게 레벨 제안을 받고(근거 `hw:games/paneveil/docs/HW-QA.md`), 기대값·레벨 변경은 별도 PR로 합니다.
