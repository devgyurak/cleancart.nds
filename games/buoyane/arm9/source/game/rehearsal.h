// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rehearsal (make DEMO=rehearsal) and the five check ROMs (DEMO=check-dot ... check-blend) run the light
// check (judge/lightcheck.c) and publish one result block at CV_RESULT_ADDR for tools/judge.py. A check ROM
// runs one scene and fills only that scene's words. No save chip, no NitroFS.
//
// Block (CvResult, 64 words: magic, id, frame, values[0..60]); id "BYRH" = 0x48525942 for the rehearsal; a
// check ROM uses the id "ck-<scene>" (see demo.c), the QA run "QARN".
//   values[0]   capture bits: 0 dot, 1 fog, 2 edge, 3 depth, 4 blend (a scene's bit is set when all its captures finished)
//   values[1]   dot bits: 0 near dot shown, 1 far 0x0 hidden, 2 PA bit 13 far dot shown, 3 mixed-W forward red,
//               4 reverse green, 5 far 1x0 shown, 6 depth 0 hides W 1, 7 depth 7FFF shows W 1/64/1000,
//               8 AA opaque dot hidden, 9 the same dot shown without AA, 10 AA translucent dot shown
//   values[2]   mixed-W dot colours: forward | reverse << 16 (raw RGB555)
//   values[3]   fog bits: 0 near fog face = reference, 1 far fog face blended, 2 density 127 = fog colour,
//               3 density 120 differs, 4 translucent case A (no fog), 5 case B (fog), 6 rear plane fog bit set -> fog,
//               7 clear -> no fog
//   values[4]   near fog | reference << 16;  values[5] density 127 | 120 << 16 (raw)
//   values[6]   edge bits: 0-3 front-only edges, 4 translucent edge+AA, 5 edge marks something, 6 AA marked line unbroken
//   values[7,8] edge pixels (raw)
//   values[9]   depth bits: 0 flat equal draws, 1 -2 steps hidden, 2 +2 steps hidden, 3 decal whole, 4 no base -> empty,
//               5 bit 12 clear hides, 6 bit 12 set draws, 7 POLYGON_ATTR in Begin/End latched, 8 alpha test ref 1F hides
//   values[10,11] equal band left and right x;  values[12] -2 | +2 step pixel counts << 16;  values[13] -1 | +1 step
//   values[14]  AA line pixels: opaque | translucent << 16 (record)
//   values[15]  blend bits: 0 highlight saturates, 1 decal alpha 0 -> vertex colour, 2 wireframe texture alpha blends,
//               3 clear alpha 0 bypass, 4 toon red index, 5 3D alpha through the BG0 blend
//   values[16..18] highlight | decal << 16, wire | bypass << 16, decal holes (raw)
//   values[19]  approaches cleared (the first of every chapter, scripted; the QA run: all 30);  values[20] route plan hash (FNV-1a of the routes); values[21] wire quad pixels: opaque white | blended << 16 (record);  values[48..54] QA words
// Keep this file, expected.json and tools/judge.py in step.

#ifndef BY_REHEARSAL_H
#define BY_REHEARSAL_H

#define REHEARSAL_ID 0x48525942u   // "BYRH"

void rehearsal_run(void);    // measures, publishes
void rehearsal_idle(void);   // never returns: keeps the block in RAM

// The check ROMs: one light-check scene each (scene index of judge/lightcheck.h).
void check_run(int scene);
void check_idle(void);

#endif // BY_REHEARSAL_H
