// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Act 1 with scripted touch input, used by the rehearsal.

#ifndef LT_ACT1_H
#define LT_ACT1_H

#include <nds.h>
#include <stdbool.h>

// Touch script: from `frame` (counted from the start of the act) the pen is
// down at (tx, ty), or up when ty == 0, until the next step.
typedef struct {
    u16 frame;
    u8 tx, ty;
} Act1Step;

typedef struct {
    int score;        // silhouette match at the end, 0-100
    bool cleared;
    int clear_frame;  // frame (from the start of the act) the clear was reached, -1 if not
} Act1Outcome;

// Plays the act with the scripted touches (nothing is read from the pen, no
// save or NitroFS is used). Returns when the act has been cleared for 30
// frames or `end_frame` is reached.
void act1_replay(const Act1Step *steps, int n, int end_frame, Act1Outcome *out);

#endif // LT_ACT1_H
