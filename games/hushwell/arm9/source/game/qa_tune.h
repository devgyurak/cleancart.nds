// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Numbers the QA autopilot run changes (core/qa.h, qa_tune()). Only difficulty
// and determinism numbers belong here: nothing that rendering, saving or
// judging reads.

#ifndef CV_QA_TUNE_H
#define CV_QA_TUNE_H

#include <nds.h>

struct QaTune {
    u8 hurt_damage;      // health lost per creature hit: 1 as shipped, 0 in QA (effects stay)
    u32 seed;            // run seed; 0 = the save's own
};

#endif // CV_QA_TUNE_H
