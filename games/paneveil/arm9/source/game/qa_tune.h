// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Numbers the QA autopilot run changes (core/qa.h, qa_tune()). Paneveil
// tunes nothing: the windows are solved from fixed scripts as shipped, and what
// the judge measures must never change. The struct exists because core/qa.c needs one.

#ifndef PV_QA_TUNE_H
#define PV_QA_TUNE_H

#include <nds.h>

struct QaTune {
    u8 none;   // no tunable number
};

#endif // PV_QA_TUNE_H
