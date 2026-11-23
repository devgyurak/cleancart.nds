# Probes

[English](README.md)

에뮬레이터 개발자를 위한 연구용 probe 모음입니다. **실기에서 확인된 적 없는 동작**(레벨 L4,
[../docs/LEVELS-KO.md](../docs/LEVELS-KO.md))을 에뮬레이터가 어떻게 처리하는지 보고하는 작은
닌텐도 DS 데모 ROM 25개입니다. 게임이 아닙니다. 타이틀도, 크레딧도, QA 모드도 없고, 120프레임 동안(10개는 더 일찍 끝남)
돌면서 결과 블록을 쓰고 화면을 그대로 유지하는 ROM뿐입니다.

용도: 각 probe는 GBATEK에 없거나 틀리게 적힌 동작에 대해 에뮬레이터가 내는 값을 기록합니다(폴리곤 RAM 개수와 리스트가 다 들어가지 않을 때의 동작,
`BOX_TEST`/`VEC_TEST` 결과, 같은 화면 2패스 캡처, 메인 메모리 디스플레이 FIFO, 줄당 OBJ 예산, 뒤집힌 UV 이음선).
실기 기준의 판정은 없습니다. 대부분의 probe에는 실기 값이 없고, 폴리곤·버텍스 RAM probe 6개에 대해 DS Lite가 낸 값은
[docs/HW-QA-KO.md](docs/HW-QA-KO.md)에 기록할 뿐 합격 기준으로 쓰지 않습니다. `tools/judge.py`는 에뮬레이터별로 값을 **기록**하고,
값이 바뀌면 알려 줍니다.

## probe와 항목

항목 85개, 모두 레벨 L4, role `target`, source `unconfirmed`(참고용: 실행을 실패시키지 않음).

| probe ROM | 항목 | 보여 주는 것 |
|---|---|---|
| `probes-l4-sys-dma-gfx` | `sys-dma-gfx-polys` | DMA로 보내는 디스플레이 리스트의 큐브: 체크포인트의 폴리곤 수 |
| `probes-l4-3d-fifo` | `3d-fifo-polys`, `3d-fifo-verts` | 지형 위의 함선: 폴리곤·버텍스 RAM 개수 |
| `probes-l4-3d-clip` | `3d-clip-polys` | 근접 평면 클리핑 화면의 폴리곤 |
| `probes-l4-3d-light` | `3d-light-polys` | 조명을 받는 기지의 폴리곤 |
| `probes-l4-3d-matrix` | `3d-matrix-polys` | 계층 구조 보스의 폴리곤 |
| `probes-l4-3d-poly` | `3d-poly-polys` | 바닥, 컬링된 큐브, 반투명 비행선, 그림자의 폴리곤 |
| `probes-l4-3d-test` | `3d-test-inside`, `3d-test-pos-w`, `3d-test-vec-x`, `3d-test-vec-z` | `BOX_TEST`, `POS_TEST`, `VEC_TEST` 결과 |
| `probes-l4-acc-stage` | `acc-stage-cut-exact`, `seam-uv-exact` | 뒤집힌 UV의 절단선 픽셀 수 |
| `probes-l4-acc-dual` | `acc-dual-both` | 두 캡처 뱅크로 보는 정면·평면 화면(Elevane에서는 L2 전제 조건 묶음) |
| `probes-l4-acc-split-hdma` | `acc-split-hdma-holes` | HBlank DMA 합성의 구멍(melonDS#2618. Elevane에서는 L3 목표로 판정하고, 여기서는 master까지 기록) |
| `probes-l4-acc-split-rear` | `rear-plane-latency` | 같은 화면 2패스의 rear plane 변형: rear plane을 설정하기 전에 잰 라우팅 지연(Elevane에서는 L2 전제 조건) |
| `probes-l4-acc-rear-plane` | `rear-plane`, `rear-plane-full` | 패스의 캡처를 3D rear plane으로 되돌리는 방식(먼저 설정): 캡처된 패스의 불투명 픽셀(일부 / 4096 전부) |
| `probes-l4-vcount-write` | `vcw-212-lines`, `vcw-212-residual`, `vcw-205-lines`, `vcw-212-210-lines`, `vcw-outside-lines` | VCOUNT 쓰기: 쓰기 한 번이 더하는 라인 수, 남는 사이클, GBATEK이 허용하는 202..212 범위 밖의 쓰기가 무시되는지(Heddlemere는 10~11라인 구간만 L2 전제 조건으로 판정). melonDS에서 남는 사이클은 코드가 ROM의 어디에 놓이느냐에 따라 달라집니다([docs/QA-KO.md](docs/QA-KO.md)) |
| `probes-l4-tex-late` | `tex-late-192` ~ `tex-late-250` | 버퍼 스왑 뒤 정해진 라인에서 텍스처 팔레트를 다시 씀: 몇 번째 라인부터 늦은 쓰기가 3D 렌더에 반영되지 않는지(렌더링은 214라인에서 시작) |
| `probes-l4-3d-fog-dot` | `fog-boundary-start`, `fog-shift-11`, `edge-screen-border`, `poly-right-edge-pixel`, `dot-queued-cutoff`, `aa-line-pixels`, `zeq-step-boundary`, `fog-alpha-glitch` | GBATEK이 느슨하게 적은 fog·edge·1-dot 모서리(첫 fog 경계의 위치, FOG_SHIFT 11, 화면 경계의 edge marking, 큐에 쌓인 1-dot, 안티앨리어싱 선 픽셀, 깊이 Equal 경계 단계). Buoyane(games/buoyane)에서 옮겨 온 것으로, Buoyane은 GBATEK이 정해 둔 것만 판정합니다. `fog-alpha-glitch`는 알파에만 거는 fog를 BG0 블렌드 뒤에서 보는 항목입니다(Buoyane 실기에서는 글리치가 안 나타남). melonDS에서 `dot-queued-cutoff`는 코드가 ROM의 어디에 놓이느냐에 따라 달라집니다([docs/QA-KO.md](docs/QA-KO.md)) |
| `probes-l4-3d-zeq-slope` | `zeq-slope-band-left`, `zeq-slope-band-right`, `zeq-slope-decal-holes` | 경사면의 깊이 Equal. Buoyane 실기가 잰 장면(같은 좌표, 같은 raw z)입니다. 평평한 바닥 위에 Equal로 그린 경사면 띠가 어디서 시작해 어디서 끝나는지, 기울어진 바닥 위 데칼 경사면에서 몇 픽셀이 테스트에 실패하는지 잽니다(실기에서는 띠가 에뮬레이터보다 4, 7픽셀 오른쪽이고 구멍이 960픽셀이었는데, 그 값은 Buoyane에서 잰 것이고 이 ROM으로 잰 것이 아닙니다) |
| `probes-l4-3d-ram-tri` | `ram-tri-poly-events`, `ram-tri-vert-events`, `ram-tri-flag`, `ram-tri-sweep-counters`, `ram-tri-draw` | 분리 삼각형으로 보는 폴리곤·버텍스 RAM 한계(GBATEK은 RAM_COUNT의 범위를 0..2048, 0..6144로만 적고 리스트가 다 들어가지 않을 때는 말하지 않음). 한 리스트에 폴리곤 65535개를 보내며 하나마다 두 카운터를 읽고(카운터가 어디서 멈추거나 되감기는지, DISP3DCNT 비트 13이 몇 번째 폴리곤에서 서는지), 이어서 4x4 칸마다 하나씩 2852개를 그린 리스트를 캡처해 남은 칸을 셉니다. 이 모음을 위해 작성. `l4-3d-ram-*` ROM 6개를 DS Lite에서 실행했습니다(그중 5개는 이전 빌드. [docs/HW-QA-KO.md](docs/HW-QA-KO.md)) |
| `probes-l4-3d-ram-strip` | `ram-strip-poly-events`, `ram-strip-vert-events`, `ram-strip-flag`, `ram-strip-sweep-counters`, `ram-strip-draw` | 같은 것을 한 줄에 삼각형 스트립 하나로(폴리곤 한계가 먼저: 삼각형 62개에 버텍스 64개) |
| `probes-l4-3d-ram-quad` | `ram-quad-poly-events`, `ram-quad-vert-events`, `ram-quad-flag`, `ram-quad-sweep-counters`, `ram-quad-draw` | 같은 것을 분리 사각형으로(버텍스 한계가 먼저: 1536 x 4) |
| `probes-l4-3d-ram-mix` | `ram-mix-a` ~ `ram-mix-f`, `ram-mix-flags` | 삼각형, 스트립 삼각형, 사각형을 섞어 한 번에 한 한계에만 닿게 한 2852개짜리 리스트 6개: 그린 칸, 그려지지 않은 첫 칸, 비트 13이 선 폴리곤과 그때의 버텍스 카운터 |
| `probes-l4-3d-ram-fit` | `ram-fit-a` ~ `ram-fit-i`, `ram-fit-flags` | 위 ROM들의 DS Lite 숫자를 읽는 한 가지 해석을 시험하려고 만든 섞은 리스트 9개: 그려지지 않은 첫 칸, 비트 13이 선 폴리곤, 첫 빈칸 뒤에 그려진 칸 |
| `probes-l4-3d-ram-fit2` | `ram-fit2-j` ~ `ram-fit2-p`, `ram-fit2-flags` | `l4-3d-ram-fit`을 DS Lite에서 돌린 뒤의 해석을 시험하는 리스트 7개. 워드 구성은 같음 |
| `probes-l4-2d-obj-line` | `2d-obj-line-budget` | 한 줄에 스프라이트 120개: 마지막 그룹이 버려지는가 |
| `probes-l4-sys-dma-disp` | `sys-dma-disp-pixels` | 캡처 소스 B로 본 메인 메모리 디스플레이 FIFO |
| `probes-l4-2d-display` | `2d-display-pixels` | 같은 것을 VRAM 디스플레이 프레임 뒤에 확인 |

ROM의 데모 이름은 `l4-<name>`입니다(결과 블록 id가 다른 게임과 달라집니다). 항목 id는 Corvane의
`expected.json`에 있던 id 그대로입니다. 다만 `rear-plane`과 `rear-plane-full`은 Elevane에서 없어진 id이고,
`ram-*`은 여기서 새로 만든 id입니다.

### Lightenette·Elevane 항목의 대역

Lightenette의 이음선 항목은 그 게임의 막(act)에서 나오는 값이라 그대로 가져올 수 없습니다. Elevane의 HBlank DMA 구멍
항목은 Elevane의 L3 목표이고(probes도 함께 기록), rear plane 픽셀 항목은 Elevane에서 없애고 이 모음으로 옮겼습니다.

| 항목 | 대응 | 비고 |
|---|---|---|
| Lightenette `seam-uv-exact` | `seam-uv-exact` (`l4-acc-stage`) | 같은 acc-stage 측정, 묶인 값 전체 0/0/18 |
| Lightenette `act2-seam-exact`, `act5-seam-exact` | **대응 없음** | 2막·5막 인형의 절단선은 게임 안에만 있음 |
| Elevane `hdma-holes-zero` | `acc-split-hdma-holes` (`l4-acc-split-hdma`) | 같은 HBlank DMA 경로와 구멍 수. Elevane의 L3 목표로도 남음 |
| Elevane `hdma-composite`, `hdma-composite-full`, `hdma-composite-right-full` | **대응 없음** | 데모는 구멍을 세지, x = 128 양쪽의 불투명 픽셀을 세지 않음 |
| Elevane `rear-plane`, `rear-plane-full` | `rear-plane`, `rear-plane-full` (`l4-acc-rear-plane`) | 예전 프리셋 2와 같은 방식(캡처를 rear plane으로, 먼저 설정). 캡처된 패스에서 셈 |

## 지금까지 측정한 것

`expected.json`의 `known`에는 프로필 7개(melonDS 1.1과 master `4ce2188` 각각 soft / GL classic / GL compute, DeSmuME)와 항목별로
각 에뮬레이터가 낸 값이 있습니다. 표는 [docs/QA-KO.md](docs/QA-KO.md)에 있습니다. `known`에 기록된 값은 에뮬레이터의 동작이지
콘솔의 동작이 아닙니다. **실기:** `l4-3d-ram-*` probe 6개를 DS Lite에서 실행했습니다(2026-11-23/24, 각각 두 번. 그중 5개는 이전 빌드).
결과 워드와 알아낸 것, 그 숫자에 맞춘 규칙은 [docs/HW-QA-KO.md](docs/HW-QA-KO.md)에 있습니다. 다른 probe는 실기에서 측정하지 않았습니다.

## 빌드와 사용

```sh
./build.sh -j8 release          # build/release/demos(ROM 25개), build/release/rom(같은 25개의 `*-hw-support.nds`, docs/HW-QA-KO.md 참조), build/release/judge(expected.json + judge.py)
tools/melonds/mds.sh demos probes/build/release/demos > probes.log        # 헤드리스 melonDS (RENDERER=soft|gl|compute)
RESULT_WORDS=16 tools/emu/emu.sh demos probes > probes.log                # 헤드리스 DeSmuME
python3 probes/tools/judge.py probes.log                                 # 값 출력(참고용, 종료 코드 0)
python3 probes/tools/judge.py --profile melonds-1.1-software probes.log  # 기록된 값이 바뀌면 종료 코드 1
python3 -m unittest discover -s probes/tools/tests                       # judge 단위 테스트
```

빌드는 Corvane과 같은 고정된 BlocksDS 이미지(Docker, `Dockerfile` 참고)를 씁니다. `probes-vX.Y.Z` 태그를 푸시하면
CI가 `./build.sh release`를 실행합니다.

## 라이선스

Apache-2.0 (`LICENSE` 참고). 모든 그래픽·폰트·3D 모델·소리는 이 저장소의 코드에서 나옵니다([ASSETS-KO.md](ASSETS-KO.md)).
서드파티 런타임 라이브러리는 `THIRD_PARTY_NOTICES`에 있습니다(라이선스 전문은 `licenses/`이고, 모든 ROM의
`/licenses/`에도 들어 있습니다).
