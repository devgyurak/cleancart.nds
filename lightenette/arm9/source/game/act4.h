// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Act 4 with scripted key input, used by the rehearsal.

#ifndef LT_ACT4_H
#define LT_ACT4_H

#include <nds.h>
#include <stdbool.h>

// Key script: at `frame` (counted from the start of the act) the keys are
// pressed for that one frame.
typedef struct {
    u16 frame;
    u16 keys;
} Act4Step;

typedef struct {
    bool cleared;
    int clear_frame;  // frame the clear was reached, -1 if not
    bool measured;    // the thread capture finished
    u32 hline;        // pixels of the horizontal thread
    u32 dline;        // pixels of the diagonal thread
} Act4Outcome;

// Plays the act from the key script (no pen, no save, no NitroFS). Returns
// when the act has been cleared for 30 frames or `end_frame` is reached.
void act4_replay(const Act4Step *steps, int n, int end_frame, Act4Outcome *out);

#endif // LT_ACT4_H
