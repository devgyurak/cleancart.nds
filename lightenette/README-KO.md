# Lightenette

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#빌드)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#빌드)

[English](README.md)

닌텐도 DS용 오리지널 퍼즐 액션 게임입니다. 등불을 들고 터치스크린으로 마리오네트
인형극을 조종합니다. 동시에 ROM이 스스로 판정하도록 만들었습니다. 자기 화면을
캡처해 정해진 색의 픽셀 수를 세고, 에뮬레이터가 실기와 어긋난 적이 있는 항목의
통과/실패를 보고하므로 다른 에뮬레이터가 회귀 테스트로 쓸 수 있습니다.

**상태: 개발 중.** 타이틀 화면까지 동작하며, 스테이지와 판정 모듈은 단계별로 추가합니다.

- 언어: C(게임, ARM9 + 자체 ARM7 코어), Python 3(빌드 시 도구)
- 툴체인: BlocksDS v1.25.0 (digest로 고정한 Docker 이미지)
- 라이선스: Apache-2.0 — 서드파티 고지는 `THIRD_PARTY_NOTICES`, 에셋 출처는 `ASSETS.md`

## 빌드

Docker만 있으면 됩니다.

```sh
./build.sh                    # lightenette.nds
./build.sh SAVE=flash         # lightenette-save-flash.nds
./build.sh SAVE=fram          # lightenette-save-fram.nds
./build.sh -j8 release        # build/release/{rom,demos}: lightenette-vX.Y.Z 태그가 배포하는 것
./build.sh clean
```

깨끗한 빌드를 두 번 하면 SHA-256이 같습니다.

## 정확도 판정

타이틀 + Select(또는 `lightenette-acc-stage.nds` 데모 ROM)로 정지 장면을 돌려, melonDS가 실기와
다르다고 보고된 세 항목을 판정합니다: 역방향 텍스처 좌표([#2008](https://github.com/melonDS-emu/melonDS/issues/2008)),
밝기 효과와 함께 쓰는 반투명 OBJ([#2503](https://github.com/melonDS-emu/melonDS/issues/2503)), 엣지 마킹·라인 폴리곤([#2549](https://github.com/melonDS-emu/melonDS/issues/2549)).
값은 화면과 `0x0237F000`의 결과 블록(구조는 `arm9/source/judge/judge.h`)에 나옵니다. NitroFS 파일은 필요 없습니다.

## 구성

```
arm9/source/game/   장면
arm9/source/judge/  정확도 판정(캡처, 픽셀 세기, 결과 블록)
arm9/source/demo/   데모 ROM 진입점(acc-stage)
arm9/source/core/   시스템, 입력, DMA, 세이브, 사운드(corvane 계층)
arm9/source/gfx/    2D/3D 계층(corvane 계층)
arm7/               자체 ARM7 코어
tools/              에셋 생성기(Python 3, 표준 라이브러리만)
```
