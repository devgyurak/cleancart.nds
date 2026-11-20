[English](HW-FINDINGS.md)

# 실기 발견

이 저장소의 게임들이 실제 콘솔에서 쟀는데 GBATEK이나 에뮬레이터가 말하는 것과 다른 값을 모은 곳입니다. 에뮬레이터
개발자가 한곳에서 볼 수 있게 정리했고, 근거(로그, 표, ROM 해시)는 각 게임의 `docs/HW-QA-KO.md`에 있습니다.

모든 항목의 콘솔: DS Lite(NTR) + R4 Plus 카드(Wood 커널), 릴리스 ROM의 hw-support 빌드, 2026-11-17~2026-11-20(Buoyane 실행 1·2: 2026-11-20).
에뮬레이터: melonDS 1.1(software, OpenGL Classic, OpenGL Compute, 헤드리스 코어)과 DeSmuME(py-desmume 0.0.9).
콘솔은 한 대뿐이라 초기형 DS나 DSi에서는 값이 다를 수 있습니다.

구성:

1. [콘솔이 GBATEK과 다른 것](#1-콘솔이-gbatek과-다른-것)(또는 GBATEK에 없는 것): `expected.json`의 기대값을 콘솔 값으로
   바꿨거나(`source: hw:dslite-ntr`) 아직 참고용입니다.
2. [GBATEK이 맞고 에뮬레이터가 다른 것](#2-gbatek이-맞고-에뮬레이터가-다른-것): 콘솔에서는 통과하고 에뮬레이터 하나 이상에서
   실패하는 판정 항목(알려진 프로필의 `ng` 목록).
3. [판정하지 않지만 다른 것](#3-판정하지-않지만-다른-것).

## 1. 콘솔이 GBATEK과 다른 것

| 게임, 항목(레벨) | GBATEK / 처음 기대값 | 콘솔 | melonDS 1.1 | DeSmuME | 상태 |
|---|---|---|---|---|---|
| Lightenette `snd-wave-pan-right-exact` (L2) | pan 126(오른쪽 끝)에서 왼쪽 믹서는 최대 2(pan 0 기준 진폭 199). GBATEK의 pan 분수 반올림은 두 가지로 읽힘: MSB를 2의 보수로 보면 +1 / -2, 크기로 보면 +1 / -1 | **3**(두 실행) | 3 | 1 | Lightenette v0.3.1부터 기대값 3, `hw:dslite-ntr` |
| Lightenette `3d-limits-ram` (L2) | RAM_COUNT(4000604h)의 폴리곤 수는 2048에서 멈춤 | flush 전에 폴리곤 2980개를 제출하면 **2979**: 2048을 넘어 계속 셈(오버플로 플래그는 켜짐) | 2048(멈춤) | n/a(카운터가 0) | v0.3.1부터 범위 2049..2980, `hw:dslite-ntr`. Lightenette와 Elevane의 `acc-dual`에서도 보임(한 패스: 콘솔 2310, melonDS 1536, 판정 안 함) |
| Ledgerine `ipc-empty-cleared` (L2) | 상대가 보내기 FIFO를 비운 뒤의 빈 읽기는 0을 돌려줌 | **`7A00000Fh`**: 앞서 보낸 16워드 묶음의 마지막 워드 A15. B(마지막으로 읽은 워드)도 C(비워진 워드)도 아님. 오류 비트는 켜지고 확인 동작은 정상 | 0 | 0 | v0.2.2부터 "0이 아님, 확인 동작 정상"으로 판정(`mask 0xC`, `expect 0x4`). 원시 워드는 v0.2.2부터 `values[44]`에 기록(지금까지 한 실행). 정확한 워드는 `ipc-empty-cleared-word`(L2, #209)가 판정 |
| Tonewick `snd-short-silent` (L2) | 3워드(최소 4워드 미만) 반복 샘플은 소리가 나지 않음 | 캡처 피크 **16384**(세 실행): 테이프 레벨을 유지하고 무음이 아님 | 0 | 16384 | v0.2.2부터 범위 16320..16448, `hw:dslite-ntr` |
| Pendulane `mem7-main-nonseq` (L3) | ARM7이 메인 RAM을 비순차로 64번 읽기: GBATEK 표대로면 9사이클씩 576 | **383**(세 실행): 읽기당 약 6(메인 RAM N32 실효 7, WRAM 1) | 384 | 64 | v0.2.2부터 범위 376..392, `hw:dslite-ntr` |
| Pendulane `mem7-code-main-seq` (L3) | 메인 RAM에서 도는 ARM7 코드, 순차 64워드: 워드당 2사이클, 128 | **142**(세 실행): 약 14사이클 더 걸림. 원인은 문서에 없음 | 128 | 128 | v0.2.2부터 범위 139..145, `hw:dslite-ntr` |
| Heddlemere `gx-trans-cycles` (L3) | MTX_TRANS는 22사이클 | **35**(560 = 35 x16) | 35(559) | 0 | Heddlemere v1.0.1부터 범위 556..564, `hw:dslite-ntr` |
| Heddlemere `gx-box-cycles` (L3) | BOX_TEST는 103사이클 | **약 126**(2020 = 126.25 x16) | 257(4114) | 2 | v1.0.1부터 범위 2012..2028, `hw:dslite-ntr`(지금까지 한 실행) |
| Heddlemere `hblank-low-arm7` (L3) | 한 줄의 HBlank가 아닌 구간은 ARM7에서 1613사이클("for whatever reason"), ARM9에서 1606 | **25712**(x16): ARM9(25710)와 같음. 두 CPU 모두 GBATEK의 1606이 맞음 | 25343(ARM9 25343) | 25634(ARM9 25632) | v1.0.1부터 범위 25680..25744, `hw:dslite-ntr` |
| Buoyane `wire-texture-alpha` (L2) | "Wire-frame polygons are always having Av=31, however, they can be made transparent by using Translucent Textures (ie. A5I3 or A3I5 formats) with At<31" (GBATEK, DS 3D Texture Blending). A5I3 텍스처(알파 15)를 쓴 와이어프레임은 블렌딩됨 | **불투명하게 그림**: 흰 와이어 픽셀 170, 블렌딩 0(`values[21]`, 실행 1·2. 실행 1은 블렌딩 비트만 있었음) | 소프트웨어·Compute: 불투명하게 그림(170). GL Classic: 그리지 않음(0) | 그리지 않음(0) | Buoyane v1.0.0부터 `values[21]` 범위 1..65535(그려짐, 블렌딩 없음), `hw:dslite-ntr` |
| Buoyane `zeq-slope-band-left`, `zeq-slope-band-right` (L2, 뺌) | 경사면의 Equal 띠가 x 101..107에서 x 149..155까지(GBATEK의 ±200h 창에 대한 우리 모델) | **109**와 **159**: 4, 7픽셀 오른쪽(실행 1·2). 폭 50은 GBATEK 허용 오차의 창 안 | 105, 152(폭 47) | 118, 141(폭 23) | 폭을 판정(`zeq-slope-band-width`, 42..54, GBATEK). 위치는 `probes/`로 감(후속) |
| Buoyane `zeq-decal-whole` (L2, 뺌) | 경사진 바닥 위 데칼에 구멍이 없음 | **구멍 있음**: 960픽셀(`values[18]`, 실행 1·2) | 구멍 없음(0) | 2120(`0x848`) | 판정 안 함(래스터라이저 반올림은 문서에 없음). 구멍은 `probes/`로 감(후속) |
| Buoyane `values[13]` `W_ZEQ_1`(판정 안 함) | 바닥에서 깊이가 한 단계(±1) 다른 평면을 Equal이 허용 오차 안에서 그림 | **0**픽셀 | 1024 + 1024픽셀(`0x04000400`) | 0 | `probes/`의 `zeq-step-boundary`가 같은 평면을 잼. 콘솔 값은 거기 기록할 예정 |

ARM7 HBlank 구간은 우리 ARM7 코어가 폴링해서 재므로 폴링 루프가 값에 섞였을 수 있습니다. 같은 방법으로 잰 ARM9 값이
GBATEK과 맞기 때문에 ARM7 값을 여기에 올렸습니다.

**Ledgerine이 A15를 읽는 이유(해석).** A0..A15가 16칸 링을 채우고 모두 읽힙니다. B는 0번 칸에 들어가 읽히고, 그다음
빈 읽기는 B를 돌려줍니다. C는 1번 칸에 들어가고, ARM9이 읽기 전에 ARM7이 보내기 FIFO를 비우면 다음 빈 읽기가 A15를
돌려줍니다. 빈 읽기가 읽기 포인터 바로 앞 칸을 돌려주고, 비우기가 포인터를 0번 칸으로 돌린다고 보면 맞습니다(그 앞 칸인
15번에 A15가 남아 있음). 한 스크립트에서 읽어 낸 해석이고 문서화된 규칙은 아닙니다.

## 2. GBATEK이 맞고 에뮬레이터가 다른 것

여기 항목은 모두 GBATEK 기대값(또는 이름을 적은 melonDS 이슈의 보고값)으로 콘솔에서 통과했습니다. "실패하는 곳"은 그
항목을 `ng`에 둔 알려진 프로필입니다.

| 게임, 항목(레벨) | 콘솔의 동작 | 실패하는 곳 |
|---|---|---|
| Paneveil `poly-wire-edges` (L1) | alpha 0 폴리곤은 테두리를 그림(와이어프레임) | melonDS GL Classic, DeSmuME |
| Corvane `acc-stage-edge` (L1), Lightenette `edge-marking`, `act2-edge-marking`, `act5-edge-3-colours` (L1) | 에지 마킹이 외곽선을 그림(melonDS #2549) | melonDS GL Classic |
| Lightenette `seam-uv`, `act2-seam`, `act5-seam` (L2) | UV를 뒤집은 절단선은 2행에만 보임(melonDS #2008) | melonDS GL Classic |
| Lightenette `lantern-blend-equal`, `act3-lantern-equal`, `act5-lantern-equal` (L2) | 반투명 OBJ는 BLDCNT none, alpha, darken에서 똑같이 보임(melonDS #2503) | melonDS GL Classic, GL Compute, DeSmuME |
| Elevane `hdma-holes-zero`, `acc-split-hdma-holes` (L3) | HBlank DMA 합성은 어느 프레임에도 구멍이 없음(melonDS #2618) | melonDS 4ce2188 GL Classic, GL Compute(이 항목에 기록한 프로필) |
| Ledgerine `ipc-empty-last` (L2) | 빈 FIFO 읽기는 마지막으로 받은 워드(B)를 돌려주고 오류 비트를 켬 | melonDS(다음 칸 `7A000001h`를 돌려줌), DeSmuME |
| Ledgerine `dma-itcm-runs` (L2) | 긴 DMA 중에도 CPU가 ITCM 코드를 계속 실행함 | melonDS(DMA 내내 CPU를 멈춤), DeSmuME |
| Ledgerine `bios-arm7-guard` (L2) | 소스가 BIOS 영역인 ARM7 CpuSet은 아무것도 쓰지 않음 | melonDS(대체 BIOS가 복사함), DeSmuME(HLE BIOS) |
| Ledgerine `math-div0`, `ipc-disabled`, `timer-reload-deferred`, `dma-regs-unchanged` (L2) | 0으로 나누기의 결과와 플래그. 꺼진 FIFO는 쓰기를 무시하고 읽기는 엿보기만 함. 동작 중에 쓴 reload는 다음 오버플로를 기다림. 전송 뒤에도 SAD, DAD, CNT_L은 처음 값 | DeSmuME |
| Tonewick `snd-hold-level` (L2) | hold 비트가 있으면 one-shot이 마지막 샘플을 유지함 | melonDS(0으로 돌아감), DeSmuME |
| Tonewick `cap-src-channel` (L2) | 소스 비트 1인 캡처 0은 채널 0만 녹음함 | melonDS(믹서 합을 캡처) |
| Tonewick `cap-add-wrap`, `cap-both-negative` (L2) | GBATEK에 적힌 캡처 버그 둘: add는 클리핑이 아니라 FFFFh와 AND, 두 채널이 음수면 -8000h | melonDS(아무것도 쓰지 않음 / -4000h), DeSmuME |
| Tonewick `pm-reg-mirror` (L2) | DS Lite에서 전원관리 레지스터 5, 6, 7은 4를 미러링함 | melonDS(레지스터 4는 40h인데 5는 0), DeSmuME |
| Tonewick `snd-short-busy`, `snd-noise-lfsr`, `snd-adpcm-loop-reload` (L2) | 3워드 one-shot은 busy 비트를 유지함. 노이즈 채널은 GBATEK 시프트 레지스터를 따름. 데이터 부분에서 시작한 ADPCM 루프는 정확히 반복됨 | DeSmuME |
| Pendulane `snd-pcm-busy`, `snd-adpcm-busy`, `snd-hold-restart` (L3) | Busy는 16샘플 PCM16 시작 18샘플 뒤, 32샘플 ADPCM 시작 42샘플 뒤에 꺼짐. Hold 채널을 다시 시작하면 옛 샘플 하나, 0 샘플 둘, 그다음 새 데이터(258) | melonDS(19, 43, 7), DeSmuME |
| Pendulane `snd-onset-pcm-psg`, `spi-512k-minus-4m` (L3) | PCM16은 PSG보다 2샘플 늦게 소리 남. 512 kHz 펌웨어 SPI 바이트는 4 MHz보다 ARM7 448사이클 더 걸림 | DeSmuME |
| Heddlemere `gx-identity-slope`, `gx-pushpop-slope`, `gx-trans-mode2-extra`, `gx-scale-mode2-extra`, `gx-full-stall-rate`, `gx-halfirq-slope` (L3) | 지오메트리 명령의 사이클 비용과 FIFO 정지가 GBATEK대로 | DeSmuME |
| Heddlemere `vcount-write-lines` (L2) | 212줄에서 VCOUNT를 쓰면 10~11줄이 반복됨 | DeSmuME |
| Heddlemere `hblank-low-arm9`, `swap-tail` (L3) | ARM9에서 한 줄의 HBlank 플래그가 1606사이클 동안 낮음(25710 x16). 맨 SWAP_BUFFERS 뒤 VBlank부터 392사이클 뒤에 엔진이 쉼(6256 = 391) | melonDS(25343, 5224), DeSmuME(25632, 528) |
| Buoyane `aa-opaque-dot-hidden` (L2) | 안티앨리어싱을 켜면 불투명 1-dot 폴리곤이 가려짐(`values[1]` 0x7ff) | 네 가지 모두: melonDS 소프트웨어, GL Classic, GL Compute, DeSmuME |
| Buoyane `edge-aa-translucent` (L2) | 안티앨리어싱에서 edge 표시 가장자리는 edge 색도 면 색도 아님 | melonDS GL Classic, GL Compute, DeSmuME |
| Buoyane `aa-marked-line-complete` (L2) | 안티앨리어싱과 edge marking에서 1픽셀 선에 틈이 없음(192/192픽셀) | melonDS GL Classic, DeSmuME |

`env` 항목(세이브 칩 접근이나 카트리지 gap처럼 테스트 하네스가 필요)은 R4에서 n/a라 넣지 않았습니다: Tonewick
`save-wel-reset`, Pendulane `cart-gap1-clk5`, `cart-gap1-clk8`.

## 3. 판정하지 않지만 다른 것

| 무엇 | 콘솔 | melonDS 1.1 | 어디서 |
|---|---|---|---|
| 3D를 그리지 않는 게임의 RDLINES_COUNT(QA 체크포인트의 `rdl=`) | 63 | 46 | Ledgerine, Tonewick, Pendulane |
| 3D가 바쁜 장면의 RDLINES_COUNT | 일부 체크포인트에서 37..43까지 내려가지만 underflow는 없음 | 어디서나 46 | Elevane, Hushwell |
| 일정 구간의 보이는 줄 HBlank IRQ 수 | 31464(Corvane), 31520 / 31465(Lightenette) | 31323, 31532 | 판정 범위 안 |
| Pendulane 원시 타이밍 경계 `values[24..31]`, `[36..39]` | 2~3 더 큼 | | 판정 안 함 |
| Pendulane `snd-rate-slope`(참고용) | 세 실행에서 13, -10, -33: 실행마다 움직임 | 830 | `unconfirmed` 유지 |
| Buoyane `fog-alpha-glitch`(전에는 L2 `unconfirmed`, 뺌) | 0: FOG_OFFSET 앞의 fog 면이 fog 없는 같은 프레임 알파 31 면처럼 보이는 일이 읽은 픽셀에서 없음(실행 1·2, 한 장면) | 0 | probe 항목이 될 예정(후속) |

## 항목 추가하기

실기 실행(스킬 `nds-hw-qa`) 뒤에, 콘솔이 GBATEK과 달랐던 항목, 이제 콘솔에서는 통과하고 에뮬레이터에서는 실패하는 판정 항목,
판정하지 않지만 알아 둘 만한 차이를 이 문서와 [HW-FINDINGS.md](HW-FINDINGS.md)에 한 줄씩 추가합니다. 콘솔 값, 에뮬레이터
값, 상태를 적고, 근거는 게임의 `docs/HW-QA-KO.md`에 둡니다.
