# Corvane QA

[English](QA.md)

기획서의 QA 요건: 모든 데모 빌드와 본 빌드를 **melonDS**에서 확인하고, 검수 기록에
melonDS 버전·데모 이름·기대 화면 설명을 남깁니다.

## 데모 빌드 공통 규칙

- 빌드: `./build.sh DEMO=<이름>` → `corvane-<이름>.nds` (`./build.sh demos`로 전부)
- 화면: 메인 화면 text BG 콘솔에 데모 이름과 결과가 나옵니다(3D 데모는 BG1 콘솔).
- 체크포인트: 시작 후 120프레임에 화면을 완성하고 `CHECKPOINT` 두 줄을 출력합니다.
  같은 값이 메인 RAM `0x0237F000`에 기록됩니다.

```
0x0237F000 u32 magic   = 0x52455643 ("CVER")
0x0237F004 u32 demo_id = FNV-1a 32 해시(데모 이름)
0x0237F008 u32 frame   = 체크포인트 시점의 VBlank 수
0x0237F00C u32 values[13] (데모마다 아래 표의 값)
```

- 입력이 필요한 데모(sys-keys, io-touch, io-power, io-rtc, io-firmware)는 체크포인트 뒤에도
  화면을 계속 갱신합니다.

## 검수 기록

melonDS 버전: ______  검수자: ______  날짜: ______

| 데모 | 확인할 것 / 기대 화면 | 체크포인트 값 (values[0..3]) | melonDS 결과 |
|---|---|---|---|
| sys-ipc | 콘솔에 IPCSYNC 값 A, FIFO 핑 1~2프레임, 입력 패킷 ≈120 | IPCSYNC, 핑 프레임, 패킷 수, KEYXY | |
| sys-wram | WRAMCNT 3→0→3, 32 KB 패턴 오류 0 | 오류 수, CRC16, 최종 WRAMCNT | |
| sys-irq | 위 절반 파랑(VBlank) / 아래 절반 빨강(VCount 96), 카운트 표시 | VBlank 120, HBlank ≈31,000, VCount 120, 타이머 ≈20 | |
| sys-timer | 분주비 4종, 캐스케이드 초 ≈2, 박자 ≈16 | 초, 박자, 1:1 측정 µs, 1:64 측정 µs | |
| sys-dma-now | 4 KB 복사 오류 0 | 오류 수 | |
| sys-dma-vblank | 배경색이 매 프레임 바뀜, 검증 120/120 | 검증 프레임 수 | |
| sys-dma-hblank | 위→아래 파랑→빨강 그라데이션 배경 | 라인 100 색, HBlank 수 | |
| sys-dma-cart | 카트 DMA CRC = NitroFS CRC = 기대값 | 찾음(1), CRC×3 | |
| sys-dma-gfx | 텍스처 큐브 회전 | 리스트 워드 수, 폴리곤 수 | |
| sys-dma-disp | 위: 컬러 그라데이션(메인 메모리 FIFO) / 아래: 라인별 바 | 픽셀 값, 바 색 | |
| sys-dma-fill | VRAM·RAM fill 오류 0 | 오류 수, 값 | |
| sys-bios | LZ77 VRAM=WRAM, CpuSet/FastSet, Div 142, Sqrt 1000, CRC16 | 차이 0, FastSet 값, 142, CRC | |
| sys-math | 나눗셈 4건, sqrt(1e12)=1000000, len(3,4,0)=5×4096, atan2=4096 | 오류 수, sqrt, len, atan2 | |
| sys-keys | 버튼/X/Y/터치/뚜껑 실시간 표시, 키 IRQ 카운트 | KEYS, 키 IRQ, KEYXY | |
| 2d-engines | 위 파랑·아래 빨강, 60프레임마다 화면 교대 | POWERCNT, DISPCNT ×2 | |
| 2d-text | 아래 화면 HUD(4bpp) + 하단 8bpp 콘솔 줄 | BG3CNT_SUB, BG0CNT_SUB | |
| 2d-affine | 회전하는 하늘(어파인) + 커지는 CORVANE 로고(확장 어파인) | BG2PA, BG3PA | |
| 2d-bitmap | 0~59: 16bpp 타이틀 + 8bpp 로고, 60~: 1024x512 성운 스크롤 | DISPCNT, BG2CNT, 단계 2 | |
| 2d-mosaic | 글자와 폭발 스프라이트가 점점 굵은 모자이크 | MOSAIC | |
| 2d-obj-normal | 100개 탄 스프라이트 원형 이동, 절반은 BG 뒤 | OAM 수 100 | |
| 2d-obj-affine | 색이 다른 운석 4개 회전·확대 + 폭발 확대 | OAM 수 | |
| 2d-obj-bitmap | 64x64 조종사 컷인 2개(하나는 반투명) | OAM 수 | |
| 2d-obj-map2d | 위(1D)와 아래(2D 매핑)에 같은 폭발 그림 | DISPCNT_SUB | |
| 2d-obj-line | 한 줄에 몰린 120개 큰 탄 — 일부 사라짐(라인 한계) | OAM 수 | |
| 2d-extpal | 같은 일러스트 왼쪽 원색 / 오른쪽 색 바뀜 + 운석 4색 | DISPCNT, 맵 엔트리 | |
| 2d-window | 두 사각 창 안만 밝음, 아래 레이더 원형 마스크 | WININ, WINOUT, WINOUT_SUB | |
| 2d-blend | 30프레임마다 알파 / 밝게 / 어둡게 / 반투명 OBJ | BLDCNT, BLDALPHA | |
| 2d-brightness | 위는 흰색으로, 아래는 검게 따로 페이드 | MASTER_BRIGHT ×2 | |
| 2d-vram | 8개 배치의 VRAMCNT A~I 표 | 표 해시 | |
| 2d-capture | 위·아래 모두 3D(보스 코어 / 기체) | DISPCAPCNT (콘솔 없음) | |
| 2d-display | 0~59 VRAM 표시, 60~ 메인 메모리 FIFO 표시 | 픽셀 값 | |
| 3d-fifo | 지형(포트 쓰기) 위 기체(DMA 리스트) | 폴리곤·정점 수, 리스트 크기 | |
| 3d-matrix | 패널·팔·집게가 계층으로 움직이는 보스 | 보스 HP 합, 폴리곤 수 | |
| 3d-light | 4색 광원이 도는 정거장 | 폴리곤 수 | |
| 3d-clip | 카메라가 벽을 스치며 근평면 클리핑 | 폴리곤·정점 수 | |
| 3d-tex-a3i5 / a5i3 / pal4 / pal16 / pal256 / 4x4 / direct | 해당 형식 텍스처 사각형 회전, 반복·뒤집기·텍스처 행렬 스크롤 | 텍스처 번호, 크기 | |
| 3d-poly | 줄무늬 바닥, 앞/뒤 컬링 큐브, 반투명 비행선, 기체 그림자 | 폴리곤 수 | |
| 3d-toon | 0~59 툰 / 60~ 하이라이트 셰이딩 | DISP3DCNT | |
| 3d-edge | 폴리곤 ID별 외곽선 | DISP3DCNT | |
| 3d-fog | 0~59 거리 포그 / 60~ 알파 전용 포그 | DISP3DCNT | |
| 3d-aa | AA 켜진 큐브 + 철망 알파 테스트 | DISP3DCNT | |
| 3d-depth | 0~59 W버퍼·단색 후면 / 60~ Z버퍼·비트맵 후면 | DISP3DCNT | |
| 3d-test | BOX INSIDE/OUTSIDE 전환, POS·VEC 결과 | 안 횟수, w, vec x, vec z | |
| 3d-compose | 3D 큐브가 2D 바다 앞→뒤, 3D 위 탄 스프라이트, BG0 흔들림 | BG0CNT, BG2CNT | |
| 3d-limits | 삼각형이 2048을 넘어가며 일부 사라짐, 오버플로 프레임 > 0 | 오버플로 프레임, 폴리곤 RAM | |
| snd-mix | 보스 BGM + 8가지 효과음 좌우 판닝 | 곡 번호, 8 | |
| snd-pcm | PCM8 단발 → PCM16 단발 → PCM8 루프 → PCM16 루프 | 샘플 번호 | |
| snd-adpcm | ADPCM 징글(단발) → 험(루프) | 샘플 번호 | |
| snd-psg | 채널 8~13 듀티 변화 비프, 노이즈 | 8, 15 | |
| snd-capture | 스테이지 3 곡 + 메아리 | 1 | |
| snd-stream | 끊김 없는 앰비언트 패드 | 크기, 샘플레이트 | |
| io-cart | NitroFS 파일 크기 목록, 카트 DMA OK, 스테이지 3 정보 | 합계, 로드 성공, 카트 DMA, 스폰 수 | |
| io-save-eeprom / flash / fram | WRITE+VERIFY OK, RELOAD OK | 쓰기, 재로드, seq 증가 1, 칩 응답 | |
| io-firmware | 이름, 선호 색 → 기체 색, 언어, 생일, 터치 보정값 | 색, 언어, 이름 길이, 기체 색 | |
| io-rtc | 날짜·시각이 매초 갱신, 스테이지 2 낮/밤 | 연, 월, 일, 시 | |
| io-touch | 펜 위치에 조준선, 픽셀/RAW 값 | 터치 중, x, y, raw x | |
| io-power | A: 위 백라이트, B: 스피커, 뚜껑 닫으면 절전 | 배터리, 뚜껑, 백라이트, 스피커 | |

## 본 빌드 검수 체크리스트

| 항목 | 확인 방법 | melonDS 결과 |
|---|---|---|
| 부팅 로고 → 인트로 → 타이틀 | 그대로 실행 | |
| 데모 플레이 | 타이틀에서 20초 대기, 다섯 스테이지가 차례로 나옴, 키를 누르면 타이틀 | |
| 새 게임 → 1~5 스테이지 → 엔딩 → 스태프 롤 → 기록 | 처음부터 끝까지 플레이 | |
| 컨티뉴 | 잔기를 모두 잃기, A로 같은 스테이지 처음부터 | |
| 이어하기 | 스테이지 클리어 후 전원 끄고 다시 켜서 CONTINUE | |
| 기록 | 날짜·시각이 RTC 값으로 저장되는지 | |
| 리플레이 | 기록 화면에서 X, 같은 판이 그대로 재생되는지 | |
| 일시정지 | Start, R로 한 프레임씩 진행, 10초 이상 두면 백라이트 꺼짐 → 키로 복귀 | |
| 뚜껑 | 플레이 중 닫기 → 절전 → 열면 일시정지 상태 | |
| 화면 스왑 설정 | 설정에서 SCREEN SWAP ON → 3D가 아래, 터치 조준 | |
| 라이선스 | 크레딧 → X로 THIRD_PARTY_NOTICES, MPL-2.0, picolibc 등 원문 표시 | |
| 소리 | 끊김·잡음 없음, 보스전 16채널 | |

## 자동 스모크 테스트 기록 (DeSmuME)

melonDS 검수 전에, 이 저장소를 만든 세션에서 헤드리스 DeSmuME 0.9.12(py-desmume 0.0.9,
x86_64 컨테이너)로 다음을 확인했습니다. melonDS 검수를 대신하지는 않습니다.

- 데모 65종: 모두 120프레임 체크포인트 도달(결과 블록 magic 확인). DeSmuME는 폴리곤 RAM
  사용량과 오버플로 플래그를 보고하지 않아 `3d-limits` 값은 0으로 나옵니다.
- 세이브: EEPROM 64 KB / FLASH / FRAM 각각 쓰기·검증·재로드 성공.
- 카트 DMA: probe.bin CRC가 NitroFS 경로·기대값과 일치.
- 본 빌드: 부팅부터 스테이지 1~5, 각 보스, 결과, 양화면 3D 엔딩, 스태프 롤, 기록 저장까지 진행.
- 결정론: 스테이지 1을 입력과 함께 녹화한 판과 기록 화면에서 재생한 판의 스테이지 상태
  메모리(CvStage, 25.5 KB)가 300·600프레임에서 같음(다른 바이트는 힙 포인터와 메시지 문자열뿐).
