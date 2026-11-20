// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The ledger: what the six rooms measured, and the result block it is published in.
//
// Result block (CvResult at CV_RESULT_ADDR, id LDRH, magic CV_RESULT_MAGIC once filled,
// 64 words in all). Every judged bit is a hardware result compared with a GBATEK constant
// or the ROM's GBATEK rule model (rooms.h); the magic word is written last.
//   values[0]   completion bits: bit n = room n was audited (n = 0..5), bit 8 = rehearsal done
//   values[1]   press (DS Maths) flags      values[2]  tube (IPC) flags
//   values[3]   clock wall (timers) flags   values[4]  belt (DMA) flags
//   values[5]   vault (memory) flags        values[6]  rules (BIOS) flags
//   values[7]   prerequisite (L1) flags
//   values[8..45] raw diagnostic values the rooms recorded (named in each room's file)
//   values[48..54] QA words, written by the QA run only (48 = verdict 1 PASS / 2 FAIL)
// Bit meanings are in tools/judge.py's expected.json items (expected.json).

#ifndef LG_LEDGER_H
#define LG_LEDGER_H

#include <nds.h>

#define LG_ROOMS    6
#define LDRH_ID     0x4852444Cu   // "LDRH"

enum {
    LV_DONE = 0, LV_PRESS, LV_TUBE, LV_CLOCK, LV_BELT, LV_VAULT, LV_RULES, LV_PRE,
    LV_RAW = 8,          // values[8..45]
    LV_QA = 48,          // values[48..54]
};
#define LV_DONE_REHEARSAL (1u << 8)

extern u32 lg_res[61];

void ledger_clear(void);
// Sets/clears bits in a result word.
void ledger_flags(int word, u32 mask, bool ok);
// Publishes lg_res as the result block (magic last).
void ledger_publish(u32 demo_id, u32 frame);

#endif // LG_LEDGER_H
