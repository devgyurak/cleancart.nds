# Elevane QA

[English](QA.md)

melonDS 1.1용 점검표입니다 (macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인할 것 | 상태 |
|---|---|---|
| 타이틀 | Start로 퍼즐 목록. 위 화면에 "ELEVANE"와 한 줄 소개, 그 아래 X 안내. 아래 화면에 라이선스, ARM7, 세이브 줄. X → 크레딧 | 헤드리스 실행은 아래 참고. melonDS 손 조작은 아직 |
| 타이틀 → Start (목록) | "CHOOSE AN ORDER", 1장 주문서 8개. 위/아래, A 시작, B 뒤로 | ✅ DeSmuME 헤드리스 (2026-11-11). melonDS 손 조작은 아직 |
| 퍼즐 (3D) | 위: 정면도, 파란 도면 벽 = 주문서, 블록은 금색, 주문서 밖 블록은 빨강. 아래: 평면도, 밝은 바닥 타일 = 주문서, 커서 타일 노랑. 글자 오버레이(ORDER/NOW 격자, BLOCKS, PAR). 십자키 이동, A +1, B -1, 평면도 칸 터치 +1, 30프레임 누르면 비움. 두 도면이 맞으면 "MATCH! A: DELIVER", A로 납품("DELIVERED ***", 블록 수와 par로 별), A 다음 퍼즐, B 목록. 1-01 스크립트 풀이: `60:START,100:A,160:A,175:RIGHT,185:A,200:RIGHT,210:A,225:RIGHT,235:A,280:A` | ✅ DeSmuME. ✅ melonDS 1.1 soft·GL·compute를 스크립트 터치로 (`160:T60/147:3,175:T105/147:3,190:T150/147:3,205:T196/147:3`): 화면이 일치함 (98,304픽셀 중 8단계 넘게 다른 것은 약 300개, 글자와 가장자리뿐). melonDS 손 조작감은 아직 |
| 퍼즐 (2D 격자) | Select: 두 화면 모두 글자만. 위에 정면도 격자(`#` 블록, `X` 초과, `+` 부족, `.` 비어 있음), 아래에 평면도 높이(커서 노랑). 규칙은 같고 3D 없이 풀 수 있음. 다시 Select로 3D 복귀 | ✅ DeSmuME (2D에서 풀고 3D로 복귀) |
| 퍼즐 → X (감상 모드) | 복사본으로 채운 광장 그림(위 화면은 엔진 B 그림), 글자는 아래 화면. 프리셋 1(HBlank DMA): POLYS 1140+1140(패스마다 2048 미만, 합치면 초과), OVERFLOW 0 0, HOLES CAP 0 BLEND 0. A로 프리셋 2(리어 플레인), B/Start로 퍼즐 복귀(쌓은 것 유지, 3D 정상) | ✅ DeSmuME(두 프리셋. 폴리곤 카운터는 DeSmuME가 0을 돌려줘서 0으로 나옴), ✅ melonDS 1.1 soft/GL/compute와 master `4ce2188` soft: 프리셋 1 `HOLES 0 0`. ❌ master `4ce2188` GL(왼쪽 검게)·Compute(왼쪽 회색): 프리셋 1 `BLEND 6144`, #2618 결함이며 퍼즐 진행은 정상. 프리셋 2: ✅ soft, ❌ GL(한쪽 빔, 별개 결함). 퍼즐 복귀 ✅ DeSmuME, melonDS 1.1 soft/GL |
| 리허설 데모 | `elevane-rehearsal.nds`는 곧장 리허설로 부팅(세이브 칩·NitroFS 없음): 스크립트로 퍼즐을 풀고, 감상 프리셋 1, 2를 돌리고 약 480프레임에 끝남. 결과 블록 id `0x48525645`(`EVRH`). 판정: `RESULT_WORDS=64 tools/emu/emu.sh run elevane/elevane-rehearsal.nds 900 out/x "" 899 \| python3 elevane/tools/judge.py --profile desmume-pydesmume-0.0.9` 또는 `RENDERER=gl tools/melonds/mds.sh run … 900 out/x "" 899 \| python3 elevane/tools/judge.py --profile melonds-1.1-gl-classic` (master는 `MELONDS_COMMIT=4ce2188`) | ✅ 2026-11-11 일곱 프로필 모두 측정, 각각 `judge.py --profile` MATCH: DeSmuME, melonDS 1.1 software/GL/compute, master `4ce2188` software/GL/compute. 예상 실패: 1.1 GL/Compute `rear-plane-full`, master GL `hdma-holes-zero` + `rear-plane-full`, master Compute `hdma-composite-full` + `hdma-holes-zero` + `rear-plane-full`. DeSmuME는 폴리곤 항목이 n/a(카운터가 0). 실기 값은 아직 없음: GBATEK/보고/rom이 아닌 항목은 모두 `unconfirmed` |
| 화면 비교 | `tools/compare.py --phase dual\|showcase`: 같은 에뮬레이터의 소프트웨어 렌더러와 연속 프레임(양화면 2, 감상 4)을 비교 | ✅ 1.1 GL/Compute는 한도 이내(양화면 최대 305/98,304픽셀, 감상 최대 1138). ❌ master GL/Compute는 감상 한도 초과(9,000~17,400): 예상대로 #2618 결함 |
| 타이틀 → X (크레딧) | 스크롤 크레딧 (libnds, 소스 URL이 있는 BlocksDS crts MPL-2.0, picolibc, libgcc). X로 라이선스 뷰어(`licenses/`의 모든 문서, X 다음 파일, B 뒤로). NitroFS가 없으면 "CART FILES NOT AVAILABLE" | 아직 확인 전 |

## 자동 QA 실행

릴리스 ROM(`elevane.nds`)의 숨은 모드입니다. 타이틀에서 L과 R을 누른 뒤 X와 Y를 눌러 네 키를 120프레임
유지합니다. 게임이 정상 흐름(타이틀 → 크레딧 → 타이틀)을 스스로 진행하고, 세이브를 한 번 쓰고
`RESULT PASS`로 끝납니다. 로그는 SD 카드의 `cleancart/elevane/qa-<날짜>-<시간>.log`입니다.
헤드리스: `SDDIR=out/ev/x tools/melonds/mds.sh run elevane/elevane.nds 2500 out/ev/x
"300:L:140,300:R:140,300:X:140,300:Y:140"`. 장이 추가되면 흐름에 합류합니다.

## 지금까지 확인한 것

무엇을 어디서 돌렸는지는 골격을 넣은 풀 리퀘스트를 참고하세요.
