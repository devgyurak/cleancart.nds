# Pendulane QA

[English](QA.md)

melonDS 1.1 점검표입니다 (macOS 키는 저장소 `AGENTS.md`).

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 황동 글자 "PENDULANE"(생성한 타일 BG)와 그 앞에서 흔들리는 스프라이트 진자(좌우 약 32도, 한 번 흔들리는 데 150프레임), 부제, 깜박이는 START 줄, 진자 테마 재생(나갈 때 멈춤); 아래 화면에 라이선스·ARM7·세이브 줄; Start는 장치 목록, Y는 벤치 장부, X는 크레딧 | headless 실행만 |
| 장치 목록 | "THE WORKSHOP", 장 이름과 울린 개수, RUNG 표시가 붙는 장치 넷, 좌우 장, 상하 장치, A 시작, B 뒤로 | headless 실행만 |
| 장치 | 위: 장, 이름, 진자 틱, 계획 틱과 게이지, 슬롯별 한 줄(고른 역과 비용); 상하 슬롯, 좌우 역, 틱이 맞으면 A로 종을 울림("THE BELL RINGS", 다음 장치) 아니면 TOO MANY / TOO FEW TICKS; X는 벤치를 한 번 돌려 아래 화면의 장부와 봉인을 채움; B 목록 | ✅ melonDS 1.1 software (1-03, 3-03 스크린샷 확인); 손으로 해 본 느낌은 확인 안 함 |
| 리듬 피날레 | 틱 여덟 개가 선으로 떨어지고 박자에 A(±6프레임), HITS n / 8, 목록으로 | headless 실행만 (QA 파일럿이 정확한 프레임에 누름) |
| 벤치 장부 | 벤치 전체를 한 번 돌리고("RUNNING THE BENCH...", 약 1초) 잰 비용을 책 값 옆에 모두 나열, 판정 창 안이면 초록 밖이면 빨강; 아래 화면에 장별 봉인; B로 뒤로 | ✅ melonDS 1.1 software (스크린샷 확인: 세 장 금색, 큰길과 음관은 아님) |
| 크레딧 | 스크롤 크레딧, X는 라이선스 뷰어 | 아직 확인 안 함 |
| rehearsal demo | `pendulane-rehearsal.nds`는 바로 벤치로 부팅; 결과 블록 id `0x48524450` (`PDRH`); README의 방법대로 `judge.py --profile`로 채점 | ✅ 2026-11-17: 프로파일 4개 모두 측정, 각각 `judge.py --profile` MATCH. melonDS 1.1(모든 렌더러): 판정 target 15개 중 11개 OK (카트 gap 둘 다 통과: 1280과 2047). NG: `mem7-code-main-seq`(콘솔은 142인데 128), `snd-pcm-busy`·`snd-adpcm-busy`(19와 43: Busy가 마지막 샘플의 끝에서 꺼져 GBATEK보다 1 늦음), `snd-hold-restart`(7: 붙잡힌 옛 값 없이 0 샘플 7개); 시작 차이 두 개는 GBATEK의 2와 8 그대로. DeSmuME py-desmume 0.0.9: 15개 중 6개 OK; NG는 같은 사운드 항목과 `snd-onset-pcm-psg`(6: PSG가 캡처 첫 샘플부터 들림), `spi-512k-minus-4m`, 카트 gap 둘 다(전송이 바로 끝나 엣지 0), `mem7-main-nonseq`(64. 콘솔과 melonDS는 383과 384), `mem7-code-main-seq`(128); `mem7-main-seq`·`mem7-vram-seq`는 1024. 수학 차이 DeSmuME 10 / 32 / 0, melonDS 9 / 32 / 0. melonDS SPI 엣지 59 / 122 / 250 / 508. 실기: [HW-QA-KO.md](HW-QA-KO.md) 참고(`mem7-main-nonseq` 383, `mem7-code-main-seq` 142) |

실기: 2026-11-17 DS Lite + R4 실행 1·2, [HW-QA-KO.md](HW-QA-KO.md) 참고.

## 자동 QA 실행

QA 로그의 `cp` 줄 끝에는 `gx=uf<n>,of<n> rdl=<min>`이 붙는다. 직전 체크포인트 이후 구간에서 DISP3DCNT bit 12(컬러 버퍼 RDLINES 언더플로)와 bit 13(폴리곤/버텍스 RAM 오버플로)이 켜져 있던 프레임 수, 그리고 RDLINES_COUNT의 최솟값(버퍼된 줄 수 − 2, 0..46. 구간에 표본이 없으면 `rdl=-`, 예: 같은 프레임의 두 체크포인트)이다. 두 플래그는 끈적하고 QA는 읽기만 하며 지우지 않으므로 값은 "플래그가 켜져 있던 프레임 수"다. 다만 게임이 플래그를 지우지 않을 때만 그렇다. bit 13을 스스로 확인(지우기)하는 게임(Corvane·Elevane·Heddlemere·Hushwell·Lightenette·Paneveil의 `g3_overflowed()`)은 확인 횟수를 `qa_game_gx_acks()`로 알리고 한 번을 "플래그가 켜져 있던 프레임"으로 센다. bit 12는 0으로 써서 언더플로 플래그가 남게 한다. `lag`처럼 참고용이라 해시에 넣지 않고 비교하지도 않으며(`tools/qa/compare.py --info`가 나란히 보여 준다), 3D를 쓰지 않는 장면은 `0,0`이다. 에뮬레이터는 이 플래그를 거의 세우지 않고 실기는 세울 수 있다.

릴리스 ROM(`pendulane.nds`)의 숨은 모드: 타이틀에서 L, R을 누른 뒤 X, Y를 눌러 네 키를 120프레임 누릅니다. 정상 흐름(타이틀 → 목록 → 장치 20개 전부, 장마다 리듬 곡으로 마무리 → 목록 → 타이틀 → 크레딧 → 타이틀)을
진행하고 벤치를 한 번 돌린 뒤 `RESULT PASS`로 끝납니다. 로그는 SD 카드(`cleancart/pendulane/qa-<날짜>-<시각>.log`). Headless:
`SDDIR=out/pd/x tools/melonds/mds.sh run games/pendulane/pendulane.nds 6000 out/pd/x "300:L:140,300:R:140,300:X:140,300:Y:140"` (먼저
`out/pd/x/cleancart/pendulane/`를 만드세요). 2026-11-17 melonDS 1.1(software)에서 확인: 프레임 4680에서 `RESULT PASS`, 장치 20개 중 20개 해결, 장 마스크 `0x1f`, 체크포인트 7개,
두 번 돌린 `tools/qa/compare.py` 차이 0, `CPU9 cpsr` 모드 0x1f; `QARN` 블록을 melonDS 1.1 software / GL Classic / GL Compute / DeSmuME에서
`judge.py --profile`로 채점해 모두 MATCH.

## 키 없이 QA 시작 (SD 마커)

SD 카드에 빈 파일 `cleancart/pendulane/autoqa`를 두세요 (폴더가 있어야 합니다). 부팅 후 첫 타이틀 화면에서 같은 QA 실행이 시작되고, 시작 전에
파일 이름이 `autoqa.used-<날짜>-<시각>`으로 바뀝니다.
