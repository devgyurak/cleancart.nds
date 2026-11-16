# Paneveil 실기 QA (DS Lite + R4)

[English](HW-QA.md)

Paneveil은 아직 실기에서 측정한 값이 없습니다. 이 문서는 실기 실행을 위한 점검표입니다. 첫 값을 기록하고
`expected.json`의 `unconfirmed` 항목을 승격하기 위한 것입니다. 콘솔은 사람이 조작하며, 아래 순서는
무엇을 하고 무엇을 적어 둘지입니다.

**자동 경로(스킬 `nds-hw-qa`):** 카드에 빈 폴더 `cleancart/paneveil/`을 만들면 리허설이 결과 블록을 거기에
씁니다(`res-*-paneveil-rehearsal.log`). 빈 파일 `cleancart/paneveil/autoqa`는 버튼을 누르지 않고도 QA 실행을
시작합니다. 이어서 `python3 tools/qa/ingest.py /Volumes/<카드> --game paneveil`이 `judge.py --hardware --partial`로
모두 채점합니다. 실기 NG 항목은 프로필이 아니라 발견 사항입니다. 기록 표는 다른 키트의 템플릿 문서를 보세요.
카드에 남은 예전 `res-*` 로그는 먼저 지웁니다.

## 하드웨어가 답해야 할 것

| 질문 | 어디에 나오는가 |
|---|---|
| 행렬 스택 레벨(GXSTAT 8-12, 13), N = 2 pop, store와 restore, 투영 스택이 GBATEK대로인가? | 리허설 값 3~11 (기대값 3 0 1 0 2 0 1 0 0) |
| load와 multiply 뒤에 CLIPMTX_RESULT와 VECMTX_RESULT가 모델과 같은가(packed GXFIFO와 직접 포트 모두)? | 값 12~15 (모두 0) |
| 버텍스 포맷 6종, 컬링, 앞/뒷면 렌더 비트, alpha, 와이어프레임, 깊이 시험, 텍스처 repeat·flip·clamp·색 0·PLTT_BASE·좌표 모드 1이 그림에서 맞는가? | 값 16~30과 flags(캡처 비트 8-12가 켜짐) |
| 와이어프레임(alpha 0)이 실기에서 그려지는가? (DeSmuME와 melonDS GL Classic은 그리지 않음) | 값 21(`poly-wire-edges`) |
| 게임이 크레딧까지 끝까지 가고(자동 QA 실행) 세이브 칩이 제대로 동작하는가? | 게임 ROM, 숨은 QA 모드 |

## 순서

1. 빌드: `cd games/paneveil && ./build.sh -j8 release`. `build/release/rom/paneveil.nds`와
   `build/release/judge/paneveil-rehearsal.nds`의 `shasum -a 256`을 적어 둡니다.
2. 두 ROM을 R4 카드에 복사하고 `cleancart/paneveil/`을 만듭니다.
3. **리허설**: `paneveil-rehearsal.nds`를 시작하고 아무것도 만지지 않고 20초쯤 기다립니다. 모든 장의 첫 창을
   플레이하고 라이트박스를 돌린 뒤 `REHEARSAL RESULT`에 값 3~30을 16진수로 보여 줍니다. 카드에 폴더가 있으면
   블록이 SD 카드에도 기록됩니다.
4. **게임**: `paneveil.nds`를 시작합니다. 1-01을 플레이하고(GLASS SIDE까지 아래, 오른쪽 한 번, A) Select로 2D 도식을
   보고 B로 돌아옵니다.
5. **자동 QA 실행**: 카드에 빈 `cleancart/paneveil/autoqa`를 두거나(또는 타이틀에서 L+R+X+Y를 2초 누름) 합니다. 게임이
   크레딧까지 스스로 진행하고(에뮬레이터에서 약 70초) `cleancart/paneveil/qa-*.log`를 씁니다. 이어서
   `python3 tools/qa/ingest.py /Volumes/<카드> --game paneveil`을 돌리고 에뮬레이터 실행과 비교합니다
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).

## 기록

- `docs/QA.md`와 `docs/QA-KO.md`(쌍): ROM SHA-256, 콘솔과 카드, 리허설 값, QA 로그 머리글(`dldi`, `nitro`,
  `save_detected`, `cart_ready`), 에뮬레이터와 처음 달라지는 체크포인트.
- `expected.json`: 실기 값이 ROM의 기대와 같은 `unconfirmed` 항목은 `source`를 `hw:ds-lite`로 바꿉니다. 실기가 다르면
  발견 사항입니다. 값을 기록하고 ROM과 기대값 중 어느 쪽이 틀렸는지 판단합니다. `known`에 프로필 `hw-ds-lite`(실패 항목)를
  추가합니다.
- `build/qa/` 아래의 것은 절대 커밋하지 않습니다.
