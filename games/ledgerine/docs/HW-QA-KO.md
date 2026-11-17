# Ledgerine 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `ledgerine.nds` 대신 `build/release/rom/ledgerine-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

**판정 스위트(QA 판정 스위트 PR 이후):** QA 실행(`autoqa`)이 이제 스위트로 끝납니다. `RESULT PASS` 뒤에 QA 블록과 `press-min`를 `res-*-ledgerine-*.log`로 쓰고 진행 파일 `suite-<stamp>.log`도 남깁니다. hw-support ROM 한 번 실행으로 충분하고, **전원은 스위트가 끝난 뒤에** 끕니다(`suite-<stamp>.log`에 `suite end`). 아래의 리허설·데모 ROM은 항목 하나를 다시 돌릴 때만 씁니다(Wood 커널에서는 SD 카드를 받지 못함). `tools/qa/ingest.py`가 파일마다 채점합니다.

Ledgerine은 아직 실기에서 아무것도 측정하지 않았습니다. 이 문서는 첫 측정을 기록하고, 에뮬레이터나 GBATEK 해석에만 근거를 둔 항목을
확정하기 위한 점검표입니다. 콘솔은 사람이 다루며, 아래는 할 일과 적어 둘 것입니다.

**자동 경로(스킬 `nds-hw-qa`):** 카드에 빈 폴더 `cleancart/ledgerine/`를 만들어 두면 리허설과 `press-min` ROM이 결과 블록을 그곳에 씁니다
(`res-*-ledgerine-rehearsal.log`, `res-*-ledgerine-press-min.log`). 빈 파일 `cleancart/ledgerine/autoqa`는 버튼을 누르지 않고도 QA 실행을
시작합니다. `python3 tools/qa/ingest.py /Volumes/<카드> --game ledgerine`이 `judge.py --hardware --partial`로 모두 채점합니다. 실기에서 NG인
항목은 프로필이 아니라 발견입니다. 카드의 예전 `res-*` 로그는 먼저 지웁니다.

## 실기가 답해야 하는 것

| 질문 | 항목 / 보이는 곳 |
|---|---|
| 빈 FIFO 읽기가 마지막 수신 워드(GBATEK)를 돌려주는가, 다음 칸(melonDS)인가, 0(DeSmuME)인가? | `ipc-empty-last`, 리허설 `values[41]`(읽은 워드) |
| 꺼진 FIFO가 쓰기를 에러 없이 무시하고, 읽으면 가장 오래된 워드를 보여 주는가? (DeSmuME는 아니라고 함) | `ipc-disabled`, `values[2]` 비트 5·6 |
| 긴 DMA 중에도 CPU가 ITCM 코드를 계속 실행하는가? (melonDS는 멈춤. 두 타이머 값의 비율이 직렬이면 1.5, 계속 돌면 약 1.0) | `dma-itcm-runs`, `values[30]`·`values[31]`(DMA만, DMA와 루프, 타이머 틱) |
| ARM7 CpuSet이 BIOS 영역의 소스를 거부하는가? (melonDS의 대체 BIOS는 복사함) | `bios-arm7-guard`, `values[6]` 비트 4 |
| 32비트 나눗셈기가 0으로 나눌 때 상위 결과 워드에 무엇을 돌려주고, DIV0 플래그가 64비트 분모 전체를 따르는가? | `math-div0`, `math-div0-flag-upper`, `values[8..11]` |
| 타이머가 도는 중에 쓴 reload가 오버플로까지 기다리는가? 전송 뒤 SAD·DAD·CNT_L이 초깃값을 유지하는가? (DeSmuME는 둘 다 아니라고 함) | `timer-reload-deferred`, `dma-regs-unchanged` |
| 모드 2의 `-2^63 / -1`이 `8000000000000000h`이고 DIV0가 없는가? (`ledgerine-press-min.nds`를 실행. DeSmuME는 여기서 멈춤) | `math-overflow-min64` |
| ARM9의 바이트 쓰기가 LCDC VRAM에서 정말 무시되는가? 게임이 크레딧까지 끝까지 도는가(자동 QA 실행)? | `vram-byte-write`, 게임 ROM, 숨은 QA 모드 |

## 순서

1. 빌드: `cd games/ledgerine && ./build.sh -j8 release`. `build/release/rom/ledgerine.nds`와 `build/release/judge/ledgerine-rehearsal.nds`,
   `ledgerine-press-min.nds`의 `shasum -a 256`을 적어 둡니다.
2. 세 ROM을 R4 카드에 복사하고 `cleancart/ledgerine/`를 만듭니다.
3. **리허설**: `ledgerine-rehearsal.nds`를 실행하고, 손대지 않은 채 여섯 방이 DONE으로 바뀔 때까지 약 5초 기다립니다.
4. **press-min**: `ledgerine-press-min.nds`를 실행합니다(화면에 그리는 것은 없습니다. 2초 기다림).
5. **자동 QA 실행**: 카드에 빈 `cleancart/ledgerine/autoqa`를 두거나(또는 타이틀에서 L+R+X+Y를 2초 누름) 게임이 크레딧까지 스스로 진행합니다(약 30초).
   `cleancart/ledgerine/qa-*.log`가 생깁니다. 2번 방은 레버를 당길 때마다 약 1초 동안 입력과 소리가 멈추며, 그 밖에 이상한 것은 없어야 합니다.
   이어서 `python3 tools/qa/ingest.py /Volumes/<카드> --game ledgerine`을 실행하고 에뮬레이터 실행과 비교합니다
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).

## 기록

- `docs/QA.md`와 `docs/QA-KO.md`(쌍): ROM SHA-256, 콘솔과 카드, 실패한 항목, QA 로그 머리글.
- `expected.json`: 실기 값이 ROM의 기대와 같은 항목은 `source`를 `hw:ds-lite`로 바꿉니다. 실기가 다르면 그 항목은 발견입니다. 값을 적고 ROM과
  기대값 중 어느 쪽이 틀렸는지 정합니다(BIOS 항목은 BIOS에 따라서만 다를 수 있으니 콘솔의 BIOS를 적어 둡니다). 실패한 항목으로 프로필
  `hw-ds-lite`를 `known`에 추가합니다.
- `build/qa/` 아래 것은 커밋하지 않습니다.
