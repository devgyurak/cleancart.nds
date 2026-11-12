// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Accuracy judge: renders a static scene, captures the display, counts pixels
// of fixed RGB555 colours and grades three emulator-accuracy cases.
//
// Result block (CvResult at CV_RESULT_ADDR, magic CV_RESULT_MAGIC once filled):
//   demo_id  JUDGE_ID
//   frame    frame the block was written (end of the 120-frame run)
//   values[0]  seam pixels per card row: row0 | row1 << 8 | row2 << 16
//              (expected 0 / 0 / 18)
//   values[1]  edge-marking pixels (> 0 passes)
//   values[2]  line-polygon pixels (> 0 passes)
//   values[3]  pass flags: bit0 seam, bit1 edge, bit2 line, bit3 lantern
//              blends alike in all three BLDCNT modes, bit4 darken works;
//              bits 8-10: the display capture of BLDCNT mode 0100 / 0140 / 01C1
//              finished (a value is only valid when its capture bit is set;
//              seam, edge, line and the no-effect backdrop come from capture 0,
//              the darkened backdrop from capture 2)
//   values[4..6]  lantern pixel with BLDCNT 0100 / 0140 / 01C1 (0xFFFF: not captured)
//   values[7..8]  backdrop pixel outside the lantern, no effect / darkened
//                 (0xFFFF: not captured)
//   The magic word is written last, after every value and a cache flush.

#ifndef LT_JUDGE_H
#define LT_JUDGE_H

#include <nds.h>
#include <stdbool.h>

#include "gfx/text.h"

#define JUDGE_ID 0x4A44474Cu  // "LGDJ"

typedef struct {
    int seam[3];
    u32 edges, lines;
    u16 lamp[3], out_none, out_dark;
    u32 flags;     // as values[3], including the capture bits 8-10
} JudgeResult;

// Sets up the 3D engine and runs the 120-frame measurement (blocking), shows
// the values on the top screen and fills res (may be NULL). With publish the
// result block is filled too, magic last; the rehearsal passes false and
// publishes one combined block itself.
void judge_run(JudgeResult *res, bool publish);

// One frame of the static scene after judge_run (A cycles BLDCNT).
void judge_frame(void);

// Writes a line on the judge screen (row 0-23), after judge_run.
void judge_text(int y, int pal, const char *s);

// Loads the 64x64 lantern glow into main OBJ VRAM tile 0 and OBJ palette 15
// (VL_JUDGE layout). Act 3 shares it with the judge.
void judge_lamp_load(void);

// Keeps the scene on screen forever (demo build).
void __attribute__((noreturn)) judge_idle(void);

#endif // LT_JUDGE_H
