# Heddlemere QA

[English](QA.md)

상태: 뼈대와 측정 코어. 아래 점검은 이 변경에서 돌렸습니다.

| 점검 | 방법 | 결과 |
|---|---|---|
| 빌드 | `./build.sh -j8`, `./build.sh DEMO=rehearsal` | 경고 0 |
| 주문 | `python3 tools/order_check.py --plan-hash` | 40개 중 40개 통과, plan 해시 `0xfc5a15bb` |
| 리허설 블록 | `tools/melonds/mds.sh run games/heddlemere/heddlemere-rehearsal.nds 1500 out/hm/reh "" 1499` | id `0x48524D48`(`HMRH`), values[0] 비트 5(ARM7 응답), values[21] = 5, values[22] = `0x1f`, values[23] = `0xfc5a15bb`(검사 도구의 해시와 같음) |
| 판정 | `tools/melonds/mds.sh run games/heddlemere/heddlemere-rehearsal.nds 3000 out/hm/r "" 2999 \| python3 games/heddlemere/tools/judge.py --profile melonds-1.1-software`(`RENDERER=gl`, `compute`). DeSmuME: `RESULT_WORDS=64 tools/emu/emu.sh run … \| … --profile desmume-pydesmume-0.0.9` | 2026-11-16, 리허설과 QA 부팅 모두 프로필 네 개(melonDS 1.1 소프트웨어, GL Classic, GL Compute, DeSmuME py-desmume 0.0.9)에서 `--profile` MATCH. melonDS(렌더러 전부)는 `hblank-low-arm9`(1606 대신 1585사이클)와 `gx-full-stall-rate`(쓰기당 19 대신 18.2사이클)에서 NG이고, DeSmuME는 GX 기울기(명령마다 1틱)·VCOUNT 쓰기 항목·half-IRQ 기울기도 NG이며 `swap-ramcount-line`은 n/a(폴리곤 카운터 없음). melonDS에서 정보용 네 항목은 ARM7 HBlank 1550 / 스왑 꼬리 328 / TRANS 35 / BOX_TEST 257. 실기 값은 아직 없음 |
| 자동 QA 실행 | 타이틀에서 L+R+X+Y를 120프레임 누르거나 SD 마커 `cleancart/heddlemere/autoqa`. headless: `SDDIR=out/hm/x tools/melonds/mds.sh run games/heddlemere/heddlemere.nds 24000 out/hm/x/o "300:L:140,300:R:140,300:X:140,300:Y:140"`(먼저 `out/hm/x/cleancart/heddlemere/`를 만들어 둠) | melonDS 1.1 소프트웨어: `RESULT PASS`, 주문 40개 중 40개, 장 마스크 `0x1f`, 체크포인트 7개. `tools/qa/parse_log.py`·`tools/qa/lint.py` 통과. 두 번 실행해 `tools/qa/compare.py` 차이 0. DeSmuME도 `QARN` 블록까지 갑니다(키 조합은 프레임 500) |

직접 플레이(melonDS): 십자키 위/아래로 칸을 고르고, 왼쪽/오른쪽으로 돌리고, A로 납품, B로 목록.
