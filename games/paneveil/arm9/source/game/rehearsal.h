// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal: a demo ROM (make DEMO=rehearsal) that runs the kiln check and the light
// box of the judge and plays the first window of every chapter with a fixed input script through
// the live play scene, then publishes one result block at CV_RESULT_ADDR for tools/judge.py.
// No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "PVRH" = 0x48525650.
//   values[0]   flags: bit 0 the kiln check ran, 1 every kiln value is the expected one
//   values[1]   windows baked by the scripted play (5)
//   values[2]   reserved
//   values[3..15]  the kiln check (judge/bench.h, KILN_*): stack levels, words that differ
//               from the model after pop / pop N / store / restore, the projection stack,
//               CLIPMTX_RESULT and VECMTX_RESULT against the model, packed GXFIFO and direct ports
// Keep this file, expected.json and tools/judge.py in step.

#ifndef PV_REHEARSAL_H
#define PV_REHEARSAL_H

#define REHEARSAL_ID 0x48525650u   // "PVRH"

void rehearsal_run(void);    // plays, measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

#endif // PV_REHEARSAL_H
