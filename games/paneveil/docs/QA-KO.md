# Paneveil QA

[English](QA.md)

melonDS 1.1용 점검표입니다 (macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인할 것 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 유리 레터링 "PANEVEIL" 뒤로 도는 3D 장미창(전용 프리셋), 어두운 패널 위에 한 줄 소개, 깜빡이는 PRESS START, X 안내, 클레리스토리 테마 음악(나갈 때 멈춤). 아래 화면에 라이선스, ARM7, 세이브 줄. Start는 창 목록, X는 크레딧 | 헤드리스 실행은 아래 참고. melonDS 손 조작은 아직 |
| 창 목록 | "CHAPTER n" 머리글, 장의 창 여섯 개. 좌우로 장 바꾸기, A 시작, B 뒤로 | 헤드리스 실행만 |
| 창 (3D) | 위: 뒤에서 빛을 받으며 세로축으로 도는 장미창과 납 테두리. 아래: 설정 12개(NOW / ORDER), 걸음 수와 par, `KILN OK` 램프. 위/아래로 설정 고르기, 좌우로 한 칸씩 돌리기, 모두 맞으면 A로 굽기("FIRED ***"), 한 번 더 A는 다음 창, B 목록. Select: 2D 도식(같은 상태를 글자로), 3D 없이 풀 수 있음 | ✅ melonDS 1.1 software (2-01 스크린샷을 눈으로 확인). 손 조작감은 아직 |
| 타이틀 → X (크레딧) | 스크롤 크레딧(libnds, 소스 URL이 있는 BlocksDS crts MPL-2.0, picolibc, libgcc). X로 라이선스 뷰어(`licenses/`의 모든 문서, X 다음 파일, B 뒤로). NitroFS가 없으면 뷰어는 "CART FILES NOT AVAILABLE"이라고 함 | 아직 확인 안 함 |
| 리허설 데모 | `paneveil-rehearsal.nds`는 곧바로 리허설로 부팅합니다(세이브 칩·NitroFS 없음). 가마 점검, 다섯 장 모두의 첫 창(스크립트), 라이트박스를 돌리고, 결과 블록 id는 `0x48525650`(`PVRH`), values[3..15]가 가마 점검 13항목, values[16..30]이 라이트박스 15항목입니다(블록은 프레임 약 510에 게시됨. 1300프레임을 돌리세요). 채점: `RESULT_WORDS=64 tools/emu/emu.sh run games/paneveil/paneveil-rehearsal.nds 1000 out/x "" 999 \| python3 games/paneveil/tools/judge.py --profile desmume-pydesmume-0.0.9` 또는 `tools/melonds/mds.sh run … 1000 out/x "" 999 \| python3 games/paneveil/tools/judge.py --profile melonds-1.1-software` (`RENDERER=gl`, `compute`) | ✅ 프로필 네 개를 2026-11-16에 측정, `judge.py --profile`이 모두 MATCH(melonDS 1.1 software·GL Compute에서 게이트 항목 30개 모두 OK. DeSmuME py-desmume 0.0.9와 melonDS 1.1 GL Classic은 30개 중 29개이고 `poly-wire-edges`가 NG: 와이어프레임 폴리곤(alpha 0)을 아예 그리지 않음. 프로필에 기록함). 실기 값은 아직 없음 |

실기: 아직 돌리지 않았습니다. [HW-QA-KO.md](HW-QA-KO.md)를 보세요.

## 자동 QA 실행

QA 로그의 `cp` 줄 끝에는 `gx=uf<n>,of<n> rdl=<min>`이 붙는다. 직전 체크포인트 이후 구간에서 DISP3DCNT bit 12(컬러 버퍼 RDLINES 언더플로)와 bit 13(폴리곤/버텍스 RAM 오버플로)이 켜져 있던 프레임 수, 그리고 RDLINES_COUNT의 최솟값(버퍼된 줄 수 − 2, 0..46. 구간에 표본이 없으면 `rdl=-`, 예: 같은 프레임의 두 체크포인트)이다. 두 플래그는 끈적하고 QA는 읽기만 하며 지우지 않으므로 값은 "플래그가 켜져 있던 프레임 수"다. 다만 게임이 플래그를 지우지 않을 때만 그렇다. bit 13을 스스로 확인(지우기)하는 게임(Corvane·Elevane·Heddlemere·Hushwell·Lightenette·Paneveil의 `g3_overflowed()`)은 확인 횟수를 `qa_game_gx_acks()`로 알리고 한 번을 "플래그가 켜져 있던 프레임"으로 센다. bit 12는 0으로 써서 언더플로 플래그가 남게 한다. `lag`처럼 참고용이라 해시에 넣지 않고 비교하지도 않으며(`tools/qa/compare.py --info`가 나란히 보여 준다), 3D를 쓰지 않는 장면은 `0,0`이다. 에뮬레이터는 이 플래그를 거의 세우지 않고 실기는 세울 수 있다.

릴리스 ROM(`paneveil.nds`)의 숨은 모드입니다. 타이틀 화면에서 L과 R을 누른 뒤 X와 Y를 눌러 넷을
120프레임 누르고 있으면 됩니다. 게임이 일반 흐름(타이틀 → 창 목록 → 스크립트 플레이어 `game/script.c`가 30개 창을 모두 플레이
→ 목록 → 타이틀 → 크레딧 → 타이틀)을 스스로 진행하고, 가마 점검과 라이트박스를 한 번 돌리고, 세이브를 한 번 쓰고,
`RESULT PASS`로 끝납니다. 로그는 SD
카드의 `cleancart/paneveil/qa-<날짜>-<시각>.log`입니다. 헤드리스: `SDDIR=out/pv/x
tools/melonds/mds.sh run games/paneveil/paneveil.nds 1500 out/pv/x
"300:L:140,300:R:140,300:X:140,300:Y:140"`(SD 로그를 보려면 `out/pv/x/cleancart/paneveil/`을 먼저 만드세요). 2026-11-16에 melonDS 1.1(software)에서 확인: 프레임 3971에 `RESULT PASS`, 창 30개 중 30개 굽기, 장 마스크 `0x1f`, 체크포인트 7개, 두 번 실행에 `tools/qa/compare.py` 차이 0. `QARN` 블록(리허설 배치 + values[48..54]의 QA 값)을 `judge.py --profile`로 채점해 melonDS 1.1 software·GL Classic·GL Compute·DeSmuME 모두 MATCH(31항목. DeSmuME와 GL Classic은 리허설과 같이 `poly-wire-edges`가 NG). 세이브 왕복(세이브 파일로 두 번 부팅): 칩 종류 2(EEPROM) 8192바이트, `CPU9 cpsr` 모드 0x1f(데이터 어보트 없음).

## 키 없이 QA 시작하기 (SD 표식)

SD 카드의 `cleancart/paneveil/autoqa`에 빈 파일을 둡니다(폴더가 먼저 있어야 함). 부팅 후 첫 타이틀
화면이 키 조합과 같은 QA 실행을 시작하고, 시작하기 전에 파일 이름을 `autoqa.used-<날짜>-<시각>`으로
바꾸므로 실행은 한 번만 일어납니다. 이름을 못 바꾸면 아무것도 시작하지 않고 `autoqa-error.log`에
한 줄이 남습니다.
