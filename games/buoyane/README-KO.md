# Buoyane

[English](README.md)

닌텐도 DS용 오리지널 밤 항구 접근 게임입니다 (작업 중: 지금은 타이틀·크레딧·판정만 있는 뼈대이고, 해도와 항해가 이어집니다).
작은 배를 몰아 지어낸 항구로 들어가며, 불빛이 어떻게 보이는지로 각 불빛의 이름을 맞힙니다. 스스로 판정도 합니다:
리허설 ROM과 확인 ROM 다섯 개가 픽셀 정확한 3D 장면을 그려 캡처하고, 다른 에뮬레이터가 회귀 테스트로 쓸 수 있는 결과 블록을 냅니다.

**시험하는 하드웨어 영역 (레벨 2, "3D 렌더링 가장자리"):** 폴리곤 규칙(1-dot 폴리곤과 DISP_1DOT_DEPTH, 깊이 테스트 Equal, far plane 비트,
POLYGON_ATTR 래치, 알파 테스트), 텍스처 블렌딩(highlight, decal, 와이어프레임), fog·edge marking·안티앨리어싱 후처리.
기본 동작은 레벨 1 전제 조건이 먼저 지킵니다.

## 빌드

```sh
./build.sh                 # buoyane.nds
./build.sh demos           # 리허설 + 확인 ROM
```

Docker가 필요합니다(고정된 이미지의 BlocksDS v1.25.0). 클린 빌드 두 번은 같은 SHA-256을 냅니다.

## 에뮬레이터 테스트로 쓰기

`buoyane-rehearsal.nds`(약 200프레임) 또는 `buoyane-check-{dot,fog,edge,depth,blend}.nds`(약 150프레임)를 돌려 `0x0237F000`의
64워드 결과 블록을 읽고 판정합니다:

```sh
tools/melonds/mds.sh run games/buoyane/buoyane-rehearsal.nds 300 out/x "" 299 | python3 games/buoyane/tools/judge.py --profile melonds-1.1-software
```

`expected.json`에 항목(레벨, 근거, 역할)과, `known`에 각 에뮬레이터가 지금 틀리는 항목(2026-11-17 측정)이 있습니다:
melonDS 1.1 소프트웨어: 없음, GL Compute: `edge-aa-translucent`, GL Classic: 15개, DeSmuME py-desmume 0.0.9: 13개. 기대값은 GBATEK 기준이고,
실기 값이 없는 네 항목은 정보용입니다. 배치: `arm9/source/game/rehearsal.h`.

## 크레딧

소스: Apache-2.0. 모든 그래픽·소리·곡은 이 저장소의 코드가 생성합니다([ASSETS-KO.md](ASSETS-KO.md)). BlocksDS로 빌드했고,
서드파티 고지는 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES)에 있습니다.
