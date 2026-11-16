// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Numbers the QA autopilot run changes (core/qa.h, qa_tune()). Ledgerine
// tunes nothing: the rooms are played with keys only, and what the audits measure
// must never change. The struct exists because core/qa.c needs one.

#ifndef LG_QA_TUNE_H
#define LG_QA_TUNE_H

#include <nds.h>

struct QaTune {
    u8 none;   // no tunable number
};

#endif // LG_QA_TUNE_H
