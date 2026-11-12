# Hushwell 실기 QA (DS Lite + R4)

[English](HW-QA.md)

Hushwell(L1, 사운드·I/O)은 데모와 자동 QA 실행으로 판정합니다. 아직 콘솔에서 실행한 적이 없습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `build/release/rom/hushwell.nds`, `build/release/demos/*.nds`
- 빈 폴더 `cleancart/hushwell/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/hushwell/autoqa`

## 실행

1. 게임 QA: `hushwell.nds`를 켜고 크레딧까지 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작).
2. 데모·리허설: ROM을 하나씩 켜고 `RESULT SAVED`가 보이면(리허설은 결과 화면이 뜰 때 조용히 기록) 리셋합니다.
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
