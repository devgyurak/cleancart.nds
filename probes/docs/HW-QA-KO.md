# Probes 실기 QA (DS Lite + R4)

[English](HW-QA.md)

probe(L4)는 기준값이 없는 연구용 ROM이고, 실기 값이 그 목적입니다. 아직 콘솔에서 실행한 적이 없습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 probe 항목에 새 기준값이 생긴 것입니다(아래 제안대로 기록).

## 카드에 넣을 것

- ROM: **hw-support 사본** `build/release/rom/probes-*-hw-support.nds` 19개(릴리스 zip `probes-vX.Y.Z.zip`에 들어 있음).
  probe ROM의 게임 코드는 `PRBS`인데(melonDS 세이브 칩 때문), R4 Plus(Wood) 커널은 코드가 `####`인 ROM에만 SD를 붙입니다.
  원래 ROM으로는 `RESULT SAVED`가 나타나지 않고 파일도 남지 않습니다. 사본은 원래 ROM과 게임 코드 4바이트와 헤더 CRC만
  다릅니다(`tools/hwsupport.py`). `demos/`의 원래 ROM은 카드에 넣지 않습니다.
- 빈 폴더 `cleancart/probes/` (없으면 ROM이 아무것도 쓰지 않음)

## 실행

1. ROM 19개를 하나씩, 순서는 상관없이 켭니다(시간이 모자라면 Buoyane에서 옮겨 온 `l4-3d-zeq-slope`와 `l4-3d-fog-dot`을 먼저).
   체크포인트 아래에 `RESULT SAVED`가 보이면 리셋합니다. 모든 ROM은 체크포인트(120프레임, `l4-vcount-write`와 `l4-tex-late`는 더 일찍)에서
   멈추고 화면을 유지하므로 버튼을 누를 일이 없습니다.
2. 19개에 15~20분쯤 걸립니다(ROM마다 몇 초, 나머지는 커널 메뉴에서 다음 파일을 고르는 시간).
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game probes`.

## 기록 (실행마다 한 블록)

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | |
| ROM SHA-256 | |
| `ingest.py` / `judge.py --hardware` 요약 | |
| 랙 프레임 | |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| | | | | | | |

## 후속 조치

`probes/tools/judge.py --hardware --partial --observed`로 `known.hardware-dslite-r4.observed`를 제안해 사용자 확인을 받은 뒤 별도 PR로 `probes/expected.json`에 넣습니다. `level-judge`가 L2/L3로 내릴 수 있는 probe를 `hw:probes/docs/HW-QA.md` 근거와 함께 제안합니다.
