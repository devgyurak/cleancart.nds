# Tonewick

[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![BlocksDS v1.25.0](https://img.shields.io/badge/BlocksDS-v1.25.0-blue)](#빌드)
[![Platform: Nintendo DS](https://img.shields.io/badge/platform-Nintendo%20DS-lightgrey)](#빌드)

[English](README.md)

닌텐도 DS용 오리지널 청음 퍼즐입니다. 플레이어는 황야의 감청소를 지키는 신호지기로, 오실로스코프로 네 지역에 걸친
끊긴 신호선 12개를 고칩니다. 신호선 하나는 콘솔 사운드 블록의 레지스터 필드 하나입니다. 경계값이 곧 의도한 풀이입니다.
duty 7인 사각파는 바닥을 떠나지 않고, 4워드보다 짧은 테이프는 선을 계속 busy로 두면서 소리를 내지 않으며, 캡처 길이 0은
1워드를 씁니다. ROM은 캡처 유닛으로 자기 사운드 블록의 소리를 직접 듣기 때문에 게임이 스스로를 판정하기도 합니다.
리허설 ROM이 사운드 채널, 캡처 유닛, 그리고 이웃한 장치(RTC, 전원관리, 세이브 칩, 펌웨어 설정)의 예외·경계값·문서화된
버그를 정확한 기댓값과 비교해 결과 블록으로 내보내므로, 다른 에뮬레이터도 회귀 테스트로 쓸 수 있습니다.

**검사하는 하드웨어 영역 (레벨 2, "소리·입출력 경계"):** 사운드 채널의 경계(duty 7, 분주, 볼륨 레지스터 127, 4워드보다 짧은
테이프, hold 비트, ADPCM 루프 재적재, 노이즈 시프트 레지스터), 사운드 캡처 유닛(길이 0, 채널 소스, add 오버플로 버그와
둘 다 음수인 버그), 그리고 옆 장치들(24시간 모드의 RTC AM/PM 플래그, 전원관리 레지스터 미러, EEPROM 상태 비트, 가장 최신
펌웨어 사용자 설정 사본).

**현재 상태.** ARM7 사운드 실험실(ARM9가 넘기는 레지스터 스크립트와 메인 RAM의 캡처 버퍼), 실험, 네 지역의 신호선
12개, 리허설과 판정기(목표 17개, 전제 조건 10개. 세이브 항목은 세이브 칩이 있을 때만 셉니다), 숨은 자동 QA 실행이
있습니다. 스코프(모델 파형과 귀가 캡처한 파형)와 터치 조작부는 들어 있고, 아침 점검 일지 장면은 이어서 만듭니다. 실기에서는 아직 측정하지 않았습니다. 진행 상황은
저장하지 않습니다.

- 언어: C(게임, ARM9 + 자체 ARM7 코어), Python 3(빌드 도구)
- 툴체인: BlocksDS v1.25.0 (다이제스트로 고정한 Docker 이미지)
- 라이선스: Apache-2.0 — 서드파티 고지는 `THIRD_PARTY_NOTICES`, 에셋 출처는 `ASSETS.md`

## 빌드

```sh
./build.sh                  # tonewick.nds
./build.sh DEMO=rehearsal   # tonewick-rehearsal.nds, judge.py가 채점하는 ROM
./build.sh -j8 release      # build/release/{rom,demos,judge}
```

빌드는 Docker 컨테이너(다이제스트로 고정한 BlocksDS)에서 돌며 호스트에는 아무것도 설치하지 않습니다.
클린 빌드를 두 번 하면 SHA-256이 같습니다.
