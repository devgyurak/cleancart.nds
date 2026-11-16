# Paneveil

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#빌드)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#빌드)

[English](README.md)

닌텐도 DS용 오리지널 스테인드글라스 공방 게임입니다. 주문서에는 장미창 하나가 적혀 있습니다.
유리, 문양, 색, 꽃잎과 고리의 수입니다. 창을 주문서대로 맞춘 뒤 가마에 구우면 되고, 돌린
횟수가 적을수록 별을 더 받습니다. 위 화면은 뒤에서 빛을 받는 창을 3D로 보여 줍니다. 스스로
판정하도록도 만들었습니다. 리허설 ROM이 3D 지오메트리 엔진(행렬 스택, 행렬 load/multiply,
지오메트리 명령, 폴리곤·텍스처 속성)을 정확한 기대값과 비교해 결과 블록을 내므로, 다른
에뮬레이터가 회귀 시험으로 쓸 수 있습니다([에뮬레이터 시험으로 쓰기](#에뮬레이터-시험으로-쓰기)).

**시험하는 하드웨어 영역(레벨 1, "3D 지오메트리·텍스처"):** 행렬 스택(push, pop, store,
restore, 투영 스택, GXSTAT 스택 레벨), 행렬 load/multiply(CLIPMTX_RESULT, VECMTX_RESULT),
지오메트리 명령(packed GXFIFO와 직접 포트, 버텍스 포맷 6종), 폴리곤 속성(앞/뒷면, alpha,
와이어프레임, 깊이 시험), 텍스처 속성(repeat, flip, clamp, 색 0, 팔레트 base, 텍스처 좌표 원천).

**상태.** 1장(유리)과 2장(문양)을 플레이할 수 있습니다. 창 12개, 위 화면의 3D 창, 아래 화면의 설정, 2D 도식
모드(Select), 가마 램프, 리허설과 그 판정기(가마 점검과 라이트박스, 30항목), 숨은 자동 QA 실행이 있습니다.
3~5장은 아직 없습니다. 실기에서 측정한 값은 없습니다.

- 언어: C(게임, ARM9 + 자체 ARM7 코어), Python 3(빌드 시점 도구)
- 툴체인: BlocksDS v1.25.0(Docker 이미지를 digest로 고정)
- 라이선스: Apache-2.0 — 서드파티 고지는 `THIRD_PARTY_NOTICES`, 에셋 출처는 `ASSETS.md`

## 빌드

```sh
./build.sh                  # paneveil.nds
./build.sh DEMO=rehearsal   # paneveil-rehearsal.nds, judge.py가 채점하는 ROM
./build.sh -j8 release      # build/release/{rom,demos,judge}
```

빌드는 Docker 컨테이너(digest로 고정한 BlocksDS) 안에서 돕니다. 호스트에는 아무것도 설치하지
않습니다. 클린 빌드를 두 번 하면 SHA-256이 같습니다.

## 에뮬레이터 시험으로 쓰기

`paneveil-rehearsal.nds`(`./build.sh DEMO=rehearsal`)는 판정을 돌린 뒤 0x0237F000에 64워드 결과 블록을
남기고(배치는 `arm9/source/game/rehearsal.h`), `tools/judge.py`가 이를 `expected.json`(목표 레벨 1,
영역 "3D geometry and texturing")과 비교합니다. 세이브 칩도 NitroFS도 필요 없습니다.

**가마 점검**은 고정된 명령열을 지오메트리 엔진에 보내고(직접 포트와 packed GXFIFO), GXSTAT 27번 비트가
0이 되기를 기다린 뒤, GXSTAT 스택 레벨과 `CLIPMTX_RESULT`(16워드), `VECMTX_RESULT`(9워드)를 GBATEK 규칙의
정수 모델과 비교합니다. 행렬 원소는 모두 1.19.12에서 정확히 표현되므로 곱셈 반올림 방식과 무관하고,
렌더러도 관여하지 않습니다.

```sh
tools/melonds/mds.sh run games/paneveil/paneveil-rehearsal.nds 400 out/pv "" 399 \
  | python3 games/paneveil/tools/judge.py --profile melonds-1.1-software
RESULT_WORDS=64 tools/emu/emu.sh run games/paneveil/paneveil-rehearsal.nds 400 out/pv "" 399 \
  | python3 games/paneveil/tools/judge.py --profile desmume-pydesmume-0.0.9
```

종료 코드 0: 게이트 항목이 모두 통과했거나(또는 실패 집합이 프로필과 같음). `expected.json`의 `known`에
에뮬레이터 버전·렌더러마다 프로필이 하나씩 있습니다.
