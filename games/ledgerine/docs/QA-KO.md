# Ledgerine QA

[English](QA.md)

melonDS 1.1용 점검표입니다 (macOS 키는 저장소 루트 `AGENTS.md` 참고).

| 장면 | 확인할 것 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 탑 그림, "LEDGERINE", 한 줄 소개, X/Select 안내. 아래 화면에 라이선스·ARM7·저장 칩 줄. Start는 1번 방, X는 크레딧, Select는 테스트 메뉴 | 헤드리스 실행(스크린샷 확인). melonDS 손 조작은 아직 |
| 테스트 메뉴 | 방 6개, 리허설, 크레딧. B는 뒤로 | 헤드리스 실행만 확인 |
| 방 | 위: 방 이름, 탑, 하드웨어, 장부 줄(`[X]`는 맞춘 줄). 아래: 고른 줄, 원하는 값, 다이얼, 레버를 당긴 뒤 `LEDGER`(모델)와 `MACHINE`(하드웨어) 값, `BALANCED` 또는 `DOES NOT BALANCE`, 두 값이 다르면 `MACHINE DISAGREES`. 위/아래는 줄 고르기, 좌/우는 다이얼 A, L/R 또는 Y/X는 다이얼 B, A는 레버, Start는 모든 줄이 맞을 때 장부 덮기, B는 뒤로 | QA 조종기로 헤드리스 실행(1번 방 스크린샷 확인). 손 조작은 아직 |
| 크레딧 | 스크롤. X는 라이선스 뷰어(NitroFS 필요), B는 뒤로 | 아직 확인 전 |

2번 방(기송관)은 레버를 한 번 당길 때마다 약 1초 동안 두 CPU의 libnds FIFO 처리를 멈추고, 그동안 ARM7은 입력 패킷을
보내지 않습니다. 그 순간 입력과 소리가 멈췄다가 다시 이어집니다(QA 실행이 그 뒤의 3~6번 방과 크레딧까지 도는 것으로만
확인했습니다).

## 데모 ROM

| 데모 | 보고하는 것 | 상태 |
|---|---|---|
| `rehearsal` | `ledgerine-rehearsal.nds`는 곧바로 리허설로 부팅합니다(저장 칩, NitroFS 없음): 모든 방의 감사. 결과 블록 id `0x4852444C`(`LDRH`), 배치는 `game/ledger.h`. `python3 tools/judge.py`(boot `rehearsal`)로 채점 | melonDS 1.1 soft / GL classic / GL compute: 프로필 MATCH. 아래 참고 |

실기: 아직 돌리지 않았습니다.

## 판정 프로필 (2026-11-16, 이 커밋의 빌드)

| 에뮬레이터 | NG 항목 | 비고 |
|---|---|---|
| melonDS 1.1 software / GL classic / GL compute (헤드리스, 내장 대체 BIOS) | `ipc-empty-last`(빈 읽기가 마지막 워드 B가 아니라 다음 칸 7A000001h를 돌려줌), `dma-itcm-runs`(DMA 내내 CPU가 멈춤: DMA만 00090027 틱, 루프를 더하면 000d7f6b 틱), `bios-arm7-guard`(대체 BIOS가 BIOS 영역에서 복사함) | 프로필 `melonds-1.1-software`, `-gl-classic`, `-gl-compute` MATCH. 세 렌더러가 같음 |
| DeSmuME (py-desmume 0.0.9) | 프로필 없음 | 헤드리스 하네스가 10분이 넘도록 리허설을 끝내지 못했습니다(ARM7 IPC 실험이 의심스럽고, 2·4·5번 방이 없던 이전 ROM은 끝났습니다). `expected.json`에 DeSmuME 프로필은 아직 없습니다 |

NG 세 항목은 실기로 확인할 결과입니다(GBATEK이 적은 내용이라 gating을 유지합니다). 실기를 돌리면 에뮬레이터 쪽 문제인지 GBATEK을
읽은 쪽 문제인지 알 수 있습니다.

## 자동 QA 실행

릴리스 ROM(`ledgerine.nds`, 배포본과 SHA-256 동일)의 숨은 모드입니다. 타이틀 화면에서 L과 R을 누른 뒤 X와 Y를 눌러 네 키를 120프레임
동안 누르고 있으면 됩니다. 게임이 정상 흐름을 스스로 진행합니다: 타이틀, 1~6번 방(조종기가 GBATEK 모델로 줄마다 다이얼 값을 찾아
키로 돌리고 레버를 당기며, Start로 장부를 덮음), 크레딧, 다시 타이틀, 그리고 `RESULT PASS`로 끝납니다. SD 카드에
`cleancart/ledgerine/qa-<날짜>-<시각>.log`가 남고, 리허설 배치의 결과 블록 `QARN`을 `python3 games/ledgerine/tools/judge.py --profile <프로필> <로그>`로
채점합니다(boot `qa`, 추가 항목 `qa-verdict`). 헤드리스: `SDDIR=out/lg/x tools/melonds/mds.sh run games/ledgerine/ledgerine.nds 3000 out/lg/x "300:L:140,300:R:140,300:X:140,300:Y:140"`
(먼저 `out/lg/x/cleancart/ledgerine/`를 만들어 둠. GL 렌더러는 `RENDERER=gl|compute`).

| 렌더러 | QA 실행 (2026-11-16) | `judge.py --profile` |
|---|---|---|
| melonDS 1.1 software | 프레임 1770에서 `RESULT PASS`, 체크포인트 6개, `parse_log.py` 통과 | `melonds-1.1-software` MATCH |
| melonDS 1.1 GL classic | PASS | `melonds-1.1-gl-classic` MATCH |
| melonDS 1.1 GL compute | PASS | `melonds-1.1-gl-compute` MATCH |

소프트웨어 렌더러로 두 번 돌린 체크포인트 해시는 같고(`tools/qa/compare.py`, 체크포인트 6개) `tools/qa/lint.py`도 통과합니다. 소프트웨어
실행에는 저장 칩이 없어서(SD 카드용 홈브류 실행) 저장 미러의 읽기 비교는 같았지만 칩 쓰기는 확인하지 못했습니다. 실제 DS Lite + R4: 아직
돌리지 않았습니다.

## 키 없이 QA 시작하기 (SD 마커)

SD 카드의 `cleancart/ledgerine/autoqa`에 빈 파일을 둡니다(폴더가 있어야 함). 부팅 후 첫 타이틀 화면이 키 조합과 같은 QA 실행을 시작하고, 시작
전에 파일 이름을 `autoqa.used-<날짜>-<시각>`로 바꿔서 한 번만 돌게 합니다. 이름 바꾸기가 실패하면 아무것도 시작하지 않고
`autoqa-error.log`에 한 줄이 남습니다. 카드로는 아직 시험하지 않았습니다.
