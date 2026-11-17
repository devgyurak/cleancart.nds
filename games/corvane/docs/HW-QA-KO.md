# Corvane 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `corvane.nds` 대신 `build/release/rom/corvane-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Corvane(L1, 그래픽·시스템)은 데모와 자동 QA 실행으로 판정합니다. 아직 콘솔에서 실행한 적이 없습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `build/release/rom/corvane.nds`, `build/release/demos/*.nds`
- 빈 폴더 `cleancart/corvane/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/corvane/autoqa`

## 실행

1. 게임 QA: `corvane.nds`를 켜고 크레딧까지 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작).
2. 데모: ROM을 하나씩 켜고 `RESULT SAVED`가 보이면 리셋합니다.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game corvane`. (`shot-*.bmp`도 함께 복사합니다).
4. shot을 melonDS와 비교합니다(`docs/QA-KO.md`의 "자동 QA 실행" 참고): `python3 tools/qa/compare.py --info <melonDS QA 로그> <카드 QA 로그>`가 shot 이름별 CRC를 나란히 보여 주면, 콘솔의 `shot-*-st3-600.bmp`와 같은 이름의 melonDS BMP를 나란히 열어 봅니다(지금은 스테이지 3만 찍습니다).

## 기록 (실행마다 한 블록)

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | |
| ROM SHA-256 | |
| `ingest.py` / `judge.py --hardware` 요약 | |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | |
| melonDS와 처음 갈리는 체크포인트 | |
| 랙 프레임 | |
| shot `st3-600`: 화면, crc, melonDS와 같은가? | |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| | | | | | | |

## 후속 조치

실기 NG는 모두 표로 사용자에게 보고하고, 여기서 `expected.json`을 고치지 않습니다. `level-judge`에게 레벨 제안을 받고(근거 `hw:games/corvane/docs/HW-QA.md`), 기대값·레벨 변경은 별도 PR로 합니다.
