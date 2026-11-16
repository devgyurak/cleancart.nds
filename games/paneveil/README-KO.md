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

**상태.** 뼈대입니다. 타이틀, 창 목록, 라이선스 뷰어가 있는 크레딧, 숨은 자동 QA 실행이 있습니다.
창, 플레이 장면, 판정기는 아직 없습니다. 실기에서 측정한 값은 없습니다.

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

판정기는 판정기 변경에서 들어옵니다. `docs/QA-KO.md`를 보세요.
