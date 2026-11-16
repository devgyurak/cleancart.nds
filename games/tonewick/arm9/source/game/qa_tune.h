// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Numbers the QA autopilot run changes (core/qa.h, qa_tune()). Tonewick
// tunes nothing: the districts are played with keys only, and what the audits measure
// must never change. The struct exists because core/qa.c needs one.

#ifndef TW_QA_TUNE_H
#define TW_QA_TUNE_H

#include <nds.h>

struct QaTune {
    u8 none;   // no tunable number
};

#endif // TW_QA_TUNE_H
