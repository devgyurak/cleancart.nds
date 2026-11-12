// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Rehearsal: act 1 played from a fixed touch script and acts 2 to 5 from fixed
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
//   values[17]  1 if the act 2 display capture finished (0: seam/edge not graded)
//   values[18]  act 3 cleared (1 = yes)
//   values[19]  act 3 marks found (0-3)
//   values[20..22]  act 3 lantern pixel with BLDCNT 0100 / 0140 / 01C1
//   values[23..24]  act 3 backdrop pixel outside the lantern, no effect / darkened
//   values[25]  act 3 captures that finished, bit m = BLDCNT mode m (7 = all three)
//   values[26]  act 3 frame of the clear (counted from the act's start)
//   values[27]  act 4 cleared (1 = yes)
//   values[28]  act 4 frame of the clear
//   values[29]  act 4 horizontal thread pixels (line polygon, information only)
//   values[30]  act 4 diagonal thread pixels (line polygon, information only)
//   values[31]  1 if the act 4 display capture finished (0: counts not graded)
//   values[32]  act 5 cleared (1 = yes)
//   values[33]  act 5 frame of the clear
//   values[34]  act 5 cut-line pixels per doll (u8x3, expected 0/0/18)
//   values[35..37]  act 5 edge pixels of the three plinth colours
//   values[38]  act 5 string line-polygon pixels (information only)
//   values[39..41]  act 5 lantern pixel with BLDCNT 0100 / 0140 / 01C1
//   values[42..43]  act 5 backdrop pixel outside the lantern, no effect / darkened
//   values[44]  act 5 captures that finished, bit m = BLDCNT mode m (7 = all three)
//   values[3] bits 8-10 do the same for the judge's three captures. A value that
//   belongs to a capture that did not finish reads 0xFFFF (pixels) and its
//   capture bit is clear; tools/judge.py grades such an item NG.
//   The magic word is written last, after the values and a cache flush.
//   The whole block is 64 words (CvResult.values[61]).

#ifndef LT_REHEARSAL_H
#define LT_REHEARSAL_H

#include <nds.h>

#include "game/act1.h"
#include "game/act2.h"
#include "game/act3.h"
#include "game/act4.h"
#include "game/act5.h"

#define REHEARSAL_ID 0x4C454852u  // "RHEL"

// Runs the rehearsal (blocking) and leaves the result block filled.
void rehearsal_run(void);

// The rehearsal's input scripts (the auto QA run plays the same ones through the
// normal scenes). `*end` is the frame the rehearsal gives each act up at.
const Act1Step *rehearsal_act1_script(int *n, int *end);
const Act2Step *rehearsal_act2_script(int *n, int *end);
const Act3Step *rehearsal_act3_script(int *n, int *end);
const Act4Step *rehearsal_act4_script(int *n, int *end);
const Act5Step *rehearsal_act5_script(int *n, int *end);

// Keeps the final screen forever (rehearsal ROM).
void __attribute__((noreturn)) rehearsal_idle(void);

#endif // LT_REHEARSAL_H
