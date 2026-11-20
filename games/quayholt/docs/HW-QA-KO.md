# Quayholt 실기 QA (DS Lite + R4) — 계획

[English](HW-QA.md)

상태: **아직 안 돌렸습니다.** 이 게임은 콘솔에서 측정한 것이 하나도 없습니다. 기대값은 모두 GBATEK과 에뮬레이터 소스를 읽어서 정한
것이고, `expected.json`의 에뮬레이터 프로필은 에뮬레이터가 하는 일을 적어 둘 뿐입니다. 실행해 보면 `unconfirmed` 항목 일곱 개가
정해지고, GBATEK을 읽은 것에만 기댄 항목도 확정됩니다. 콘솔은 사람이 다룹니다. 아래는 할 일과 적을 것입니다. 스킬 `nds-hw-qa`를
보세요.

**Wood 계열 R4 커널:** 카드에 `quayholt.nds` 대신 `build/release/rom/quayholt-hw-support.nds`를 넣습니다(게임 코드가 `####`인
같은 ROM. 이런 커널은 그런 ROM만 DLDI 패치하고, 아니면 ROM이 SD 카드를 보지 못합니다). 데모와 리허설 ROM은 자기 코드를 씁니다.

## 위험과 실행 순서

D, E 부두의 ARM9 측정(`l2-pu`, `l2-tcm`)은 IRQ를 막은 채 몇 마이크로초 동안 예외 벡터를 옮기고, 보호 영역을 바꾸고, DTCM을 옮깁니다.
전부 되돌리지만, 콘솔이 에뮬레이터와 다르게 동작하면 거기서 멈출 수 있습니다(전원을 다시 켜면 됩니다). SD 로그를 잃을 수도 있습니다.
그래서 안전한 ROM부터 돌립니다:

1. 리허설(`quayholt-rehearsal.nds`)은 D와 E를 포함하므로 **처음이 아닙니다**. `quayholt-l2-loads.nds`로 시작하고, 이어서
   `l2-blocks`, `l2-alu`, `l2-cache`(모두 평범한 사용자 모드 코드)를 돌립니다.
2. 그다음 `quayholt-l2-tcm.nds`, 이어서 `quayholt-l2-pu.nds`(예외 하네스).
3. 마지막으로 리허설과 자동 QA 실행(`cleancart/quayholt/autoqa`). 이 실행은 30개 작업도 모두 풀고 판정 스위트도 돌립니다.
   전원을 끄기 전에 `suite-<stamp>.log`에 `suite end`가 찍힐 때까지 기다립니다.

먼저 카드에 빈 폴더 `cleancart/quayholt/`를 만듭니다. 데모는 거기에 `res-*-quayholt-<demo>.log`를 씁니다. 그다음
`python3 tools/qa/ingest.py /Volumes/<card> --game quayholt`이 모든 파일을 `judge.py --hardware --partial`로 채점합니다.
실기에서 NG인 항목은 프로필이 아니라 발견입니다.

## 실기가 답해야 할 것 (1순위)

| 질문 | 항목 |
|---|---|
| ARM9 데이터 캐시가 DMA 뒤에 옛 값을 유지하고, clean 전까지 쓰기를 붙잡고, invalidate에서 dirty 줄을 버리며, 명령 캐시가 옛 코드를 유지하는가? (넷 다 미확인. 에뮬레이터는 일관된 결과를 보임) | `dcache-dma-stale`, `dcache-wb-dirty`, `dcache-inval-drop`, `icache-stale` |
| 제어 비트 17이면 DTCM 읽기, 비트 19이면 ITCM 읽기가 뒤쪽 버스로 가는가? (melonDS는 두 비트를 무시) | `dtcm-load-mode`, `itcm-load-mode` |
| ARM9의 빈 목록 LDM이 베이스를 40h 옮기고 아무것도 읽지 않는가? | `ldm9-empty` |
| STM {..PC}가 두 CPU에서 STM 주소 + 12를 저장하는가? | `stm-pc12` (보고 melonDS#2312가 실기 값을 줌) |
| ARM7에서 Rn = PC이고 레지스터로 시프트하는 ALU가 PC + 12를 읽는가? | `alu7-rn-pc12` |
| TCM도 보호 영역 검사를 받는가? (시험은 01000000h의 ITCM 미러가 영역에 덮여 있다고 가정) | E 부두 전체. `l2-tcm`에서 data abort가 나면 멈춤으로 보임 |
| BKPT가 abort 모드로 prefetch abort 벡터에 가고 LR = BKPT + 4인가? | `bkpt9-pabt` |

실기 값이 ROM의 기대와 같은 항목은 `expected.json`에서 `source`를 `hw:dslite-ntr`로 바꿉니다(`unconfirmed`는 gating target이
됩니다). 실기가 다르면 발견입니다: 값을 적고 ROM과 기대 중 무엇이 틀렸는지 정합니다. 에뮬레이터를 맞출 수 없는 항목은
`probes/`로 옮깁니다. `known`에 프로필 `hw-ds-lite`(실패 항목)를 더합니다.

## 순서

1. 빌드: `cd games/quayholt && ./build.sh -j8 release`. `build/release/rom/quayholt.nds`, `quayholt-hw-support.nds`, 데모 ROM의
   `shasum -a 256`을 적어 둡니다.
2. ROM을 카드에 복사하고 `cleancart/quayholt/`를 만든 뒤 위 순서로 돌리며 화면에 보이는 것을 적습니다.
3. 카드를 `ingest.py`로 읽고 에뮬레이터 실행과 비교합니다(`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
4. 이 파일("Run 1" 절), `docs/QA.md`, `docs/QA-KO.md`에 기록합니다(ROM SHA-256, 콘솔과 카드, 실패 항목).

`build/qa/` 아래는 절대 커밋하지 않습니다.
