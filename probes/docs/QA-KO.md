# Probes QA

[English](QA.md)

probe는 누구도 판정하지 않습니다. 실기 값이 없기 때문입니다. 이 문서는 에뮬레이터가 한 동작(측정 표)과 ROM마다
화면에 보여야 하는 것을 기록합니다.

## 모든 probe ROM의 공통 규칙

- 빌드: `./build.sh DEMO=l4-<name>` → `probes-l4-<name>.nds` (15개 전부는 `./build.sh`, 배포 폴더는 `./build.sh -j8 release`).
- 화면: probe 이름과 결과가 텍스트 BG 콘솔에 나옵니다(3D probe는 메인 화면의 BG1 콘솔).
- 체크포인트: 시작 120프레임 뒤에 화면이 완성되고 `CHECKPOINT` 줄이 출력됩니다. 같은 값이 메인 RAM
  `0x0237F000`에 기록됩니다(`u32 magic 'CVER'`, `u32` 데모 이름의 FNV-1a, `u32 frame`, `u32 values[13]`).
- probe는 입력이 필요 없습니다. QA 모드도, 끝까지 플레이하는 실행도 없습니다.

## 측정 방법

```sh
./build.sh -j8 release
RENDERER=soft    tools/melonds/mds.sh demos probes/build/release/demos > soft.log      # gl, compute도 같음
RESULT_WORDS=16  tools/emu/emu.sh demos probes > desmume.log
python3 probes/tools/judge.py --observed --partial soft.log      # expected.json known의 `observed` JSON
python3 probes/tools/judge.py --profile melonds-1.1-software soft.log   # 종료 코드 0 = 변화 없음
```

melonDS 출력의 `SAVE 2 ...`(타입 0이 아니라 2: melonDS가 목록에 두지 않은 게임 코드에는 항상 세이브 칩이 붙음)는
ROM이 정상 부팅했다는 뜻입니다. 15개 probe 모두 일곱 프로필(melonDS 1.1과 master `4ce2188` 각각 soft / GL classic / GL compute, DeSmuME)에서 체크포인트에 도달했습니다. master: `MELONDS_COMMIT=4ce2188 RENDERER=... tools/melonds/mds.sh demos ...`.

## 기대 화면

헤드리스 DeSmuME에서 확인했습니다(2026-11-12, `out/demos/*_final.png`, 기존 14개 전부 눈으로 확인, `l4-acc-rear-plane`은 melonDS soft/GL/Compute와 DeSmuME에서 결과 블록으로 확인). 모든 probe의 그림이 화면에
나옵니다(큐브, 지형 위의 함선, 보스, 기지, 벽 복도, 큐브가 있는 바닥, 등불이 있는 카드 줄, 2패스 도시, FIFO 그라데이션,
스프라이트 띠). 대부분의 probe는 그 위에 콘솔을 위쪽 화면에 출력하고, `l4-2d-display`는 그라데이션만 보이며,
`l4-sys-dma-disp`, `l4-acc-dual`, `l4-acc-split-*`는 콘솔을 아래쪽 화면에 출력합니다. 눈으로 본 것은 DeSmuME 스크린샷뿐이고,
melonDS 실행은 결과 블록으로 비교했습니다(Corvane 데모와 같은 숫자).

| probe | 확인할 것 / 기대 화면 | 체크포인트 값 (values[0..3]) | 관측(melonDS 소프트웨어) |
|---|---|---|---|
| l4-sys-dma-gfx | 텍스처가 있는 큐브가 회전 | 큐브 리스트 워드 수, 폴리곤 수 | `9a 2` (리스트 154워드, 폴리곤 2) |
| l4-sys-dma-disp | 위: 색 그라데이션(메인 메모리 FIFO) / 아래: 줄별 막대 | 픽셀 값, 막대 색 | `f190 208c` |
| l4-2d-obj-line | 한 띠에 큰 탄 120개, 마지막 그룹을 버리는 에뮬레이터는 없음 | OAM 개수 | `78` (스프라이트 120) |
| l4-2d-display | 0~59 VRAM 디스플레이, 60~ 메인 메모리 FIFO 디스플레이 | 픽셀 값 | `0 800a` |
| l4-3d-fifo | 지형(포트 쓰기) 위의 함선(DMA 리스트) | 폴리곤·버텍스 수, 리스트 크기 | `118 29c 14a` (280 / 668) |
| l4-3d-matrix | 패널·팔·집게가 계층적으로 움직이는 보스 | 보스 HP 합계, 폴리곤 수 | `190 6b` (폴리곤 107) |
| l4-3d-light | 네 가지 색의 조명이 도는 기지 | 폴리곤 수 | `27` (폴리곤 39) |
| l4-3d-clip | 카메라가 벽을 스치며 근접 평면 클리핑 | 폴리곤·버텍스 수 | `21 86` (33 / 134) |
| l4-3d-poly | 줄무늬 바닥, 앞면·뒷면 컬링 큐브, 반투명 비행선, 함선 그림자 | 폴리곤 수 | `51` (폴리곤 81) |
| l4-3d-test | BOX INSIDE/OUTSIDE 전환, POS·VEC 결과 | INSIDE 횟수, w, vec x, vec z | `66 51a8 9b7 fffff36c` |
| l4-acc-stage | 뒤집힌 UV 카드, 선 폴리곤 줄에 매달린 외곽선 인형, 어두워진 3D 위의 반투명 등불 | 카드 줄별 절단선 픽셀, 외곽선 픽셀, 선 픽셀, 통과 플래그 | `120000 b7 2c1 1f` (소프트웨어) |
| l4-acc-dual | 위 정면 / 아래 평면, 이어서 캡처로 합성한 2패스 대도시 | 프레임 지연, 정면·평면 클리어색 픽셀, 합성 픽셀, 통과 플래그 | `1 b4b4ab3b 1c501df9 1f` (소프트웨어) |
| l4-acc-split-rear | 한 화면을 60fps 2패스로, 분할선이 8프레임마다 이동. 캡처한 쪽이 3D rear plane으로 돌아옴 | 지연, 변형, 분할 x | `1 3 56` |
| l4-acc-split-hdma | 같은 장면을 캡처 뱅크에서 엔진 B 비트맵 BG로 HBlank DMA | 지연, 변형, 분할 x, 구멍이 있는 프레임 수 | `1 6 56 0` (master GL/Compute `1 6 56 808`: 마지막 16프레임 중 8 + 8프레임에 구멍, 구멍 픽셀 2056개) |
| l4-acc-rear-plane | 같은 2패스 방식에 rear plane 사용(Elevane 프리셋 2에 대응). rear plane을 먼저 설정한 뒤 캡처된 패스의 불투명 샘플을 셉니다(4096 = 양쪽 절반) | 지연, 변형, 분할 x, 0, 불투명 샘플, 왼쪽 절반, 오른쪽 절반(values[4..6]) | `1 7 56 0`, `1000 800 800`(soft, DeSmuME); GL: `580 580 0` |

## 측정 표 (2026-11-12)

이 트리에서 빌드한 릴리스 probe ROM(경고 없음)입니다. "기준값"은 항목의 check에 적힌 값 또는 범위로, 기록된 관측(probe를 작성할 때 에뮬레이터가 낸 값, 또는 그 동작이 가질 것으로
예상한 값)이며 실기 값이 **아닙니다**. judge는 각 에뮬레이터를 자기 열과 비교합니다. mask가 있는 항목은 mask를 적용한 값입니다.

| 항목 | probe | 기준값 | 1.1 soft | 1.1 GL classic | 1.1 GL compute | master soft | master GL classic | master GL compute | DeSmuME |
|---|---|---|---|---|---|---|---|---|---|
| `sys-dma-gfx-polys` | l4-sys-dma-gfx | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 0 |
| `3d-fifo-polys` | l4-3d-fifo | 280 | 280 | 280 | 280 | 280 | 280 | 280 | 0 |
| `3d-fifo-verts` | l4-3d-fifo | 668 | 668 | 668 | 668 | 668 | 668 | 668 | 0 |
| `3d-clip-polys` | l4-3d-clip | 33 | 33 | 33 | 33 | 33 | 33 | 33 | 0 |
| `3d-light-polys` | l4-3d-light | 39 | 39 | 39 | 39 | 39 | 39 | 39 | 0 |
| `3d-matrix-polys` | l4-3d-matrix | 107 | 107 | 107 | 107 | 107 | 107 | 107 | 0 |
| `3d-poly-polys` | l4-3d-poly | 81 | 81 | 81 | 81 | 81 | 81 | 81 | 0 |
| `3d-test-inside` | l4-3d-test | 102 | 102 | 102 | 102 | 102 | 102 | 102 | 102 |
| `3d-test-pos-w` | l4-3d-test | 20904 | 20904 | 20904 | 20904 | 20904 | 20904 | 20904 | 20904 |
| `3d-test-vec-x` | l4-3d-test | 2487 | 2487 | 2487 | 2487 | 2487 | 2487 | 2487 | 2487 |
| `3d-test-vec-z` | l4-3d-test | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964076 | 4294964077 |
| `acc-stage-cut-exact` | l4-acc-stage | 1179648 (mask 0xff0000) | 1179648 | 0 | 1179648 | 1179648 | 0 | 1179648 | 1179648 |
| `acc-dual-both` | l4-acc-dual | 1 (mask 0x1) | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `acc-split-hdma-holes` | l4-acc-split-hdma | 0 | 0 | 0 | 0 | 0 | 2056 | 2056 | 0 |
| `2d-obj-line-budget` | l4-2d-obj-line | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 |
| `sys-dma-disp-pixels` | l4-sys-dma-disp | 0 | 0 | 0 | 0 | 0 | 653 | 653 | 736 |
| `2d-display-pixels` | l4-2d-display | 0 | 0 | 0 | 0 | 0 | 768 | 768 | 768 |
| `seam-uv-exact` | l4-acc-stage | 1179648 | 1179648 | 4608 | 1179648 | 1179648 | 4608 | 1179648 | 1179648 |
| `rear-plane-latency` | l4-acc-split-rear | 0 to 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| `rear-plane` | l4-acc-rear-plane | non-zero | 4096 | 1408 | 1408 | 4096 | 1408 | 1408 | 4096 |
| `rear-plane-full` | l4-acc-rear-plane | 4096 | 4096 | 1408 | 1408 | 4096 | 1408 | 1408 | 4096 |

에뮬레이터마다 다른 점:

- **DeSmuME**: 폴리곤·버텍스 RAM 카운터가 0입니다(7개 항목, judge는 n/a로 표시). `3d-test-vec-z`는 1 더 큽니다(`...077`
  대 `...076`). `l4-sys-dma-disp`와 `l4-2d-display`의 메인 메모리 디스플레이 FIFO 캡처는 각각 768개 중 736개, 768개 중
  768개가 틀립니다(그 경우 캡처 소스 B가 화면에 나오는 것과 다름).
- **melonDS GL classic**(1.1, master): 세 번째 카드 줄에 절단선이 없고(`acc-stage-cut-exact` 0) 두 번째 줄에 있습니다
  (`seam-uv-exact`는 0x1200: 0/0/18이 아니라 0/18/0). 소프트웨어와 GL compute는 0/0/18입니다.
- **melonDS GL classic과 compute, 1.1과 master**(소프트웨어와 DeSmuME은 아님): `l4-acc-rear-plane`의 캡처된 패스가 일부만
  나옵니다. 4096개 중 1408개이며 모두 왼쪽 절반이고 오른쪽 절반은 비어 있습니다. Elevane 프리셋 2(없어진 `rear-plane`,
  `rear-plane-full`)도 같습니다.
- **melonDS master `4ce2188` GL classic과 compute**(melonDS#2618): HBlank DMA 합성에 매 프레임 구멍이 있어
  `acc-split-hdma-holes`가 2056입니다(마지막 16프레임 중 8 + 8프레임에 구멍, 1.1과 master 소프트웨어는 0). 같은 master
  빌드는 메인 메모리 디스플레이 FIFO에서 1.1이 0인 곳에 653개(`sys-dma-disp-pixels`)와 768개(`2d-display-pixels`)가 틀립니다.
- **일곱 모두**: 스프라이트 120개 줄에서 마지막 OBJ 그룹이 남습니다(`2d-obj-line-budget` 3: 줄당 예산에서 스프라이트를
  버리는 에뮬레이터는 없고, 콘솔이 어디서 버리는지는 아무도 측정하지 않았습니다).
- 나머지(폴리곤 수, `BOX_TEST`/`POS_TEST`/`VEC_TEST` 결과, 정면·평면 화면, rear plane 지연 1)는 melonDS의 모든 프로필에서 같고,
  3D 테스트 값은 `vec-z`만 빼고 DeSmuME과도 같습니다. 프레임 지연은 부팅 때 걸려 있는 SwapBuffers를 측정 루프 전에
  비우도록 고친 뒤 모든 곳에서 1입니다(고치기 전 Elevane의 복사본은 2였음).

## 실기

측정하지 않았습니다. 실기 결과가 생기면 해당 항목은 더 낮은 레벨로 옮겨 가며(`../../docs/LEVELS-KO.md`) 이 모음에서 나갑니다.
