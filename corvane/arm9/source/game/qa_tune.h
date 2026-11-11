// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Numbers the QA autopilot run changes (core/qa.h, qa_tune()). Only difficulty
// numbers belong here: nothing that rendering, saving or judging reads.

#ifndef CV_QA_TUNE_H
#define CV_QA_TUNE_H

#include <nds.h>

struct QaTune {
    u16 boss_hp_pct;     // boss part HP, percent (100 = as shipped)
    u8 lives_lost_per_hit;   // 1 = a hit costs a life, 0 = the ship survives
    u8 lives;            // lives a new game starts with
    u8 bombs;            // bombs a new game starts with
};

#endif // CV_QA_TUNE_H
