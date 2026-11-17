# Tonewick QA

[English](QA.md)

melonDS 1.1용 점검표입니다 (macOS 키는 저장소 루트 `AGENTS.md` 참고).

| 장면 | 확인할 것 | 상태 |
|---|---|---|
| 타이틀 | 위 화면에 황혼의 황야, 신호탑, 스코프 창이 있는 감청소, "TONEWICK", 한 줄 소개, X/Select 안내. 아래 화면에 라이선스·ARM7·저장 칩 줄. Start는 1번 지역, X는 크레딧, Select는 테스트 메뉴 | 헤드리스 실행(스크린샷 확인). melonDS 손 조작은 아직 |
| 테스트 메뉴 | 지역 4개, 아침 점검 일지, 리허설, 크레딧. B는 뒤로 | 헤드리스 실행만 확인 |
| 지역 | 위: 이름, 탑, GBATEK 절, 신호선 세 줄(`[X]`는 푼 줄), 모델 파형(파랑)과 듣고 난 뒤 귀가 캡처한 파형(노랑)이 뜨는 스코프, 램프(`TRUE EAR`, `THE EAR DISAGREES`, `NO SIGNAL`). 아래: 고른 줄, 필요한 값, 놋쇠 다이얼(다이얼 A는 좌/우, 다이얼 B는 L/R 또는 Y/X)과 레버(A). 터치: 다이얼 왼쪽 절반은 뒤로, 오른쪽 절반은 앞으로, 레버는 듣기. 세 줄을 모두 풀면 Start로 근무 종료, B는 뒤로 | QA 조종기와 키 입력 실행으로 헤드리스 확인(스크린샷 확인). 터치 손 조작은 아직 |
| 아침 점검 일지 | 시계·전원·테이프(세이브 칩)·펌웨어 읽기값과 각 규칙의 `AGREES` 또는 `DIFFERS`. B는 뒤로 | 헤드리스 실행(스크린샷 확인) |
| 크레딧 | 스크롤. X는 라이선스 뷰어(NitroFS 필요), B는 뒤로 | 헤드리스 실행(QA 실행이 지나감) |

레버를 당기면 몇 프레임 동안 멈춥니다(ARM7가 사운드 스크립트를 돌리고 귀가 듣습니다. 실험 하나에 5~8프레임이고, 아침
점검 일지 줄은 장치를 읽습니다). 그동안 레버는 눌린 모습으로 그려집니다. 스피커 앰프는 실험하는 동안 꺼지므로 게임에서는
소리가 나지 않으며, 첫 실험이 돌면 타이틀 음악이 멈춥니다.

## 데모 ROM

| 데모 | 보고하는 것 | 상태 |
|---|---|---|
| `rehearsal` | `tonewick-rehearsal.nds`는 곧장 리허설로 부팅합니다(세이브 칩·NitroFS 없음). 스크립트로 신호선 12개를 풀고 모든 지역을 감사합니다. 결과 블록 id `0x48525754`(`TWRH`), 배치는 `game/lab.h`. `python3 tools/judge.py`(부트 `rehearsal`)로 채점 | melonDS 1.1 soft / GL classic / GL compute와 DeSmuME: 프로필 MATCH, 아래 참고 |

실기: 아직 돌리지 않았습니다. [HW-QA-KO.md](HW-QA-KO.md) 참고.

## 판정 프로필 (2026-11-16, 이 커밋의 빌드)

리허설과 릴리스 ROM의 QA 실행(부트 `qa`)은 같은 판정을 냅니다. **프로필은 12시 이후에 돌리세요.** `rtc-ampm-24h`는 AM/PM 플래그를 읽는데, 정오 전에는 어느 쪽이든 0이라 n/a가 되고 프로필 비교가 `NEWLY N/A`를 보고합니다. melonDS: `RTC=2026-11-16-15-30-00`, DeSmuME(호스트 시계): `RTC_HOUR=15 tools/emu/emu.sh run ...`.

| 에뮬레이터 | NG 항목 | 비고 |
|---|---|---|
| melonDS 1.1 software / GL classic / GL compute (헤드리스, 내장 대체 BIOS, `RTC=2026-11-16-15-30-00`) | `snd-hold-level`(hold 비트 미구현: one-shot이 끝나면 출력이 0으로 돌아감), `cap-src-channel`(비트 1을 무시: 채널 0의 2000h가 아니라 믹서 합 4000h가 캡처됨), `cap-add-wrap`(add 비트를 켠 캡처가 아무것도 쓰지 않음), `cap-both-negative`(-8000h가 아니라 두 채널을 더한 -4000h), `pm-reg-mirror`(생성한 펌웨어가 DS Lite라고 하는데 레지스터 4는 40h, 레지스터 5는 0) | 프로필 `melonds-1.1-software`, `-gl-classic`, `-gl-compute` MATCH. 렌더러 셋이 같습니다. SD 이미지로 돌린 QA 실행에서는 세이브 항목 셋이 n/a입니다(홈브류 실행에는 세이브 칩이 없음). 에뮬레이트한 8 KB EEPROM(`SAV=`, 프로필 `melonds-1.1-software-save-chip`)에서는 셋 모두 OK |
| DeSmuME (py-desmume 0.0.9, HLE BIOS, EEPROM, `RTC_HOUR=15`로 현지 시각 15시) | `snd-short-busy`(3워드 one-shot이 끝나면서 busy가 내려감), `snd-short-silent`(3워드 테이프가 소리를 냄), `snd-hold-level`, `snd-noise-lfsr`(일치 비트 152), `snd-adpcm-loop-reload`(루프가 재적재되지 않음), `cap-add-wrap`(캡처 없음), `cap-both-negative`(-2000h), `pm-reg-mirror`, `save-wel-reset`(쓰기 뒤에도 WEL이 켜진 채) | 프로필 `desmume-pydesmume-0.0.9` MATCH. `snd-duty7-low`는 통과합니다 |

NG 항목은 모두 실기와 견줘 볼 결과입니다(GBATEK이 적은 내용이라 항목은 gating으로 둡니다). 특히 캡처 버그 둘은 GBATEK에만
적혀 있습니다. DS Lite가 다른 값을 내면 어떻게 하는지는 [HW-QA-KO.md](HW-QA-KO.md)의 "실기 결과 판정" 절에 적었습니다.

## 자동 QA 실행

QA 로그의 `cp` 줄 끝에는 `gx=uf<n>,of<n> rdl=<min>`이 붙는다. 직전 체크포인트 이후 구간에서 DISP3DCNT bit 12(컬러 버퍼 RDLINES 언더플로)와 bit 13(폴리곤/버텍스 RAM 오버플로)이 켜져 있던 프레임 수, 그리고 RDLINES_COUNT의 최솟값(−2, 0..46)이다. 끈적한 플래그는 읽기만 하고 지우지 않으므로 값은 "플래그가 켜져 있던 프레임 수"다. `lag`처럼 참고용이라 해시에 넣지 않고 비교하지도 않으며(`tools/qa/compare.py --info`가 나란히 보여 준다), 3D를 쓰지 않는 장면은 `0,0`이다. 에뮬레이터는 이 플래그를 거의 세우지 않고 실기는 세울 수 있다.

릴리스 ROM(`tonewick.nds`)의 숨은 모드입니다. 타이틀에서 L과 R을 누른 뒤 X와 Y를 눌러 네 키를 120프레임 동안 누릅니다.
게임이 일반 흐름을 스스로 진행합니다: 타이틀, 지역 1~4(조종기가 GBATEK 모델로 줄마다 다이얼 값을 찾아 키로 돌리고 듣습니다.
Start로 근무를 끝냅니다), 크레딧, 다시 타이틀. 그러고 `RESULT PASS`로 끝납니다. SD 카드의 로그(`cleancart/tonewick/qa-<날짜>-<시각>.log`)와
리허설 배치를 쓰는 결과 블록 `QARN`은 러너 출력(`RESULT` 줄)으로 `python3 games/tonewick/tools/judge.py --boot qa --profile <프로필>`(추가 항목
`qa-verdict`)에 넣어 채점합니다. SD 로그에는 결과 블록이 없습니다. 헤드리스: `RTC=2026-11-16-15-30-00 SDDIR=out/tw/x tools/melonds/mds.sh run games/tonewick/tonewick.nds 3000 out/tw/x "300:L:140,300:R:140,300:X:140,300:Y:140" | python3 games/tonewick/tools/judge.py --boot qa --profile melonds-1.1-software`
(먼저 `out/tw/x/cleancart/tonewick/`를 만드세요. GL 렌더러는 `RENDERER=gl|compute`, SD 로그는 `parse_log.py`로 확인합니다).

| 렌더러 | QA 실행 (2026-11-16) | `judge.py --profile` |
|---|---|---|
| melonDS 1.1 software | 프레임 1582에서 `RESULT PASS`, 체크포인트 4개, `parse_log.py` ok | `melonds-1.1-software` MATCH |
| melonDS 1.1 GL classic | PASS | `melonds-1.1-gl-classic` MATCH |
| melonDS 1.1 GL compute | PASS | `melonds-1.1-gl-compute` MATCH |
| DeSmuME (py-desmume 0.0.9) | verdict 1 | `desmume-pydesmume-0.0.9` MATCH |

소프트웨어 실행 두 번은 체크포인트 해시가 같습니다(`tools/qa/compare.py`). `tools/qa/lint.py`도 통과합니다. 상태 해시는
완료 비트와 푼 줄 수만 덮습니다. 측정값은 콘솔에서 실행마다 조금씩 움직이기 때문입니다. 소프트웨어 실행에는 세이브 칩이 없습니다
(SD 카드용 홈브류 실행). 칩 실행(`SAV=`)은 `-save-chip` 프로필로 채점하고 세이브를 다시 읽습니다(`SAVE 2 8192`, `CPU9 cpsr`
모드 1F, 데이터 어보트 없음). 실제 DS Lite + R4: 아직 돌리지 않았습니다.

## 키 없이 QA 시작하기 (SD 표식)

SD 카드에 빈 파일 `cleancart/tonewick/autoqa`를 둡니다(폴더가 먼저 있어야 합니다). 부팅 뒤 첫 타이틀 화면이 키 조합과 같은
QA 실행을 시작하고, 시작하기 전에 파일 이름을 `autoqa.used-<날짜>-<시각>`으로 바꿔 한 번만 돌게 합니다. 이름 바꾸기에
실패하면 아무것도 시작하지 않고 `autoqa-error.log`에 한 줄을 남깁니다. 카드로는 아직 해보지 않았습니다.
