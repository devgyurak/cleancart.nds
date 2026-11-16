# Ledgerine QA

[한국어](QA-KO.md)

Checklist for melonDS 1.1 (macOS keys: see the repository `AGENTS.md`). The scaffold only.

| Scene | Check | Status |
|---|---|---|
| Title | the tower picture, "LEDGERINE", the tagline and the X/Select hint on the top screen; licence, ARM7 and save lines on the bottom screen. Start opens room 1, X the credits, Select the test menu | headless run only; not yet by hand in melonDS |
| Test menu | six rooms, the rehearsal, the credits; B back | headless run only |
| Room (stub) | room name, tower, hardware; A closes the books and goes on, after room 6 to the credits | headless run only |
| Credits | scrolls; X opens the licence viewer (needs NitroFS), B back | not yet checked |

## Demo ROMs

| Demo | What it reports | Status |
|---|---|---|
| `rehearsal` | all six audits in order, then one `LDRH` block (`game/ledger.h`); stub: only the completion bits | headless run only |
