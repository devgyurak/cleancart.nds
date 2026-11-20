// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The tally: what the six piers measured, and the result block it is published in.
//
// Result block (CvResult at CV_RESULT_ADDR, id QYRH, magic CV_RESULT_MAGIC once filled, 64 words).
// Every judged value is a result code (common/qy_class.h: 1 = the behaviour GBATEK describes,
// 2/3 = known alternatives, 0xFF other, 0xFFFF = never measured). The magic word is written last.
//   values[0]    completion bits: bit n = pier n was measured (0 A loads, 1 B blocks, 2 C ALU,
//                3 D protection unit, 4 E tightly coupled memory, 5 F caches), bit 8 = rehearsal done
//   values[1..3]   A1-A3     ldrh7-odd ldrsh7-odd thumb7-ldrh-odd
//   values[4..10]  B1-B7     stm7-base-new ldm7-base-nowb ldm9-base-wb stm7-empty thumb7-stmia-base-new
//                            stm-pc12 ldm9-empty
//   values[11..14] C1-C4     alu7-rn-pc12 qdadd9-dbl-sat smla9-q-nosat cp15-prev5-bit15
//   values[15..20] D1-D6     cp15-ctrl-bits bkpt9-pabt pu-priority pu-background pu-user-ro pu-code-xn
//   values[21..26] E1-E6     itcm-over-dtcm dtcm-mirror itcm-size tcm-dma-bus dtcm-load-mode itcm-load-mode
//   values[27..30] F1-F4     dcache-dma-stale dcache-wb-dirty dcache-inval-drop icache-stale
//   values[31]   prerequisite bits (0 ldr/str word, 1 plain LDM/STM, 2 ARM7 step, 3 exception harness,
//                4 PU read/write, 5 DTCM read/write, 6 DMA copy, 7 cache enable)
//   values[32]   jobs cleared (the game), values[33] plan hash
//   values[48..54] QA words, written by the QA run only (48 = verdict 1 PASS / 2 FAIL)
// The raw words behind every code are in the raw page (cv_ipc.h CV_QY_RAW, 0x0237F800): the ARM7's
// per step at [step * 4], the ARM9's at [64 + item * 4]; the QA log prints them.

#ifndef QY_LEDGER_H
#define QY_LEDGER_H

#include <nds.h>

#define QYRH_ID 0x48525951u   // "QYRH"

enum {
    QV_DONE = 0,
    QV_A1 = 1, QV_A2, QV_A3,
    QV_B1 = 4, QV_B2, QV_B3, QV_B4, QV_B5, QV_B6, QV_B7,
    QV_C1 = 11, QV_C2, QV_C3, QV_C4,
    QV_D1 = 15, QV_D2, QV_D3, QV_D4, QV_D5, QV_D6,
    QV_E1 = 21, QV_E2, QV_E3, QV_E4, QV_E5, QV_E6,
    QV_F1 = 27, QV_F2, QV_F3, QV_F4,
    QV_PRE = 31, QV_JOBS = 32, QV_PLAN = 33,
    QV_QA = 48,
};
#define QV_FIRST 1
#define QV_LAST  30
#define QV_UNMEASURED 0xFFFFu

#define QD_LOADS   (1u << 0)
#define QD_BLOCKS  (1u << 1)
#define QD_ALU     (1u << 2)
#define QD_PU      (1u << 3)
#define QD_TCM     (1u << 4)
#define QD_CACHE   (1u << 5)
#define QD_REHEARSAL (1u << 8)

extern u32 qy_res[61];

// Empties the tally: every code unmeasured, no bit set.
void ledger_clear(void);
void ledger_flags(int word, u32 mask, bool ok);
// Publishes qy_res as the result block (magic last).
void ledger_publish(u32 demo_id, u32 frame);

#endif // QY_LEDGER_H
