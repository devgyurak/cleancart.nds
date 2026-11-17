# Heddlemere 실기 QA (DS Lite + R4)

[English](HW-QA.md)

**Wood 계열 R4 커널:** 카드에는 `heddlemere.nds` 대신 `build/release/rom/heddlemere-hw-support.nds`를 넣습니다. 홈브루 게임 코드 `####`만 다른 같은 ROM입니다. 이런 커널은 이 코드의 ROM에만 DLDI를 붙이고, 게임 고유 코드로는 ROM이 SD 카드를 보지 못합니다(QA 로그와 결과 파일이 남지 않음). 데모와 리허설 ROM은 고유 코드 그대로입니다.

**판정 스위트(QA 판정 스위트 PR 이후):** QA 실행(`autoqa`)이 이제 스위트로 끝납니다. `RESULT PASS` 뒤에 QA 블록(리허설 항목을 담음)를 `res-*-heddlemere-*.log`로 쓰고 진행 파일 `suite-<stamp>.log`도 남깁니다. hw-support ROM 한 번 실행으로 충분하고, **전원은 스위트가 끝난 뒤에** 끕니다(`suite-<stamp>.log`에 `suite end`). 아래의 리허설·데모 ROM은 항목 하나를 다시 돌릴 때만 씁니다(Wood 커널에서는 SD 카드를 받지 못함). `tools/qa/ingest.py`가 파일마다 채점합니다.

Heddlemere는 아직 실기에서 잰 값이 없습니다. 이 문서는 실기 점검표입니다. 첫 값을 기록하고, `expected.json`의 `unconfirmed`
네 항목을 승격하고, 가장 위험한 gating 수치(ARM9 HBlank 낮은 구간, GBATEK 1606)를 확정하기 위한 것입니다. 콘솔은 사람이
조작하고, 아래 단계는 할 일과 적을 것입니다. 대상은 DS Lite 한 대이고 DSi·3DS 값은 없습니다.

**자동 경로(스킬 `nds-hw-qa`):** 카드에 빈 폴더 `cleancart/heddlemere/`를 만들면 리허설이 결과 블록을 거기에
(`res-*-heddlemere-rehearsal.log`) 씁니다. 빈 파일 `cleancart/heddlemere/autoqa`는 버튼을 누르지 않고 QA 실행을 시작합니다.
`python3 tools/qa/ingest.py /Volumes/<card> --game heddlemere`가 `judge.py --hardware --partial`로 모두 채점합니다.
실기에서 NG인 항목은 프로필이 아니라 발견 사항입니다. 카드의 옛 `res-*` 로그는 먼저 지웁니다.

## 실기가 답해야 할 것

| 질문 | 어디에 나오나 | GBATEK | melonDS 1.1 |
|---|---|---|---|
| ARM9에서 HBlank 플래그는 얼마나 낮은가? (실측 전까지 정보용. 1606±2가 반복되면 gating으로 올림) | values[3] (x16) | 1606사이클 (25696) | 1584 |
| ... ARM7에서는? (GBATEK 1613 "for whatever reason". 우리 ARM7 코어에서 폴링하므로 폴링 루프가 섞였을 수 있음) | values[4] | 1613 | 1584 |
| 라인·프레임 길이 | values[1], [2] | 2130 (34080) / 560190 | 같음 |
| 212라인의 VCOUNT 쓰기가 10 또는 11라인을 반복하는가? (남는 사이클 values[8]은 기록만 하고 판정하지 않음: probes/ 몫) | values[7], [8] | 10 또는 11 / 0-4 | 11 / 1 |
| MTX_IDENTITY, PUSH+POP, 모드 2의 TRANS(+30), 모드 2의 SCALE(+0) | values[9], [10], [12], [13] | 19 / 53 / 30 / 0 | 같음 |
| MTX_TRANS와 BOX_TEST (GBATEK과 melonDS 값이 다른 두 개) | values[11], [14] | 22 / 103 | 35 / 257 |
| FIFO 가득 참 쓰기당 정지, 절반 비움 IRQ의 항목당 시간 | values[15], [16] | 19 / 19 | 18.9 / 19.0 |
| 빈 SWAP_BUFFERS 뒤 VBlank부터 엔진이 쉴 때까지 | values[17] | 392 | 328 |
| 191·192라인 HBlank의 폴리곤 RAM 개수 | values[18], [19] | 64 / 0 | 64 / 0 |
| 192~213라인에 쓴 텍스처 팔레트가 렌더에 보이는가? | values[20] (틀린 라인 0) | 0 | 0 |

## 단계

1. 빌드: `cd games/heddlemere && ./build.sh -j8 release`. `build/release/rom/heddlemere.nds`와
   `build/release/judge/heddlemere-rehearsal.nds`의 `shasum -a 256`을 적어 둡니다.
2. 두 ROM을 R4 카드에 복사하고 `cleancart/heddlemere/`를 만듭니다.
3. **리허설을 세 번**(결과가 반복되어야 함): `heddlemere-rehearsal.nds`를 켜고 아무것도 건드리지 않은 채 약 60초
   기다립니다. 측정(한동안 화면은 마지막 주문 그대로)하고, 장마다 첫 주문을 플레이한 뒤 `REHEARSAL RESULT`와
   값을 16진수로 보여 줍니다. 카드에 폴더가 있으면 블록이 SD에도 써집니다.
4. **게임**: `heddlemere.nds`를 켭니다. 1-01을 플레이합니다(목록에서 A, A, 칸을 맞추고 A). Select로 도면 보기, B로 돌아옵니다.
   납품 뒤 모터 램프를 보세요. GBATEK 비용이 맞다면 실기에서는 OK가 나와야 합니다.
5. **자동 QA 실행**: 카드에 빈 `cleancart/heddlemere/autoqa`를 둡니다(또는 타이틀에서 L+R+X+Y를 2초 누릅니다). 게임이 크레딧까지
   스스로 플레이하고 `cleancart/heddlemere/qa-*.log`를 씁니다. 그다음
   `python3 tools/qa/ingest.py /Volumes/<card> --game heddlemere`로 가져오고 에뮬레이터 실행과 비교합니다
   (`python3 tools/qa/compare.py <emu.log> <hw.log> --info`).

## 기록

- `docs/QA.md`·`docs/QA-KO.md`(쌍)와 이 페이지의 표: ROM SHA-256, 콘솔과 카드, 리허설 블록 세 개.
- `expected.json`: 실기 값이 반복되는 `unconfirmed` 항목은 `source`를 `hw:docs/HW-QA.md`로 바꾸고 범위를 잰 값으로 고칩니다.
  gating 항목의 실기 값이 GBATEK 수치와 다르면 기대값을 실측값으로 바꾸고 근거를 `hw:`로 달고 PR에 이유를 적습니다. 세 번의
  실행 사이에 반복되지 않는 값은 여기가 아니라 `probes/`(L4)로 보냅니다. `known`에 `hw-ds-lite` 프로필을 추가합니다.
- `build/qa/` 아래의 것은 절대 커밋하지 않습니다.
