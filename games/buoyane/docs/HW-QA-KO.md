# Buoyane 실기 QA

[English](HW-QA.md)

아직 돌리지 않았습니다. Buoyane은 실제 본체에서 돌아간 적이 없어서, `expected.json`의 모든 판정은 GBATEK과 에뮬레이터에서 나왔습니다.
이 실행의 목적은 `unconfirmed` 네 항목(`aa-opaque-dot-hidden`, `fog-alpha-glitch`, `aa-marked-line-complete`, `wire-texture-alpha`)과
`edge-aa-translucent`, `attr-latch-begin`입니다. DS Lite에서 한 번 돌리면 각 항목이 gating `hw:` 항목이 될지 `probes/`(레벨 4)로 갈지 정해집니다.

순서(스킬 `nds-hw-qa`, DS Lite + R4 Plus): `buoyane-hw-support.nds`(게임 코드가 `####`인 릴리스 ROM. `tools/hwsupport.py`가 만들어 SD 카드를 받게 합니다)를 카드에 두고,
`cleancart/buoyane/` 폴더와 그 안의 빈 파일 `autoqa`를 만든 뒤 켜서 타이틀을 건드리지 않습니다. QA 실행이 접근 30개를 진행하고 `qa-*.log`를 쓰며,
판정 스위트가 장면 5개의 `res-*-check-<scene>.log`를 씁니다. 거두어 `tools/qa/ingest.py`, `python3 games/buoyane/tools/judge.py --hardware`를 실행하고 이 표와 영어 쌍에 기록합니다.

| 항목 | 에뮬레이터 결과(소프트웨어 / GL Classic / GL Compute / DeSmuME) | 실기 |
|---|---|---|
| `aa-opaque-dot-hidden` | 모두 NG | 미확인 |
| `fog-alpha-glitch` | 모두 NG | 미확인 |
| `wire-texture-alpha` | melonDS는 NG(와이어프레임이 불투명, `a = 31`), 나머지도 NG | 미확인 |
| `aa-marked-line-complete` | 정보용 | 미확인 |
| `edge-aa-translucent` | 소프트웨어 OK, GL Compute·GL Classic·DeSmuME NG | 미확인 |
| `attr-latch-begin` | 네 가지 모두 OK | 미확인 |
