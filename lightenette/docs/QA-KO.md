# Lightenette QA

[English](QA.md)

melonDS 1.1용 체크리스트입니다(macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 생성한 그림(등불 빛무리, 조종대에 매달린 마리오네트, LIGHTENETTE 워드마크), 안내 문구와 키 안내, 아래 화면에 라이선스/ARM7/세이브 줄. 타이틀 곡 재생. Start → 1막, X → 크레딧, Select → 테스트 메뉴 | ✅ DeSmuME 헤드리스(2026-11-11), melonDS 직접 조작은 아직 |
| 타이틀 → X (크레딧) | 스크롤하는 크레딧(libnds, 소스 URL이 있는 BlocksDS crts MPL-2.0, picolibc, libgcc). X로 라이선스 뷰어(`licenses/`의 텍스트 8개, X 다음 파일, B 뒤로). NitroFS가 없으면 뷰어가 "CART FILES NOT AVAILABLE"을 표시 | ✅ DeSmuME 헤드리스(크레딧, 뷰어), melonDS 직접 조작은 아직 |
| 타이틀 → Start (1막) | 위: 그림자와 종이 마리오네트, 가로대에 이어진 줄, MATCH %. 아래: 슬라이더 4개. 슬라이더를 끌면 해당 팔다리가 움직이고 MATCH가 오릅니다. 85% 이상 20프레임이면 "CLEAR!", A는 타이틀, B는 뒤로. 스크립트 클리어: `40:START,80:T32/51:25,120:T96/64:25,160:T160/74:25,200:T224/114:25` (결과 블록 ID `0x31544341`: 점수, 클리어, 각도 + 90) | ✅ DeSmuME·melonDS 소프트웨어 헤드리스, 스크립트 터치(2026-11-11), melonDS에서 직접 만져 본 조작감은 아직 |
| 타이틀 → Select | 테스트 메뉴(위/아래, A, B로 뒤로): *정확도 판정*(약 2초 뒤 값이 나오고 B/Start로 타이틀 복귀)과 *2막: 인형 뒤돌리기*, *3막: 표식 찾기*, *4막: 줄 풀기*, *5막: 피날레*와 *리허설: 1~5막*(막들이 스스로 진행된 뒤 판정, 화면 끝에 `ACT1 97 OK F59` / `ACT2 OK F… SEAM 0/0/18`) | ✅ 판정은 DeSmuME, melonDS 소프트웨어(2026-11-11), 메뉴 키 조작은 아직 직접 확인 전 |
| 타이틀 → Select → 2막 | 위: 받침대 위 종이 인형 3개와 줄, 고른 인형 표시. 아래: 인형마다 NOW/AIM 방향과 OK/TURN. 좌우 또는 터치로 고르고 L/R(또는 아래쪽 절반 터치)로 뒤돌립니다: 카드가 좁아졌다 뒤집히고 넓어집니다. 인형 A 앞, B 뒤, C 뒤를 20프레임 유지하면 "CLEAR!", 이어서 화면을 캡처해 위 화면에 `SEAM 0 0 18 OK` / `EDGE n OK`가 나옵니다. 스크립트: `30:SELECT,50:DOWN,70:A,100:RIGHT,110:R,135:RIGHT,145:L`(결과 블록 ID `0x32544341`) | ✅ DeSmuME 헤드리스, 스크린샷 확인(2026-11-11), 직접 만져 본 조작감은 아직 |
| 타이틀 → Select → 3막 | 위: 거의 까만 무대(3D에 BLDCNT 01C1), 그려 둔 표식 세 개와 빛나는 반투명 등불. 아래: 표식(X, 찾으면 `*`)과 등불(@) 지도. D-pad나 드래그로 등불을 움직이고, 표식에서 16픽셀 안에 15프레임 머물면 켜집니다. 셋 다 찾으면 "CLEAR!" 후 등불이 자리 잡고 `LAMP <없음> <알파> <밝기감소> OK/NG`와 `DARK <그대로> <어둡게> OK`가 나옵니다. 스크립트: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:A:4,100:LEFT:8,100:DOWN:16,140:LEFT:32,140:UP:40,200:RIGHT:76,200:DOWN:4`(결과 블록 ID `0x33544341`) | ✅ DeSmuME 헤드리스, 스크린샷 확인(2026-11-11), 손맛은 미확인 |
| 타이틀 → Select → 4막 | 위: 가로대, 핀 네 개로 내려오는 색깔 줄 네 가닥(어두운 사각형 + 라인 폴리곤), 아래에 가로·대각선 실 하나씩. 아래: 핀마다 걸린 줄(S1~S4), 커서, CROSSINGS n. L/R이나 터치로 핀을 고르고 A나 두 번째 터치로 두 줄을 맞바꿉니다. 엇갈림 0이 20프레임이면 "CLEAR!" 후 `HLINE n OK` / `DLINE n OK`. 스크립트: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:DOWN:4,80:A:4,100:A:2,104:RIGHT:2,108:A:2,118:A:2,122:RIGHT:2,126:RIGHT:2,130:A:2,140:A:2,144:LEFT:2,148:A:2`(결과 블록 ID `0x34544341`) | ✅ DeSmuME 헤드리스, 스크린샷 확인(2026-11-11), 조작감은 아직 직접 확인 전 |
| 타이틀 → Select → 5막 | 위: 엣지 색 세 가지의 받침대 위 인형 3개가 있는 어두운 무대, 라인 폴리곤 줄, 등불. 아래: 인형마다 NOW/AIM 면, LIT/DARK, OK/TODO. D-pad나 드래그로 등불을 움직이고 L/R/A(또는 아래쪽 터치)로 등불 아래 인형을 돌립니다. 모두 밝히고 돌려 놓으면(A 뒤, B 앞, C 뒤) 20프레임 뒤 "CLEAR!", 등불이 자리 잡고 `SEAM`, `EDGE`, `LINE`, `LAMP`, `DARK`가 나옵니다. 스크립트: `30:SELECT:4,50:DOWN:4,60:DOWN:4,70:DOWN:4,80:DOWN:4,90:A:4,110:UP:8,130:LEFT:36,160:R:2,185:RIGHT:72,250:R:2`(결과 블록 ID `0x35544341`) | ✅ DeSmuME 헤드리스, 스크린샷 확인(2026-11-11), 조작감은 아직 직접 확인 전 |
| 리허설 데모 | `lightenette-rehearsal.nds`는 세이브 칩과 NitroFS 없이 곧바로 리허설로 부팅, 결과 블록 ID `0x4C454852`. `python3 lightenette/tools/judge.py <로그>`가 `expected.json`과 대조 | 아래 참고 |
| acc-stage 데모 | `lightenette-acc-stage.nds`, 타이틀 → Select 장면과 같은 값, 결과 블록 `0x0237F000`: 매직 `0x52455643`, ID `0x4A44474C` | 아래 참고 |

## 판정 측정값 (2026-11-11, 헤드리스, GL은 Mesa llvmpipe, 실기 미측정)

결과 블록 `values[0..3]`(재단선 행, 엣지, 라인, 플래그)과 등불 값 `values[4..6]`:

| 렌더러 | 재단선 | 엣지 | 라인 | 플래그 | 등불 NONE/ALPHA/DARK |
|---|---|---|---|---|---|
| melonDS 소프트웨어 | 0/0/18 | 183 | 705 | 1f | 318f/318f/318f |
| melonDS GL Classic | 0/18/0 | 0 | 710 | 14 | 0d2c/318f/0d2c |
| melonDS GL Compute | 0/0/18 | 183 | 834 | 17 | 0d2c/318f/0d2c |
| DeSmuME | 0/0/18 | 188 | 338 | 17 | 318f/318f/1d6e |

리허설 블록 추가 값(`values[9..17]`): 1막 점수 97, 클리어 1, 클리어 프레임 59, 종료 1, 2막 클리어 1(`values[13]`) — 렌더러 4종에서 같습니다(막 로직은 에뮬레이터와 무관). 2막 화면 판정(`values[14]` 인형별 재단선, `values[15]` 엣지 픽셀):

| 렌더러 | 2막 재단선 | 2막 엣지 |
|---|---|---|
| melonDS 소프트웨어 | 0/0/18 | 228 |
| melonDS GL Classic | 0/18/0 | 0 |
| melonDS GL Compute | 0/0/18 | 228 |
| DeSmuME | 0/0/18 | 240 |

결과 블록은 이제 64워드(`CvResult.values[61]`)입니다. 전부 읽으세요(DeSmuME는 `RESULT_WORDS=64`).

3막 블록 추가 값(`values[18..26]`): 클리어 1, 표식 3, 등불 픽셀 없음/알파/밝기감소, 배경 픽셀 그대로/어둡게, 캡처 완료 1, 클리어 프레임 181. 측정(헤드리스, 2026-11-11):

| 렌더러 | 등불 없음/알파/밝기감소 | 배경 그대로/어둡게 |
|---|---|---|
| melonDS 소프트웨어 | 25da/25da/25da | 2463/0c21 |
| melonDS GL Classic | 0d2c/25da/0d2c | 2463/0c21 |
| melonDS GL Compute | 0d2c/25da/0d2c | 2463/0c21 |
| DeSmuME | 25da/25da/1972 | 2463/1042 |

4·5막 블록 추가 값(`values[27..44]`, 헤드리스, 2026-11-11):

| 렌더러 | 4막 가로 / 대각선 실 픽셀 | 5막 재단선 | 5막 엣지 픽셀(세 색) | 5막 줄 라인 픽셀 | 5막 등불 없음/알파/밝기감소 | 배경 그대로/어둡게 |
|---|---|---|---|---|---|---|
| melonDS 소프트웨어 | 208 / 208 | 0/0/18 | 76/76/76 | 93 | 318f/318f/318f | 2463/0c21 |
| melonDS GL Classic | 208 / 208 | 18/0/0 | 0/0/0 | 93 | 0d2c/318f/0d2c | 2463/0c21 |
| melonDS GL Compute | 208 / 208 | 0/0/18 | 76/76/76 | 186 | 0d2c/318f/0d2c | 2463/0c21 |
| DeSmuME | 8 / 208 | 0/0/18 | 80/80/80 | 93 | 318f/318f/1d6e | 2463/1042 |

5막은 깨끗한 거울 뒷면(인형 A)과 앞면(인형 B)을 평범한 거울 뒷면(인형 C) 옆에 두므로, melonDS GL Classic에서는 #2008 재단선이 인형 C가 아니라 인형 A에 나타납니다(`18/0/0`). 실·줄 픽셀 수는 `unconfirmed`(정보용)입니다. DeSmuME는 가로 라인 폴리곤을 거의 그리지 않고(8px) melonDS Compute는 세로 줄을 두 배 굵게 그립니다.

리허설 ROM(1100프레임, melonDS에서는 약 864, DeSmuME에서는 약 954프레임에 끝남)에 `tools/judge.py`를 돌린 결과:

| 렌더러 | 판정 | NG 항목 |
|---|---|---|
| melonDS 소프트웨어 | OK (종료 코드 0) | - |
| melonDS GL Classic | NG (1) | seam-uv, edge-marking, lantern-blend-equal, act2-seam, act2-edge-marking, act3-lantern-equal, act5-seam, act5-edge-3-colours, act5-lantern-equal |
| melonDS GL Compute | NG (1) | lantern-blend-equal, act3-lantern-equal, act5-lantern-equal |
| DeSmuME | NG (1) | lantern-blend-equal, act3-lantern-equal, act5-lantern-equal |

`line-polygon`, `act4-hline`, `act4-dline`, `act5-line-polygon`은 `unconfirmed`라 정보용입니다(line-polygon 705/710/834/338). 기대값: 재단선 0/0/18, 등불은 세 모드 모두 같은 값(GBATEK). 값은 corvane의 `acc-stage`
데모와 같습니다. DeSmuME ≠ melonDS ≠ 실기.
