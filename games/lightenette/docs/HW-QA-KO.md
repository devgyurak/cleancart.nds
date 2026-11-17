# Lightenette 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `lightenette.nds` 대신 `build/release/rom/lightenette-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Lightenette(L2)는 리허설, `l2-*` 데모, 자동 QA 실행으로 판정합니다. 2026-11-17 DS Lite에서 판정 스위트를 포함한 QA 실행을 했습니다(아래 실행 1). 판정 항목 37개 중 OK 29, 참고용 6, 판정 NG 2(`snd-wave-pan-right-exact`, `3d-limits-ram`)이고, 두 NG 모두 우리 기대값에 대한 발견입니다. v0.3.1에서 두 값을 정하고 실행 1의 참고용 항목을 승격했으며, v0.3.1로 한 실행 2는 **37개 중 37개 OK**이고 두 값이 그대로 나왔습니다. 절차는 스킬 `nds-hw-qa`에 있고, 이 문서는 결과를 기록하는 곳입니다.
실기 값은 "에뮬레이터 프로필"이 아닙니다. 실기에서 NG가 나오면 우리 기대값이나 레벨을 다시 봐야 한다는 발견입니다(제안은 `level-judge`, 변경은 별도 PR).

## 카드에 넣을 것

- ROM: `lightenette.nds`의 hw-support 빌드 하나만. QA 실행이 모든 것을 판정합니다. `RESULT PASS` 뒤에 QA 블록(리허설과 `acc-stage` 항목을 담음)을 쓰고 `l2-*` 데모 10개를 돌려 데모마다 결과 파일 하나와 진행 파일 `suite-<stamp>.log`를 남깁니다. 따로 있는 데모·리허설·`acc-stage` ROM은 스위트 줄이 `saved=0`인 항목 하나를 다시 돌릴 때만 씁니다(Wood 커널에서는 SD 카드를 받지 못함).
- 빈 폴더 `cleancart/lightenette/` (없으면 ROM이 아무것도 쓰지 않음) 및 QA 실행용 빈 파일 `cleancart/lightenette/autoqa`

## 실행

1. 게임 QA: `lightenette-hw-support.nds`를 켜고 그냥 둡니다(표시 파일이 타이틀에서 QA를 시작). 크레딧 뒤에 데모가 화면에 나옵니다.
2. 전원은 스위트가 끝난 뒤에 끕니다(melonDS는 `suite end demos=10 saved=10`을 씀. 실행 1의 참고 참조).
3. Mac: `python3 tools/qa/ingest.py /Volumes/<카드> --game lightenette`.

## 기록 (실행마다 한 블록)

### 실행 2: QA 실행과 판정 스위트, 2026-11-17 (v0.3.1)

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, 실행 1과 같은 DS Lite(NTR), 카드, Wood R4 커널(파일 날짜 2002-01-15) |
| ROM | `lightenette-hw-support.nds` v0.3.1, 로그 헤더의 build `542e848ee9ea`(릴리스 파일: SHA-256 `2498d167a4baf75d85f70775268beeefb126f7ecb558379253e21c68aa5abc8c`, 481,644바이트). ROM 코드는 v0.3.0과 같고 v0.3.1은 기대값(#177)과 주석만 바꿈. 파일은 Finder로 카드에서 복사했으므로 카드의 ROM 복사본 자체는 해시하지 않음 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 31줄, 체크포인트 6개, `start=marker`. 스위트: QA 블록과 데모 10개 중 10개 `saved=1`, timeout 없음. 블록 파일 11개 판정: **판정 항목 37개 중 37개 OK**(QA 블록 L1 14/14, L2 10/10. 데모 L1 3/3, L2 10/10), 참고용 항목 없음, n/a 없음 |
| QA 로그 헤더 | 실행 1과 같음(`sd=1 fat_fail=0`, R4 DLDI, `cart_ready=0 nitro=1`, `save_detected=1`) |
| 처음 갈리는 체크포인트 | 없음. act1-act5와 judge 해시가 실행 1 및 melonDS 1.1 soft와 같음 |
| 랙 프레임 | 90 / 1 / 1 / 1 / 1 / 41 (실행 1: 89 / 1 / 1 / 1 / 1 / 41) |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모두 `uf0,of0 rdl=46` |

실행 2와 실행 1을 단어 단위로 비교하면, 판정하지 않거나 범위로 판정하는 값을 빼고 모든 결과 블록이 같습니다: QA 랙 합계, `snd-capture` 피크(values[5]), `sys-irq-hblank`(31465, 실행 1은 31520, 둘 다 31297..31560 안). 실행 1에서 정한 두 값은 그대로 나왔습니다: `snd-wave-pan-right-exact` 3, `3d-limits-ram` 2979(`l2-acc-dual` values[4]도 다시 폴리곤 2310, 오버플로 플래그 켜짐).

참고: `suite-*.log`가 이번에도 `l2-sys-irq saved=1`(프레임 2999) 뒤에서 `suite end` 줄 없이 끝납니다.

### 실행 1: QA 실행과 판정 스위트, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | `lightenette-hw-support.nds` v0.3.0 `e7fc0ad92f620290896bf4c162f95403769ec4e50096ce13532bcafe9a519bea`(릴리스와 같음). melonDS도 같은 파일로 실행 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 31줄, 체크포인트 6개, build `c40913a7fff7`, `start=marker`로 시작. 스위트: QA 블록과 데모 10개 중 10개 `saved=1`, timeout 없음. 블록 파일 11개 판정: QA 블록 L1 14/14, L2 6/6 OK +4 참고용. 데모: 판정 NG 2개(아래), 나머지 OK. 합계 OK 29, **NG 2**, 참고용 6, n/a 없음 |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | melonDS 1.1 soft와는 없음. act1-act5와 judge 해시가 같음(`compare.py`, 차이 0). GL Classic은 act2부터, GL Compute는 act3부터 다름(각 프로필의 알려진 렌더러 NG) |
| 랙 프레임 | 89 / 1 / 1 / 1 / 1 / 41 (act1-act5, judge). melonDS soft 1 / 0 / 0 / 0 / 0 / 37. act 1에는 SD 카드로 미러링하는 세이브 쓰기가 들어 있음 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모든 체크포인트에서 `uf0,of0 rdl=46`, melonDS와 같음 |

| 항목 | 실기 | melonDS 1.1 soft | GL | Compute | DeSmuME | 차이? |
|---|---|---|---|---|---|---|
| `snd-wave-pan-right-exact` (GBATEK, 기대 0..2) | **3** (NG) | 3 (NG) | 3 (NG) | 3 (NG) | OK(프로필, 실행 안 함) | 실기가 GBATEK 2/128 경계가 아니라 melonDS와 같음. 우리 기대값에 대한 발견. pan 0 기준 진폭은 둘 다 199 |
| `3d-limits-ram` (GBATEK, 기대 2048) | **2979** (NG) | 2048 | 2048 | 2048 | n/a(프로필, 실행 안 함) | 있음. 콘솔에서는 flush 직전에 읽은 RAM_COUNT 폴리곤 수가 2048을 넘어 계속 셈(마지막 프레임에 삼각형 2980개 제출). melonDS는 2048에서 멈춤 |
| `3d-limits-overflow` (unconfirmed) | 47 | 47 | 47 | 47 | n/a(프로필, 실행 안 함) | 없음 |
| `l2-acc-dual` values[4] (한 패스: 폴리곤 수, 오버플로 플래그. 판정 안 함) | 2310, 1 | 1536, 1 | 비교 안 함 | 비교 안 함 | 실행 안 함 | 있음, 같은 RAM_COUNT 동작. `acc-dual-over` / `-passes`는 둘 다 OK. 캡처 해시 values[7..9]도 다름(버려지는 폴리곤이 다름) |
| `line-polygon` / `act4-hline` / `act4-dline` / `act5-line-polygon` (unconfirmed) | 697 / 208 / 208 / 93 | 697 / 208 / 208 / 93 | 698 / 208 / 208 / 93 | 826 / 208 / 208 / 186 | 실행 안 함 | melonDS soft와 같음 |
| `acc-dual-halves` (unconfirmed) | 0x8 | 0x8 | 0x8 | 0x8 | 실행 안 함 | 없음 |
| `sys-irq-hblank` | 31520 | 31532 | 비교 안 함 | 비교 안 함 | 실행 안 함 | 둘 다 31297..31560 안 |
| 나머지 판정 항목 | melonDS soft와 같음 | | | | | 없음 |

참고:
- `suite-*.log`가 마지막 데모(`l2-sys-irq saved=1`, 프레임 3069) 뒤에서 끝나고, melonDS가 그 데모 3프레임 뒤에 쓰는 `suite end` 줄이 없습니다. 데모 파일은 모두 카드에 있어 빠진 항목은 없습니다. Corvane과 Hushwell 실행에서도 같은 줄이 없었습니다. 그 줄 전에 전원을 껐는지, 실기에서 마무리 단계가 멈추는지는 아직 열려 있습니다.
- 판정 항목이 아니면서 다른 값: QA 랙 합계(values[53]), `snd-capture` values[5](캡처 피크).

## 후속 조치

실기 NG는 모두 표로 사용자에게 보고하고, 여기서 `expected.json`을 고치지 않습니다. `level-judge`에게 레벨 제안을 받고(근거 `hw:games/lightenette/docs/HW-QA.md`), 기대값·레벨 변경은 별도 PR로 합니다.
