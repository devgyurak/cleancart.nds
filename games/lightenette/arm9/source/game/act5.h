// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Act 5 with scripted key input, used by the rehearsal.

#ifndef LT_ACT5_H
#define LT_ACT5_H

#include <nds.h>
#include <stdbool.h>

// Key script: the keys are held for `len` frames starting at `frame` (counted
// from the start of the act); L, R and A also count as pressed on `frame`.
typedef struct {
    u16 frame;
    u16 len;
    u16 keys;
} Act5Step;

typedef struct {
    bool cleared;
    int clear_frame;   // frame the clear was reached, -1 if not
    u32 measured;      // capture bits that finished, bit m = BLDCNT mode m (7 = all)
    u32 seam;          // cut-line pixels per doll: doll0 | doll1 << 8 | doll2 << 16
    u32 edge[3];       // edge-marking pixels of the three plinth colours
    u32 lines;         // string line-polygon pixels
    u16 lamp[3];       // lantern pixel with BLDCNT 0100 / 0140 / 01C1
    u16 out_none;      // backdrop pixel outside the lantern, BLDCNT 0100
    u16 out_dark;      // ... BLDCNT 01C1
} Act5Outcome;

// Plays the act from the key script (no pen, no save, no NitroFS). Returns
// when the measurement is done or `end_frame` is reached.
void act5_replay(const Act5Step *steps, int n, int end_frame, Act5Outcome *out);

#endif // LT_ACT5_H
