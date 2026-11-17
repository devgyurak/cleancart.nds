# Corvane 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `corvane.nds` 대신 `build/release/rom/corvane-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Corvane(L1, 그래픽·시스템)은 데모와 자동 QA 실행으로 판정합니다. 2026-11-17 DS Lite에서 자동 QA 실행이 통과했습니다(아래 실행 1). 데모는 아직 콘솔에서 실행하지 않았습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `build/release/rom/corvane.nds`, `build/release/demos/*.nds`
- 빈 폴더 `cleancart/corvane/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/corvane/autoqa`(같은 이름의 폴더도 됩니다. ROM은 이름만 확인하고 바꾸므로, 휴대폰으로 카드를 준비할 때 편합니다)

## 실행

1. 게임 QA: `corvane.nds`를 켜고 크레딧까지 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작).
2. 데모: ROM을 하나씩 켜고 `RESULT SAVED`가 보이면 리셋합니다.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game corvane`.

## 기록 (실행마다 한 블록)

### 실행 1: 자동 QA, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 로그 날짜는 2002-01-15 |
| ROM SHA-256 | `corvane-hw-support.nds` v0.3.2 `b8fbbca47b49d445d076cfc2f139cbbcd43e478868ac6487ca7f8a4ccbfa587e`(카드의 파일이 릴리스와 같음). melonDS 비교에는 `corvane.nds` v0.3.2 `26da3829e12bff4e633d9995b3f673070dbe8cf92171555b195f9c015317846c`도 사용 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 실행만 했고 데모는 없어 채점할 블록 없음. `parse_log.py`: PASS, 45줄, 체크포인트 5개, build `6f32321674c1`, sd=1. 마지막 줄 `RESULT PASS reason=credits lines=45 dropped=0`, `start=marker`로 시작 |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | 없음. melonDS 1.1 soft에서 같은 ROM과 `corvane.nds`로 `compare.py`: 체크포인트 5개, 차이 0 |
| 랙 프레임 | 60 / 182 / 213 / 181 / 200 (st1–st5). melonDS 58 / 118 / 134 / 119 / 129 |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| 체크포인트 해시 st1–st5 | `5ef6d723 c2bafc29 e8c0a1c2 57d27b7b 37db96a8` | 같음 | 실행 안 함 | 실행 안 함 | 실행 안 함 | 없음 |
| 스테이지별 게임 프레임 | 2429 / 2585 / 2309 / 2760 / 1216 | 같음 | | | | 없음 |
| 스테이지별 VBlank 수(스테이지 진입~결과) | 2533 / 2693 / 2437 / 2871 / 1301 | 2471 / 2627 / 2369 / 2801 / 1271 | | | | 있음(참고) |
| 칩 세이브 쓰기 | 6회 모두 `chip_ok=0`, SD 사본 `sd_match=1` | 칩 없음(`save_detected=-1`) | | | | R4에서 예상된 결과 |
| `cart_ready` | 0 | 1 | | | | 플래시카트에서 예상된 결과 |

발견(참고 정보, 판정 항목 아님):
- **Wood R4는 `####` ROM에만 SD를 줍니다.** 게임 고유 코드 `CRVN`이면 커널이 세이브를 에뮬레이션하고 DLDI를 붙이지 않아, 표시 파일도 로그도 없었습니다. `corvane-io-qalog`는 `NO SD CARD (RAM ONLY)`였고, 같은 ROM을 `####`로 바꾸자 `SD LOG: OPEN`이 나왔습니다. 이제 릴리스에 `<게임>-hw-support.nds`가 들어 있습니다(#151).
- **콘솔이 melonDS보다 약 2.5% 느립니다.** 게임 로직의 프레임 수는 같지만, 콘솔에서는 스테이지마다 60–70개 프레임이 더 VBlank를 2개씩 씁니다. melonDS가 프레임 처리를 콘솔보다 빠르게 합니다. 원인(CPU인지 3D인지)은 아직 모릅니다.
- **R4의 칩 세이브:** 카드가 EEPROM처럼 응답하지만 쓰기 검증이 한 번도 맞지 않습니다(`chip_ok=0`). 모든 세이브는 SD 사본에 남았습니다. 이 카드에서 칩 세이브는 검증되지 않습니다.

## 후속 조치

실기 NG는 모두 표로 사용자에게 보고하고, 여기서 `expected.json`을 고치지 않습니다. `level-judge`에게 레벨 제안을 받고(근거 `hw:games/corvane/docs/HW-QA.md`), 기대값·레벨 변경은 별도 PR로 합니다.
