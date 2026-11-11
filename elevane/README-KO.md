# Elevane

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#빌드)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#빌드)

[English](README.md)

닌텐도 DS용 오리지널 복셀 실루엣 퍼즐입니다. 블록을 쌓아 정면도와 평면도를 주문서에
맞춥니다. 두 화면 모두 모델을 3D로 보여 줍니다.

**상태.** 1장(퍼즐 8개)을 두 화면 3D와 2D 격자 모드로 플레이할 수 있고, 라이선스 뷰어가 있는
크레딧과 숨은 자동 QA 실행이 있습니다. 2~5장, 감상 모드, 정확도 리허설은 이후 변경에서 들어옵니다.
실기 측정값은 아직 없습니다.

- 언어: C (게임, ARM9 + 자체 ARM7 코어), Python 3 (빌드 도구)
- 툴체인: BlocksDS v1.25.0 (다이제스트로 고정한 Docker 이미지)
- 라이선스: Apache-2.0 — 서드파티 고지는 `THIRD_PARTY_NOTICES`, 에셋 출처는 `ASSETS.md`

## 빌드

```sh
./build.sh                  # elevane.nds
./build.sh SAVE=flash|fram  # 세이브 칩 변형
./build.sh -j8 release      # build/release/{rom,demos} (태그 elevane-vX.Y.Z가 CI에서 실행)
./build.sh clean
```

모든 빌드는 고정된 Docker 이미지 안에서 하며, 클린 빌드 두 번은 같은 SHA-256을 냅니다.
플레이: 타이틀에서 Start, 주문서를 고른 뒤 십자키와 A/B 또는 아래 화면 터치로 쌓습니다.
Select로 2D 격자와 3D를 바꿉니다. 자동 QA 실행: 타이틀에서 L+R+X+Y를 120프레임 누릅니다 ([docs/QA-KO.md](docs/QA-KO.md) 참고).
