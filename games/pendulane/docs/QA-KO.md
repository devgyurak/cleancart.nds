# Pendulane QA

[English](QA.md)

melonDS 1.1 점검표입니다 (macOS 키는 저장소 `AGENTS.md`).

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 글자만 있는 임시 타이틀(`TODO(title)`), 아래 화면에 라이선스·ARM7·세이브 줄; Start는 벤치 장부, X는 크레딧 | headless 실행만 |
| 벤치 장부 | 수학 엣지 네 개(ARM9 사이클)와 책 값 36 / 68 / 68 / 26, 차이 세 개와 10 / 32 / 0, "MATH RESULTS RIGHT", B로 뒤로 | headless 실행만 |
| 크레딧 | 스크롤 크레딧, X는 라이선스 뷰어 | 아직 확인 안 함 |
| rehearsal demo | `pendulane-rehearsal.nds`는 바로 벤치로 부팅; 결과 블록 id `0x48524450` (`PDRH`); README의 방법대로 `judge.py --profile`로 채점 | ✅ 2026-11-17: 프로파일 4개(DeSmuME py-desmume 0.0.9, melonDS 1.1 software / GL Classic / GL Compute) 모두 판정 항목 5개 중 5개 OK. 차이: DeSmuME 10 / 32 / 0, melonDS 9 / 32 / 0 (책 값은 10 / 32 / 0, 창은 ±2). 실기 값은 아직 없음 |

실기: 아직 돌리지 않았습니다. [HW-QA-KO.md](HW-QA-KO.md) 참고.

## 자동 QA 실행

릴리스 ROM(`pendulane.nds`)의 숨은 모드: 타이틀에서 L, R을 누른 뒤 X, Y를 눌러 네 키를 120프레임 누릅니다. 정상 흐름(타이틀 → 크레딧 → 타이틀)을
진행하고 벤치를 한 번 돌린 뒤 `RESULT PASS`로 끝납니다. 로그는 SD 카드(`cleancart/pendulane/qa-<날짜>-<시각>.log`). Headless:
`SDDIR=out/pd/x tools/melonds/mds.sh run games/pendulane/pendulane.nds 2400 out/pd/x "300:L:140,300:R:140,300:X:140,300:Y:140"` (먼저
`out/pd/x/cleancart/pendulane/`를 만드세요). 2026-11-17 melonDS 1.1(software)에서 확인: 프레임 1137에서 `RESULT PASS`, 체크포인트 2개,
두 번 돌린 `tools/qa/compare.py` 차이 0, `CPU9 cpsr` 모드 0x1f; `QARN` 블록을 melonDS 1.1 software / GL Classic / GL Compute / DeSmuME에서
`judge.py`로 채점해 모두 OK.

## 키 없이 QA 시작 (SD 마커)

SD 카드에 빈 파일 `cleancart/pendulane/autoqa`를 두세요 (폴더가 있어야 합니다). 부팅 후 첫 타이틀 화면에서 같은 QA 실행이 시작되고, 시작 전에
파일 이름이 `autoqa.used-<날짜>-<시각>`으로 바뀝니다.
