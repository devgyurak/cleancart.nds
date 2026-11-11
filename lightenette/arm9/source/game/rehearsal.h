// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Rehearsal: act 1 played from a fixed touch script and acts 2 and 3 from fixed
// key scripts embedded in the ROM, then the accuracy judge, with one result block
// at the end. It reads no pen, uses
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
//   values[18]  act 3 cleared (1 = yes)
//   values[19]  act 3 marks found (0-3)
//   values[20..22]  act 3 lantern pixel with BLDCNT 0100 / 0140 / 01C1
//   values[23..24]  act 3 backdrop pixel outside the lantern, no effect / darkened
//   values[25]  1 if the act 3 display captures finished
//   values[26]  act 3 frame of the clear (counted from the act's start)

#ifndef LT_REHEARSAL_H
#define LT_REHEARSAL_H

#include <nds.h>

#define REHEARSAL_ID 0x4C454852u  // "RHEL"

// Runs the rehearsal (blocking) and leaves the result block filled.
void rehearsal_run(void);

// Keeps the final screen forever (rehearsal ROM).
void __attribute__((noreturn)) rehearsal_idle(void);

#endif // LT_REHEARSAL_H
