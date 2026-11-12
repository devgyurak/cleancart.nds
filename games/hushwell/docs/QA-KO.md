# Hushwell QA

[English](QA.md)

기획서의 QA 요건: 모든 데모 빌드와 본 빌드를 **melonDS**에서 확인하고, 검수 기록에 melonDS
버전, 데모 이름, 기대 화면을 남깁니다.

## 모든 데모 빌드 공통 규칙

- 빌드: `./build.sh DEMO=<이름>` → `hushwell-<이름>.nds` (전부는 `./build.sh demos`)
- 화면: 메인 화면의 텍스트 BG 콘솔에 데모 이름과 결과가 나옵니다(3D 데모는 BG1 콘솔).
- 체크포인트: 시작 후 120프레임에 화면이 완성되고 `CHECKPOINT` 두 줄이 찍힙니다. 같은 값을
  메인 RAM `0x0237F000`에 씁니다.

```
0x0237F000 u32 magic   = 0x52455643 ("CVER")
0x0237F004 u32 demo_id = 데모 이름의 FNV-1a 32 해시
0x0237F008 u32 frame   = 체크포인트 시점의 VBlank 수
0x0237F00C u32 values[13] (데모별, 아래 표)
```

- 입력이 필요한 데모(sys-keys, io-touch, io-power, io-rtc, io-firmware)는 체크포인트 뒤에도
  화면을 계속 갱신합니다.

## 자동 QA 실행

숨김 모드(출시 ROM과 같은 ROM): 타이틀 화면에서 **L+R+X+Y를 120프레임 누르고 있으면** 게임이 프롤로그와
챕터 1~5를 스스로 걷습니다(괴물에게 맞아도 체력이 닳지 않는 것 외에는 출시 그대로). 문을 열고, 챕터 3
다이얼 코드를 맞추고, 테이프를 전부 모아 우물 엔딩을 보고, 크레딧을 거쳐 기록 화면에서 멈춥니다.
로그는 SD 카드의 `cleancart/hushwell/qa-<날짜>-<시각>.log`, 결과 블록은 `0x0237F000`의 `QARN`입니다.
헤드리스: `SDDIR=out/mds/x tools/melonds/mds.sh run games/hushwell/hushwell.nds 40000 out/mds/x
"700:L:140,700:R:140,700:X:140,700:Y:140"` 후 `tools/qa/parse_log.py`.

| 환경 | 결과 (2026-11-11, 이 트리의 빌드) |
|---|---|
| melonDS 1.1 헤드리스, 소프트웨어 렌더러, SD 이미지 | ✅ 23,644프레임에 `RESULT PASS reason=credits`, 챕터 체크포인트 6개, 테이프 전부(`3e`), 엔딩 2(우물), 세이브 미러 7개 읽기 검증 일치(`chip_type=-1`: 홈브루 실행, 세이브 칩 없음) |
| 같은 ROM 두 번째 실행 | ✅ 체크포인트 해시 6개 전부 동일(`tools/qa/compare.py`) |
| DeSmuME 헤드리스(SD 없음) | ✅ `QARN` 블록 판정 1, 챕터 6개, 테이프 `3e`, 마지막 상태 해시 `fea01683` = melonDS(랙은 다름, 비교 대상 아님) |
| 실기 DS Lite + R4 | 아직 안 돌림 |

## 검수 기록

melonDS 버전: ______  검수자: ______  날짜: ______

| 데모 | 확인할 것 / 기대 화면 | 체크포인트 값 (values[0..3]) | melonDS 결과 |
|---|---|---|---|
| sys-ipc | 콘솔에 IPCSYNC 값 A, FIFO 핑 1~2프레임, 입력 패킷 약 120 | IPCSYNC, 핑 프레임, 패킷 수, KEYXY | |
| sys-wram | WRAMCNT 3→0→3, 32 KB 패턴 오류 0 | 오류 수, CRC16, 최종 WRAMCNT | |
| sys-irq | 위 절반 파랑(VBlank) / 아래 절반 빨강(VCount 96), 카운터 표시 | VBlank 120, HBlank 약 31,000, VCount 120, 타이머 약 20 | |
| sys-timer | 분주비 네 가지, 캐스케이드 초 약 2, 박자 약 16 | 초, 박자, 1:1 측정 µs, 1:64 측정 µs | |
| sys-dma-now | 4 KB 복사 오류 0 | 오류 수 | |
| sys-dma-vblank | 배경색이 매 프레임 바뀜, 120/120 검증 | 검증 프레임 | |
| sys-dma-hblank | 위에서 아래로 파랑→빨강 그라데이션 배경 | 100번 줄 색, HBlank 수 | |
| sys-dma-cart | 카트 DMA CRC = NitroFS CRC = 기대값 | 찾음(1), CRC ×3 | |
| sys-dma-gfx | 텍스처 입힌 정육면체 회전 | 리스트 워드 수, 폴리곤 수 | |
| sys-dma-disp | 위: 색 그라데이션(메인 메모리 FIFO) / 아래: 줄마다 막대 | 픽셀 값, 막대 색 | |
| sys-dma-fill | VRAM·RAM 채우기 오류 0 | 오류 수, 값 | |
| sys-bios | LZ77 VRAM = WRAM, CpuSet/FastSet, Div 142, Sqrt 1000, CRC16 | 차이 0, FastSet 값, 142, CRC | |
| sys-math | 나눗셈 4개, sqrt(1e12)=1000000, len(3,4,0)=5×4096, atan2=4096 | 오류 수, sqrt, len, atan2 | |
| sys-keys | 버튼/X/Y/터치/뚜껑 실시간 표시, 키 IRQ 수 | KEYS, 키 IRQ, KEYXY | |
| 2d-engines | 위 파랑, 아래 빨강, 60프레임마다 화면 스왑 | POWERCNT, DISPCNT ×2 | |
| 2d-text | 위: 콘솔 / 아래: 챕터 HUD(지도 틀, 게이지, 미터)와 콘솔 두 줄 | BG3CNT_SUB, BG0CNT_SUB | |
| 2d-affine | 도는 손그림 지도 타일(어파인) + 커지는 HUSHWELL 로고(확장 어파인) | BG2PA, BG3PA | |
| 2d-bitmap | 0~59: 16bpp 타이틀 + 8bpp 로고, 60~: 모든 챕터 맵을 그린 1024x512 도면 스크롤 | DISPCNT, BG2CNT, 단계 2 | |
| 2d-mosaic | 글자와 배터리 스프라이트가 점점 굵은 모자이크 | MOSAIC | |
| 2d-obj-normal | 먼지·빗방울 스프라이트 100개가 원을 그리며 이동, 절반은 BG 뒤 | OAM 수 100 | |
| 2d-obj-affine | 네 가지 색의 초상화 4개 회전·확대 + 커지는 신호탄 아이콘 | OAM 수 | |
| 2d-obj-bitmap | 64x64 찢어진 사진 두 장(하나는 반투명) | OAM 수 | |
| 2d-obj-map2d | 같은 테이프 아이콘이 위(1D)와 아래(2D 매핑)에 | DISPCNT_SUB | |
| 2d-obj-line | 한 띠에 열쇠 스프라이트 120개 — 일부가 사라짐(라인 한계) | OAM 수 | |
| 2d-extpal | 무선 교신 그림: 왼쪽 원래 색 / 오른쪽 바뀐 색 + 초상화 하나를 확장 팔레트 4개로 | DISPCNT, 맵 엔트리 | |
| 2d-window | 위: 두 사각형만 밝음 / 아래: 열쇠구멍 OBJ 윈도우로 보이는 HUD | WININ, WINOUT, WINOUT_SUB | |
| 2d-blend | 30프레임마다 알파 / 밝게 / 어둡게 / 반투명 OBJ | BLDCNT, BLDALPHA | |
| 2d-brightness | 위는 흰색으로, 아래는 검정으로 따로 페이드 | MASTER_BRIGHT ×2 | |
| 2d-vram | 배치 10가지의 VRAMCNT A~I 표 | 표 해시 | |
| 2d-capture | 양쪽 화면 3D(탈출 엔딩: 길 위의 차 / 우물) | DISPCAPCNT(콘솔 없음) | |
| 2d-display | 0~59 VRAM 표시, 60~ 메인 메모리 FIFO 표시 | 픽셀 값 | |
| 3d-fifo | 1장 병동에서 제자리 회전: 벽은 포트 쓰기, 무선기와 신호탄 상자는 DMA 리스트 | 폴리곤/정점 수, 무선기 리스트 크기, 맵 로드 | |
| 3d-matrix | 제자리에서 걷는 그것, 머리·팔·다리가 push/pop 계층으로 움직임 | 몸통 리스트 크기, 폴리곤 수 | |
| 3d-light | 네 가지 색 조명이 도는 제단 | 폴리곤 수 | |
| 3d-clip | 병동 벽에 바짝 붙은 카메라, near plane 클리핑 | 폴리곤/정점 수, 맵 로드 | |
| 3d-tex-a3i5 / a5i3 / pal4 / pal16 / pal256 / 4x4 / direct | 그 형식 텍스처의 사각형이 반복·뒤집기·텍스처 행렬 스크롤과 함께 회전 | 텍스처 id, 크기 | |
| 3d-poly | 타일 바닥, 앞면/뒷면 컬링 정육면체, 반투명 자동차, 무선기 그림자 | 폴리곤 수 | |
| 3d-toon | 0~59 툰 / 60~ 하이라이트 셰이딩 | DISP3DCNT | |
| 3d-edge | 폴리곤 ID별 외곽선 | DISP3DCNT | |
| 3d-fog | 프롤로그 산길: 0~59 거리 포그 / 60~ 알파 전용 포그 | DISP3DCNT, 폴리곤 수, 맵 로드 | |
| 3d-aa | AA 정육면체 + 알파 테스트 잔가지 | DISP3DCNT | |
| 3d-depth | 0~59 W 버퍼와 단색 후면 / 60~ Z 버퍼와 비트맵 후면 | DISP3DCNT | |
| 3d-test | BOX INSIDE/OUTSIDE 전환, POS와 VEC 결과 | inside 수, w, vec x, vec z | |
| 3d-compose | 3D 정육면체가 2D 빗줄기 레이어 앞 → 뒤, 3D 위 스프라이트, BG0 흔들림 | BG0CNT, BG2CNT | |
| 3d-limits | 삼각형이 2048개를 넘어 일부 사라짐, 초과 프레임 > 0 | 초과 프레임, 폴리곤 RAM | |
| snd-mix | 추격 BGM + 좌우로 패닝한 효과음 8개 | 곡 번호, 8 | |
| snd-pcm | PCM8 단발(발소리) → PCM16 단발(천둥) → PCM8 반복 → PCM16 반복 | 샘플 번호 | |
| snd-adpcm | ADPCM 스팅(단발) → 험(반복) | 샘플 번호 | |
| snd-psg | 채널 8~13에서 듀티비가 바뀌는 비프, 노이즈 | 8, 15 | |
| snd-capture | 기록 보관실 곡 + 에코 | 1 | |
| snd-stream | 끊김 없는 빗소리와 저음 배경음 | 크기, 샘플레이트 | |
| io-cart | NitroFS 파일 크기, 카트 DMA OK, 3장 맵 정보 | 합계, 로드 성공, 카트 DMA, 물건 수 | |
| io-save-eeprom / flash / fram | WRITE+VERIFY OK, RELOAD OK | 쓰기, 다시 읽기, seq +1, 칩 응답 | |
| io-firmware | 이름, 선호 색 → 손전등 색, 오늘 생일 여부, 언어, 터치 보정 | 색, 언어, 이름 길이, 손전등 색 | |
| io-rtc | 날짜와 시각이 매초 갱신 | 연, 월, 일, 시 | |
| io-touch | 펜 위치에 조준점, 픽셀/원시 값 | 터치 중, x, y, 원시 x | |
| io-power | A: 위 백라이트, B: 스피커, 뚜껑을 닫으면 절전 | 배터리, 뚜껑, 백라이트, 스피커 | |

## 판정(`expected.json`, `tools/judge.py`)

각 데모의 결과 블록을 `tools/judge.py`가 `expected.json`과 대조합니다(항목 138개, 데모 65개 전부와 QA 실행). 항목마다
1~4의 `level`과 `level_basis`가 있으며([docs/LEVELS-KO.md](../../../docs/LEVELS-KO.md)), 레벨은 판정에 영향을 주지 않습니다.

| 종류 | 판정하는 것 | 예 |
|---|---|---|
| (a) 자체 검사, 문서화된 상수 | 오류 수 0, `sqrt(10^12)` = 1000000, CRC 일치, 폴리곤 2048 | sys-wram, sys-dma-*, sys-bios, sys-math, 3d-limits |
| (b) 레지스터 되읽기 | 레지스터가 데모가 쓴 값을 그대로 돌려줌(쓰기 전용 레지스터는 판정하지 않음) | 2d-engines, 2d-window, 2d-blend, 3d-toon, 3d-compose |
| (c) 타이밍 수치 | GBATEK 수치(프레임당 263줄, 버스 33.513982 MHz)에서 유도한 범위 안의 횟수 | sys-irq, sys-timer, sys-dma-vblank |
| (d) 화면·소리 | ROM 안에서 판정: 데모가 자기 출력을 캡처(디스플레이 캡처로 뱅크 D)해 GBATEK 규칙과 픽셀을 비교하고, VRAM 매핑은 별칭으로 읽어 확인하고, 소리 데모는 ARM7가 알려 주는 채널·캡처 상태를 읽음 | sys-dma-hblank, sys-dma-disp, 2d-affine, 2d-mosaic, 2d-obj-*, 2d-vram, 2d-display, snd-* |

출처: `GBATEK`과 `rom` 항목은 판정에 반영(gating)되고, `unconfirmed`(아직 실기 값이 없음, 예: 모델의 폴리곤 수)는 참고용입니다.
DeSmuME는 폴리곤 카운터가 0이라 해당 항목이 n/a입니다(`na_when`).

```sh
tools/melonds/mds.sh demos games/hushwell/build/release/demos > demos.log     # GL 렌더러는 RENDERER=gl|compute
python3 games/hushwell/tools/judge.py --profile melonds-1.1-software demos.log
RESULT_WORDS=16 tools/emu/emu.sh demos games/hushwell > ds.log && python3 games/hushwell/tools/judge.py --profile desmume-pydesmume-0.0.9 ds.log
```

known 프로필(2026-11-12 측정, 실기 값은 아직 없음): melonDS 1.1 소프트웨어·GL(Classic)·GL(Compute)는 판정 항목을 전부 통과하고, DeSmuME는 폴리곤
카운터가 없어(해당 항목 n/a) 참고용 항목만 다릅니다(키 IRQ 수, `VEC_TEST` z, 메인 메모리 디스플레이 FIFO 캡처). 이 게임의 장면에 달린 값(폴리곤 수, 2d-text의
HUD 레지스터 값, 서브 엔진의 WINOUT, 로고의 CRC)은 Hushwell 고유의 값이라 Corvane과 다릅니다. `hushwell.nds`의 자동 QA 실행은 데모 `qa`로
판정합니다(`--demo qa`): melonDS 1.1(소프트웨어)과 DeSmuME에서 `RESULT PASS`.

(d) 종류의 데모는 프로브 결과를 `values[4..8]`(비교한 샘플 수, 틀린 수, 캡처 완료, 부가 값)에 담습니다. 판정하지 않는 것: 서브 화면의
줄 시작 DMA 막대(`sys-dma-disp`), 프로브한 픽셀 밖의 그림, 그리고 "채널이 동작 중 / 캡처 링이 무음이 아님"을 넘는 소리 내용.
참고용(`unconfirmed`) 프로브: 메인 메모리 디스플레이 FIFO 캡처(`sys-dma-disp`, `2d-display`: DeSmuME에서 실패하고, 디스플레이가 같은
FIFO를 읽는 동안 캡처에 무엇이 보이는지는 문서에 없음)와 줄당 OBJ 예산(`2d-obj-line`: GBATEK는 줄당 약 1210사이클이라 적지만 어느
에뮬레이터도 마지막 스프라이트를 버리지 않고 정확한 계산은 확인하지 못함). ARM7는 입력 패킷(`CvInputPacket.snd_busy`, `cap_peak`,
`cap_busy`)에 채널 상태를 담아 보냅니다.

## 본 빌드 체크리스트

| 항목 | 확인 방법 | melonDS 결과 |
|---|---|---|
| 부팅 로고 → 경고문 → 타이틀 | 그냥 실행 | |
| 데모 플레이 | 타이틀에서 20초 대기, 챕터가 차례로 재생, 키를 누르면 타이틀로 | |
| 새 게임 → 프롤로그 → 1~5장 → 엔딩 → 스태프 롤 → 기록 | 처음부터 끝까지 플레이 | |
| 두 엔딩 | 테이프 5개 미만으로 5장 클리어(탈출), 5개 모두 모아 우물에서 A(우물) | |
| 무선기와 사망 | 무선기 사용 후 세 번 맞기, 사망 화면에서 A면 그 무선기로 돌아감 | |
| 이어하기 | 일시정지에서 그만둔 뒤 타이틀의 CONTINUE로 마지막 무선기에서 재개 | |
| 청각 | 그것 근처에서 걸으면 안전, 달리거나 물을 헤치면 다가옴, 어둠 속에서 L(숨죽이기)이면 지나감 | |
| 신호탄 | 그것에게 R+A로 쏘면 기절(빛나며 흔들림), 신호탄은 떨어짐 | |
| 열쇠구멍 | 잠긴 문에서 A를 누르면 열쇠구멍 너머가 보임 | |
| 프롤로그 오프닝 | 새 게임: 차 안에 앉아 있고, 와이퍼가 움직이고, 무선이 끊기고, 형체가 길을 지나감. A로 건너뜀 | |
| 2장 | 물빛(커스틱)이 화면 위에서 일렁임(어파인 BG + HBlank DMA), 화면 아래쪽이 더 강함 | |
| 3장 | 영사기가 필름 재생(메인 메모리 표시). 테이프 3을 PLAY하며 REC, 그다음 REV: 번호 4-1-7이 나오고 목소리가 바로 들림. 다이얼로 문이 열림 | |
| 3장 초상화 | 그것이 다가오면 왼쪽 위에 초상화가 깜빡임: 원래 → 바램 → 뒤틀림 → 반전 | |
| 녹음기 사진 | PHOTO 후 VIEW PHOTO: 찍은 화면이 아래 화면에 나옴(4장과 추격 중에는 안 됨) | |
| 마이크 | 그것 근처에서 마이크에 숨을 불거나 말하면 그것이 들음(MIC LISTENS 끄면 안 들음) | |
| 5장 오프닝 | 통로 전에 갱도를 내려가는 장면. A로 건너뜀 | |
| 4장 밤의 방문자 | NIGHT VISIT을 ALWAYS로(또는 0:00~4:59에 플레이): 외곽선만 있는 형체가 따라오며 배터리를 깎음 | |
| 5장 추격 | 마지막 열쇠를 주우면 양쪽 화면 3D(앞/뒤), 벌레 떼 근처에서 폴리곤 수 초과 | |
| 뚜껑 | 플레이 중 닫기 → 절전 → 열기: 일시정지 상태, 그것이 쫓아오는 중 | |
| 기록 | 클리어 시간, 엔딩, 테이프 수가 RTC 날짜와 함께 저장 | |
| 리플레이 | 기록 화면에서 X: 마지막 챕터 플레이를 똑같이 재생 | |
| 일시정지 도구 | X 조명 켜기, Y 시야 마스크 끄기, B 개발자 콘솔, R 한 프레임 진행, 10초 방치 시 백라이트 꺼짐 | |
| 화면 스왑 | 플레이 중 Select로 스왑, 설정의 SCREEN SWAP이면 3D가 아래에서 시작 | |
| 라이선스 | 크레딧 → X로 THIRD_PARTY_NOTICES, MPL-2.0, picolibc 등 원문 표시 | |
| 사운드 | 그것의 발소리가 좌우로 패닝, 가까우면 잡음 상승, 심장박동이 빨라짐, 끊김 없음 | |

## 자동 스모크 테스트 기록 (DeSmuME)

melonDS 검수 전에, 이 게임을 만든 세션에서 헤드리스 DeSmuME 0.9.12(py-desmume, x86_64
컨테이너, `tools/emu/`)로 다음을 확인했습니다. melonDS 검수를 대신하지 않습니다.

- 데모 65개: 모두 120프레임 체크포인트 도달(결과 블록 매직 확인). DeSmuME는 폴리곤 RAM
  사용량과 초과 플래그를 보고하지 않아 `3d-limits`와 `3d-fifo`의 폴리곤 수는 0으로 읽힙니다.
- 본 빌드: 부팅 → 경고문 → 타이틀 → 무선 교신 → 여섯 챕터 각각 진입해 걷기(지도가 채워지며
  회전, 녹음기 열기, 손전등, 신호탄 발사), 달리는 소리를 들은 그것이 쫓아와 세 번 공격하고
  그 사이 물러나기, 사망 화면, QA 챕터 클리어 → 챕터 클리어 화면 → 양화면 탈출 엔딩 →
  크레딧 → 기록 저장, 기록/설정/사운드 테스트 화면, 데모 플레이.
- 5장 양화면 추격(RAM의 `W.chase`를 써서 강제로 시작): 위 화면에 앞쪽, 아래 화면에 불 꺼진
  뒤쪽 시점, 추격 중 일시정지해도 두 화면이 깨지지 않음.
- 깨끗한 빌드 두 번의 `hushwell.nds` SHA-256이 같음.
- 걷는 구간을 건너뛰려고 RAM(플레이어 위치, 테이프)도 쓰는 테스트 하네스로 돌린 시나리오:
  - 3장: 영사기 필름이 메인 메모리 표시로 재생되고 끝나며, 찢어진 사진과 긁힌 숫자가 나옴.
    터치로 다이얼을 4-1-7에 맞추면 문이 열림.
  - 4장: 설정의 NIGHT VISIT = ALWAYS가 저장됨. 방문자가 외곽선만 빛나는 검은 형체로 나타나
    두 칸쯤 떨어져 따라오며 배터리를 깎음.
  - 5장: 테이프 다섯 개를 모두 가지면 우물에서 "내려가기 / 올라가기"를 묻고, A면 우물
    엔딩(위: 줄어드는 하늘, 아래: 얼굴들), 크레딧에 ENDING: THE WELL. B면 탈출 엔딩.
  - 뚜껑: 닫으면 절전, 열면 일시정지 화면, START로 재개.
  - 리플레이: 1장 플레이(걷기, 돌기, 그것의 공격)와 기록 화면에서 재생한 리플레이의 챕터
    상태가 챕터 프레임 300과 500에서 같음(맵 힙 포인터와 화면 메시지 포인터·타이머만 다름).
- 새 기능(같은 하네스): 차 안 오프닝(앞유리 윈도우, 와이퍼, 지나가는 형체), 5장 갱도 하강,
  2장 물빛 일렁임, 그것이 다가올수록 확장 팔레트가 바뀌는 3장 초상화, 녹음기 REC(사운드
  캡처로 RAM에 녹음) 후 REV(스트림 채널로 역재생)로 번호가 나오는 것, PHOTO / VIEW PHOTO.
  마이크 값이 입력 로그에 들어간 뒤 리플레이 검사를 다시 했고 여전히 같음.
- RAM을 쓰지 않은 전체 플레이: 입력 봇(버튼과 터치만 사용하고, 화면을 보듯 플레이어 위치만
  RAM에서 읽음)이 타이틀부터 프롤로그와 1~5장을 지나 테이프 다섯 개를 모두 줍고, 문을 모두
  열고, 3장 다이얼을 터치로 맞추고, 그것에게 신호탄을 쓰며, 우물 엔딩과 스태프 롤을 거쳐 기록
  화면까지 약 44,000프레임에 완주함. 1장과 2장에서 한 번씩 죽었고 그때마다 챕터 시작에서
  이어 감. 이 플레이로 버그 세 개를 찾아 고침: 물러나는 그것이 공격할 수 있던 것, 추격 중
  그것이 달리기와 거의 같은 속도에 공격 뒤 멈춤이 없던 것, 양화면 뒤쪽 시점이 잠깐 플레이어
  방향값을 바꾸던 것.
- 스모크 테스트로 확인하지 못한 것: 벌레 떼
  폴리곤 초과(DeSmuME는 0을 보고), 역재생된 녹음이 실제로 어떻게 들리는지와 마이크(헤드리스
  실행에는 오디오 입력이 없음), melonDS에서의 모든 것.
