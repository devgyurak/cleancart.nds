# Tonewick 실기 QA (DS Lite + R4)

[English](HW-QA.md)

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
| GBATEK이 적은 캡처 버그 둘이 실제로 있는가? add 오버플로: 7000h + 7000h가 -2000h로 캡처됨. 둘 다 음수: -8000h. 둘 다 다른 사람이 확인한 적이 없습니다. 콘솔이 다르면 두 항목은 `probes/`로 옮깁니다 | `cap-add-wrap`(`values[21]`), `cap-both-negative`(`values[22]`) |
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
3. **리허설**: `tonewick-rehearsal.nds`를 시작하고 아무것도 만지지 말고 지역 네 곳이 DONE이 될 때까지 10초쯤 기다립니다.
   이어폰을 쓰거나 볼륨을 낮추세요. 실험하는 동안 스피커 앰프는 꺼지지만 콘솔에 소리를 내라고 시키는 실험입니다.
4. **자동 QA 실행**: 카드에 빈 `cleancart/tonewick/autoqa`를 둡니다(또는 타이틀에서 L+R+X+Y를 2초 누릅니다). 게임이 스스로
   크레딧까지(약 30초) 진행하고 `cleancart/tonewick/qa-*.log`를 씁니다.
   그 뒤 `python3 tools/qa/ingest.py /Volumes/<카드> --game tonewick`를 실행하고 에뮬레이터 실행과 견줍니다
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).
5. 선택, 손으로: 테스트 메뉴(타이틀에서 Select)와 아침 점검 일지를 열고 보이는 것을 적어 둡니다.

## 기록

- `docs/QA.md`와 `docs/QA-KO.md`(쌍둥이): ROM SHA-256, 콘솔과 카드, 실패한 항목, QA 로그 머리글.
- `expected.json`: 실기 값이 ROM의 기대와 맞는 항목은 `source`를 `hw:ds-lite`로 바꾸고 `level_basis`에 `hw:`를 더합니다.
  실기가 다르면 그 항목은 발견입니다. 값을 기록하고 ROM과 기대 중 어느 쪽이 틀렸는지 정합니다. `known`에 프로필 `hw-ds-lite`
  (실패한 항목)를 더합니다.
- `build/qa/` 아래는 절대 커밋하지 않습니다.
