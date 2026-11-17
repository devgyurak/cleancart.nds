# Tonewick 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `tonewick.nds` 대신 `build/release/rom/tonewick-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

Tonewick은 아직 콘솔에서 측정하지 않았습니다. 이 문서는 그 실행의 점검표입니다. 첫 값을 기록하고, 에뮬레이터나 GBATEK 읽기만
뒷받침하는 항목을 확정하기 위한 것입니다. 콘솔은 사람이 조작하며, 아래 단계는 할 일과 적어 둘 것입니다.

**자동 경로(스킬 `nds-hw-qa`):** 카드에 빈 폴더 `cleancart/tonewick/`를 만들면 리허설이 결과 블록을 그곳에 씁니다
(`res-*-tonewick-rehearsal.log`). 빈 파일 `cleancart/tonewick/autoqa`는 버튼을 누르지 않고도 QA 실행을 시작합니다.
`python3 tools/qa/ingest.py /Volumes/<카드> --game tonewick`가 `judge.py --hardware --partial`로 모두 채점합니다. 실기의 NG 항목은
프로필이 아니라 발견입니다. 카드의 예전 `res-*` 로그는 먼저 지웁니다. 세이브 항목 셋(`env: cart`)은 R4에서 n/a입니다
(로더 뒤에 세이브 칩이 없음).

## 실기가 답해야 하는 것

| 질문 | 항목 / 보이는 곳 |
|---|---|
| GBATEK이 적은 캡처 버그 둘이 실제로 있는가? add 오버플로: 7000h + 7000h가 -2000h로 캡처됨. 둘 다 음수: -8000h. 둘 다 다른 사람이 확인한 적이 없습니다. 다를 때 어떻게 하는지는 아래 "실기 결과 판정" 절에 있습니다 | `cap-add-wrap`(`values[21]`), `cap-both-negative`(`values[22]`) |
| 채널 0을 캡처 소스(비트 1)로 하면 채널 0 혼자의 값이 나오는가? | `cap-src-channel`, `values[20]` |
| 캡처 길이 0은 1워드인가? | `cap-len-zero`, `values[18]` |
| duty 7 PSG 파형은 LOW에 머무는가? (DeSmuME는 예전에 HIGH라고 함) | `snd-duty7-low`, `values[2]` |
| hold 비트가 one-shot의 마지막 샘플을 유지하는가? (melonDS와 DeSmuME는 아님) | `snd-hold-level`, `values[11]` (원값 `values[13]`, `values[14]`) |
| 3워드 테이프는 busy를 유지하면서 조용한가? | `snd-short-busy`, `snd-short-silent`, `values[9]`, `values[10]` |
| 노이즈 채널이 X = 7FFFh, XOR 6000h를 정확히 따르는가? 볼륨 레지스터 127은 64의 정확히 두 배이고 분주 3은 정확히 1/16인가? | `snd-noise-lfsr`, `snd-vol127-full`, `snd-voldiv16` |
| ADPCM 루프가 index와 값을 재적재하는가? | `snd-adpcm-loop-reload`, `values[15]` |
| 어떤 기종인가(펌웨어 바이트 1Dh), 레지스터 4, 5, 0Ch가 문서대로 미러되는가? | `pm-reg-mirror`, `values[27..31]` |
| 24시간 모드에서 12~23시에 AM/PM 플래그가 서 있는가? | `rtc-ampm-24h`, `values[25]`, `values[26]` |
| 가장 최신 펌웨어 사용자 설정 사본이 로더가 027FFC80h에 남긴 사본인가? (R4 커널은 오래된 사본이나 아무것도 안 남길 수 있음: 로더를 적어 두세요) | `fw-user-newest`, `values[35]`, `values[36]` |
| WREN 뒤와 쓰기 뒤의 WEL, RDSR 비트 4~6 (카트리지 세이브 칩이 있는 콘솔이 필요하며 R4에서는 안 됨) | `save-wel-*`, `save-status-fixed`, `values[32..34]` |

## 단계

1. 빌드: `cd games/tonewick && ./build.sh -j8 release`. `build/release/rom/tonewick.nds`와
   `build/release/judge/tonewick-rehearsal.nds`의 `shasum -a 256`을 적어 둡니다.
2. 두 ROM을 R4 카드에 복사하고 `cleancart/tonewick/`를 만듭니다.
3. **리허설**: 오전에 한 번, 12시 이후에 한 번 돌립니다. 정오 전에는 AM/PM 플래그가 어느 쪽이든 0이라 `rtc-ampm-24h`가 n/a입니다. `tonewick-rehearsal.nds`를 시작하고 아무것도 만지지 말고 지역 네 곳이 DONE이 될 때까지 10초쯤 기다립니다.
   이어폰을 쓰거나 볼륨을 낮추세요. 실험하는 동안 스피커 앰프는 꺼지지만 콘솔에 소리를 내라고 시키는 실험입니다.
4. **자동 QA 실행**: 카드에 빈 `cleancart/tonewick/autoqa`를 둡니다(또는 타이틀에서 L+R+X+Y를 2초 누릅니다). 게임이 스스로
   크레딧까지(약 30초) 진행하고 `cleancart/tonewick/qa-*.log`를 씁니다.
   그 뒤 `python3 tools/qa/ingest.py /Volumes/<카드> --game tonewick`를 실행하고 에뮬레이터 실행과 견줍니다
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
5. 선택, 손으로: 테스트 메뉴(타이틀에서 Select)와 아침 점검 일지를 열고 보이는 것을 적어 둡니다.

## 실기 결과 판정

모든 항목에 해당하며, 특히 GBATEK에만 적힌 캡처 버그 둘(`cap-add-wrap`, `cap-both-negative`)에 중요합니다. 판정하기 전에 결합을
배제하세요. 두 버그는 볼륨 레지스터 127이 128으로 세어진다는 것(채널 경로의 단위 이득)에 기대므로 `snd-vol127-full`,
`snd-voldiv16`, `cap-src-channel`을 먼저 보세요. 거기서 실패하면 나머지는 그 때문입니다. 리허설은 세 번 이상, QA 실행은 한 번,
가진 콘솔마다(DS Lite와 구형 DS, 펌웨어 바이트 1Dh는 `values[27]`) 돌립니다.

| 결과 | 뜻 | 바뀌는 것 (파일, 실행을 기록하는 PR에서) |
|---|---|---|
| 값이 매번 범위 안 | GBATEK과 ROM이 맞음 | `expected.json`: `source`는 `hw:ds-lite`(기종), `level_basis`에 `; hw:docs/HW-QA.md`(실행을 기록한 파일) 추가. 항목은 레벨 2 유지 |
| 범위를 벗어나지만 매번 같고 설명이 있음(다른 자료가 말하는 버그, 레지스터 미러, 로더) | 기대값이 틀렸고 항목은 문서화됨 | `expected.json`: 범위나 `expect`를 측정값으로 바꾸고 `source`는 `hw:ds-lite`, `level_basis`는 `hw:docs/HW-QA.md`, 설명은 `title`에. 항목은 레벨 2 유지. 줄의 모델도 따라감(`junction.c`의 10·11번 줄은 GBATEK 버그를 모델로 씁니다) |
| 값이 실행마다 또는 콘솔마다 다르거나 아무도 설명하지 못함 | 문서화된 동작이 아님: 레벨 4 연구 | **한 PR에서 모두 합니다**(`role: relocate` 항목은 `mklevels.py --check`가 실패시키므로 커밋하지 않습니다). 항목은 `expected.json`에서 빠지고, 그 실험을 담은 데모 ROM이 `probes/`에 들어가며(에뮬레이터별 `known` 프로필, `probes/AGENTS.md` 참고), 지역 감사는 숫자를 원값으로 남겨도 되고, 10·11번 줄은 그대로 동작하며(모델은 GBATEK 버그, 귀의 답은 옆에 표시), 프로필의 `ng`에서 항목을 빼고, `mklevels.py`가 LEVELS와 README 칸을 다시 만듭니다 |

실행을 기록하는 사람(콘솔이 있는 관리자)이 PR을 엽니다. 프로필 `hw-ds-lite`는 콘솔에서 실패하는 항목을 나열합니다.

## 기록

- `docs/QA.md`와 `docs/QA-KO.md`(쌍둥이): ROM SHA-256, 콘솔과 카드, 실패한 항목, QA 로그 머리글.
- `expected.json`: 실기 값이 ROM의 기대와 맞는 항목은 `source`를 `hw:ds-lite`로 바꾸고 `level_basis`에 `hw:`를 더합니다.
  실기가 다르면 그 항목은 발견입니다. 값을 기록하고 ROM과 기대 중 어느 쪽이 틀렸는지 정합니다. `known`에 프로필 `hw-ds-lite`
  (실패한 항목)를 더합니다.
- `build/qa/` 아래는 절대 커밋하지 않습니다.
