// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Routes and the almanac (generated from tools/byroute: assets.h by_look, cv_routes). A chapter has
// BY_APPROACHES approaches of BY_GATES gates. At a gate the 3D view shows the light of the true type in a
// view; the chart offers three candidate types and exactly one of them has that look (the checker proves it).

#ifndef BY_MODEL_H
#define BY_MODEL_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    u8 view;      // 0..2
    u8 truth;     // the light that is drawn, 0..5
    u8 opt[3];    // the candidate types on the chart
} ByGate;

// Chapter 1..BY_CHAPTERS, approach 0..BY_APPROACHES-1, gate 0..BY_GATES-1.
ByGate by_gate(int chapter, int approach, int gate);

// The look (0..3) of a light type in a view, from the almanac.
int by_look_of(int chapter, int type, int view);

// The option index (0..2) the almanac names for the gate: the one whose look equals the truth's.
int by_almanac_answer(int chapter, ByGate g);

// FNV-1a of the packed routes; equal to BY_PLAN_HASH when the blob is intact.
u32 by_plan_hash(void);

#endif // BY_MODEL_H
