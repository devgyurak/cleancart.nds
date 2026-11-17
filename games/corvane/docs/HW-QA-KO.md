# Corvane 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `corvane.nds` 대신 `build/release/rom/corvane-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Corvane(L1, 그래픽·시스템)은 데모와 자동 QA 실행으로 판정합니다. 2026-11-17 DS Lite에서 판정 스위트를 포함한 QA 실행이 통과했습니다(아래 실행 1). 판정 항목 92개 중 92개 OK입니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `build/release/rom/corvane-hw-support.nds` 하나만. QA 실행이 모든 것을 판정합니다(크레딧 뒤 데모 48개). 단독 데모 ROM은 파일에 `saved=0`이 보인 데모 하나를 다시 돌릴 때만 씁니다(`io-qalog`는 필요 없음: 항목이 QA 블록 파일로 채점됨).
- 빈 폴더 `cleancart/corvane/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/corvane/autoqa`

## 실행

1. 게임 QA: `corvane-hw-support.nds`를 켜고 스위트가 끝날 때까지 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작).
2. 스위트가 끝날 때까지 기다립니다(로그 끝이 `suite end demos=48 saved=48`). 데모를 따로 돌릴 필요는 없습니다.
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game corvane`. (`shot-*.bmp`도 함께 복사합니다).
4. shot을 melonDS와 비교합니다(`docs/QA-KO.md`의 "자동 QA 실행" 참고): `python3 tools/qa/compare.py --info <melonDS QA 로그> <카드 QA 로그>`가 shot 이름별 CRC를 나란히 보여 주면, 콘솔의 `shot-*-st3-600.bmp`와 같은 이름의 melonDS BMP를 나란히 열어 봅니다(지금은 스테이지 3만 찍습니다).

## 기록 (실행마다 한 블록)

### 실행 1: QA 실행과 판정 스위트, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | `corvane-hw-support.nds` v0.4.0 `1840bfd91d3fcc9db3a5a7deb5498853e30a0895e22ef23e028501b14eb707c9`(릴리스와 같음). melonDS도 같은 파일로 실행 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 56줄, 체크포인트 5개, build `01c2a1fd6f25`, `start=marker`로 시작. 스위트: 데모 48개 중 48개 `saved=1`, timeout 없음. 블록 파일 49개(데모 48 + QA 블록) 판정: **L1 92/92 OK** +1 참고용, `io-qalog-verdict`는 QA 블록으로 OK |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | 없음. st1-st5 해시가 melonDS 1.1 soft와 같음(`compare.py`, 차이 0) |
| 랙 프레임 | 60 / 182 / 227 / 181 / 200 (st1-st5). melonDS 58 / 118 / 145 / 119 / 129. 콘솔은 같은 게임 프레임을 VBlank 2개짜리 프레임이 더 많은 채로 돈다 |
| shot `st3-600`: 화면, crc, melonDS와 같은가? | 위 화면, `e2f3e3a0`. melonDS `9f1afc67`. 49,152픽셀 중 13개가 다르고, 모두 기체의 대각선 폴리곤 모서리 한 줄(x 128-153, y 123-135)에서 색 채널 하나가 1단계씩 다름(예: `0x5652` 대 `0x5672`). 모서리 색 보간의 반올림 차이 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모든 체크포인트에서 `uf0,of0 rdl=46`, melonDS와 같음 |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| `sys-irq-hblank-visible` | 31464 | 31323 | 실행 안 함 | 실행 안 함 | 실행 안 함 | 있음, 둘 다 22848..31560 안 |
| `sys-irq-vcount` | 120 | 119 | 실행 안 함 | 실행 안 함 | 실행 안 함 | 있음, 둘 다 118..121 안 |
| 나머지 판정 항목 90개 | melonDS와 같음 | | | | | 없음 |

참고:
- `suite-*.log`가 마지막 데모(`sys-irq saved=1`) 뒤에서 끝나고, melonDS가 쓰는 `suite end` 줄이 없습니다. 데모 파일은 모두 카드에 있어 빠진 항목은 없습니다. 그 줄 전에 전원을 껐는지, 마무리 단계가 멈췄는지는 아직 모릅니다. 다음 실행에서 확인합니다.
- 판정 항목이 아니면서 다른 값: `sys-timer` values[0], [3](참고용 단어).

## 후속 조치

실기 NG는 모두 표로 사용자에게 보고하고, 여기서 `expected.json`을 고치지 않습니다. `level-judge`에게 레벨 제안을 받고(근거 `hw:games/corvane/docs/HW-QA.md`), 기대값·레벨 변경은 별도 PR로 합니다.
