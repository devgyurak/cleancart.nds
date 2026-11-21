# Quayholt

[English](README.md)

닌텐도 DS용 오리지널 크레인 두 대 항구 퍼즐이자, 에뮬레이터 개발자를 위한 스스로 판정하는 테스트 ROM입니다.
현재 상태: **측정 하네스 완성**(적재, 블록, 계산, 세관, 사물함 부두, 리허설, 판정). 장부판(캐시) 부두와 항구 게임은
이어서 들어옵니다.

목표 레벨 **L2**, 영역 **ARM 코어 가장자리**: ARM9(v5TE)·ARM7(v4T) 명령 가장자리와 ARM9 CP15(보호 유닛, TCM, 캐시).
판정 항목마다 고정된 시험 데이터에 명령 하나(또는 CP15 동작 하나)를 실행하고, 원시 결과를 GBATEK에서 손으로 계산한
상수와 비교합니다. L2의 뜻은 [docs/LEVELS-KO.md](../../docs/LEVELS-KO.md)를 보세요.

## 판정 항목 (현재)

| 부두 | 항목 |
|---|---|
| A 적재 | `ldrh7-odd`, `ldrsh7-odd`, `thumb7-ldrh-odd` |
| B 블록 | `stm7-base-new`, `ldm7-base-nowb`, `ldm9-base-wb`, `stm7-empty`, `thumb7-stmia-base-new`, `stm-pc12`, `ldm9-empty` (미확인) |
| C 계산 | `alu7-rn-pc12`, `qdadd9-dbl-sat`, `smla9-q-nosat`, `cp15-prev5-bit15` |
| D 세관 | `cp15-ctrl-bits`, `bkpt9-pabt`, `pu-priority`, `pu-background`, `pu-user-ro`, `pu-code-xn` |
| E 사물함 | `itcm-over-dtcm`, `dtcm-mirror`, `itcm-size`, `tcm-dma-bus`, `dtcm-load-mode` (미확인), `itcm-load-mode` (미확인) |

전제 조건(L1): 정렬된 LDR/STR 워드, ARM·Thumb 상태의 일반 LDM/STM, FIFO로 측정 단계에 응답하는 ARM7, SWI와 미정의
명령을 받는 예외 하네스, 읽기/쓰기 보호 영역, 쓴 값을 돌려주는 DTCM, 메인 RAM DMA 복사. 미확인 항목은 아직
실기 근거가 없어서 DS Lite 실행이 맞다고 확인하기 전까지는 정보용입니다([docs/QA-KO.md](docs/QA-KO.md)).

## 빌드와 판정

```sh
./build.sh demos        # quayholt-rehearsal.nds 와 부두별 ROM
python3 tools/judge.py --profile melonds-1.1-software run.log
```

`expected.json`은 항목과, 에뮬레이터별로 알려진 실패를 적습니다. melonDS 프로필은 인터프리터 기준이므로 melonDS의
JIT를 끄고 실행하세요.

## 라이선스

전부 Apache-2.0이고, ROM은 제3자 런타임 라이브러리와 그 고지도 담습니다(`THIRD_PARTY_NOTICES`, `licenses/`).
모든 에셋은 코드가 생성합니다([ASSETS-KO.md](ASSETS-KO.md)).
