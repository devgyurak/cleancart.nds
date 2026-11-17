// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal: a demo ROM (make DEMO=rehearsal) that runs the bench and publishes one result block at
// CV_RESULT_ADDR for tools/judge.py. No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "PDRH" = 0x48524450.
//   values[0]   flags: bit 0 the math bench found every edge, bit 2 the timer cascade ran
//   values[3]   DIV 32/32 edge minus SQRT edge, ARM9 cycles (half bus cycles; signed)
//   values[4]   DIV 64/32 edge minus DIV 32/32 edge (signed)
//   values[5]   DIV 64/64 edge minus DIV 64/32 edge (signed)
//   values[23]  prerequisite flags: bit 0 the math results were right, bit 2 the timer cascade works
//   values[24..27] raw edges: DIV 32/32, DIV 64/32, DIV 64/64, SQRT (sled length; 0xFFFFFFFF = not found)
// Items that are not measured yet read 0xFFFF (NG). Keep this file, expected.json and tools/judge.py in step.

#ifndef PD_REHEARSAL_H
#define PD_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID 0x48524450u   // "PDRH"
#define PD_VALUES 56

// Slots of values[].
enum {
    PDV_FLAGS = 0,
    PDV_MATH_DIV_SQRT = 3,
    PDV_MATH_64_32 = 4,
    PDV_MATH_6464_6432 = 5,
    PDV_PRE = 23,
    PDV_RAW = 24,
};

// Fills res[0..PD_VALUES) from the bench (the same measurement in the rehearsal and in the QA epilogue).
void bench_fill(u32 *res);

void rehearsal_run(void);    // measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

#endif // PD_REHEARSAL_H
