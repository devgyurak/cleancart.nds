// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Numbers the QA autopilot run changes (core/qa.h, qa_tune()). Lightenette
// tunes nothing: the acts clear from the rehearsal scripts as shipped, and what
// the judge measures (pictures, captures, blends, edges, lines) must never
// change. The struct exists because core/qa.c needs one.

#ifndef LT_QA_TUNE_H
#define LT_QA_TUNE_H

#include <nds.h>

struct QaTune {
    u8 none;   // no tunable number
};

#endif // LT_QA_TUNE_H
