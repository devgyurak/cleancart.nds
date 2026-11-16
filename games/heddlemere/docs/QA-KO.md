# Heddlemere QA

[English](QA.md)

상태: 뼈대. 아래 점검은 이 변경에서 돌렸고, 측정 항목은 다음 변경들에서 추가합니다.

| 점검 | 방법 | 결과 |
|---|---|---|
| 빌드 | `./build.sh -j8`, `./build.sh DEMO=rehearsal` | 경고 0 |
| 주문 | `python3 tools/order_check.py --plan-hash` | 40개 중 40개 통과, plan 해시 `0xfc5a15bb` |
| 리허설 블록 | `tools/melonds/mds.sh run games/heddlemere/heddlemere-rehearsal.nds 1500 out/hm/reh "" 1499` | id `0x48524D48`(`HMRH`), values[0] 비트 5(ARM7 응답), values[21] = 5, values[22] = `0x1f`, values[23] = `0xfc5a15bb`(검사 도구의 해시와 같음) |
| 자동 QA 실행 | 타이틀에서 L+R+X+Y를 120프레임 누르거나 SD 마커 `cleancart/heddlemere/autoqa`. headless: `SDDIR=out/hm/x tools/melonds/mds.sh run games/heddlemere/heddlemere.nds 24000 out/hm/x/o "300:L:140,300:R:140,300:X:140,300:Y:140"`(먼저 `out/hm/x/cleancart/heddlemere/`를 만들어 둠) | melonDS 1.1 소프트웨어: `RESULT PASS`, 주문 40개 중 40개, 장 마스크 `0x1f`, 체크포인트 7개. `tools/qa/parse_log.py`·`tools/qa/lint.py` 통과 |

직접 플레이(melonDS): 십자키 위/아래로 칸을 고르고, 왼쪽/오른쪽으로 돌리고, A로 납품, B로 목록.
