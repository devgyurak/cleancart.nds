// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal: a demo ROM (make DEMO=rehearsal) that runs the kiln check and the light
// box of the judge and plays the first window of every chapter with a fixed input script through
// the live play scene, then publishes one result block at CV_RESULT_ADDR for tools/judge.py.
// No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "PVRH" = 0x48525650.
// The layout is filled in by the judge changes; values[0] is the flags word.

#ifndef PV_REHEARSAL_H
#define PV_REHEARSAL_H

#define REHEARSAL_ID 0x48525650u   // "PVRH"

void rehearsal_run(void);    // plays, measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

#endif // PV_REHEARSAL_H
