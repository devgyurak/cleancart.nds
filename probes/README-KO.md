# Probes

[English](README.md)

에뮬레이터 개발자를 위한 연구용 probe 모음입니다. **실기에서 확인된 적 없는 동작**(레벨 L4,
[../docs/LEVELS-KO.md](../docs/LEVELS-KO.md))을 에뮬레이터가 어떻게 처리하는지 보고하는 작은
닌텐도 DS 데모 ROM 14개입니다. 게임이 아닙니다. 타이틀도, 크레딧도, QA 모드도 없고, 120프레임 동안
돌면서 결과 블록을 쓰고 화면을 그대로 유지하는 ROM뿐입니다.

용도: 각 probe는 GBATEK에 없거나 틀리게 적힌 동작에 대해 에뮬레이터가 내는 값을 기록합니다(폴리곤 RAM 개수,
`BOX_TEST`/`VEC_TEST` 결과, 같은 화면 2패스 캡처, 메인 메모리 디스플레이 FIFO, 줄당 OBJ 예산, 뒤집힌 UV 이음선).
아직 실기 값이 없으므로 실기 기준의 판정은 없습니다. `tools/judge.py`는 에뮬레이터별로 값을 **기록**하고,
값이 바뀌면 알려 줍니다.

## probe와 항목

항목 19개, 모두 레벨 L4, role `target`, source `unconfirmed`(참고용: 실행을 실패시키지 않음).

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
| `probes-l4-acc-dual` | `acc-dual-both` | 두 캡처 뱅크로 보는 정면·평면 화면 |
| `probes-l4-acc-split-hdma` | `acc-split-hdma-holes` | HBlank DMA 합성의 구멍(melonDS#2618 관련) |
| `probes-l4-acc-split-rear` | `rear-plane-latency` | 같은 화면 2패스의 rear plane 변형: 라우팅 지연 |
| `probes-l4-2d-obj-line` | `2d-obj-line-budget` | 한 줄에 스프라이트 120개: 마지막 그룹이 버려지는가 |
| `probes-l4-sys-dma-disp` | `sys-dma-disp-pixels` | 캡처 소스 B로 본 메인 메모리 디스플레이 FIFO |
| `probes-l4-2d-display` | `2d-display-pixels` | 같은 것을 VRAM 디스플레이 프레임 뒤에 확인 |

ROM의 데모 이름은 `l4-<name>`입니다(결과 블록 id가 Corvane과 달라집니다). 항목 id는 Corvane의
`expected.json`에 있던 id 그대로입니다.

### Lightenette·Elevane 항목의 대역

Lightenette의 이음선 항목 3개와 Elevane의 HBlank DMA·rear plane 항목 6개는 각 게임의 막(act)과 리허설에서
나오는 값이라 그대로 가져올 수 없습니다. 같은 것을 재는 범위에서만 Corvane의 대응 데모로 다룹니다.

| 이쪽으로 옮겨 오는 항목 | 대응 | 비고 |
|---|---|---|
| Lightenette `seam-uv-exact` | `seam-uv-exact` (`l4-acc-stage`) | 같은 acc-stage 측정, 묶인 값 전체 0/0/18 |
| Lightenette `act2-seam-exact`, `act5-seam-exact` | **대응 없음** | 2막·5막 인형의 절단선은 게임 안에만 있음 |
| Elevane `hdma-holes-zero` | `acc-split-hdma-holes` (`l4-acc-split-hdma`) | 같은 HBlank DMA 경로와 구멍 수 |
| Elevane `hdma-composite`, `hdma-composite-full`, `hdma-composite-right-full` | **대응 없음** | Corvane 데모는 구멍을 세지, x = 128 양쪽의 불투명 픽셀을 세지 않음 |
| Elevane `rear-plane`, `rear-plane-full` | **대응 없음** | `rear-plane-latency`는 rear plane 경로의 라우팅 지연만 재며, 캡처된 픽셀 수는 재지 않음 |

## 지금까지 측정한 것

`expected.json`의 `known`에는 프로필 4개(melonDS 1.1 소프트웨어, GL classic, GL compute, DeSmuME)와 항목별로
각 에뮬레이터가 낸 값이 있습니다. 표는 [docs/QA-KO.md](docs/QA-KO.md)에 있습니다. **실기는 측정하지
않았습니다.** 기록된 값은 에뮬레이터의 동작이지 콘솔의 동작이 아닙니다.

## 빌드와 사용

```sh
./build.sh -j8 release          # build/release/demos(ROM 14개)와 build/release/judge(expected.json + judge.py)
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
