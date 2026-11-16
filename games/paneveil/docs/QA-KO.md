# Paneveil QA

[English](QA.md)

melonDS 1.1용 점검표입니다 (macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인할 것 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 "PANEVEIL"과 한 줄 소개, 그 아래 X 안내. 아래 화면에 라이선스, ARM7, 세이브 줄. Start는 창 목록, X는 크레딧 | 헤드리스 실행은 아래 참고. melonDS 손 조작은 아직 |
| 창 목록 | "CHAPTER n" 머리글, 장의 창 여섯 개. 좌우로 장 바꾸기, A 시작, B 뒤로 | 헤드리스 실행만 |
| 창 (3D) | 위: 뒤에서 빛을 받으며 세로축으로 도는 장미창과 납 테두리. 아래: 설정 12개(NOW / ORDER), 걸음 수와 par, `KILN OK` 램프. 위/아래로 설정 고르기, 좌우로 한 칸씩 돌리기, 모두 맞으면 A로 굽기("FIRED ***"), 한 번 더 A는 다음 창, B 목록. Select: 2D 도식(같은 상태를 글자로), 3D 없이 풀 수 있음 | ✅ melonDS 1.1 software (2-01 스크린샷을 눈으로 확인). 손 조작감은 아직 |
| 타이틀 → X (크레딧) | 스크롤 크레딧(libnds, 소스 URL이 있는 BlocksDS crts MPL-2.0, picolibc, libgcc). X로 라이선스 뷰어(`licenses/`의 모든 문서, X 다음 파일, B 뒤로). NitroFS가 없으면 뷰어는 "CART FILES NOT AVAILABLE"이라고 함 | 아직 확인 안 함 |
| 리허설 데모 | `paneveil-rehearsal.nds`는 곧바로 리허설로 부팅합니다(세이브 칩·NitroFS 없음). 가마 점검, 1·2장의 첫 창(스크립트), 라이트박스를 돌리고, 결과 블록 id는 `0x48525650`(`PVRH`), values[3..15]가 가마 점검 13항목, values[16..30]이 라이트박스 15항목입니다(프레임 약 260). 채점: `RESULT_WORDS=64 tools/emu/emu.sh run games/paneveil/paneveil-rehearsal.nds 400 out/x "" 399 \| python3 games/paneveil/tools/judge.py --profile desmume-pydesmume-0.0.9` 또는 `tools/melonds/mds.sh run … 400 out/x "" 399 \| python3 games/paneveil/tools/judge.py --profile melonds-1.1-software` (`RENDERER=gl`, `compute`) | ✅ 프로필 네 개를 2026-11-16에 측정, `judge.py --profile`이 모두 MATCH(melonDS 1.1 software·GL Compute에서 게이트 항목 30개 모두 OK. DeSmuME py-desmume 0.0.9와 melonDS 1.1 GL Classic은 30개 중 29개이고 `poly-wire-edges`가 NG: 와이어프레임 폴리곤(alpha 0)을 아예 그리지 않음. 프로필에 기록함). 실기 값은 아직 없음 |

실기: 아직 돌리지 않았습니다. [HW-QA-KO.md](HW-QA-KO.md)를 보세요.

## 자동 QA 실행

릴리스 ROM(`paneveil.nds`)의 숨은 모드입니다. 타이틀 화면에서 L과 R을 누른 뒤 X와 Y를 눌러 넷을
120프레임 누르고 있으면 됩니다. 게임이 일반 흐름(타이틀 → 창 목록 → 모든 창 → 목록 → 타이틀 →
크레딧 → 타이틀)을 스스로 진행하고, 세이브를 한 번 쓰고, `RESULT PASS`로 끝납니다. 로그는 SD
카드의 `cleancart/paneveil/qa-<날짜>-<시각>.log`입니다. 헤드리스: `SDDIR=out/pv/x
tools/melonds/mds.sh run games/paneveil/paneveil.nds 1500 out/pv/x
"300:L:140,300:R:140,300:X:140,300:Y:140"`.

## 키 없이 QA 시작하기 (SD 표식)

SD 카드의 `cleancart/paneveil/autoqa`에 빈 파일을 둡니다(폴더가 먼저 있어야 함). 부팅 후 첫 타이틀
화면이 키 조합과 같은 QA 실행을 시작하고, 시작하기 전에 파일 이름을 `autoqa.used-<날짜>-<시각>`으로
바꾸므로 실행은 한 번만 일어납니다. 이름을 못 바꾸면 아무것도 시작하지 않고 `autoqa-error.log`에
한 줄이 남습니다.
