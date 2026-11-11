// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Rehearsal: act 1 played from a fixed touch script and act 2 from a fixed key
// script embedded in the ROM, then the accuracy judge, with one result block at the end. It reads no pen, uses
// no save chip and no NitroFS, so it runs the same on any cart kernel.
//
// Result block (CvResult at CV_RESULT_ADDR, magic CV_RESULT_MAGIC once filled):
//   demo_id  REHEARSAL_ID
//   frame    frame the block was written
//   values[0..8]  the judge's values, layout in judge/judge.h
//   values[9]   act 1 silhouette score at the end (0-100)
//   values[10]  act 1 cleared (1 = yes)
//   values[11]  act 1 frame of the clear (counted from the act's start)
//   values[12]  1 once the whole rehearsal has finished
//   values[13]  act 2 cleared (1 = yes)
//   values[14]  act 2 cut-line pixels per doll: d0 | d1 << 8 | d2 << 16 (0/0/18)
//   values[15]  act 2 edge-marking pixels around the plinths
//   values[16]  act 2 frame of the clear (counted from the act's start)
//   values[17]  1 if the act 2 display capture finished

#ifndef LT_REHEARSAL_H
#define LT_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID 0x4C454852u  // "RHEL"

// Runs the rehearsal (blocking) and leaves the result block filled.
void rehearsal_run(void);

// Keeps the final screen forever (rehearsal ROM).
void __attribute__((noreturn)) rehearsal_idle(void);

#endif // LT_REHEARSAL_H
