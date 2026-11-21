# Quayholt QA

[English](QA.md)

melonDS 1.1용 점검표입니다(macOS 키는 저장소 `AGENTS.md` 참고). 판정 프로필은 인터프리터 기준이므로 melonDS의 JIT를
끄고 실행하세요.

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 해질녘 항구와 크레인 두 대, 스텐실 글자 "QUAYHOLT", 소개 문구, X/Select 안내. 아래 화면에 라이선스, ARM7, 저장 칩 줄. Start와 Select는 테스트 메뉴, X는 크레딧 | headless 실행(스크린샷 확인), melonDS 손 확인은 아직 |
| 테스트 메뉴 | 리허설과 크레딧, B로 뒤로 | headless 실행만 |
| 부두 | 위 화면에 부두 이름과 진행도, `BALANCED`/`CROOKED` 표시가 붙은 작업 다섯 개. 위/아래 선택, A 열기, L/R 다른 부두, B 타이틀 | QA 조종사로 headless 실행(스크린샷 확인), 손 확인은 아직 |
| 작업 | 지시문, 원하는 분필 값, 갈고리가 있는 부두, 들어 올린 뒤 THE BOOK / THE CRANE 값, `BALANCED`와 `CROOKED BOX` 줄. 아래: 크레인(X), 세팅(좌우, L/R은 5칸), A 들어 올리기, B 뒤로 | QA 조종사로 headless 실행(스크린샷 확인), 손 확인은 아직 |
| 리허설 | 부두 여섯 줄. 이 빌드에 있는 부두(A~F)는 DONE, 나머지는 LATER | headless 실행 |
| 크레딧 | 스크롤, X로 라이선스 뷰어(NitroFS 필요), B로 뒤로 | 아직 확인 안 함 |

## 데모 ROM

`make DEMO=devcheck`는 개발용 도구입니다(`DEMOS`에 없고 채점하지 않음): 항구의 모든 장치를 두 크레인과 모든 세팅으로 실행하고 비뚤어진 상자를 나열합니다. melonDS 1.1: 세팅 93개 실행, 19개 비뚤어짐(위에 적은 알려진 차이), DeSmuME: 32개.

| 데모 | 보고하는 것 | 상태 |
|---|---|---|
| `rehearsal` | `quayholt-rehearsal.nds`: 이 빌드의 모든 측정(저장 칩·NitroFS 없음). 블록 id `QYRH`, 배치는 `arm9/source/game/ledger.h`. `python3 tools/judge.py`(boot `rehearsal`)로 채점 | melonDS 1.1 soft / GL classic / GL compute, DeSmuME: 프로필 MATCH |
| `l2-loads` | `quayholt-l2-loads.nds`: A 부두 (블록 id `QYLO`) | 네 곳 모두 프로필 MATCH |
| `l2-blocks` | `quayholt-l2-blocks.nds`: B 부두 (블록 id `QYBL`) | 네 곳 모두 프로필 MATCH |
| `l2-alu` | `quayholt-l2-alu.nds`: C 부두 (블록 id `QYAL`) | 네 곳 모두 프로필 MATCH |
| `l2-pu` | `quayholt-l2-pu.nds`: D 부두, 제어 레지스터·BKPT·보호 유닛 (블록 id `QYPU`) | 네 곳 모두 프로필 MATCH |
| `l2-tcm` | `quayholt-l2-tcm.nds`: E 부두, TCM (블록 id `QYTC`) | 네 곳 모두 프로필 MATCH |
| `l2-cache` | `quayholt-l2-cache.nds`: F 부두, 캐시 (블록 id `QYCA`) | 네 곳 모두 프로필 MATCH |

## 판정 프로필 (2026-11-20, 이 커밋의 빌드)

| 에뮬레이터 | NG 항목 | 비고 |
|---|---|---|
| melonDS 1.1 soft / GL classic / GL compute (인터프리터) | `ldrh7-odd`, `ldrsh7-odd`, `thumb7-ldrh-odd`, `stm7-base-new`, `stm7-empty`, `thumb7-stmia-base-new`, `stm-pc12`, `alu7-rn-pc12`, `cp15-prev5-bit15`, `bkpt9-pabt` | CPU 코어를 공유해서 렌더러끼리 같습니다. `ldm9-empty`, `dtcm-load-mode`, `itcm-load-mode`, 캐시 항목 네 개(`dcache-dma-stale`, `dcache-wb-dirty`, `dcache-inval-drop`, `icache-stale`)도 다릅니다(정보용): melonDS는 제어 비트 17·19를 무시하고 데이터 캐시 내용을 두지 않으며, 두 에뮬레이터 모두 캐시 항목 네 개에서 캐시 없는 결과를 보입니다 |
| DeSmuME py-desmume 0.0.9 | melonDS와 같은 아홉 개(`bkpt9-pabt` 제외)와 `ldm7-base-nowb`, `ldm9-base-wb`, `pu-priority`, `pu-background`, `pu-user-ro`, `pu-code-xn`(보호 유닛 없음), `dtcm-mirror`, `itcm-size`, `tcm-dma-bus` | `ldm9-empty`, 두 load 모드, 캐시 항목 네 개도 다릅니다(정보용) |

실기: 아직 안 돌렸습니다. 첫 DS Lite 실행이 `unconfirmed` 항목을 정합니다(`docs/HW-QA.md`, 작성 예정).
