# Hushwell 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `hushwell.nds` 대신 `build/release/rom/hushwell-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Hushwell(L1, 사운드·I/O)은 데모와 자동 QA 실행으로 판정합니다. 아직 콘솔에서 실행한 적이 없습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `hushwell.nds`의 hw-support 빌드 하나만. QA 실행이 모든 것을 판정합니다: 크레딧 뒤에 채점 데모 16개를 돌려 결과 파일을 하나씩 씁니다. `io-save-flash` / `io-save-fram`은 돌리지 않습니다(다른 세이브 칩). 이 항목들과 모든 `env: cart` 항목은 실기에서 n/a입니다(2026-11-17 결정, R4는 칩 세이브를 검증하지 못함). 단독 데모 ROM은 파일에 `saved=0`이 보인 데모 하나를 다시 돌릴 때만 씁니다.
- 빈 폴더 `cleancart/hushwell/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/hushwell/autoqa`

## 실행

1. 게임 QA: `hushwell.nds`를 켜고 크레딧까지 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작).
2. 스위트가 끝날 때까지 기다립니다(로그 끝이 `suite end demos=16 saved=16`). 데모를 따로 돌릴 필요는 없습니다.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game hushwell`.

## 기록 (실행마다 한 블록)

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | |
| ROM SHA-256 | |
| `ingest.py` / `judge.py --hardware` 요약 | |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | |
| melonDS와 처음 갈리는 체크포인트 | |
| 랙 프레임 | |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| | | | | | | |

## 후속 조치

실기 NG는 모두 표로 사용자에게 보고하고, 여기서 `expected.json`을 고치지 않습니다. `level-judge`에게 레벨 제안을 받고(근거 `hw:games/hushwell/docs/HW-QA.md`), 기대값·레벨 변경은 별도 PR로 합니다.
