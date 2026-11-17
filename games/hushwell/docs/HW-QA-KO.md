# Hushwell 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `hushwell.nds` 대신 `build/release/rom/hushwell-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Hushwell(L1, 사운드·I/O)은 데모와 자동 QA 실행으로 판정합니다. 2026-11-17 DS Lite에서 판정 스위트를 포함한 QA 실행이 통과했습니다(아래 실행 1). 판정 항목 27개 중 27개 OK이고, 플래시카트에서 잴 수 없는 10개는 n/a입니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `hushwell.nds`의 hw-support 빌드 하나만. QA 실행이 모든 것을 판정합니다: 크레딧 뒤에 채점 데모 16개를 돌려 결과 파일을 하나씩 씁니다. `io-save-flash` / `io-save-fram`은 돌리지 않습니다(다른 세이브 칩). 이 항목들과 모든 `env: cart` 항목은 실기에서 n/a입니다(2026-11-17 결정, R4는 칩 세이브를 검증하지 못함). 단독 데모 ROM은 파일에 `saved=0`이 보인 데모 하나를 다시 돌릴 때만 씁니다.
- 빈 폴더 `cleancart/hushwell/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/hushwell/autoqa`

## 실행

1. 게임 QA: `hushwell-hw-support.nds`를 켜고 스위트가 끝날 때까지 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작).
2. 스위트가 끝날 때까지 기다립니다(로그 끝이 `suite end demos=16 saved=16`). 데모를 따로 돌릴 필요는 없습니다.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game hushwell`.

## 기록 (실행마다 한 블록)

### 실행 1: QA 실행과 판정 스위트, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | `hushwell-hw-support.nds` v0.3.0 `3bad02796ecdeee48537f689d10408ed7433c4c6010b2b9afb26e0284744485f`(릴리스와 같음). melonDS도 같은 파일로 실행 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 53줄, 체크포인트 6개, build `dcb1b33f744e`, `start=marker`로 시작. 스위트: 데모 16개 중 16개 `saved=1`, timeout 없음. 블록 파일 17개(데모 16 + QA 블록) 판정: **L1 27/27 OK** +2 참고용. n/a 10개(카트: `sys-dma-cart-found/-dma/-nitro`, `io-cart-ok/-ready`. 칩 세이브: `io-save-eeprom-write/-reload/-seq`. 터치: `io-touch-x/-y`) |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | 없음. ch0-ch5 해시가 melonDS 1.1 soft와 같음(`compare.py`, 차이 0) |
| 랙 프레임 | 83 / 2196 / 2113 / 1476 / 765 / 899 (ch0-ch5). melonDS 80 / 1824 / 1481 / 944 / 538 / 849 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모두 `uf0,of0`. `rdl`은 46 / 46 / 43 / 46 / 46 / **39**(melonDS는 모든 체크포인트에서 46). 콘솔의 3D 렌더 버퍼 여유가 ch2와 ch5에서 줄었고 언더플로는 없음 |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| `io-firmware-name` | 3 | 7 | 실행 안 함 | 실행 안 함 | 실행 안 함 | 있음, 본체 사용자 이름과 melonDS 기본 이름의 차이. 둘 다 0..10 안 |
| `io-rtc-month` / `-day` / `-hour` | 1 / 15 / 6 | 11 / 17 / 11 | 실행 안 함 | 실행 안 함 | 실행 안 함 | 있음, 본체 시계 미설정(2002-01-15). 모두 범위 안 |
| 카트 항목(5개) | 준비 안 됨(플래시카트) | 준비됨 | | | | 플래시카트에서 n/a |
| 나머지 판정 항목 | melonDS와 같음 | | | | | 없음 |

참고:
- `suite-*.log`가 마지막 데모(`io-save-eeprom saved=1`) 뒤에서 끝나고, melonDS가 쓰는 `suite end` 줄이 없습니다(Corvane 실행도 같음). 데모 파일은 모두 카드에 있어 빠진 항목은 없습니다. 그 줄 전에 전원을 껐는지, 마무리 단계가 멈췄는지는 아직 모릅니다. 다음 실행에서 확인합니다.
- 판정 항목이 아니면서 다른 값: `snd-capture` values[5](캡처 피크), `io-save-eeprom` values[3].

## 후속 조치

실기 NG는 모두 표로 사용자에게 보고하고, 여기서 `expected.json`을 고치지 않습니다. `level-judge`에게 레벨 제안을 받고(근거 `hw:games/hushwell/docs/HW-QA.md`), 기대값·레벨 변경은 별도 PR로 합니다.
