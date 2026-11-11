# Lightenette QA

[English](QA.md)

melonDS 1.1용 체크리스트입니다(macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 "LIGHTENETTE"와 안내 문구, 아래 화면에 라이선스/ARM7/세이브 줄 | ✅ DeSmuME 헤드리스(2026-11-11), melonDS는 아직 |
| 타이틀 → Start (1막) | 위: 그림자와 종이 마리오네트, 가로대에 이어진 줄, MATCH %. 아래: 슬라이더 4개. 슬라이더를 끌면 해당 팔다리가 움직이고 MATCH가 오릅니다. 85% 이상 20프레임이면 "CLEAR!", A는 타이틀, B는 뒤로. 스크립트 클리어: `40:START,80:T32/51:25,120:T96/64:25,160:T160/74:25,200:T224/114:25` (결과 블록 ID `0x31544341`: 점수, 클리어, 각도 + 90) | ✅ DeSmuME·melonDS 소프트웨어 헤드리스, 스크립트 터치(2026-11-11), melonDS에서 직접 만져 본 조작감은 아직 |
| 타이틀 → Select | 테스트 메뉴(위/아래, A, B로 뒤로): *정확도 판정*(약 2초 뒤 값이 나오고 B/Start로 타이틀 복귀)과 *리허설: 1막*(막이 스스로 진행된 뒤 판정, 화면 끝에 `ACT1 97 OK F59`) | ✅ 판정은 DeSmuME, melonDS 소프트웨어(2026-11-11), 메뉴 키 조작은 아직 직접 확인 전 |
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

리허설 블록 추가 값(`values[9..12]`): 1막 점수 97, 클리어 1, 클리어 프레임 59, 종료 1 — 렌더러 4종에서 같습니다(막 로직은 에뮬레이터와 무관).

리허설 ROM(330프레임)에 `tools/judge.py`를 돌린 결과:

| 렌더러 | 판정 | NG 항목 |
|---|---|---|
| melonDS 소프트웨어 | OK (종료 코드 0) | - |
| melonDS GL Classic | NG (1) | seam-uv, edge-marking, lantern-blend-equal |
| melonDS GL Compute | NG (1) | lantern-blend-equal |
| DeSmuME | NG (1) | lantern-blend-equal |

`line-polygon`은 `unconfirmed`(705/710/834/338)라 정보용입니다. 기대값: 재단선 0/0/18, 등불은 세 모드 모두 같은 값(GBATEK). 값은 corvane의 `acc-stage`
데모와 같습니다. DeSmuME ≠ melonDS ≠ 실기.
