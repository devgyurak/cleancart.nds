# Ledgerine

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#빌드)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#빌드)

[English](README.md)

닌텐도 DS용 오리지널 기계실 장부 퍼즐입니다. 플레이어는 기송관으로 이어진 탑 두 개(위·아래 화면)에 걸친
오래된 회계소의 야간 서기입니다. 방마다 다이얼과 레버가 달린 기계가 하나씩 있고, 레버를 당기면 콘솔의 실제
하드웨어 장치가 돌아가 그 결과가 장부에 적힙니다. 경계값이 곧 의도한 풀이입니다. 서기 0명에게 나눈 동전 7개는
7개 그대로 돌아오고, 빈 기송관은 마지막 캡슐을 메아리로 돌려줍니다. 게임은 스스로를 판정하기도 합니다.
리허설 ROM이 시스템 하드웨어(수학 장치, FIFO, 타이머, DMA, WRAM·VRAM 제어, BIOS)의 예외·경계값·에러 경로를
정확한 기댓값과 비교해 결과 블록으로 내보내므로, 다른 에뮬레이터도 회귀 테스트로 쓸 수 있습니다.

**검사하는 하드웨어 영역 (레벨 2, "시스템 경계"):** DS 수학 장치의 예외와 경계(0으로 나누기, 오버플로, 예약 모드,
64비트 제곱근), IPC FIFO(빈 읽기, 에러 비트, full, 꺼짐, 엣지 트리거 IRQ), 타이머(cascade, reload, 멈춘 값 읽기),
DMA(repeat reload 규칙, 전송 뒤 레지스터 상태, DMA 중 ITCM 코드 실행), WRAM·VRAM 제어(절반 교체, 미러, 바이트 쓰기,
뱅크 오프셋), BIOS(CpuFastSet, LZ77, BitUnPack, RLUnComp, ARM7 CpuSet 보호).

**현재 상태.** 뼈대입니다. 타이틀, 테스트 메뉴, 라이선스 뷰어가 있는 크레딧, 방 장면 임시본, 리허설 임시본, 빈 방
여섯 개를 지나는 숨은 자동 QA 실행이 들어 있습니다. 여섯 방과 판정기는 뒤따르는 변경에서 들어옵니다. 실기에서는
아무것도 측정하지 않았습니다.

- 언어: C (게임, ARM9 + 자체 ARM7 코어), Python 3 (빌드 도구)
- 툴체인: BlocksDS v1.25.0 (다이제스트로 고정한 Docker 이미지)
- 라이선스: Apache-2.0 — 서드파티 고지는 `THIRD_PARTY_NOTICES`, 에셋 출처는 `ASSETS.md`

## 빌드

```sh
./build.sh                  # ledgerine.nds
./build.sh DEMO=rehearsal   # 리허설 ROM
./build.sh demos            # 모든 데모 ROM
./build.sh -j8 release      # build/release/{rom,demos,judge}
```

모든 빌드는 고정된 BlocksDS 컨테이너 안에서 이루어지며, 클린 빌드 두 번은 같은 SHA-256을 냅니다.
문서: [QA 점검표](docs/QA-KO.md) · [에셋](ASSETS-KO.md).
