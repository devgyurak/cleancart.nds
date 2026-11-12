# Probes 실기 QA (DS Lite + R4)

[English](HW-QA.md)

probe(L4)는 기준값이 없는 연구용 ROM이고, 실기 값이 그 목적입니다. 아직 콘솔에서 실행한 적이 없습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 probe 항목에 새 기준값이 생긴 것입니다(아래 제안대로 기록).

## 카드에 넣을 것

- ROM: `build/release/demos/*.nds` (every probe ROM)
- 빈 폴더 `cleancart/probes/` (없으면 ROM이 아무것도 쓰지 않음)

## 실행

1. probe ROM을 하나씩 켜고 `RESULT SAVED`가 보이면 리셋합니다.
2. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game probes`.

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

`probes/tools/judge.py --hardware --partial --observed`로 `known.hardware-dslite-r4.observed`를 제안해 사용자 확인을 받은 뒤 별도 PR로 `probes/expected.json`에 넣습니다. `level-judge`가 L2/L3로 내릴 수 있는 probe를 `hw:probes/docs/HW-QA.md` 근거와 함께 제안합니다.
