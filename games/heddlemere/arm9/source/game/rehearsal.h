// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal: a demo ROM (make DEMO=rehearsal) that measures the scanline and 3D engine timing of the
// judge, plays the first order of every chapter with a fixed input script through the live play scene and
// publishes one result block at CV_RESULT_ADDR for tools/judge.py. No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "HMRH" = 0x48524D48.
//   values[0]   flags: bit 0 every measurement finished (no wait timed out), bit 5 the ARM7 core answered,
//               bit 6 the ARM7 measured its HBlank span
//   values[1..20]  the timing measurements (judge/timing.h, TM_*): line and frame length, HBlank low span on
//               ARM9 / ARM7 (x16), VBlank flag lines, LYC / swap flags, VCOUNT write lines and residual, geometry
//               command slopes (x16), swap tail, RAM_COUNT at lines 191 / 192; 0xFFFF = not measured
//   values[21]  orders delivered by the scripted play (5), in the QA run all orders (40)
//   values[22]  mask of the chapters whose first order was delivered (0x1f), in the QA run chapters finished
//   values[23]  plan hash: FNV-1a of the ticks the planned programs of each chapter's first order run
//   values[48..54]  QA words (QA boot only, like Paneveil; 48 = verdict)
// Keep this file, expected.json and tools/judge.py in step.

#ifndef HM_REHEARSAL_H
#define HM_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID 0x48524D48u   // "HMRH"

enum {
    RH_FLAGS = 0,
    RH_ORDERS = 21,
    RH_CHAPTERS = 22,
    RH_PLAN_HASH = 23,
    RH_VALUES = 32,
};

#define RH_FLAG_DONE (1u << 0)
#define RH_FLAG_ARM7 (1u << 5)
#define RH_FLAG_ARM7_SPAN (1u << 6)

u32 rehearsal_flags(void);   // the flags every boot reports (ARM7 answered)
// Runs the ARM7 and ARM9 timing measurements into r[0..20] (flags in r[0]); draws nothing.
void rehearsal_measure(u32 *r);
void rehearsal_run(void);    // plays, measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

#endif // HM_REHEARSAL_H
