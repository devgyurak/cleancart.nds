// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Act 2 with scripted key input, used by the rehearsal.

#ifndef LT_ACT2_H
#define LT_ACT2_H

#include <nds.h>
#include <stdbool.h>

// Key script: at `frame` (counted from the start of the act) the keys are
// pressed for that one frame.
typedef struct {
    u16 frame;
    u16 keys;
} Act2Step;

typedef struct {
    bool cleared;
    int clear_frame;  // frame the clear was reached, -1 if not
    u32 measured;     // capture bits that finished (1 = the seam capture)
    u32 seam;         // cut-line pixels per doll: doll0 | doll1 << 8 | doll2 << 16
    u32 edges;        // edge-marking pixels around the plinths
} Act2Outcome;

// Plays the act from the key script (no pen, no save, no NitroFS). Returns
// when the act has been cleared for 30 frames or `end_frame` is reached.
void act2_replay(const Act2Step *steps, int n, int end_frame, Act2Outcome *out);

#endif // LT_ACT2_H
