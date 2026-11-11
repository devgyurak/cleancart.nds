# Elevane QA

[English](QA.md)

melonDS 1.1용 점검표입니다 (macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인할 것 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 "ELEVANE"와 한 줄 소개, 그 아래 X 안내. 아래 화면에 라이선스, ARM7, 세이브 줄. X → 크레딧 | 헤드리스 실행은 아래 참고. melonDS 손 조작은 아직 |
| 타이틀 → X (크레딧) | 스크롤 크레딧 (libnds, 소스 URL이 있는 BlocksDS crts MPL-2.0, picolibc, libgcc). X로 라이선스 뷰어(`licenses/`의 모든 문서, X 다음 파일, B 뒤로). NitroFS가 없으면 "CART FILES NOT AVAILABLE" | 아직 확인 전 |

## 자동 QA 실행

릴리스 ROM(`elevane.nds`)의 숨은 모드입니다. 타이틀에서 L과 R을 누른 뒤 X와 Y를 눌러 네 키를 120프레임
유지합니다. 게임이 정상 흐름(타이틀 → 크레딧 → 타이틀)을 스스로 진행하고, 세이브를 한 번 쓰고
`RESULT PASS`로 끝납니다. 로그는 SD 카드의 `cleancart/elevane/qa-<날짜>-<시간>.log`입니다.
헤드리스: `SDDIR=out/ev/x tools/melonds/mds.sh run elevane/elevane.nds 2500 out/ev/x
"300:L:140,300:R:140,300:X:140,300:Y:140"`. 장이 추가되면 흐름에 합류합니다.

## 지금까지 확인한 것

무엇을 어디서 돌렸는지는 골격을 넣은 풀 리퀘스트를 참고하세요.
