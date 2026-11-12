// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal: a demo ROM (make DEMO=rehearsal) that plays the first puzzle of every
// chapter that has puzzles with a fixed input script through the live play scene, then
// runs both showcase presets, and publishes one result block at CV_RESULT_ADDR for
// tools/judge.py. No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "EVRH" = 0x48525645.
//   values[0]   flags: bit 0 every scripted puzzle delivered, 1 front picture in bank D
//               (clear-colour pixels > 0, no plan clear colour), 2 plan picture in bank C,
//               3 each showcase pass under 2048 polygons and the two together over it
//               (needs the polygon counters), 4 no polygon/vertex overflow flag,
//               5 preset 1 sub BG holes 0, 6 preset 1 composite has pixels left and
//               right, 7 preset 2 captured bank has pixels, 8 polygon counters
//               available (DeSmuME returns 0)
//   values[1]   routing latency in calls (a constant, see gfx/dual.c)
//   values[2]   front clear-colour pixels in bank D, last puzzle (every 8th line: 24 of 192)
//   values[3]   plan clear-colour pixels in bank C, last puzzle (every 8th line)
//   values[4]   cross: plan clear in D << 16 | front clear in C (both must be 0)
//   values[5]   bit mask of chapters played, bit n-1 = chapter n
//   values[6]   puzzles delivered
//   values[7]   showcase polygons: left pass | right pass << 16 (preset 1)
//   values[8]   showcase vertices: left | right << 16 (preset 1)
//   values[9]   overflow flags: preset 1 left, right (bits 0,1), preset 2 (bits 2,3)
//   values[10]  preset 1 holes after capture-only frames (every 4th line sampled)
//   values[11]  preset 1 holes after blend frames
//   values[12]  preset 1 composite opaque pixels left of x = 128 (every 8th line, full is 3072)
//   values[13]  preset 1 composite opaque pixels right of x = 128 (full is 3072)
//   values[14]  preset 1 composite hash (FNV-1a of the sampled lines of the sub BG bitmap)
//   values[15]  preset 2 opaque pixels (every 12th) of the bank captured last (full is 4096)
//   values[16]  preset 2 polygons: left | right << 16
//   values[17]  puzzle blocks used in the last puzzle << 8 | its par

#ifndef EV_REHEARSAL_H
#define EV_REHEARSAL_H

#define REHEARSAL_ID 0x48525645u   // "EVRH"

void rehearsal_run(void);    // plays, measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

#endif // EV_REHEARSAL_H
