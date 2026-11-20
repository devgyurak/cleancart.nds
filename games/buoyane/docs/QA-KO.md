# Buoyane QA

[English](QA.md)

melonDS 1.1용 점검표입니다(macOS 키는 저장소 `AGENTS.md`). "헤드리스"는 별말이 없으면 melonDS 1.1 코어 실행기 `tools/melonds/mds.sh`(소프트웨어 렌더러)입니다. 아직 melonDS 앱에서 손으로 확인한 것은 없습니다. 실기: DS Lite + R4에서 실행 1(2026-11-20), [HW-QA-KO.md](HW-QA-KO.md) 참고.

| 장면 | 확인 | 상태 |
|---|---|---|
| 타이틀 | 3D 엔진이 그린 밤 항구(안개로 사라지는 바다 띠, edge marking이 있는 곶 두 개, 반투명 빛줄기가 도는 램프)와 그 앞의 "BUOYANE" 글자, slackwater 곡. Start는 해도, X는 크레딧 | 헤드리스 스크린샷(프레임 190) |
| 해도 | 다섯 장과 `n/6` 완료 수, 위/아래 장, 좌/우 접근, A로 출항, B로 타이틀 | 헤드리스 스크린샷 |
| 항해 | 위: 그 장의 보기에서 참 불빛의 모습, 아래: 후보 불빛 세 개. 좌/우로 고르고 A로 답하며(맞으면 차임, 틀리면 클릭음), Select는 후보마다 연감의 모습을, B·Start는 뒤로. 관문 세 개로 접근 하나가 끝납니다 | 헤드리스: QA 파일럿이 30개 전부 진행 |
| 크레딧·라이선스 뷰어 | 스크롤 크레딧(libnds, BlocksDS crts MPL-2.0과 소스 URL, picolibc, libgcc), X로 라이선스 뷰어 | 아직 확인 안 함 |
| 리허설 데모 | `buoyane-rehearsal.nds`: 장마다 첫 접근(스크립트), 이어서 확인 장면 5개. 블록 id `0x48525942`(`BYRH`), 프레임 약 500에 게시, 1500프레임 실행. 판정: `tools/melonds/mds.sh run games/buoyane/buoyane-rehearsal.nds 1500 out/x "" 1499 \| python3 games/buoyane/tools/judge.py --profile melonds-1.1-software`(`RENDERER=gl`, `compute`. DeSmuME는 `RESULT_WORDS=64 tools/emu/emu.sh run …`에 `--profile desmume-pydesmume-0.0.9`) | ✅ 프로필 4개 모두 MATCH, 2026-11-17, 실기 1 뒤 변경 후 다시 2026-11-20 |
| 확인 ROM | `buoyane-check-dot|fog|edge|depth|blend.nds`: 장면 하나씩(약 150프레임), id `ckdo`, `ckfo`, `cked`, `ckde`, `ckbl`. 모든 프로필 MATCH | ✅ 2026-11-17 |

## 자동 QA 실행

릴리스 ROM(`buoyane.nds`)의 숨은 모드: 타이틀에서 L과 R을 누른 뒤 X와 Y를 눌러 네 키를 120프레임 누르고 있거나, SD 카드에 빈 파일
`cleancart/buoyane/autoqa`를 둡니다(폴더가 있어야 합니다). 게임이 정상 흐름을 스스로 진행합니다: 타이틀, 해도, 접근 30개(스크립트 플레이어
`by_pilot_keys()`가 관문마다 연감이 가리키는 후보를 고름), 해도, 타이틀, 크레딧, 타이틀. 그다음 확인 장면을 한 번 돌리고 판정(`RESULT PASS`)을 쓴 뒤,
판정 스위트가 확인 장면 5개를 돌려 각각 `res-*-check-<scene>.log`로 씁니다. 조정하는 값은 없고(`qa_tune.h`), 끝난 장마다 체크포인트 하나, 판정 하나, 크레딧 하나가 있습니다.

헤드리스: `SDDIR=out/by/x tools/melonds/mds.sh run games/buoyane/buoyane.nds 12000 out/by/x "300:L:140,300:R:140,300:X:140,300:Y:140"`
(먼저 `out/by/x/cleancart/buoyane/`를 만듭니다). 2026-11-17 melonDS 1.1(소프트웨어)에서 확인: 프레임 약 3140에 `RESULT PASS`, 접근 30개 중 30개, 장 마스크 `0x1f`,
체크포인트 7개, 두 번 돌린 `tools/qa/compare.py` 차이 0, `tools/qa/lint.py` 통과, `QARN` 블록을 `judge.py --profile`로 판정해 melonDS 1.1 소프트웨어와 DeSmuME에서 MATCH,
`tools/qa/suite_compare.py`: 스위트 블록 5개가 단독 ROM의 블록과 같음(SAME). 저장: 접근을 끝낼 때마다 게임이 저장하고(실행이 쓰기마다 로그에 거울로 남깁니다), 진행이 든 세이브로 평소처럼 부팅하면 해도에 보이고, QA 실행이 끝난 뒤에는 실행 전의 진행이 해도에 보입니다(실행 중의 30/30이 아님).
실기 실행 1(DS Lite + R4, 2026-11-20): QA 실행 PASS, 체크포인트 해시가 melonDS 소프트웨어와 같고, 판정 NG 4개가 있었으며 그 뒤 기대값을 바꿨습니다([HW-QA-KO.md](HW-QA-KO.md)의 "실행에서 나온 기대값"). 모든 렌더러의 리허설 블록을 새 `expected.json`으로 다시 채점했고 네 프로필 모두 MATCH입니다(2026-11-20, `QARN` 블록과 check ROM은 다시 돌리지 않음).
아직: GL Classic·Compute QA 실행, melonDS 앱.

## 재현성

클린 `./build.sh` 두 번이 같은 SHA-256을 냅니다(2026-11-17, 뼈대 단계 기준. 릴리스 전에 다시 확인).
