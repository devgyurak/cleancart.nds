# Pendulane 실기 QA (DS Lite + R4)

[English](HW-QA.md)

Pendulane은 아직 콘솔에서 아무것도 재지 않았습니다. 이 문서는 실행 점검표로, 첫 값을 기록하고 `expected.json`의 `unconfirmed` 항목을 승격할 수 있게 합니다.
콘솔은 사람이 조작합니다 (스킬 `nds-hw-qa`): 카드에 빈 폴더 `cleancart/pendulane/`를 만들면 리허설이 결과 블록을 거기에 씁니다.
`python3 tools/qa/ingest.py /Volumes/<card> --game pendulane`가 `judge.py --hardware --partial`로 채점합니다. 실기 NG는 프로파일이 아니라 발견입니다.

## 실기가 답해야 할 것

| 질문 | 어디에 나오나 |
|---|---|
| DIV 모드 0은 버스 클럭 18, 모드 1·2는 34, SQRT는 13이 걸리나 (GBATEK "DS Maths")? | 리허설 값 3~5 (반사이클 10, 32, 0; 창 ±2) |
| ARM9 쓰기 버퍼가 엣지를 옮기나 (콘솔에서는 고정 경로가 같지 않을 수 있음)? | 세 차이가 창 안에 남는지; 값 24~27의 원시 엣지는 참고용 |

## 기록

| 날짜 | 콘솔 | 카드 | 결과 |
|---|---|---|---|
| — | — | — | 아직 안 돌림 |
