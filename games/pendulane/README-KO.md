# Pendulane

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#빌드)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#빌드)

[English](README.md)

닌텐도 DS용 오리지널 자동인형 공방 게임입니다 (제작 중). 스스로를 판정하도록 만들어졌습니다: 리허설 ROM이 콘솔에서 CPU·버스·사운드
타이밍을 재고 (지금은 나눗셈·제곱근 장치의 busy 시간, 이어서 SPI, ARM7 메모리, 카트 gap, 사운드 시작 지연) 결과 블록을 내놓으므로,
다른 에뮬레이터가 회귀 테스트로 쓸 수 있습니다.

**검사하는 하드웨어 영역 (레벨 3, "CPU, bus and sound timing"):** 지금은 GBATEK "DS Maths"의 하드웨어 나눗셈(모드 0, 1, 2)과
제곱근 장치의 busy 시간입니다. 모든 값은 ITCM의 nop 슬라이드로 찾은 두 엣지의 차이라서 측정 자체의 고정 비용이 상쇄됩니다.

**상태.** 뼈대와 수학 벤치입니다. 장치 게임은 아직 없고 타이틀 화면은 임시입니다. 실기에서는 아무것도 재지 않았습니다.

- 언어: C (게임, ARM9 + 자체 ARM7 코어), Python 3 (빌드 도구)
- 툴체인: BlocksDS v1.25.0 (digest로 고정한 Docker 이미지)
- 라이선스: Apache-2.0 — 서드파티 고지는 `THIRD_PARTY_NOTICES`, 에셋 출처는 `ASSETS.md`

## 빌드

```sh
./build.sh                  # pendulane.nds
./build.sh DEMO=rehearsal   # 리허설 ROM
./build.sh clean
```

## 에뮬레이터 테스트로 쓰기

`pendulane-rehearsal.nds`를 부팅해 약 60프레임을 돌린 뒤 `0x0237F000`의 64워드 결과 블록(id `PDRH`)을 읽습니다. 채점은
[English README](README.md#using-it-as-an-emulator-test)의 명령과 같습니다. `judge.py`는 판정 항목이 모두 OK(또는 프로파일 일치)이면
0으로 끝납니다. 항목은 `expected.json`에 GBATEK 섹션과 난이도 레벨과 함께 있습니다 (저장소 루트 `docs/LEVELS-KO.md`).

## 조작

타이틀: Start는 벤치 장부(측정을 바로 보는 화면), X는 크레딧. melonDS 키 배치는 저장소 `AGENTS.md`를 보세요.
