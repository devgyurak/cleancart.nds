# Elevane 실기 QA (DS Lite + R4)

[English](HW-QA.md)

Elevane은 아직 실기에서 측정한 값이 하나도 없습니다. 이 문서는 실행 점검표이며, 첫 값을 기록하고
`expected.json`의 `unconfirmed` 항목을 승격하기 위한 것입니다. 콘솔은 사람이 조작하고, 아래는 할 일과
적어 둘 것입니다.

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
6. 리허설 사진 채점: `python3 elevane/tools/judge.py --values <hex> <hex> ...`(값 0~17을 순서대로,
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
