# Lightenette QA

[English](QA.md)

melonDS 1.1용 체크리스트입니다(macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 "LIGHTENETTE"와 안내 문구, 아래 화면에 라이선스/ARM7/세이브 줄 | ✅ DeSmuME 헤드리스(2026-11-11), melonDS는 아직 |
| 타이틀 → Start (1막) | 위: 그림자와 종이 마리오네트, 가로대에 이어진 줄, MATCH %. 아래: 슬라이더 4개. 슬라이더를 끌면 해당 팔다리가 움직이고 MATCH가 오릅니다. 85% 이상 20프레임이면 "CLEAR!", A는 타이틀, B는 뒤로. 스크립트 클리어: `40:START,80:T32/51:25,120:T96/64:25,160:T160/74:25,200:T224/114:25` (결과 블록 ID `0x31544341`: 점수, 클리어, 각도 + 90) | ✅ DeSmuME·melonDS 소프트웨어 헤드리스, 스크립트 터치(2026-11-11), melonDS에서 직접 만져 본 조작감은 아직 |
| 타이틀 → Select | 테스트 메뉴(위/아래, A, B로 뒤로): *정확도 판정*(약 2초 뒤 값이 나오고 B/Start로 타이틀 복귀)과 *2막: 인형 뒤돌리기*와 *리허설: 1·2막*(두 막이 스스로 진행된 뒤 판정, 화면 끝에 `ACT1 97 OK F59` / `ACT2 OK F… SEAM 0/0/18`) | ✅ 판정은 DeSmuME, melonDS 소프트웨어(2026-11-11), 메뉴 키 조작은 아직 직접 확인 전 |
| 타이틀 → Select → 2막 | 위: 받침대 위 종이 인형 3개와 줄, 고른 인형 표시. 아래: 인형마다 NOW/AIM 방향과 OK/TURN. 좌우 또는 터치로 고르고 L/R(또는 아래쪽 절반 터치)로 뒤돌립니다: 카드가 좁아졌다 뒤집히고 넓어집니다. 인형 A 앞, B 뒤, C 뒤를 20프레임 유지하면 "CLEAR!", 이어서 화면을 캡처해 위 화면에 `SEAM 0 0 18 OK` / `EDGE n OK`가 나옵니다. 스크립트: `30:SELECT,50:DOWN,70:A,100:RIGHT,110:R,135:RIGHT,145:L`(결과 블록 ID `0x32544341`) | ✅ DeSmuME 헤드리스, 스크린샷 확인(2026-11-11), 직접 만져 본 조작감은 아직 |
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

결과 블록은 이제 32워드입니다. 전부 읽으세요(DeSmuME는 `RESULT_WORDS=32`).

리허설 ROM(480프레임)에 `tools/judge.py`를 돌린 결과:

| 렌더러 | 판정 | NG 항목 |
|---|---|---|
| melonDS 소프트웨어 | OK (종료 코드 0) | - |
| melonDS GL Classic | NG (1) | seam-uv, edge-marking, lantern-blend-equal, act2-seam, act2-edge-marking |
| melonDS GL Compute | NG (1) | lantern-blend-equal |
| DeSmuME | NG (1) | lantern-blend-equal |

`line-polygon`은 `unconfirmed`(705/710/834/338)라 정보용입니다. 기대값: 재단선 0/0/18, 등불은 세 모드 모두 같은 값(GBATEK). 값은 corvane의 `acc-stage`
데모와 같습니다. DeSmuME ≠ melonDS ≠ 실기.
