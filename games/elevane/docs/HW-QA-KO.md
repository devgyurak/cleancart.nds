# Elevane 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `elevane.nds` 대신 `build/release/rom/elevane-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

**판정 스위트(QA 판정 스위트 PR 이후):** QA 실행(`autoqa`)이 이제 스위트로 끝납니다. `RESULT PASS` 뒤에 QA 블록, 리허설, `l3-*` 데모 9개를 `res-*-elevane-*.log`로 쓰고 진행 파일 `suite-<stamp>.log`도 남깁니다. hw-support ROM 한 번 실행으로 충분하고, **전원은 스위트가 끝난 뒤에** 끕니다(`suite-<stamp>.log`에 `suite end`). 아래의 리허설·데모 ROM은 항목 하나를 다시 돌릴 때만 씁니다(Wood 커널에서는 SD 카드를 받지 못함). `tools/qa/ingest.py`가 파일마다 채점합니다.

**2026-11-17 DS Lite 실행 1(이 문서 끝에 기록):** QA 실행 PASS, 판정 항목 모두 OK.

실행 1 전에는 실기에서 잰 값이 없었습니다. 이 문서는 실행 점검표이며, 첫 값을 기록하고
`expected.json`의 `unconfirmed` 항목을 승격하기 위한 것입니다. 콘솔은 사람이 조작하고, 아래는 할 일과
적어 둘 것입니다.

**자동 경로(스킬 `nds-hw-qa`):** 카드에 빈 폴더 `cleancart/elevane/`를 만들면 ROM이 결과 블록을 거기에 기록합니다
(데모에는 `RESULT SAVED`가 뜸). 빈 파일 `cleancart/elevane/autoqa`는 버튼 없이 QA 실행을 시작합니다.
이후 `python3 tools/qa/ingest.py /Volumes/<카드> --game elevane`가 `judge.py --hardware --partial`로 모두 판정합니다.
아래 사진 경로(3·6단계)는 카드에 쓸 수 없을 때의 대체 수단입니다. 실기 NG 항목은 프로필이 아니라 발견입니다. 기록 표는 다른 키트의 틀을 참고하세요.

## 실기가 답해야 할 것

| 질문 | 확인 위치 |
|---|---|
| 양화면 3D(디스플레이 캡처, 화면마다 30fps)가 맞게 보이고, 라우팅 레이턴시 1 호출이 맞는가? | 게임, 아무 퍼즐: 두 화면이 서로 맞는 정면도/평면도를 보여 주고 깜박임이 없음 |
| 감상 프리셋 1(HBlank DMA, melonDS #2618 경로)이 매 프레임 그림 전체를 그리는가? | 리허설 값 10, 11(구멍) = 0, 12/13 = 3072. 게임: 퍼즐에서 X, 위 화면 확인 |
| 프리셋 2(리어 플레인)가 양쪽 절반을 그리는가? | 리허설 값 15 = 4096. 게임: 감상 모드에서 A |
| 두 패스가 각각 폴리곤 2048개/정점 6144개 미만이고 넘침 플래그가 없는가? | 리허설 값 7, 8, 9 (실기에는 폴리곤 카운터가 있음) |
| x = 128에 밝은 이음매 세로줄이 어디서든 생기는가? (에뮬레이터에서는 이 경로들에서 보이지 않음) | 감상 모드와 양화면 보기를 눈으로 확인 |
| 게임이 크레딧까지 완주하고(자동 QA), 세이브 칩이 정상 동작하는가? | 게임 ROM, 숨은 QA 모드 |

## 순서

1. 빌드: `cd elevane && ./build.sh -j8 release`. `build/release/rom/elevane.nds`와
   `build/release/judge/elevane-rehearsal.nds`의 `shasum -a 256`을 적어 둡니다.
2. 두 ROM을 R4 카드에 복사합니다(`cleancart/elevane/`용 공간을 남겨 둠).
3. **리허설**: `elevane-rehearsal.nds`를 시작하고 아무것도 건드리지 않은 채 1분쯤 기다립니다. 장마다 첫 퍼즐을
   풀고 감상 프리셋 두 개를 돌린 뒤 `REHEARSAL RESULT`와 값 0~17(16진수)을 표시합니다. 위 화면을 사진으로 찍습니다
   (또렷하게, 모든 줄이 읽히게). 돌아가는 동안 감상 그림이 깜박이거나 구멍이 보였는지도 적어 둡니다.
4. **게임**: `elevane.nds`를 시작합니다. 퍼즐 하나(`1-01`: 네 개짜리 한 줄, 납품은 A)를 플레이하고 X로 감상 모드,
   A로 프리셋 전환, B로 복귀합니다. 프리셋마다 그림이 완전한지 / 한쪽이 빈지(어느 쪽) / 깜박이는지 /
   밝은 이음매 세로줄이 있는지 적습니다.
5. **자동 QA 실행**(스킬 `nds-hw-qa`): 타이틀에서 L과 R을 누른 뒤 X와 Y를 눌러 네 키를 2초 유지합니다.
   게임이 크레딧까지 스스로 플레이하고(약 5분) SD 카드에 `cleancart/elevane/qa-*.log`를 씁니다.
   이후 `python3 tools/qa/ingest.py /Volumes/<card> --game elevane`, 에뮬레이터 실행과의 비교는
   `python3 tools/qa/compare.py <emu.log> <hw.log> --info`.
6. 리허설 사진 채점: `python3 games/elevane/tools/judge.py --values <hex> <hex> ...`(값 0~17을 순서대로,
   종료 코드 0 = 필수 항목 실패 없음). `--profile melonds-1.1-software`를 붙이면 에뮬레이터와 어떻게 다른지 봅니다.

## 기록

- `docs/QA.md`와 `docs/QA-KO.md`(쌍둥이): ROM SHA-256, 콘솔과 카드, 리허설 값, 프리셋별 모습, QA 로그 헤더
  (`dldi`, `nitro`, `save_detected`, `cart_ready`), 에뮬레이터와 처음 달라지는 체크포인트, 칩이 감지되지 않았다면
  "chip save not verified on R4".
- `expected.json`: 실기 값이 ROM 기대값과 같은 `unconfirmed` 항목은 `source`를 `hw:ds-lite`로, `model`을
  `"DS Lite"`로 바꿉니다. 실기가 다르면 그 항목은 발견 사항입니다: 참고용으로 두고 값을 기록한 뒤 ROM과 기대값 중
  무엇이 틀렸는지 정합니다. `known`에 프로필 `hw-ds-lite`(실패 항목)를 추가합니다.
- 이음매 세로줄이 보이면: 지시안의 두 해결안(뷰포트를 한 열 겹치기, 블렌드 가중치 바꾸기)을 구현해 콘솔에서 비교하고
  줄이 가려지는 쪽을 기본값으로 정합니다.
- `build/qa/` 아래는 절대 커밋하지 않습니다.

## 실행 1: QA 실행과 판정 스위트, 2026-11-17

| 항목 | 값 |
|---|---|
| 날짜, 콘솔, 카드, R4 커널 | 2026-11-17, DS Lite(NTR), "R4 Plus" 듀얼 코어 카드, Wood R4 커널(`__rpg/`, `_DS_MENU.DAT`). 본체 시계가 설정되지 않아 파일 날짜는 2002-01-15 |
| ROM SHA-256 | `elevane-hw-support.nds` v0.2.1 `15c360c5bf71fd2eeb2a4f1935b93a09ac90cf558730c62fa0fe3f375e9aeea9`(465,260바이트, 릴리스와 같음). melonDS 1.1 soft도 같은 파일로 실행 |
| `ingest.py` / `judge.py --hardware` 요약 | QA 로그 PASS, 32줄, 체크포인트 6개, build `538d47ce7408`, `start=marker`로 시작. 스위트: QA 블록, 이어서 리허설과 `l3-*` 데모 9개, 10개 중 10개 `saved=1`, timeout 없음. `suite end demos=10 saved=10`, probe 12개 모두 `ok=1`, `suite done` 줄 없음(참고 참조). 판정: **판정 항목 20개 중 20개 OK**(게이트 L1 5/5, L3 3/3. 참고용 L2 12개도 OK), n/a 없음 |
| QA 로그 헤더(`sd`, `fat_fail`, `dldi`, `save_detected`, `cart_ready`, `nitro`) | `sd=1 fat_fail=0 fat_pre=1 errno=0 drive=fat:/ dldi="R4(DS) - Revolution for DS (v2)" cart_ready=0 nitro=1`; `save_build=EEPROM save_detected=1 save_available=1` |
| melonDS와 처음 갈리는 체크포인트 | 없음. ch1-ch5와 credits 해시가 melonDS 1.1 soft와 같음(`compare.py`, 차이 0) |
| 랙 프레임 | 22 / 22 / 22 / 22 / 22 / 23 (ch1-ch5, credits). melonDS soft 20 / 20 / 20 / 20 / 20 / 21 |
| 체크포인트별 3D 플래그(`gx=` `rdl=`) | 모두 `uf0,of0`. `rdl`은 46 / 43 / 37 / 46 / 42 / 46(melonDS는 모두 46). ch2, ch3, ch5에서 렌더 버퍼 여유가 줄었고 언더플로는 없음 |

| 항목 | 실기 | melonDS 1.1 soft | 차이? |
|---|---|---|---|
| `hdma-holes-zero`, `acc-split-hdma-holes` (L3, melonDS#2618) | OK(구멍 없음) | OK | 없음 |
| `l3-acc-dual` values[4] (한 패스: 폴리곤 수, 오버플로 플래그. 판정 안 함) | 2310, 1 | 1536, 1 | 있음. 콘솔의 RAM_COUNT는 2048에서 멈추지 않음(Lightenette와 같음). 캡처 해시 values[7..9]도 다름 |
| 리허설 values[14] (그림 해시, 판정 안 함) | 다름 | | 있음, 판정 안 함 |
| 나머지 판정 항목 | melonDS soft와 같음 | | 없음 |

참고: 스위트는 이제 마지막 리셋 전에 `suite end`를 쓰고(#186) 그 줄은 카드에 있습니다. 리셋 뒤의 probe 12줄도 `wait60`까지 모두 있습니다. 실행의 마지막 쓰기인 `suite done` 줄만 빠졌습니다(melonDS는 씀). 따라서 줄을 잃게 만드는 것은 리셋이 아니라, 카드에 마지막으로 쓴 줄이 사라지는 것입니다. 아직 열려 있습니다.
