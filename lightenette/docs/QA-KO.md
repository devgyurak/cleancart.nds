# Lightenette QA

[English](QA.md)

melonDS 1.1용 체크리스트입니다(macOS 키는 저장소 `AGENTS.md` 참고).

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 "LIGHTENETTE"와 안내 문구, 아래 화면에 라이선스/ARM7/세이브 줄 | ✅ DeSmuME 헤드리스(2026-11-11), melonDS는 아직 |
| 타이틀 → Select | 판정 장면: 약 2초 뒤 값이 나오고 B/Start로 타이틀 복귀 | ✅ DeSmuME, melonDS 소프트웨어(2026-11-11) |
| acc-stage 데모 | `lightenette-acc-stage.nds`, 타이틀 → Select 장면과 같은 값, 결과 블록 `0x0237F000`: 매직 `0x52455643`, ID `0x4A44474C` | 아래 참고 |

## 판정 측정값 (2026-11-11, 헤드리스, GL은 Mesa llvmpipe, 실기 미측정)

결과 블록 `values[0..3]`(재단선 행, 엣지, 라인, 플래그)과 등불 값 `values[4..6]`:

| 렌더러 | 재단선 | 엣지 | 라인 | 플래그 | 등불 NONE/ALPHA/DARK |
|---|---|---|---|---|---|
| melonDS 소프트웨어 | 0/0/18 | 183 | 705 | 1f | 318f/318f/318f |
| melonDS GL Classic | 0/18/0 | 0 | 710 | 14 | 0d2c/318f/0d2c |
| melonDS GL Compute | 0/0/18 | 183 | 834 | 17 | 0d2c/318f/0d2c |
| DeSmuME | 0/0/18 | 188 | 338 | 17 | 318f/318f/1d6e |

기대값: 재단선 0/0/18, 등불은 세 모드 모두 같은 값(GBATEK). 값은 corvane의 `acc-stage`
데모와 같습니다. DeSmuME ≠ melonDS ≠ 실기.
