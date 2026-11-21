// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#ifndef QY_STEP7_H
#define QY_STEP7_H

#include <stdint.h>

// Runs one measurement step on the ARM7 (cv_ipc.h CV_QY_*); returns its result code (qy_class.h).
uint32_t step7_run(uint32_t step);

#endif // QY_STEP7_H
